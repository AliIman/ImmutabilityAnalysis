#include "ClassQuery.h"

#include "FunctionUtil.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Module.h>

#include <queue>

#define HAVE_DECL_BASENAME 1
#include <libiberty/demangle.h>

using namespace llvm;
using namespace immutability;

struct demangle_builtin_type_info {
  /* Type name.  */
  const char *name;
  /* Length of type name.  */
  int len;
};

struct demangle_operator_info {
  /* Mangled name.  */
  const char *code;
  /* Real name.  */
  const char *name;
  /* Length of real name.  */
  int len;
  /* Number of arguments.  */
  int args;
};

namespace {

bool isBaseType(const StructType *T) {
  if (T->getName() == "class.base" || T->getName() == "struct.base") {
    return false;
  }
  if (T->getName().endswith(".base")) {
    return true;
  }
  auto FirstSplit = T->getName().rsplit('.');
  APInt Num;
  bool NotValid = FirstSplit.second.getAsInteger(10, Num);
  if (!NotValid) {
    auto SecondSplit = FirstSplit.first.rsplit('.');
    if (SecondSplit.second == "base") {
      return true;
    }
  }
  return false;
}

unsigned getDebugInfoVersion(const Module &M) {
  auto *Val
    = cast_or_null<ConstantAsMetadata>(M.getModuleFlag("Debug Info Version"));
  if (!Val) {
    return 0;
  }
  return cast<ConstantInt>(Val->getValue())->getZExtValue();
}

const StructType * findNonBaseType(const Module &M, const StructType *BaseT) {
  assert(isBaseType(BaseT) && "Required base type");
  StringRef NonBaseTypeName = BaseT->getName().substr(
    0, BaseT->getName().size() - StringRef(".base").size());
  const StructType *NonBaseType = nullptr;
  for (const StructType *T : M.getIdentifiedStructTypes()) {
    if (NonBaseTypeName == T->getName()) {
      NonBaseType = T;
      break;
    }
  }
  return NonBaseType;
}

void checkDefinition(const DISubprogram *Definition,
                     bool &IsMethod,
                     bool &IsConst,
                     const DICompositeType *&DebugTy) {
  IsMethod = false;
  IsConst = false;
  DebugTy = nullptr;

  const DINodeArray Variables = Definition->getRetainedNodes();
  if (Variables.size() == 0) {
    return;
  }
  const DILocalVariable *Variable0 = cast<const DILocalVariable>(Variables[0]);
  if (Variable0->getName() != "this") {
    return;
  }

  IsMethod = true;

  auto DebugPointerTy = cast<DIDerivedType>(Variable0->getType().resolve());
  const DIType *DebugStructTy = DebugPointerTy->getBaseType().resolve();
  if (auto VolatileTy = dyn_cast<DIDerivedType>(DebugStructTy)) {
    if (VolatileTy->getTag() == dwarf::DW_TAG_volatile_type) {
	    DebugStructTy = VolatileTy->getBaseType().resolve();
	  }
  }
  if (auto ConstTy = dyn_cast<DIDerivedType>(DebugStructTy)) {
    if (ConstTy->getTag() != dwarf::DW_TAG_const_type) {
	    llvm::errs() << "NAME " << Definition->getName() << '\n';
	    llvm::errs() << "Hmm... " << dwarf::DW_TAG_volatile_type << '\n';
	    llvm::errs() << "WTF? " << ConstTy->getTag() << " " << ConstTy->getName() << '\n';
    }
    assert(ConstTy->getTag() == dwarf::DW_TAG_const_type);
    IsConst = true;
    DebugTy = cast<DICompositeType>(ConstTy->getBaseType().resolve());
  }
  else {
    DebugTy = cast<DICompositeType>(DebugStructTy);
  }
}

bool isNoopOrConstantFunction(const Function *F) {
  for (const BasicBlock &BB : *F) {
    for (const Instruction &I : BB) {
      if (isa<CallInst>(I) || isa<InvokeInst>(I)) {
        if (isa<DbgInfoIntrinsic>(I)) {
          continue;
        }
        return false;
      }
      else if (auto AI = dyn_cast<AllocaInst>(&I)) {
        continue;
      }
      else if (auto BI = dyn_cast<BranchInst>(&I)) {
        if (BI->isUnconditional()) {
          continue;
        }
        return false;
      }
      else if (auto SI = dyn_cast<StoreInst>(&I)) {
        if (isa<AllocaInst>(SI->getPointerOperand())
            && isa<Argument>(SI->getValueOperand())) {
          continue;
        }
        return false;
      }
      else if (auto LI = dyn_cast<LoadInst>(&I)) {
        if (isa<AllocaInst>(LI->getPointerOperand())) {
          continue;
        }
        return false;
      }
      else if (auto RI = dyn_cast<ReturnInst>(&I)) {
        if (RI->getType()->isVoidTy()) {
          continue;
        }
        if (isa<Constant>(RI->getReturnValue())) {
          continue;
        }
        return false;
      }
      else {
        return false;
      }
    }
  }

  return true;
}

bool isDeclarationPublic(const DISubprogram *Declaration, bool IsCXXClass) {
  if (Declaration->isPrivate()
      || Declaration->isProtected()
      || Declaration->isPublic()) {
    return Declaration->isPublic();
  }

  // By default classes are private, structs are public
  if (IsCXXClass) {
    return false;
  }
  else {
    return true;
  }
}

void getDemangleComponents(struct demangle_component *DC,
                           bool &FunctionConstThis,
                           StringRef &FunctionName,
                           struct demangle_component *&FunctionType) {
  FunctionConstThis = false;
  assert(DC->type == DEMANGLE_COMPONENT_TYPED_NAME);

  struct demangle_component *Left = DC->u.s_binary.left;
  struct demangle_component *QualName;

  if (Left->type == DEMANGLE_COMPONENT_REFERENCE_THIS) {
    Left = Left->u.s_binary.left;
  }

  if (Left->type == DEMANGLE_COMPONENT_CONST_THIS) {
    FunctionConstThis = true;
    QualName = Left->u.s_binary.left;
  }
  else {
    QualName = Left;
  }
  if (QualName->type != DEMANGLE_COMPONENT_QUAL_NAME) {
    if (QualName->type == DEMANGLE_COMPONENT_LOCAL_NAME) {
    }
    else if (QualName->type == DEMANGLE_COMPONENT_TEMPLATE) {
      QualName = QualName->u.s_binary.left;
    }
  }

  FunctionType = DC->u.s_binary.right;
  assert(FunctionType->type == DEMANGLE_COMPONENT_FUNCTION_TYPE);

  /*
  if (QualName->type == DEMANGLE_COMPONENT_REFERENCE_THIS) {
    QualName = QualName->u.s_binary.left;
    QualName = Left->u.s_binary.left;
  }
  */
  if (QualName->type == DEMANGLE_COMPONENT_QUAL_NAME) {
    struct demangle_component *QualifiedName = QualName->u.s_binary.right;

    if (QualifiedName->type == DEMANGLE_COMPONENT_TAGGED_NAME) {
      // The right node is the ABI (likely cxx11)
      QualifiedName = QualifiedName->u.s_binary.left;
    }

    switch (QualifiedName->type) {
    case DEMANGLE_COMPONENT_NAME:
      FunctionName = StringRef(QualifiedName->u.s_name.s,
                               QualifiedName->u.s_name.len);
      break;
    case DEMANGLE_COMPONENT_OPERATOR: // 49
      FunctionName = StringRef(QualifiedName->u.s_operator.op->name);
      break;
    case DEMANGLE_COMPONENT_CONVERSION: // 52
      QualifiedName = QualifiedName->u.s_binary.left;
      if (QualifiedName->type == DEMANGLE_COMPONENT_CONST) { // 27
        QualifiedName = QualifiedName->u.s_binary.left;
      }
      else if (QualifiedName->type == DEMANGLE_COMPONENT_QUAL_NAME) { // 1
        QualifiedName = QualifiedName->u.s_binary.right;
        assert(QualifiedName->type == DEMANGLE_COMPONENT_NAME); // 0
        FunctionName = StringRef(QualifiedName->u.s_name.s,
                                 QualifiedName->u.s_name.len);
      }
      else if (QualifiedName->type == DEMANGLE_COMPONENT_TEMPLATE) { // 4
        QualifiedName = QualifiedName->u.s_binary.left;
        assert(QualifiedName->type == DEMANGLE_COMPONENT_QUAL_NAME); // 1
        QualifiedName = QualifiedName->u.s_binary.right;
        assert(QualifiedName->type == DEMANGLE_COMPONENT_NAME); // 0
        FunctionName = StringRef(QualifiedName->u.s_name.s,
                                 QualifiedName->u.s_name.len);
      }
      else {
        assert(QualifiedName->type == DEMANGLE_COMPONENT_BUILTIN_TYPE); //39
        FunctionName = StringRef(QualifiedName->u.s_builtin.type->name,
                                 QualifiedName->u.s_builtin.type->len);
      }
      break;
    default:
      errs() << "Unknown Qualified Name: " << QualifiedName->type << '\n';
      errs() << "best getess: " << DEMANGLE_COMPONENT_CONVERSION << '\n';
      llvm_unreachable("Unhandled Qualified Name");
    }
  }
  else if (QualName->type == DEMANGLE_COMPONENT_LOCAL_NAME) {
    struct demangle_component *TypedName = QualName->u.s_binary.left;
    assert(TypedName->type == DEMANGLE_COMPONENT_TYPED_NAME);
    struct demangle_component *Name = TypedName->u.s_binary.left;
    if (Name->type == DEMANGLE_COMPONENT_QUAL_NAME) {
      Name = Name->u.s_binary.right;
      //errs() << "Name type: " << Name->type << '\n';
    }
    assert(Name->type == DEMANGLE_COMPONENT_NAME);
    FunctionName = StringRef(Name->u.s_name.s,
                             Name->u.s_name.len);
  }
  else {
      errs() << "Unknown Qual Name: " << QualName->type << '\n';
      errs() << "???: " << DEMANGLE_COMPONENT_REFERENCE_THIS << '\n';
    llvm_unreachable("Unhandled Qual Name");
  }
}

bool isSameDemangleComponents(struct demangle_component *DC1,
                              struct demangle_component *DC2) {
  if (DC1 == nullptr || DC2 == nullptr) {
    if (DC1 == DC2) {
      return true;
    }
    return false;
  }

  if (DC1->type != DC2->type) {
    return false;
  }

  switch (DC1->type) {
  case DEMANGLE_COMPONENT_FUNCTION_TYPE:
  case DEMANGLE_COMPONENT_ARGLIST:
  case DEMANGLE_COMPONENT_TEMPLATE:
  case DEMANGLE_COMPONENT_QUAL_NAME:
    break;
  case DEMANGLE_COMPONENT_CONST: // 27
  case DEMANGLE_COMPONENT_REFERENCE:
  case DEMANGLE_COMPONENT_POINTER: // 34
    return isSameDemangleComponents(DC1->u.s_binary.left,
                                    DC2->u.s_binary.left);
  case DEMANGLE_COMPONENT_NAME: // 0
  case DEMANGLE_COMPONENT_SUB_STD: {
    auto S1 = DC1->u.s_string;
    auto S2 = DC2->u.s_string;
    if (S1.len != S2.len) {
      return false;
    }
    return strncmp(S1.string, S2.string, S1.len);
  }
  case DEMANGLE_COMPONENT_TEMPLATE_PARAM:
    return DC1->u.s_number.number == DC2->u.s_number.number;
  case DEMANGLE_COMPONENT_BUILTIN_TYPE:
    return DC1->u.s_builtin.type == DC2->u.s_builtin.type;
  default:
    errs() << DC1->type << '\n';
    errs() << DEMANGLE_COMPONENT_BUILTIN_TYPE << '\n';
    llvm_unreachable("Unhandled Demangle Component Type");
  }

  bool SameLeft = isSameDemangleComponents(DC1->u.s_binary.left,
                                           DC2->u.s_binary.left);
  if (!SameLeft) {
    return false;
  }
  bool SameRight = isSameDemangleComponents(DC1->u.s_binary.right,
                                            DC2->u.s_binary.right);
  return SameRight;
}

bool isCtorOrDtor(const Function &F) {
  bool IsCtorOrDtor = false;
  std::string Name = F.getName().str();

  void *ToFree;
  struct demangle_component *DC = cplus_demangle_v3_components(
    Name.c_str(), DMGL_PARAMS | DMGL_ANSI, &ToFree);

  // _ZN9__gnu_cxx17__normal_iteratorIPKP4NodeSt6vectorIS2_SaIS2_EEEC2IPS2_EERKNS0_IT_NS_11__enable_ifIXsr3std10__are_sameISB_SA_EE7__valueES7_E6__typeEEE
  // Won't demangle, so just ignore it (isn't C++ nice and clear?)
  if (!DC) {
    return true;
  }

  if (DC->type == DEMANGLE_COMPONENT_THUNK || DC->type == DEMANGLE_COMPONENT_VIRTUAL_THUNK) {
    free(ToFree);
    return IsCtorOrDtor;
  }
  if (DC->type != DEMANGLE_COMPONENT_TYPED_NAME) {
    llvm::errs() << "OK? " << DC->type << '\n';
    llvm::errs() << "constant " << DEMANGLE_COMPONENT_THUNK << '\n';
  }
  assert(DC->type == DEMANGLE_COMPONENT_TYPED_NAME);

  struct demangle_component *Left = DC->u.s_binary.left;
  if (Left->type == DEMANGLE_COMPONENT_QUAL_NAME) {
    struct demangle_component *QualifiedName = Left->u.s_binary.right;
    switch (QualifiedName->type) {
    case DEMANGLE_COMPONENT_CTOR:
    case DEMANGLE_COMPONENT_DTOR:
      IsCtorOrDtor = true;
      break;
    }
  }

  free(ToFree);

  return IsCtorOrDtor;
}

bool isIgnoredType(const StructType *T) {
  StringRef Name = T->getName();
  /*
  if (isBaseType(T)) {
    T->dump();
  }
  assert((!isBaseType(T)) && "Unexpected base type");
  */

  if (!(T->getName().startswith("class")
        || T->getName().startswith("struct"))) {
    return true;
  }
  if (T->getName().startswith("class.std::")
      || T->getName().startswith("struct.std::")) {
    return true;
  }
  if (T->getName().startswith("class.__gnu_cxx::")
      || T->getName().startswith("struct.__gnu_cxx::")) {
    return true;
  }
  if (T->getName().startswith("class.google::")
      || T->getName().startswith("struct.google::")) {
    return true;
  }
  if (T->getName().startswith("class.anon.")
      || T->getName().startswith("struct.anon.")) {
    return true;
  }
  if (T->getName() == "class.anon"
      || T->getName() == "struct.anon") {
    return true;
  }

  if (T->getName() == "class.codegen::style::structure::Module") {
    return true;
  }
  return false;
}

bool isSameFunctionNameAndParams(const Function *F1, const Function *F2) {
  std::string Name1 = F1->getName().str();
  std::string Name2 = F2->getName().str();

  void *ToFree1, *ToFree2;
  struct demangle_component *DC1 = cplus_demangle_v3_components(
    Name1.c_str(), DMGL_PARAMS | DMGL_ANSI, &ToFree1);
  assert(DC1 != nullptr);
  struct demangle_component *DC2 = cplus_demangle_v3_components(
    Name2.c_str(), DMGL_PARAMS | DMGL_ANSI, &ToFree2);
  assert(DC2 != nullptr);

  bool FunctionConstThis1;
  StringRef FunctionName1;
  struct demangle_component *FunctionType1;
  getDemangleComponents(DC1, FunctionConstThis1, FunctionName1, FunctionType1);

  bool FunctionConstThis2;
  StringRef FunctionName2;
  struct demangle_component *FunctionType2;
  getDemangleComponents(DC2, FunctionConstThis2, FunctionName2, FunctionType2);

  bool IsSame = false;
  if ((FunctionConstThis1 == FunctionConstThis2)
      && (FunctionName1 == FunctionName2)
      && isSameDemangleComponents(FunctionType1, FunctionType2)) {
    IsSame = true;
  }

  free(ToFree1);
  free(ToFree2);

  return IsSame;
}

}

void ClassQuery::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
}

void ClassQuery::print(raw_ostream &O, const Module *M) const {
  O << "Class Query Results\n";
  const Function *F = nullptr;
  for (const StructType *T : Types) {
    O << "# " << T->getName() << '\n';
    const DICompositeType *DebugTy = TypeToDebugType.lookup(T);
    O << "  - Direct Supertypes: " << DirectSupertypes.lookup(DebugTy).size()
      << '\n';
    for (auto DebugSuperTy : DirectSupertypes.lookup(DebugTy)) {
      O << "    - " << DebugSuperTy->getName() << '\n';
    }
    if (Supertypes.lookup(T).size() != 0) {
      O << "  - Supertypes\n";
      for (auto SuperTy : Supertypes.lookup(T)) {
        O << "    - " << SuperTy->getName() << '\n';
      }
    }
    if (VTables.lookup(T).size() != 0) {
      O << "  - Virtual Table\n";
      unsigned VirtualIndex = 0;
      for (auto F : VTables.lookup(T)) {
        O << "    - " << VirtualIndex << ": ";
        if (F) {
          O << F->getName();
        }
        else {
          O << "[null]";
        }
        O << '\n';
        ++VirtualIndex;
      }
    }
    O << "  - Public Const Methods: " << PublicConstMethods.lookup(T).size()
      << '\n';
    for (const Function *F : PublicConstMethods.lookup(T)) {
      O << "    - " << F->getName() << '\n';
    }
  }
}

bool ClassQuery::runOnModule(Module &M) {
  assert(M.getDwarfVersion() == 4 && "Requires DWARF 4");
  assert(getDebugInfoVersion(M) == 3 && "Requires Debug Info 3");

  
  populateDirect(M);

  for (const StructType *T : Types) {
    populateNonDirect(T);
  }
  
  for (const Function *F: AllMethods) {
    for (const BasicBlock &BB : F->getBasicBlockList()) {
      for (auto I = BB.begin(), IE = BB.end(); I != IE; ++I) {
        const MDNode *TBAANode = I->getMetadata(LLVMContext::MD_tbaa);
        if (TBAANode) {
          if (auto TBAAOp0 = dyn_cast<MDNode>(TBAANode->getOperand(0))) {
            if (auto TBAAOp0Str = dyn_cast<MDString>(TBAAOp0->getOperand(0))) {
              if (TBAAOp0Str->getString() == "vtable pointer") {
                handleVTablePointer(I, BB.begin(), IE);
              }
            }
          }
        }
      }
    }
  }

  errs() << ":: ClassQuery::runOnModule\n";

  return false;
}

/*
void ClassQuery::populateTypesAndBaseTypes(const Module &M) {
  for (const StructType *T : M.getIdentifiedStructTypes()) {
    if (!(T->getName().startswith("class")
          || T->getName().startswith("struct"))) {
      continue;
    }
    if (T->getName().startswith("class.std::")
        || T->getName().startswith("struct.std::")) {
      continue;
    }
    if (T->getName().startswith("class.__gnu_cxx::")
        || T->getName().startswith("struct.__gnu_cxx::")) {
      continue;
    }
    if (isBaseType(T)) {
      BaseTypes[T] = findNonBaseType(M, T);
    }
    else {
      Types.push_back(T);
    }
  }
}
*/

void ClassQuery::insertSupertypes(const DICompositeType *DebugTy) {
  DebugTypeSet &Supertypes = DirectSupertypes[DebugTy];

  const DINodeArray Elements = DebugTy->getElements();
  for (auto Element : Elements) {
    const DIDerivedType *DerivedTy = dyn_cast<DIDerivedType>(Element);
    if (!DerivedTy) {
      return;
    }
    if (DerivedTy->getTag() != dwarf::DW_TAG_inheritance) {
      return;
    }
    const DIType *BaseTy = DerivedTy->getBaseType().resolve();
    while (auto DT = dyn_cast<DIDerivedType>(BaseTy)) {
      assert(DT->getTag() == dwarf::DW_TAG_typedef);
      BaseTy = DT->getBaseType().resolve();
    }
    auto DebugSuperTy = cast<DICompositeType>(BaseTy);
    Supertypes.insert(DebugSuperTy);
  }
}

void ClassQuery::populateDirect(const Module &M) {
  for (const Function &F : M.getFunctionList()) {
    if (F.empty()) {
      continue;
    }

    const StructType *T = findStructType(&F);
    if (!T) {
      continue;
    }

    const DISubprogram *Definition = F.getSubprogram();
    assert(Definition && "Definition required");
    assert(Definition->isDefinition()
           && "Subprogram expected to be definition");
    bool IsMethod, IsConst;
    const DICompositeType *DebugTy;
    checkDefinition(Definition, IsMethod, IsConst, DebugTy);

    if (!IsMethod) {
      continue;
    }

    if (isCtorOrDtor(F)) {
      continue;
    }

    /*
    if (isNoopOrConstantFunction(&F)) {
      continue;
    }
    */
    /*
    if (isBaseType(T)) {
        F.dump();
    }
    */
    
    if (isIgnoredType(T)) {
      if (IgnoredTypes.count(T) == 0) {
        IgnoredTypes.insert(T);
        TypeToDebugType[T] = DebugTy;
        DebugTypeToType[DebugTy] = T;
        insertSupertypes(DebugTy);
      }
      continue;
    }

    AllMethods.push_back(&F);

    if (Types.count(T) == 0) {
      Types.insert(T);
      TypeToDebugType[T] = DebugTy;
      DebugTypeToType[DebugTy] = T;
      insertSupertypes(DebugTy);
    }

    const DISubprogram *Declaration = Definition->getDeclaration();
    assert(Declaration && "Declaration required");
    bool IsCXXClass = T->getName().startswith("class");
    bool IsPublic = isDeclarationPublic(Declaration, IsCXXClass);

    if (IsConst && IsPublic) {
      DirectPublicConstMethods[T].insert(&F);
    }

    unsigned Virtuality = Declaration->getVirtuality();
    assert((Virtuality == 0 || Virtuality == 1)
           && "Virtuality expected to be 0 or 1");

    if (Virtuality == 0) {
      continue;
    }

    unsigned VirtualIndex = Declaration->getVirtualIndex();
    unsigned MinVirtualMethods = VirtualIndex + 1;

    if (DirectVirtualMethods[T].size() < MinVirtualMethods) {
      DirectVirtualMethods[T].resize(MinVirtualMethods);
    }
    DirectVirtualMethods[T][VirtualIndex] = &F;
  }
}

void ClassQuery::populateNonDirect(const StructType *T) {
  std::set<const DICompositeType *> Visited;
  std::queue<const DICompositeType *> Queue;

  Queue.push(TypeToDebugType[T]);
  auto &ThisPublicConstMethods = PublicConstMethods[T];
  auto &ThisSupertypes = Supertypes[T];
  auto &VTable = VTables[T];
  while (!Queue.empty()) {
    auto CurrentDebugTy = Queue.front();
    Queue.pop();
    Visited.insert(CurrentDebugTy);
    for (auto Supertype : DirectSupertypes[CurrentDebugTy]) {
      if (Visited.count(Supertype) == 0) {
        if (DebugTypeToType.count(Supertype) > 0) {
          ThisSupertypes.insert(DebugTypeToType[Supertype]);
        }
        Queue.push(Supertype);
      }
    }

    if (DebugTypeToType.count(CurrentDebugTy) == 0) {
      continue;
    }

    auto CurrentTy = DebugTypeToType[CurrentDebugTy];

    for (const Function *F : DirectPublicConstMethods[CurrentTy]) {
      bool IsSame = false;
      for (const Function *CurrentF : ThisPublicConstMethods) {
        if (isSameFunctionNameAndParams(F, CurrentF)) {
          IsSame = true;
          break;
        }
      }
      if (!IsSame) {
        ThisPublicConstMethods.insert(F);
      }
    }

    unsigned VirtualIndex = 0;
    for (const Function *F : DirectVirtualMethods[CurrentTy]) {
      unsigned MinVirtualMethods = VirtualIndex + 1;
      if (VTable.size() < MinVirtualMethods) {
        VTable.resize(MinVirtualMethods);
      }

      if (F) {
        if (VTable[VirtualIndex] == nullptr) {
          VTable[VirtualIndex] = F;
        }
        else {
          // The same virtual index slot should be the same name and params
          //assert(isSameFunctionNameAndParams(F, VTable[VirtualIndex]));
        }
      }
      ++VirtualIndex;
    }
  }
}

const StructType *ClassQuery::findStructType(const Function *F) {
  const Argument *FirstArg = getFirstNonRetArg(F);
  if (!FirstArg) {
    return nullptr;
  }
  const PointerType *ArgT = dyn_cast<PointerType>(FirstArg->getType());
  if (!ArgT) {
    return nullptr;
  }
  const StructType *ArgElementT = dyn_cast<StructType>(ArgT->getElementType());
  if (!ArgElementT) {
    return nullptr;
  }
  return ArgElementT;
}

void ClassQuery::handleVTablePointer(BasicBlock::const_iterator I,
                                     BasicBlock::const_iterator IB,
                                     BasicBlock::const_iterator IE) {
  assert(I != IB && "VTable load should not be the first instruction");
  --I;
  const BitCastInst &BI = cast<BitCastInst>(*I);
  ++I;
  // A store instruction should only happen in a constructor
  if (isa<StoreInst>(*I)) {
    IgnoredInsts.insert(&BI);
    IgnoredInsts.insert(&*I);
    return;
  }
  const LoadInst &LI = cast<LoadInst>(*I);
  assert(I != IE && "VTable load should not be the last instruction");
  ++I;
  if (!isa<GetElementPtrInst>(*I)) {
    I->dump();
    I->getParent()->dump();
  }
  const GetElementPtrInst &GI = cast<GetElementPtrInst>(*I);
  assert(I != IE && "VTable gep should not be the last instruction");
  ++I;
  if (isa<BitCastInst>(*I)) {
    IgnoredInsts.insert(&*I);
    ++I;
  }
  if (!isa<LoadInst>(*I)) {
    I->dump();
    I->getParent()->dump();
  }
  const LoadInst &LEntryI = cast<LoadInst>(*I);
  assert(I != IE && "VTable load entry should not be the last instruction");
  ++I;
  // Skip all the instructions for the rest of the arguments
  while (!(isa<CallInst>(*I) || isa<InvokeInst>(*I))) {
    ++I;
  }
  const Instruction *CI = &*I;
  //const CallInst &CI = cast<CallInst>(*I);

  assert(GI.getNumIndices() == 1 && "VTable gep expected to have one index");
  unsigned Index = cast<ConstantInt>(*(GI.idx_begin()))->getLimitedValue();
  // assert(Index == 0 && "VTable gep expected to have zero as first index");

  VTableInsts.insert(std::make_pair(&BI, Index));
  VTableInsts.insert(std::make_pair(&LI, Index));
  VTableInsts.insert(std::make_pair(&GI, Index));
  VTableInsts.insert(std::make_pair(&LEntryI, Index));
  VTableInsts.insert(std::make_pair(CI, Index));
}

const ClassQuery::FunctionSet &ClassQuery::getPublicConstMethods(
    const StructType *T) {
  return PublicConstMethods[T];
}

const ClassQuery::TypeSet &ClassQuery::getTypes() {
  return Types;
}

bool ClassQuery::isIgnoredInst(const Instruction *I) {
  return IgnoredInsts.count(I) > 0;
}

bool ClassQuery::isVTableInst(const Instruction *I) {
  return VTableInsts.count(I) > 0;
}

const Function *ClassQuery::getVTableEntry(const Instruction *I,
                                           const StructType *T) {
  assert(VTableInsts.count(I) > 0 && "Instruction must involved in vtable");
  unsigned Index = VTableInsts[I];
  auto &VTable = VTables[T];
  if (Index >= VTable.size()) {
    // Assuming that this function is a noop
    return nullptr;
  }
  const Function *F = VTable[Index];
  //assert(F && "Instruction vtable entry not found");
  return F;
}

bool ClassQuery::isSupertype(const StructType *T,
                             const StructType *SuperTy) const {
  struct Entry {
    const StructType *Ty;
    Indices Idx;
  };
  std::queue<Entry> Q;
  Q.push({T, {}});
  while (!Q.empty()) {
    Entry &E = Q.front();
    for (unsigned I = 0; I < E.Ty->getNumElements(); ++I) {
      Indices Idx = E.Idx;
      Idx.push_back(I);
      if (const auto ElementTy = dyn_cast<StructType>(E.Ty->getElementType(I))) {
        if (isBaseType(ElementTy)) {
          unsigned NumElementsBase = ElementTy->getNumElements();
          unsigned NumElementsSuper = SuperTy->getNumElements();
          if (NumElementsSuper >= NumElementsBase) {
            bool AllSame = true;
            for (unsigned I = 0; I < NumElementsBase; ++I) {
              if (ElementTy->getElementType(I) != SuperTy->getElementType(I)) {
                AllSame = false;
                break;
              }
            }
            if (AllSame) {
              return true;
            }
          }
        }
        if (ElementTy == SuperTy) {
          return true;
        }
        Q.push({ElementTy, Idx});
      }
    }
    Q.pop();
  }
  return false;
  //return Supertypes.lookup(T).count(SuperTy) > 0;
}

ClassQuery::Indices ClassQuery::getSupertypeIndices(const StructType *T,
                                                    const StructType *SuperTy) {
  /*
  assert((TypeToDebugType.count(T) > 0) && "Type has no debug type");
  assert((TypeToDebugType.count(SuperTy) > 0) && "Supertype has no debug type");
  */
  //if (!isSupertype(T, SuperTy)) {
/*
    if (T->getName() == "class.google::protobuf::Message"
        || T->getName() == "class.google::protobuf::io::ArrayOutputStream"
        || SuperTy->getName() == "class.google::protobuf::io::ZeroCopyOutputStream") {
      Indices Idx;
      Idx.push_back(0);
      return Idx;
    }
*/
    //  errs() << "Type "; T->dump();
    //  errs() << " Does not have supertype "; SuperTy->dump();
    //}

    //assert(Supertypes[T].count(SuperTy) > 0
    //     && "Supertype argument is not a subtype");

  /*
  Indices Idx;

  bool Found = false;
  for (unsigned I = 0; I < T->getNumElements(); ++I) {
    if (const StructType *ContainedTy =
        dyn_cast<StructType>(T->getElementType(I))) {
      if (ContainedTy == SuperTy) {
        Idx.push_back(I);
        Found = true;
        break;
      }
    }
  }
  */

  struct Entry {
    const StructType *Ty;
    Indices Idx;
  };
  std::queue<Entry> Q;
  Q.push({T, {}});
  while (!Q.empty()) {
    Entry &E = Q.front();
    for (unsigned I = 0; I < E.Ty->getNumElements(); ++I) {
      Indices Idx = E.Idx;
      Idx.push_back(I);
      if (const auto ElementTy = dyn_cast<StructType>(E.Ty->getElementType(I))) {
        if (isBaseType(ElementTy)) {
          unsigned NumElementsBase = ElementTy->getNumElements();
          unsigned NumElementsSuper = SuperTy->getNumElements();
          if (NumElementsSuper >= NumElementsBase) {
            bool AllSame = true;
            for (unsigned I = 0; I < NumElementsBase; ++I) {
              if (ElementTy->getElementType(I) != SuperTy->getElementType(I)) {
                AllSame = false;
                break;
              }
            }
            if (AllSame) {
              return Idx;
            }
          }
        }
        if (ElementTy == SuperTy) {
          return Idx;
        }
        if (ElementTy->getName() == "class.clang::CodeGen::ABIInfo") {
            return Idx;
        }

        Q.push({ElementTy, Idx});
      }
    }
    Q.pop();
  }

  errs() << "T: "; T->print(errs());
  errs() << "\n===\n";
  errs() << "SuperTy: "; SuperTy->print(errs());

  llvm_unreachable("Supertype not found in struct");
  /*
  if (!Found) {
    errs() << "T: "; T->dump();
    errs() << "SuperTy: "; SuperTy->dump();
  }
  assert(Found && "Supertype not found in struct");
  return Idx;
  */
}

char ClassQuery::ID = 0;
static RegisterPass<ClassQuery> X("class-query", "Class Queries", false, true);
