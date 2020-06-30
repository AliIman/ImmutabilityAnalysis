#include "MemQuery.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Module.h>

/*
#include <llvm/Support/Allocator.h>

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/SmallSet.h>
#include <llvm/IR/ConstantRange.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/ValueMap.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Pass.h>
*/
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace immutability;

void MemQuery::populateAllocs(const BitCastInst &I) {
  if (I.getSrcTy() != AllocReturnTy) {
    return;
  }

  auto CI = dyn_cast<CallInst>(I.getOperand(0));
  if (!CI) {
    return;
  }

  bool HasUse = false;
  for (auto &U : CI->uses()) {
    HasUse = true;
    if (U.getUser() != &I) {
      return;
    }
  }
  if (!HasUse) {
    return;
  }

  const Function *F = CI->getCalledFunction();
  if (!F) {
    return;
  }
  const StringRef &Name = F->getName();
  if (AllocFunctionNames.count(Name) > 0) {
    IgnoredInsts.insert(CI);
    IsAlloc.insert(&I);
  }
}

void MemQuery::populateAsserts(const CallInst &I) {
  const Function *F = I.getCalledFunction();
  if (!F) {
    return;
  } 
  const StringRef &Name = F->getName(); 
  if (AssertFunctionNames.count(Name) > 0) {
    IgnoredInsts.insert(&I);
  }
}

void MemQuery::populateMem(const BitCastInst &I) {
  if (I.getDestTy() != AllocReturnTy) {
    return;
  }

  bool HasUse = false;
  const CallInst *MemCall = nullptr;
  for (auto &U : I.uses()) {
    HasUse = true;
    auto CI = dyn_cast<CallInst>(U.getUser());
    if (!CI) {
      return;
    }
    if (MemCall) {
      return;
    }
    MemCall = CI;
  }
  if (!HasUse) {
    return;
  }

  if (auto II = dyn_cast<IntrinsicInst>(MemCall)) {
    switch (II->getIntrinsicID()) {
    case Intrinsic::lifetime_start:
    case Intrinsic::lifetime_end:
      IgnoredInsts.insert(&I);
      IgnoredInsts.insert(MemCall);
      return;
    }
  }

  if (auto MI = dyn_cast<MemIntrinsic>(MemCall)) {
    if (auto MSI = dyn_cast<MemSetInst>(MI)) {
      const Value *V =  MSI->getValue();
      if (!(isa<ConstantInt>(V))) {
        return;
      }
      const ConstantInt *CI = cast<ConstantInt>(V);
      if (CI->isZero()) {
        IsZero.insert(&I);
        IgnoredInsts.insert(MemCall);
      }
      else if (CI->isMinusOne()) {
        IsAllOnes.insert(&I);
        IgnoredInsts.insert(MemCall);
      }
      else {
        llvm_unreachable("Unhandled MemSet");
      }
    }
    else if (auto MCI = dyn_cast<MemCpyInst>(MI)) {
      // TODO: MemCpy temp ignored completely
      IgnoredInsts.insert(&I);
      IgnoredInsts.insert(MemCall);
    }
    else if (auto MMI = dyn_cast<MemMoveInst>(MI)) {
      // TODO: MemMove temp ignored completely
      IgnoredInsts.insert(&I);
      IgnoredInsts.insert(MemCall);
    }
    else {
      llvm_unreachable("TODO: ImmutabilityAnalysis::populateMem");
    }
  }
}

void MemQuery::getAnalysisUsage(AnalysisUsage &AU) const {
  errs() << ":: MemQuery::getAnalysisUsage begin\n";
  AU.setPreservesAll();
  errs() << ":: MemQuery::getAnalysisUsage end\n";
}

void MemQuery::print(raw_ostream &O, const Module *M) const { 
  errs() << ":: MemQuery::print\n";
}

bool MemQuery::runOnModule(Module &M) {
  AllocReturnTy = Type::getInt8PtrTy(M.getContext());
  for (auto &F : M.functions()) {
    for (auto &BB : F.getBasicBlockList()) {
      for (auto &I : BB.getInstList()) {
        if (auto BCI = dyn_cast<BitCastInst>(&I)) {
          populateAllocs(*BCI);
          populateMem(*BCI);
        }
        else if (auto CallI = dyn_cast<CallInst>(&I)) {
          populateAsserts(*CallI);
        }
      }
    }
  }
  errs() << ":: MemQuery::runOnModule\n";
}

char MemQuery::ID = 0;
static RegisterPass<MemQuery> X("mem-query", "Mem Queries", false, true);
