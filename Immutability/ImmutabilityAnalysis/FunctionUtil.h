#ifndef LLVM_ANALYSIS_IMMUTABILITY_FUNCTION_UTIL
#define LLVM_ANALYSIS_IMMUTABILITY_FUNCTION_UTIL

#include <llvm/IR/Function.h>

namespace llvm {
namespace immutability {

__attribute__((always_inline))
inline const Argument *getFirstNonRetArg(const Function *F) {
  for (const Argument &A : F->args()) {
    if (A.hasStructRetAttr()) {
      continue;
    }
    // Assume this is the first non sret argument
    return &A;
  }
  return nullptr;
}

 __attribute__((always_inline))
inline const Argument *getThisArg(const Function *Method) {
  const Argument *ThisArg = getFirstNonRetArg(Method);
  assert(ThisArg && "Cannot find this argument for method");
  return ThisArg;
}

 __attribute__((always_inline))
inline bool isRelevant(const Function *F, const Value *V) {
  if (const Instruction *I = dyn_cast<Instruction>(V)) {
    if (I->getParent()->getParent() == F) {
      return true;
    }
    else {
      return false;
    }
  }
  else if (const Argument *A = dyn_cast<Argument>(V)) {
    for (auto &Arg : F->args()) {
      if (A == &Arg) {
        return true;
      }
    }
    return false;
  }
  else if (isa<Constant>(V)) {
    return false;
  }
  else {
    llvm_unreachable("?");
  }
}

}
}

#endif
