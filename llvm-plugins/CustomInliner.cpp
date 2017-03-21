/*
 * CustomInliner.cpp
 *
 *  Created on: Nov 10, 2015
 *      Author: haller
 */

#include <llvm/Transforms/IPO/InlinerPass.h>
#include <llvm/Analysis/InlineCost.h>

#include <metadata.h>

using namespace llvm;

struct CustomInliner : public Inliner {
    static char ID;

    CustomInliner() : Inliner(ID) {}

    InlineCost getInlineCost(CallSite CS) {
        if (Function *Callee = CS.getCalledFunction()) {
            StringRef func_name = Callee->getName();
            if (ISMETADATAFUNC(func_name.str().c_str()) ||
                    func_name == "lookup_metaptr" ||
                    func_name == "checkptr") {
                return InlineCost::getAlways();
            }
	}

        return InlineCost::getNever();
    }


};

char CustomInliner::ID = 0;
static RegisterPass<CustomInliner> X("custominline", "Custom Inliner Pass", true, false);




