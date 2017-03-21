#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/CallSite.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Debug.h>

#include <string>
#include <list>
#include <set>
#include <vector>
#include <cassert>

#include <Utils.h>
#include <metadata.h>

#define DEBUG_TYPE "mask-pointers"

/*
 * TODO:
 *  - custom mem allocators
 *  - saturation arith (look into x86_64 possibilities and perf)
 *  - ARM bench: better support imm64 and saturation arith (not for gpr?)
 *  - check amount of dyn ptr arith
 *  - check amount of negative ptr arith
 *  - check alloc sizes
 *
 *  null ptr derefs
 */

/* For 64-bit pointer mode (i.e., bound in sign-extend bit) */
//#if MODE64
//#  define PTR_MASK (0x7fffffffffffULL) /* 47 bits (user addrs) */
//#else
//#  define PTR_MASK (0xffffffffULL) /* 47 bits (user addrs) */
//#endif

#define OVERFLOW_MASK (1ULL << 63)

using namespace llvm;

struct MaskPointers : public FunctionPass {
    static char ID;

    MaskPointers() : FunctionPass(ID) {}
    bool runOnFunction(Function &F) override;

    void instrumentCallExt(CallSite *CS);
    void instrumentCallExtWrap(CallSite *CS);
    void instrumentCallByval(CallSite *CS);
    void instrumentCallExtNestedPtrs(CallSite *CS);
    void instrumentCmpPtr(CmpInst *CMP);
    void instrumentMemAccess(Instruction *ins);
    void instrumentPtrSub(Instruction *ins);
};

char MaskPointers::ID = 0;
static RegisterPass<MaskPointers> X("mask-pointers",
        "Mask high bits of pointers at loads and stores (SFI)");

static inline Value *maskPointer(Value *ptr, IRBuilder<> &B) {
    Value *asInt = B.CreatePtrToInt(ptr, B.getInt64Ty(), "as_int");
    Value *masked = B.CreateAnd(asInt, PTR_MASK | OVERFLOW_MASK, "masked");
    return B.CreateIntToPtr(masked, ptr->getType(), "as_ptr");
}

static void maskPointerArgs(CallSite *CS) {
    IRBuilder<> B(CS->getInstruction());

    for (unsigned i = 0, n = CS->getNumArgOperands(); i < n; i++) {
        Value *arg = CS->getArgOperand(i);
        if (arg->getType()->isPointerTy())
            CS->setArgument(i, maskPointer(arg, B));
    }
}

/*
 * Mask pointers to external functions.
 * TODO: don't do this if we can determine it's not eg heap based.
 */
void MaskPointers::instrumentCallExt(CallSite *CS) {
    Function *F = CS->getCalledFunction();
    if (CS->isInlineAsm())   /* XXX inline asm should actually be masked? */
        return;
    if (!F)                  /* XXX indirect calls? */
        return;
    if (!F->isDeclaration()) /* not external */
        return;

    // FIXME: use subclasses of CallInst here
    if (F->getName().startswith("llvm.eh.") ||
        F->getName().startswith("llvm.dbg.") ||
        F->getName().startswith("llvm.lifetime."))
        return;

    maskPointerArgs(CS);
}

void MaskPointers::instrumentCallExtWrap(CallSite *CS) {
    Function *F = CS->getCalledFunction();
    if (CS->isInlineAsm() || !F)
        return;

    if (F->getName() != "_E__pr_info" && /* sphinx3 vfprintf wrapper */
        F->getName() != "_ZN12pov_frontend13MessageOutput6PrintfEiPKcz" && /* povray vsnprintf wrapper */
        F->getName() != "_ZN8pov_base16TextStreamBuffer6printfEPKcz" && /* povray vsnprintf wrapper */
        F->getName() != "_ZN3pov10Debug_InfoEPKcz" && /* povray vsnprintf wrapper */
        F->getName() != "_ZN6cEnvir9printfmsgEPKcz") /* omnetpp vsprintf wrapper */
        return;

    maskPointerArgs(CS);
}

void MaskPointers::instrumentCallByval(CallSite *CS) {
    Function *F = CS->getCalledFunction();
    if (CS->isInlineAsm()) /* XXX inline asm should actually be masked? */
        return;
    if (F && F->isDeclaration()) /* external functions are handled above */
        return;

    IRBuilder<> B(CS->getInstruction());

    for (unsigned i = 0, n = CS->getNumArgOperands(); i < n; i++) {
        Value *arg = CS->getArgOperand(i);
        if (arg->getType()->isPointerTy() && CS->paramHasAttr(i + 1, Attribute::ByVal))
            CS->setArgument(i, maskPointer(arg, B));
    }
}

static void maskNestedPointers(Value *val, CompositeType *elTy,
        std::vector<Value*> &indices, IRBuilder<> &B) {
    unsigned n = elTy->isStructTy() ?
        cast<StructType>(elTy)->getNumElements() :
        cast<ArrayType>(elTy)->getNumElements();

    for (unsigned i = 0; i < n; i++) {
        Type *ty = elTy->getTypeAtIndex(i);

        if (ty->isPointerTy()) {
            DEBUG(dbgs() << "masking nested pointer of type " << *ty <<
                    " in value: " << *val << "\n");
            indices.push_back(B.getInt32(i));
            Value *ptr = B.CreateInBoundsGEP(val, indices);
            indices.pop_back();
            Value *masked = maskPointer(B.CreateLoad(ptr), B);
            B.CreateStore(masked, ptr);
        }
        else if (ty->isAggregateType()) {
            indices.push_back(B.getInt32(i));
            maskNestedPointers(val, cast<CompositeType>(ty), indices, B);
            indices.pop_back();
        }
    }
}

void MaskPointers::instrumentCallExtNestedPtrs(CallSite *CS) {
    static std::map<StringRef, std::vector<unsigned>> whitelist = {
        /* inlined std::list::push_back */
        {"_ZNSt8__detail15_List_node_base7_M_hookEPS0_", {1}},
        /* inlined std::string += std::string */
        {"_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE9_M_appendEPKcm", {0}}
    };

    Function *F = CS->getCalledFunction();
    if (!F)
        return;

    auto it = whitelist.find(F->getName());
    if (it == whitelist.end())
        return;

    assert(F->isDeclaration());
    IRBuilder<> B(CS->getInstruction());
    std::vector<Value*> indices = {B.getInt64(0)};

    for (unsigned i : it->second) {
        Value *arg = CS->getArgOperand(i);
        Type *elTy = cast<PointerType>(arg->getType())->getElementType();
        assert(elTy->isAggregateType());
        DEBUG(dbgs() << "mask nested pointers in arg " << i <<
                " of:" << *CS->getInstruction() << "\n");
        maskNestedPointers(arg, cast<CompositeType>(elTy), indices, B);
    }
}

void MaskPointers::instrumentCmpPtr(CmpInst *ins) {
    Value *arg1 = ins->getOperand(0);
    Value *arg2 = ins->getOperand(1);
    assert(arg1->getType()->isPointerTy() == arg2->getType()->isPointerTy());
    if (!arg1->getType()->isPointerTy())
        return;

    IRBuilder<> B(ins);
    ins->setOperand(0, maskPointer(arg1, B));
    ins->setOperand(1, maskPointer(arg2, B));
}

/*
 * Mask out metadata bits in pointers when a pointer is accessed. It does not
 * mask out the overflow bit, so out-of-bound accesses will cause a fault.
 */
void MaskPointers::instrumentMemAccess(Instruction *ins) {
    int ptrOperand = isa<StoreInst>(ins) ? 1 : 0;
    IRBuilder<> B(ins);
    Value *ptr = ins->getOperand(ptrOperand);
    ins->setOperand(ptrOperand, maskPointer(ptr, B));

    /* Also mask writes of pointers to externs (e.g., environ). */
    /* TODO: we don't have to mask the ptr above if global value */
    GlobalVariable *gv = dyn_cast<GlobalVariable>(ptr->stripPointerCasts());
    if (isa<StoreInst>(ins) && gv && !gv->hasInitializer() && gv->getType()->isPointerTy()) {
        ins->setOperand(0, maskPointer(ins->getOperand(0), B));
    }
}

/*
 * Pointer subtraction looks like this:
 *   %a = ptrtoint %ptr_a
 *   %b = ptrtoint %ptr_b
 *   %diff = sub %a, %b
 *   %offset = div %diff, <element size>
 *
 * Mask %ptr_a and %ptr_b to avoid incorporating size metadata in the offset.
 */
void MaskPointers::instrumentPtrSub(Instruction *ins) {
    PtrToIntInst *a = dyn_cast<PtrToIntInst>(ins->getOperand(0));
    PtrToIntInst *b = dyn_cast<PtrToIntInst>(ins->getOperand(1));
    if (!a || !b)
        return;

    //errs() << "instrumenting pointer subtraction in " <<
    //    ins->getParent()->getParent()->getName()
    //    << ":" << *ins << "\n";

    IRBuilder<> B(ins);
    ins->setOperand(0, B.CreateAnd(a, PTR_MASK));
    ins->setOperand(1, B.CreateAnd(b, PTR_MASK));
}


bool MaskPointers::runOnFunction(Function &F) {
    if (ISMETADATAFUNC(F.getName().str().c_str()))
        return false;

    //DEBUG(errs() << "Function " << F.getName() << " before: " << F << "\n");

    std::vector<Instruction*> mems;
    std::vector<GetElementPtrInst*> geps;

    for (BasicBlock &bb : F) {
        for (Instruction &i : bb) {
            Instruction *ins = &i;
            if (isa<StoreInst>(ins) || isa<LoadInst>(ins)) {
                mems.push_back(ins);
            }
            else if (isa<CallInst>(ins) || isa<InvokeInst>(ins)) {
                CallSite CS(ins);
                instrumentCallExt(&CS);
                instrumentCallExtWrap(&CS);
                instrumentCallByval(&CS);
                instrumentCallExtNestedPtrs(&CS);
            }
            else if (CmpInst *CMP = dyn_cast<CmpInst>(ins)) {
                instrumentCmpPtr(CMP);
            }
            else if (ins->isBinaryOp() && ins->getOpcode() == Instruction::Sub) {
                instrumentPtrSub(ins);
            }
        }
    }

    for (Instruction *mem : mems)
        instrumentMemAccess(mem);

    //DEBUG(errs() << "Function " << F.getName() << " after: " << F << "\n");

    return true;
}
