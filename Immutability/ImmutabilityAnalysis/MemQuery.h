#ifndef LLVM_ANALYSIS_MEM_QUERY
#define LLVM_ANALYSIS_MEM_QUERY

#include <llvm/Pass.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/StringSet.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>

using namespace llvm;

namespace llvm {
namespace immutability {

class MemQuery : public ModulePass {
  const Type *AllocReturnTy;

  SmallPtrSet<const BitCastInst *, 16> IsAlloc;
  SmallPtrSet<const BitCastInst *, 16> IsZero;
  SmallPtrSet<const BitCastInst *, 16> IsAllOnes;

  SmallPtrSet<const Instruction *, 16> IgnoredInsts;
public:
  static char ID;
  MemQuery() : ModulePass(ID) {
    AllocFunctionNames.insert("_Znwm");
    AllocFunctionNames.insert("_Znam");
    AllocFunctionNames.insert("malloc");

    AssertFunctionNames.insert("_Z8__assertPKcS0_mi");
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override;
  void print(raw_ostream &O, const Module *M) const override;
  bool runOnModule(Module &M) override;

  bool isAlloc(const BitCastInst *I) {
    return IsAlloc.count(I) > 0;
  }
  bool isZero(const BitCastInst *I) {
    return IsZero.count(I) > 0;
  }
  bool isIgnoredInst(const Instruction *I) {
    return IgnoredInsts.count(I);
  }

private:
  void populateAllocs(const BitCastInst &I);
  void populateAsserts(const CallInst &I);
  void populateMem(const BitCastInst &I);

  StringSet<> AllocFunctionNames;
  StringSet<> AssertFunctionNames;
};

}
}

#endif
