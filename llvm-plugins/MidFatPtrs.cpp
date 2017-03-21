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

#include <metadata.h>            /* defines PTR_BITS */
#include <metapagetable_core.h>  /* defines pageTable */

#define DEBUG_TYPE "MidFatPtrs"

#include "Utils.h"
#include "MetaPointerUtils.h"
#include "PointerSink.h"

/* Disable certains steps of the pass for partial numbers/testing */

using namespace llvm;

class MidFatPtrs : public ModulePass {
public:
    static char ID;
    MidFatPtrs() : ModulePass(ID) {}
    virtual bool runOnModule(Module &M);

private:
    Module *M;
    Function *LookupMetaPtrFunc;

    bool runOnFunction(Function &F);

    void instrumentCallAlloc(CallSite *CS);
    void instrumentCallExt(CallSite *CS);
    void instrumentCallExtWrap(CallSite *CS);
    void instrumentCallByval(CallSite *CS);
    void instrumentCallExtNestedPtrs(CallSite *CS);
    void instrumentCmpPtr(CmpInst *ins);
    void instrumentMemAccess(Instruction *ins);
    void instrumentSinks(sinklist_t &Sinks);
    void instrumentGlobals(Module &M);

    void putMetaPointerInHighBits(Instruction *Ptr);
    Value *buildMetaPointer(Value *Ptr, IRBuilder<> &B);
    Value *buildFatPointer(Value *Ptr, Value *HighBits, IRBuilder<> &B);
};

char MidFatPtrs::ID = 0;
static RegisterPass<MidFatPtrs> X("midfatptrs", "MidFat pointers for bounds checking");

/*
 * Get the insert point after the specified instruction. For non-terminators
 * this is the next instruction. For `invoke` intructions, create a new
 * fallthrough block that jumps to the default return target, and return the
 * jump instruction.
 */
static Instruction *getInsertPointAfter(Instruction *ins) {
    if (InvokeInst *invoke = dyn_cast<InvokeInst>(ins)) {
        BasicBlock *dst = invoke->getNormalDest();
        BasicBlock *newBlock = BasicBlock::Create(ins->getContext(),
                "invoke_insert_point", dst->getParent(), dst);
        BranchInst *br = BranchInst::Create(dst, newBlock);
        invoke->setNormalDest(newBlock);

        /* Patch references in PHI nodes in original successor */
        BasicBlock::iterator it(dst->begin());
        while (PHINode *phi = dyn_cast<PHINode>(it)) {
            int i;
            while ((i = phi->getBasicBlockIndex(invoke->getParent())) >= 0)
                phi->setIncomingBlock(i, newBlock);
            it++;
        }

        return br;
    }

    if (isa<PHINode>(ins))
        return ins->getParent()->getFirstInsertionPt();

    assert(!isa<TerminatorInst>(ins));
    return std::next(BasicBlock::iterator(ins));
}

static inline Value *maskPointer(Value *ptr, IRBuilder<> &B) {
    if (isPtrIntTy(ptr->getType()))
        return B.CreateAnd(ptr, PTR_MASK, "masked");

    assert(ptr->getType()->isPointerTy());
    Value *asInt = B.CreatePtrToInt(ptr, B.getInt64Ty(), "as_int");
    Value *masked = B.CreateAnd(asInt, PTR_MASK, "masked");
    return B.CreateIntToPtr(masked, ptr->getType(), "as_ptr");
}

Value *MidFatPtrs::buildMetaPointer(Value *Ptr, IRBuilder<> &B) {
    Value *PtrInt = B.CreatePtrToInt(Ptr, B.getInt64Ty(), "ptrint");
    return B.CreateCall(LookupMetaPtrFunc, PtrInt, "metaptr");
}

Value *MidFatPtrs::buildFatPointer(Value *Ptr, Value *HighBits, IRBuilder<> &B) {
    Value *Upper = B.CreateShl(HighBits, PTR_BITS, "highbits");
    Value *LowBits = B.CreatePtrToInt(Ptr, B.getInt64Ty(), "lowbits");
    Value *Combined = B.CreateOr(LowBits, Upper, "combined");
    return B.CreateIntToPtr(Combined, Ptr->getType(), "fatptr");
}

/*
 * Look up the meta pointer of a pointer and store it in its high bits.
 */
void MidFatPtrs::putMetaPointerInHighBits(Instruction *Ptr) {
    assert(Ptr->getType()->isPointerTy());

    /* Cache uses before creating more */
    std::vector<User*> Users(Ptr->user_begin(), Ptr->user_end());

    /* Shift metaptr and put in high bits of allocated pointer */
    IRBuilder<> B(getInsertPointAfter(Ptr));
    Value *MetaPtr = buildMetaPointer(Ptr, B);
    Value *New = buildFatPointer(Ptr, MetaPtr, B);

    /* Replace uses */
    for (User *U : Users)
        U->replaceUsesOfWith(Ptr, New);
}

static bool isMalloc(Function *F) {
    /* TODO: str[n]dup, [posix_]memalign, msvc new ops */
    static std::set<std::string> mallocFuncs = {
        "malloc",
        "valloc",
        "_Znwj", /* new(unsigned int) */
        "_ZnwjRKSt9nothrow_t",
        "_Znwm", /* new(unsigned long) */
        "_ZnwmRKSt9nothrow_t",
        "_Znaj", /* new[](unsigned int) */
        "_ZnajRKSt9nothrow_t",
        "_Znam", /* new[](unsigned long) */
        "_ZnamRKSt9nothrow_t",

        /* custom allocators */
        "Perl_safesysmalloc",
        "xmalloc"
    };
    return mallocFuncs.find(F->getName().str()) != mallocFuncs.end();
}

static bool isCalloc(Function *F) {
    return F->getName() == "calloc";
}

static bool isRealloc(Function *F) {
    static std::set<std::string> reallocFuncs = {
        "realloc",
        "reallocf",

        /* custom allocators */
        "Perl_safesysrealloc",
    };
    return reallocFuncs.find(F->getName().str()) != reallocFuncs.end();
}

/*
 * Insert object size in pointers after allocations.
 *
 * Partially reimplements MemoryBuiltins.cpp from llvm to detect allocators.
 */
void MidFatPtrs::instrumentCallAlloc(CallSite *CS) {
    Instruction *Call = CS->getInstruction();
    Function *F = CS->getCalledFunction();
    if (!F || F->isIntrinsic())
        return;

    Function *parentFunc = CS->getParent()->getParent();

    /* Ignore stdlib allocations in custom wrappers */
    if (isMalloc(parentFunc) || isCalloc(parentFunc) || isRealloc(parentFunc))
        return;

    /* XXX Does crazy environ stuff. */
    if (parentFunc->getName() == "Perl_my_setenv")
        return;

    if (isMalloc(F) || isCalloc(F) || isRealloc(F))
        putMetaPointerInHighBits(Call);
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
 */
void MidFatPtrs::instrumentCallExt(CallSite *CS) {
    Function *F = CS->getCalledFunction();
    if (CS->isInlineAsm())
        return;
    if (!F)
        return;
    if (!F->isDeclaration()) /* not external */
        return;

    if (F->getName().startswith("llvm.eh.") ||
        F->getName().startswith("llvm.dbg.") ||
        F->getName().startswith("llvm.lifetime."))
        return;

    maskPointerArgs(CS);
}

void MidFatPtrs::instrumentCallExtWrap(CallSite *CS) {
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

void MidFatPtrs::instrumentCallByval(CallSite *CS) {
    Function *F = CS->getCalledFunction();
    if (CS->isInlineAsm())
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

void MidFatPtrs::instrumentCallExtNestedPtrs(CallSite *CS) {
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

void MidFatPtrs::instrumentCmpPtr(CmpInst *ins) {
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
void MidFatPtrs::instrumentMemAccess(Instruction *ins) {
    int ptrOperand = isa<StoreInst>(ins) ? 1 : 0;
    Value *ptr = ins->getOperand(ptrOperand);

    IRBuilder<> B(ins);
    ins->setOperand(ptrOperand, maskPointer(ptr, B));

    /* Also mask writes of pointers to externs (e.g., environ). */
    GlobalVariable *gv = dyn_cast<GlobalVariable>(ptr->stripPointerCasts());
    if (isa<StoreInst>(ins) && gv && !gv->hasInitializer() && gv->getType()->isPointerTy()) {
        ins->setOperand(0, maskPointer(ins->getOperand(0), B));
    }
}

/*
 * Create masks for ptrints and replace uses in sinks with the masked value.
 */
void MidFatPtrs::instrumentSinks(sinklist_t &Sinks) {
    for (auto &SinkList : Sinks) {
        Instruction *PtrInt = SinkList.first;

        IRBuilder<> B(getInsertPointAfter(PtrInt));
        Instruction *Masked = cast<Instruction>(maskPointer(PtrInt, B));

        for (Instruction *Sink : SinkList.second) {
            Sink->replaceUsesOfWith(PtrInt, Masked);
        }
    }
}

bool MidFatPtrs::runOnFunction(Function &F) {
    if (ISMETADATAFUNC(F.getName().str().c_str()))
        return false;

    if (&F == LookupMetaPtrFunc || F.getName() == "initialize_global_metapointers")
        return false;

    std::vector<Instruction*> mems;
    sinklist_t sinks;
    std::set<Instruction*> knownPtrInts;

    for (BasicBlock &bb : F) {
        for (Instruction &i : bb) {
            Instruction *ins = &i;
            if (isa<StoreInst>(ins) || isa<LoadInst>(ins)) {
                mems.push_back(ins);
            }
            if (isPtrInt(ins)) {
                collectSinks(ins, sinks, knownPtrInts);
            }
        }
    }

    for (BasicBlock &bb : F) {
        for (Instruction &i : bb) {
            Instruction *ins = &i;
            if (isa<CallInst>(ins) || isa<InvokeInst>(ins)) {
                CallSite CS(ins);
                instrumentCallExt(&CS);
                instrumentCallExtWrap(&CS);
                instrumentCallByval(&CS);
                instrumentCallExtNestedPtrs(&CS);
                instrumentCallAlloc(&CS);
            }
            else ifcast(CmpInst, cmp, ins) {
                instrumentCmpPtr(cmp);
            }
        }
    }

    instrumentSinks(sinks);

    for (Instruction *mem : mems)
        instrumentMemAccess(mem);

    return true;
}

static Function *createHelperFunction(Module *M, FunctionType *Ty, const Twine &N,
        bool AlwaysInline = true) {
    Function *F = Function::Create(Ty, GlobalValue::InternalLinkage, N, M);
    if (AlwaysInline)
        F->addFnAttr(Attribute::AlwaysInline);
    BasicBlock::Create(F->getContext(), "entry", F);
    return F;
}

/*
 * Build metaptr:
 *   unsigned long page = ptrInt / METALLOC_PAGESIZE;
 *   unsigned long entry = pageTable[page];
 *   unsigned long alignment = entry & 0xFF;
 *   char *metabase = (char*)(entry >> 8);
 *   unsigned long pageOffset = ptrInt - (page * METALLOC_PAGESIZE);
 *   char *metaptr = metabase + ((pageOffset >> alignment) * size);
 *   return (unsigned long)metaptr;
 */
static Function *createMetaPtrLookupHelper(Module &M) {
    Type *i64 = Type::getInt64Ty(M.getContext());
    FunctionType *FnTy = FunctionType::get(i64, i64, false);
    Function *F = createHelperFunction(&M, FnTy, "lookup_metaptr");
    IRBuilder<> B(&F->getEntryBlock());

    ConstantInt *PageTableInt = B.getInt64((unsigned long long)pageTable);
    ConstantInt *PageSize = B.getInt64(METALLOC_PAGESIZE);
    Value *PtrInt = F->getArgumentList().begin();
    Value *PageTable = B.CreateIntToPtr(PageTableInt, i64->getPointerTo(), "pagetable");
    Value *Page = B.CreateUDiv(PtrInt, PageSize, "page");
    Value *EntryPtr = B.CreateInBoundsGEP(PageTable, Page, "entry_ptr");
    Value *Entry = B.CreateLoad(EntryPtr, "entry");
    Value *Alignment = B.CreateAnd(Entry, 0xff, "alignment");
    Value *MetaBase = B.CreateLShr(Entry, 8, "metabase");
    Value *PageBase = B.CreateMul(Page, PageSize, "pagebase");
    Value *PageOffset = B.CreateSub(PtrInt, PageBase, "pageoffset");
    Value *PageOffsetShifted = B.CreateLShr(PageOffset, Alignment, "pageoffset_shr");
    unsigned long OffsetShift = DeepMetadata ? sizeof (unsigned long) : MetadataBytes;
    Value *MetaOffset = B.CreateMul(PageOffsetShifted, B.getInt64(OffsetShift), "metaoffset");
    Value *MetaPtr = B.CreateAdd(MetaBase, MetaOffset, "metaptr");

    if (DeepMetadata) {
        Value *CastMetaPtr = B.CreateIntToPtr(MetaPtr, i64->getPointerTo(), "deepptr_ptr");
        MetaPtr = B.CreateLoad(CastMetaPtr, "deepptr");
    }

    B.CreateRet(MetaPtr);
    return F;
}

bool MidFatPtrs::runOnModule(Module &M) {
    LookupMetaPtrFunc = createMetaPtrLookupHelper(M);

    for (Function &F : M)
        runOnFunction(F);

    return true;
}
