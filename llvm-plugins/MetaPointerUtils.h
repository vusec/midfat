#ifndef META_POINTER_UTILS_H
#define META_POINTER_UTILS_H

#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>

#include "Utils.h"

namespace llvm {

static inline bool isPtrIntTy(Type *Ty) {
    return Ty->isIntegerTy(64);
}

static LoadInst *getLoadInst(Value *V, std::set<PHINode*> &VisitedPHINodes) {
    ifcast(LoadInst, LI, V)
        return LI;

    ifcast(PHINode, PN, V) {
        if (VisitedPHINodes.count(PN) == 0) {
            VisitedPHINodes.insert(PN);
            for (Use &U : PN->operands()) {
                if (LoadInst *LI = getLoadInst(U.get(), VisitedPHINodes))
                    return LI;
            }
        }
    }

    return nullptr;
}

}

#endif /* !META_POINTER_UTILS_H */
