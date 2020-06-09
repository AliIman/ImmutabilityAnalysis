#include "ImmutabilityAnalysis.h"

#include "Debug.h"
#include "FunctionAnalysis.h"
//#include "FunctionUtil.h"
#include "Node.h"

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
#include <llvm/Support/raw_ostream.h>

#include <deque>

using namespace llvm;
using namespace immutability;

char ImmutabilityAnalysis::ID = 0;
static RegisterPass<ImmutabilityAnalysis> X("immutability", "Immutability Analysis", false, true);

namespace {

void getAllPointees(NodeSetT &S, NodePtr N) {
  if (N->isPointer()) {
    if (N->hasPointerPointee()) {
      auto Pointee = N->getPointerPointee();
      S.insert(Pointee);
    }
  }
  else if (N->isStruct()) {
    for (unsigned I = 0; I < N->getStructNumElements(); ++I) {
      if (N->hasStructElement(I)) {
        auto Field = N->getStructElement(I);
        getAllPointees(S, Field);
      }
    }
  }
}

std::vector<BasicBlockEdge> getIgnoredEdges(const Function *F) {
  const BasicBlock *SingleExit = nullptr;
  bool SingleExitValid = true;
  for (const BasicBlock &BB : F->getBasicBlockList()) {
    if (hasNoSuccessors(&BB)) {
      if (!SingleExit) {
        SingleExit = &BB;
      }
      else {
        SingleExitValid = false;
      }
    }
  }
  std::vector<BasicBlockEdge> IgnoredEdges;
  if (SingleExitValid) {
    if (getNumPredecessors(SingleExit) == 2) {
      for (const BasicBlock *PredBB : predecessors(SingleExit)) {
        IgnoredEdges.push_back(BasicBlockEdge(PredBB, SingleExit));
      }
    }
  }
  return IgnoredEdges;
}

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

const StructType * findNonBaseType(const Module &M, const StructType *BaseT) {
  assert(isBaseType(BaseT) && "Required base type");
  StringRef NonBaseTypeName = BaseT->getName().substr(
    0, BaseT->getName().size() - StringRef(".base").size());
  const StructType *NonBaseType = nullptr;
  for (const StructType *T : M.getIdentifiedStructTypes()) {
    if (T->getName() == NonBaseTypeName) {
      NonBaseType = T;
      break;
    }
  }
  return NonBaseType;
}

void addSubStructs(const Module &M,
                   SmallPtrSet<const StructType *, 16> &SubStructs,
                   const StructType *StructTy) {
  for (const Type *Element : StructTy->elements()) {
    if (const StructType *SubStruct = dyn_cast<StructType>(Element)) {
      SubStructs.insert(SubStruct);
      if (isBaseType(SubStruct)) {
        SubStructs.insert(findNonBaseType(M, SubStruct));
      }
      addSubStructs(M, SubStructs, SubStruct);
    }
  }
}

}

bool ImmutabilityAnalysis::runOnModule(Module &M) {

    errs() << ":: ImmutabilityAnalysis::runOnModule\n";

    bool DEBUG = false;
    const char *PackageIDEnv = getenv("IMMUTABILITY_PACKAGE_ID");
    assert(PackageIDEnv != nullptr);
    unsigned PackageID;
    bool Err = StringRef(PackageIDEnv).getAsInteger(10, PackageID);
    assert(!Err);

    errs() << ":: I'm here!!! ImmutabilityAnalysis::runOnModule!\n";
    Q = make_unique<Query>(getAnalysis<ClassQuery>(),
                           getAnalysis<MemQuery>());

    database::setup();
    unsigned NumClasses = 0;
    auto Entries = database::getPublicMethods(PackageID);
    for (auto &Entry : Entries) {
      database::CurRecordDeclID = Entry.ID;

      SmallPtrSet<const Function *, 16> PublicConstMethods;
      bool HasUnknownMethod = false;
      for (database::MethodEntry &ME : Entry.Methods) {
        const Function *F = M.getFunction(ME.MangledName);
        /* if (ME.MangledName != "_ZNK8Sequence7PolySNP7ThetaPiEv") continue; // TODO: DELETE */
        /* if (ME.MangledName != "_ZNK8Sequence7PolySNP15StochasticVarPiEv") continue; // TODO: DELETE */
        /* if (ME.MangledName != "_ZNK8Sequence7PolySNP9FuLiDStarEv" */
        /*     && ME.MangledName != "_ZNK8Sequence7PolySNP7ThetaPiEv") continue; // TODO: DELETE */
        /* if (ME.MangledName != "_ZNK8Sequence7PolySNP6HprimeERKb") continue; // TODO: DELETE */
        /* if (ME.MangledName != "_ZNK12TestThisWeak4getXEv") continue; // TODO: DELETE */
        /* if (ME.MangledName != "_ZNK8CacheBad8getValueEv" */
        /*     && ME.MangledName != "_ZNK8CacheBad9unrelatedEv") continue; */
        /* if (ME.MangledName != "_ZNK8CacheBad8getValueEv") continue; */
        /* if (ME.MangledName != "_ZNK8Sequence7PolySNP6WallsQEv") continue; */
        /* if (!(ME.MangledName == "_ZNK8Sequence7PolySNP6DandVHEv" */
        /*       || ME.MangledName == "_ZNK8Sequence7PolySNP14DisequilibriumERKjRKd")) continue; */

        if (F) {
          if (F->empty()) {
            HasUnknownMethod = true;
            break;
          }
          if (F->getName().contains("DebugString")) {
            continue;
          }
/*
          if (!F->getName().contains("Wall")
              && !F->getName().contains("Walsdfasdls")) {
            //continue;
          }
*/
/*
          if (!F->getName().contains("ThetaPi")
              && !F->getName().contains("StochasticVarPi")
              && !F->getName().contains("SamplingVarPi")) {
            continue;
          }
*/
          PublicConstMethods.insert(F);
        }
      }
      if (PublicConstMethods.empty())
        continue;
/*
      if (PublicConstMethods.size() >= 32)
        continue;
*/
      if (HasUnknownMethod)
        continue;

/*
      // fish
      if (Entry.Name == "env_vars_snapshot_t")
        continue;
      if (Entry.Name == "pager_t")
        continue;
      if (Entry.Name == "parser_t")
        continue;
      if (Entry.Name == "complete_entry_opt")
        continue;
      if (Entry.Name == "env_universal_t")
        continue;
      // mosh
      if (Entry.Name == "Base64Key")
        continue;
      if (Entry.Name == "UserStream")
        continue;
      if (Entry.Name == "Clear")
        continue;
      if (Entry.Name == "Collect")
        continue;
      if (Entry.Name == "CSI_Dispatch")
        continue;
      if (Entry.Name == "Esc_Dispatch")
        continue;
      if (Entry.Name == "Hook")
        continue;
      if (Entry.Name == "Put")
        continue;
      if (Entry.Name == "Unhook")
        continue;
      if (Entry.Name == "Execute")
        continue;
      if (Entry.Name == "OSC_Start")
        continue;
      if (Entry.Name == "OSC_Put")
        continue;
      if (Entry.Name == "OSC_End")
        continue;
      if (Entry.Name == "UserByte")
        continue;
      if (Entry.Name == "Resize")
        continue;
      if (Entry.Name == "Ignore")
        continue;
      if (Entry.Name == "Param")
        continue;
      if (Entry.Name == "Print")
        continue;
      if (Entry.Name == "Cell")
        continue;
      if (Entry.Name == "Complete")
        continue;
      if (Entry.Name == "Display")
        continue;
      if (Entry.Name == "NotificationEngine")
        continue;
      if (Entry.Name == "PredictionEngine")
        continue;
      // libsequence
      if (Entry.Name == "Comeron95")
        continue;
      if (Entry.Name == "FST")
        continue;
      if (Entry.Name == "GranthamWeights2")
        continue;
      if (Entry.Name == "GranthamWeights3")
        continue;
      if (Entry.Name == "PolySIM")
        continue;
      if (Entry.Name == "PolySites")
        continue;
      // protobuf
      if (Entry.Name == "Message")
        continue;
      if (Entry.Name == "Any")
        continue;
      if (Entry.Name == "Api")
        continue;
      if (Entry.Name == "BoolValue")
        continue;
      //if (Entry.Name == "BytesValue")
      //  continue;
      if (Entry.Name == "AccessInfo")
        continue;
      if (Entry.Name == "FieldIndexSorter")
        continue;
      if (Entry.Name == "RepeatedPtrFieldMessageAccessor"
          || Entry.Name == "RepeatedPtrFieldStringAccessor")
        continue;
if(Entry.Name != "BytesValue") continue;
*/

      /* if (Entry.Name != "PolySNP") */
      /*  continue; */

      /*
      if (Entry.Name == "Kimura80")
          DEBUG=true;
      if (!DEBUG)
       continue;
      if (Entry.Name == "PolySIM")
          continue;
      */

      /* if (Entry.Name != "PolySNP") */
      /*  continue; */
      /* if (Entry.Name != "GranthamWeights2") */
      /*  continue; */
      // if (Entry.Name != "Print")
      //  continue;

      if (Entry.Name == "PolySIM")
       continue;
      if (Entry.Name == "env_vars_snapshot_t")
       continue;
      if (Entry.Name == "pager_t")
        continue;
      if (Entry.Name == "paser_t")
        continue;
      if (Entry.Name == "Complete")
        continue;
      // if (Entry.Name != "Clear")
      //   continue;

      errs() << "\033[1;34m" << Entry.ID << "\033[0;34m " << Entry.Name
             << " (" << PublicConstMethods.size() << " methods)\033[m\n";

      StructType *T = nullptr;
      for (const Function *F : PublicConstMethods) {
        const Argument *A = getThisArg(F);
        auto StructTy = cast<StructType>(A->getType()->getPointerElementType());
        if (!T)
          T = StructTy;
        else {
          if (T != StructTy) {
            SmallPtrSet<const StructType *, 16> SubStructs;

            addSubStructs(M, SubStructs, StructTy);
            if (SubStructs.count(T)) {
              T = StructTy;
            }
          }
        }
      }

      if (T->getName() == "class.Parser::Action.118") {
        for (const Function *F : PublicConstMethods) {
          const Argument *A = getThisArg(F);
          auto StructTy = cast<StructType>(A->getType()->getPointerElementType());
          if (StructTy != T) {
            T = StructTy;
          }
        }
      }

      CurrentType = T;
      IncompleteMethodInitialStates.clear();
      CompleteMethodInitialStates.clear();

      analyzeMethods(Entry.Name, PublicConstMethods);
      ++NumClasses;
    }
    errs() << "Analyzed " << NumClasses << " classes\n";
    database::finish();
    return false;

    /*
    auto &CQ = Q->C;

    for (const StructType *T : CQ.getTypes()) {
      // Bad Fish classes
      if (T->getName() == "class.parse_node_tree_t.662"
          || T->getName() == "class.job_t"
          || T->getName() == "class.pager_t"
          || T->getName() == "class.history_item_t.294"
          || T->getName() == "class.env_vars_snapshot_t"
          || T->getName() == "class.parser_t") {
        continue;
      }

      // Bad Ninja classes
      if (T->getName() == "struct.Node") {
        continue;
      }

      // Bad Mosh classes
      if (T->getName() == "class.Terminal::Display"
          || T->getName() == "class.Overlay::ConditionalOverlayRow.3480"
          || T->getName() == "class.Terminal::DrawState"
          || T->getName() == "class.Terminal::Complete"
          || T->getName() == "class.Terminal::Framebuffer"
          || T->getName() == "class.Parser::Print"
          || T->getName() == "class.Parser::Resize"
          || T->getName() == "class.Overlay::PredictionEngine"
          || T->getName() == "class.ClientBuffers::UserMessage"
          || T->getName() == "class.Overlay::NotificationEngine"
          || T->getName() == "class.TransportBuffers::Instruction"
          || T->getName() == "class.Network::UserStream") {
        continue;
      }

      // Bad PS3 classes
      if (T->getName() == "struct.shader_code::builder::writer_t"
          || T->getName() == "struct.rsx::fragment_program::decompiler<shader_code::glsl_language>::instruction_t"
          || T->getName().startswith("struct.shader_code::clike_language_impl::expression")
          || T->getName() == "struct.shader_code::clike_language_impl::expression_t.63") {
        continue;
      }

      // Bad sequence classes
      if (T->getName() == "class.Sequence::PolyTable"
          || T->getName() == "class.Sequence::SimData.635"
          || T->getName() == "class.Sequence::PolySIM"
          || T->getName() == "class.Sequence::PolySNP"
          || T->getName() == "class.Sequence::ClustalW"
          || T->getName() == "class.Sequence::samrecord"
          || T->getName() == "class.Sequence::FST"
          || T->getName() == "class.Sequence::GranthamWeights3"
          || T->getName() == "class.Sequence::RedundancyCom95.781"
          || T->getName() == "class.Sequence::Comeron95") {
        continue;
      }

      CurrentType = T;
      IncompleteMethodInitialStates.clear();
      CompleteMethodInitialStates.clear();

      auto &PublicConstMethods = CQ.getPublicConstMethods(T);
      if (PublicConstMethods.size() == 0) {
        continue;
      }
      errs() << "\033[1;34mStruct %" << T->getName() << " ("
             << "public const methods: " << PublicConstMethods.size()
             << ")\033[0m\n";
      analyzeMethods(PublicConstMethods);
    }

    return false;
    */
}

bool ImmutabilityAnalysis::hasEquivalentInitialState(
    GraphPtr &State, std::vector<GraphPtr> &InitialStates) {
  for (auto &InitialState : InitialStates) {
    //if (State->equivalent(*InitialState, nullptr)) {
    if (State->moreSpecific(*InitialState, nullptr)) {
      return true;
    }
  }
  return false;
}

void ImmutabilityAnalysis::handleFinalState(const FunctionSet &Methods,
                                            GraphPtr FinalState) {
  Mutex.lock();
  assert(FinalState);

  for (auto Method : Methods) {
    const Argument *ThisArg = getThisArg(Method);;
    GraphPtr NextState = FinalState->clone();
    NextState->changeThis(ThisArg);

    if (hasEquivalentInitialState(NextState,
                                  CompleteMethodInitialStates[Method])) {
      continue;
    }
    if (hasEquivalentInitialState(NextState,
                                  IncompleteMethodInitialStates[Method])) {
      continue;
    }
#if DEBUG_IMMUTABILITY_ANALYSIS
    dbgs() << "METHOD: Push incomplete initial state to "
           << Method->getName() << '\n';
    NextState->dump();
#endif
    IncompleteMethodInitialStates[Method].push_back(std::move(NextState));
  }
  Mutex.unlock();
}

unsigned NumCalls = 0;
#include <unordered_set>
bool isCallSite(const Instruction *I) {
  if (isa<DbgDeclareInst>(I)
      || isa<DbgValueInst>(I)
      || isa<DbgInfoIntrinsic>(I)) {
    return false;
  }
  else if (auto II = dyn_cast<IntrinsicInst>(I)) {
    switch (II->getIntrinsicID()) {
    case Intrinsic::lifetime_start:
    case Intrinsic::lifetime_end:
      return false;
    default:
      break;
    }
  }

  if (isa<CallInst>(I) || isa<InvokeInst>(I)) {
    return true;
  }
  return false;
}
class FunctionTest {
  const Function *CurrentFunction;
  const FunctionTest *ParentTest;
  std::deque<const BasicBlock *> Worklist;
  void addToWorklist(const BasicBlock *BB) {
    for (auto Entry : Worklist) {
      if (BB == Entry) { return; }
    }
    Worklist.push_back(BB);
  }
  const BasicBlock *getFromWorklist() {
    const BasicBlock *BB = Worklist.front();
    Worklist.pop_front();
    return BB;
  }
  bool isRecursive(const Function *F) const {
    if (CurrentFunction == F) {
      return true;
    }
    if (ParentTest == nullptr) {
      return false;
    }
    return ParentTest->isRecursive(F);
  }
  void run() {
    addToWorklist(&CurrentFunction->getEntryBlock());
    std::unordered_set<const BasicBlock *> Visited;
    while (!Worklist.empty()) {
      const BasicBlock *BB = getFromWorklist();
      Visited.insert(BB);

      for (const Instruction &I : *BB) {
        if (isCallSite(&I)) {
          CallSite CS(const_cast<Instruction *>(&I));
          const Function *Callee = CS.getCalledFunction();
          if (Callee && !Callee->empty()) {
            if (!isRecursive(Callee)) {
              FunctionTest Test(Callee, this);
            }
          }
        }
      }

      for (const BasicBlock *SuccBB : successors(BB)) {
        if (!Visited.count(SuccBB)) {
          addToWorklist(SuccBB);
        }
      }

    }
  }
public:
  FunctionTest(const Function *Current, const FunctionTest *Parent=nullptr)
      : CurrentFunction(Current), ParentTest(Parent) {
    ++NumCalls;
    run();
  }
};

void ImmutabilityAnalysis::checkArgument(const Function *Method, const GraphPtr &ResultState, const Argument *Arg) {
      // Non-this argument
      NodePtr N = ResultState->getMapping(Arg);

      NodeSetT Pointees;
      getAllPointees(Pointees, N);
      for (auto Pointee : Pointees) {
        Pointee->setIsRead();
        for (auto &TN : Pointee->getThisEdges()) {
          TN->setIsRead();
        }
        auto S = Node::getReachable(Pointee);
        for (auto R : S) {
          if (R->isThis()) {
            database::addIssue(Method->getName(), "ESCAPEARG");
            // errs() << "    \033[1;31mESCAPEARG @ "
            //        << Method->getName() << "\033[0m\n";
            break;
          }
          for (auto ThisN : R->getThisEdges()) {
            database::addIssue(Method->getName(), "ESCAPEARG");
            //errs() << "    \033[1;31mESCAPEARG @ "
            //       << Method->getName() << "\033[0m\n";
            break;
          }
          R->setIsRead();
          for (auto &TN : R->getThisEdges()) {
            TN->setIsRead();
          }
        }
      }
      /*
      auto S = Node::getReachable(N);
      for (auto N : S) {
        if (N->isSeq()) {
          if (N->isThis()) {
            errs() << "    \033[1;31mESCAPEARG @ "
                    << Method->getName() << "\033[0m\n";
            break;
          }
          for (auto ThisN : N->getThisEdges()) {
            errs() << "    \033[1;31mESCAPEARG @ "
                   << Method->getName() << "\033[0m\n";
            break;
          }
        }
        N->setIsRead();
        for (auto &TN : N->getThisEdges()) {
          TN->setIsRead();
        }
      }
      */
}

void ImmutabilityAnalysis::checkReturn(const Function *Method, const GraphPtr &ResultState) {
      auto Return = ResultState->getReturn();
      /*
      Return->setIsRead();
      for (auto &TN : Return->getThisEdges()) {
        TN->setIsRead();
      }
      */
      auto S = Node::getReachable(ResultState->getReturn());
      for (auto N : S) {
        N->setIsRead();
        for (auto &TN : N->getThisEdges()) {
          TN->setIsRead();
        }
      }
      if (ResultState->getReturn()->isPointer()) {
        for (auto N : S) {
          if (N->isThis()) {
            std::string Description;
            llvm::raw_string_ostream DescriptionOS(Description);
            DescriptionOS << "ESCAPERET @ " << Method->getName();
            if (Method) {
              database::addIssue(Method->getName(), DescriptionOS.str());
            }
          }
          for (auto ThisN : N->getThisEdges()) {
            std::string Description;
            llvm::raw_string_ostream DescriptionOS(Description);
            DescriptionOS << "ESCAPERET @ " << Method->getName();
            if (Method) {
              database::addIssue(Method->getName(), DescriptionOS.str());
            }
          }
        }
      }
}

void ImmutabilityAnalysis::runMethod(const GraphPtr &InitialState, std::string &ClassName, const FunctionSet &Methods, const Function *Method, const BasicBlockEdge *IgnoredEdge) {
  errs() << "  \033[1;36m" << IterationNum << "\033[0;36m "
         << Method->getName() << "\033[m\n";
  const Argument *ThisArg = getThisArg(Method);
  assert(InitialState);
  FunctionAnalysis FA(Q.get(), nullptr, Method, InitialState, Method, IgnoredEdge);
  auto ResultState = FA.getResult();
  ResultState->dot(ClassName, IterationNum, Method->getName());
  if (!ResultState->isBottom()) {
    for (const Argument &A : Method->args()) {
      if (ThisArg == &A) {
        continue;
      }
      // Non-this argument
      checkArgument(Method, ResultState, &A);
    }
    if (ResultState->hasReturn()) {
      checkReturn(Method, ResultState);
    }
    ResultState->removeAllExcept(ThisArg);
    ResultState->fixupThis(ThisArg);
    assert(ResultState->getMapping(ThisArg)->isThis());
    GraphPtr FinalState = ResultState->clone();
    handleFinalState(Methods, std::move(FinalState));
  }
  ++IterationNum;
}

void ImmutabilityAnalysis::iteration(std::string &ClassName, const FunctionSet &Methods) {
  Mutex.lock();
  if (IncompleteMethodInitialStates.empty()) {
    Mutex.unlock();
    return;
  }
  auto I = IncompleteMethodInitialStates.begin();

  auto &InitialStates = I->second;
  if (InitialStates.empty()) {
    IncompleteMethodInitialStates.erase(I);
    Mutex.unlock();
    return;
  }

  GraphPtr InitialState = std::move(InitialStates.back());
  InitialStates.pop_back();

  const Function *Method = I->first;
    // errs() << "  \033[1;36m" << IterationNum << "\033[0;36m "
    //        << Method->getName() << "\033[m\n";

    // std::string TMP = Method->getName().str() + "Initial";
    // InitialState->dot(ClassName, IterationNum, TMP);

  // database::setMethod(Method->getName());

    // const Argument *ThisArg = getThisArg(Method);

  std::vector<BasicBlockEdge> IgnoredEdges = getIgnoredEdges(Method);

#if DEBUG_IMMUTABILITY_ANALYSIS
  dbgs() << "METHOD: Push complete initial state to "
         << Method->getName() << '\n';
  InitialState->dump();
#endif
  CompleteMethodInitialStates[Method].push_back(InitialState->clone());

  Mutex.unlock();

  if (IgnoredEdges.empty()) {
    runMethod(InitialState, ClassName, Methods, Method);
  }
  else {
    for (const BasicBlockEdge &IgnoredEdge : IgnoredEdges) {
      runMethod(InitialState, ClassName, Methods, Method, &IgnoredEdge);
    }
  }
}

void ImmutabilityAnalysis::analyzeMethods(std::string &ClassName, const FunctionSet &Methods) {
  /*
  for (auto Method : Methods) {
    if (Method->getName() != "_ZNK19env_vars_snapshot_t3getERKNSt7__cxx1112basic_stringIwSt11char_traitsIwESaIwEEE") {
      continue;
    }
    errs() << "  \033[32mMethod " << Method->getName() << "\033[0m\n";
    FunctionTest Test(Method);
    errs() << "    Calls: " << NumCalls << '\n';
  }
  */
  //return;

  for (auto Method : Methods) {
    // if (Method->getName() != "_ZNK18complete_entry_opt19expected_dash_countEv") {
    //   continue;
    // }
    //if (Method->getName() != "_ZNK19env_vars_snapshot_t3getERKNSt7__cxx1112basic_stringIwSt11char_traitsIwESaIwEEE") {
    //  continue;
    //}

    const Argument *ThisArg = getThisArg(Method);;
    GraphPtr UninitializedClone = Graph::createEmptyExceptThis(Q.get(), ThisArg,
                                                               CurrentType);
#if DEBUG_IMMUTABILITY_ANALYSIS
    dbgs() << "METHOD: Push incomplete initial state to "
           << Method->getName() << '\n';
    UninitializedClone->dump();
#endif
    IncompleteMethodInitialStates[Method].push_back(
        std::move(UninitializedClone));
  }

  IterationNum = 0;
  // while (!IncompleteMethodInitialStates.empty()) {
  while (!(allFuturesInvalid() && incompleteMethodInitialStatesEmpty())) {
    size_t Index = getFutureAvailable();
    Futures[Index] = Pool.async([this, &ClassName, &Methods] {
      iteration(ClassName, Methods);
    });
    checkFutures();
  }
}
