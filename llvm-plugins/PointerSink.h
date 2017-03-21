#ifndef POINTER_SINKS_H
#define POINTER_SINKS_H

#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>

#include "Utils.h"
#include "MetaPointerUtils.h"

namespace llvm {

typedef std::list<std::pair<Instruction*, std::vector<Instruction*>>> sinklist_t;

/*
 * Check if the result of an instruction represents a pointer as an integer:
 * - ptrtoint
 * - bitcast (ptr -> int)
 * - load when it loads a pointer (then the ptr ptr is bitcast to an int ptr)
 */
static inline bool isPtrInt(Instruction *I) {
    if (!isPtrIntTy(I->getType()))
        return false;

    if (isa<PtrToIntInst>(I))
        return true;

    if (isa<LoadInst>(I))
        return false;

    ifcast(BitCastInst, BC, I)
        return BC->getSrcTy()->isPointerTy();

    return false;
}

static inline bool isAnd(Instruction *I) {
    return I->isBinaryOp() && I->getOpcode() == Instruction::And;
}

/*
 * A pointer mask preserves the metapointer if if the mask is constant and has
 * all ones in the upper (metadata) bits.
 */
static bool andDefinitelyPreservesMetaPtr(Instruction *I, Instruction *PtrOp) {
    unsigned OtherOpIndex = I->getOperand(0) == PtrOp ? 1 : 0;
    ifcast(ConstantInt, Mask, I->getOperand(OtherOpIndex)) {
        return Mask->getZExtValue() >= ~PTR_MASK;
    }
    return false;
}

/*
 * A pointer mask zeroes the metapointer if if the mask is constant and has
 * all zeroes in the upper (metadata) bits.
 */
static bool andDefinitelyZeroesMetaPtr(Instruction *I, Instruction *PtrOp) {
    unsigned OtherOpIndex = I->getOperand(0) == PtrOp ? 1 : 0;
    ifcast(ConstantInt, Mask, I->getOperand(OtherOpIndex)) {
        return Mask->getZExtValue() <= PTR_MASK;
    }
    return false;
}

/*
 * Check if an instruction 'sinks' a ptrint (uses it in a way that requires
 * masking).
 */
static bool isSinkOf(Instruction *Sink, Instruction *PtrInt) {
    /* Trivial sinks: these are always sinks:
     * - cmp, sub, add, xor, rem, div, gep, sh[rl], switch, ret, itofp, insert
     *   (xors are sometimes used to create uglygeps)
     * - external/asm/intrinsic call */
    switch (Sink->getOpcode()) {
        case Instruction::ICmp:
        case Instruction::Sub:
        case Instruction::Add:
        case Instruction::Xor:
        case Instruction::URem:
        //case Instruction::Mul: FIXME: have not seen these (yet)
        case Instruction::SDiv: // FIXME: omnetpp
        case Instruction::UDiv:
        case Instruction::GetElementPtr:
        case Instruction::LShr:
        case Instruction::AShr:
        case Instruction::Shl:
        case Instruction::Or:  // xalancbmk (TraverseSchema.cpp:2658)
        case Instruction::Switch:
        case Instruction::Ret:
        case Instruction::SIToFP: // FIXME: omnetpp
        case Instruction::UIToFP: // FIXME: perl wtf... should we mask this?
        case Instruction::InsertElement: // FIXME: dealII
            return true;
    }

    ifcast(CallInst, CI, Sink) {
        Function *F = CI->getCalledFunction();
        return !F || F->isIntrinsic() || F->isDeclaration();
    }

    /* Non-trivial sinks: these need more analysis:
     * - and: if the mask may not preserve metadata bits */
    if (isAnd(Sink))
        return !andDefinitelyPreservesMetaPtr(Sink, PtrInt);

    return false;
}

/*
 * Trivial non-sinks are uses of ptrints that definitely do not need masking:
 * - store, inttoptr, bitcast (int -> ptr)
 * - internal call: don't know if causes problems, just print a warning for now
 * - and: if the mask already zeroes the metapointer
 * - trunc: if the destination type fits within PTR_BITS
 */
static bool isTrivialNonSinkOf(Instruction *I, Instruction *PtrInt) {
    if (isa<StoreInst>(I) || isa<IntToPtrInst>(I))
        return true;

    ifcast(BitCastInst, BC, I) {
        assert(isPtrIntTy(BC->getSrcTy()));

        if (!BC->getDestTy()->isPointerTy()) {
            errs() << "Warning: bitcast might sink ptrint:\n";
            errs() << "  ptrint: " << *PtrInt << "\n";
            errs() << "  bitcast:" << *BC << "\n";
            return true;
        }

        return BC->getDestTy()->isPointerTy();
    }

    ifcast(CallInst, CI, I) {
        Function *F = CI->getCalledFunction();
        if (!F || F->isIntrinsic() || F->isDeclaration())
            return false;

        if (ISMETADATAFUNC(F->getName().str().c_str()))
            return true;

        errs() << "Warning: call to internal function might sink ptrint:\n";
        errs() << "  ptrint:" << *PtrInt << "\n";
        errs() << "  call:  " << *CI << "\n";
        return true;
    }

    if (isAnd(I))
        return andDefinitelyZeroesMetaPtr(I, PtrInt);

    ifcast(TruncInst, TI, I) {
        errs() << "found trunc:" << *TI << "\n";
        IntegerType *DestTy = cast<IntegerType>(TI->getDestTy());
        return DestTy->getBitWidth() <= PTR_BITS;
    }

    return false;
}

/*
 * Collect sinks for a ptrint source. Recursively follow phi nodes, selects and
 * pointer masks to find final 'sink' instructions that need masking. We
 * whitelist all uses that definitely need or don't need masking and error if
 * we encounter something unexpected.
 */
static void collectSinks(Instruction *I, sinklist_t &Sinks,
        std::set<Instruction*> &KnownPtrInts) {
    if (KnownPtrInts.count(I) > 0)
        return;
    KnownPtrInts.insert(I);

    std::vector<Instruction*> ISinks, RecursiveUses;

    for (User *U : I->users()) {
        if (Instruction *UI = dyn_cast<Instruction>(U)) {
            if (isa<PHINode>(UI) || isa<SelectInst>(UI) ||
                    (isAnd(UI) && andDefinitelyPreservesMetaPtr(UI, I))) {
                RecursiveUses.push_back(UI);
            }
            else if (isTrivialNonSinkOf(UI, I)) {
                continue;
            }
            else if (isSinkOf(UI, I)) {
                ISinks.push_back(UI);
            }
            else {
                errs() << "Error: found use of ptrint and not sure if should mask.\n";
                errs() << "  ptrint:" << *I << "\n";
                errs() << "  sink?: " << *UI << "\n";
                exit(1);
            }
        }
    }

    if (!ISinks.empty())
        Sinks.push_back(std::make_pair(I, ISinks));

    for (Instruction *UI : RecursiveUses)
        collectSinks(UI, Sinks, KnownPtrInts);
}

}

#endif /* !POINTER_SINKS_H */
