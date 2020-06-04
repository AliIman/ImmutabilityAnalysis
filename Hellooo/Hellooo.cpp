#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
struct Hellooo : public FunctionPass {
	static char ID;
	Hellooo() : FunctionPass(ID) {}

	bool runOnFunction(Function &F) override {
   		errs() << "Hellooo: ";
    		errs().write_escaped(F.getName()) << '\n';
    		return false;
	}
};
}

char Hellooo::ID = 0;
static RegisterPass<Hellooo> X("hellooo", "Hellooo World Pass", false /* Only looks at CFG */, false /* Analysis Pass */);

