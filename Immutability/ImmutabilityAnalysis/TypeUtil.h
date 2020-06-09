#ifndef LLVM_ANALYSIS_IMMUTABILITY_TYPE_UTIL_H
#define LLVM_ANALYSIS_IMMUTABILITY_TYPE_UTIL_H

#include <llvm/IR/DerivedTypes.h>

namespace llvm {
namespace immutability {

bool isRecursiveTy(const StructType *StructTy);

}
}

#endif
