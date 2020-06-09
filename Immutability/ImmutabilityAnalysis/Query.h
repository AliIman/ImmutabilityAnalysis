#ifndef LLVM_ANALYSIS_IMMUTABILITY_QUERY
#define LLVM_ANALYSIS_IMMUTABILITY_QUERY

#include "ClassQuery.h"
#include "MemQuery.h"

namespace llvm {
namespace immutability {

class Query {
public:
  ClassQuery &C;
  MemQuery &M;

  Query(ClassQuery &C, MemQuery &M) : C(C), M(M) {}

  bool isIgnoredInst(const Instruction *I) {
    return C.isIgnoredInst(I) || M.isIgnoredInst(I);
  }
};

}
}

#endif
