#ifndef LLVM_ANALYSIS_CLASS_QUERY
#define LLVM_ANALYSIS_CLASS_QUERY

#include "llvm/Pass.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"

#include <set>

using namespace llvm;

namespace llvm {
namespace immutability {

class ClassQuery : public ModulePass {
public:
  static char ID;
  ClassQuery() : ModulePass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override;
  void print(raw_ostream &O, const Module *M) const override;
  bool runOnModule(Module &M) override;

  typedef SmallPtrSet<const StructType *, 16> TypeSet;
  typedef SmallPtrSet<const Function *, 16> FunctionSet;
  typedef std::vector<const StructType *> StructTypes;
  typedef SmallVector<unsigned, 4> Indices;

  const TypeSet &getTypes();
  const FunctionSet &getPublicConstMethods(const StructType *T);

  bool isIgnoredInst(const Instruction *I);
  bool isVTableInst(const Instruction *I);
  const Function *getVTableEntry(const Instruction *I, const StructType *T);

  bool isSupertype(const StructType *T, const StructType *SuperTy) const;
  Indices getSupertypeIndices(const StructType *T, const StructType *SuperTy);

private:
  typedef std::vector<const Function *> Functions;
  typedef DenseMap<const StructType *, const StructType *> TypeMap;
  typedef DenseMap<const StructType *, TypeSet> TypesMap;
  typedef DenseMap<const StructType *, FunctionSet> FunctionsMap;
  typedef DenseMap<const StructType *, Functions> IndexedFunctionsMap;
  typedef DenseMap<const Instruction *, unsigned> VTableInstsMap;

  typedef DenseMap<const StructType *, const DICompositeType *>
      TypeToDebugTypeMap;
  typedef DenseMap<const DICompositeType *, const StructType *>
      DebugTypeToTypeMap;
  typedef SmallPtrSet<const DICompositeType *, 4> DebugTypeSet;
  typedef DenseMap<const DIType *, DebugTypeSet> InheritanceMap;

  TypeSet IgnoredTypes;
  TypeSet Types;
  TypeToDebugTypeMap TypeToDebugType;
  DebugTypeToTypeMap DebugTypeToType;
  InheritanceMap DirectSupertypes;
  Functions AllMethods;

  FunctionsMap DirectPublicConstMethods;
  IndexedFunctionsMap DirectVirtualMethods;

  void insertSupertypes(const DICompositeType *DebugTy);

  TypesMap Supertypes;
  IndexedFunctionsMap VTables;
  FunctionsMap PublicConstMethods;

  std::set<const Instruction *> IgnoredInsts;
  VTableInstsMap VTableInsts;

  const StructType *getTypeFromName(const StringRef &Name);

  void populateTypesAndBaseTypes(const Module &M);
  void populateDirectSubAndSupertypes(const StructType *T,
                                      const DISubprogram *Definition);

  void populateDirect(const Module &M);
  void populateNonDirect(const StructType *T);

  const StructType *findStructType(const Function *F);

  void handleVTablePointer(BasicBlock::const_iterator I,
                           BasicBlock::const_iterator IB,
                           BasicBlock::const_iterator IE);
};

}
}

#endif
