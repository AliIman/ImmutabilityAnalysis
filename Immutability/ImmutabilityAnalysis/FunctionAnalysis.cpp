#include "FunctionAnalysis.h"

#include "Debug.h"
#include "TypeUtil.h"

using namespace llvm;
using namespace immutability;

unsigned llvm::immutability::DELETENumCalls = 0;

bool llvm::immutability::hasNoSuccessors(const BasicBlock *BB) {
  succ_const_iterator SI = succ_begin(BB), E = succ_end(BB);
  if (SI == E) { return true; }
  else         { return false; }
}

unsigned llvm::immutability::getNumPredecessors(const BasicBlock *BB) {
  unsigned Num = 0;
  for (const BasicBlock *Pred : predecessors(BB)) {
    ++Num;
  }
  return Num;
}

namespace {

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

bool hasNoPredecessors(const BasicBlock *BB) {
  const_pred_iterator PI = pred_begin(BB), E = pred_end(BB);
  if (PI == E) { return true; }
  else         { return false; }
}

BasicBlock::const_iterator getFirstIter(const BasicBlock *BB) {
  return BB->begin();
}

BasicBlock::const_iterator getLastIter(const BasicBlock *BB) {
  return --(BB->end());
}

}

void FunctionAnalysis::addToWorklist(const BasicBlock *BB) {
  for (auto Entry : Worklist) {
    if (BB == Entry) { return; }
  }
#if DEBUG_FUNCTION_ANALYSIS
  dbgs() << "WORKLIST: BB " << BB << " added\n";
#endif
  Worklist.push_back(BB);
}

const BasicBlock *FunctionAnalysis::getFromWorklist() {
  const BasicBlock *BB = Worklist.front();
  Worklist.pop_front();
  return BB;
}

size_t FunctionAnalysis::getWorklistSize() const {
  return Worklist.size();
}

const GraphPtr &FunctionAnalysis::getPredOrNullState(BasicBlockEdge Edge) {
  if (States.count(Edge) > 0) {
    return States[Edge];
  }
  else {
    return Null;
  }
}

const GraphPtr &FunctionAnalysis::getPredOrInitialState(BasicBlockEdge Edge) {
  const GraphPtr &PredState = getPredOrNullState(Edge);
  if (PredState) {
    return PredState;
  }
  else {
    return Initial;
  }
}

bool FunctionAnalysis::shouldWait(const BasicBlock *BB) {
  for (const_pred_iterator PI = pred_begin(BB), E = pred_end(BB); PI != E;
       ++PI) {
    const BasicBlock *PredBB = *PI;

    auto Edge = BasicBlockEdge(PredBB, BB);

    if (IgnoredEdge && IgnoredEdge->getStart() == PredBB && IgnoredEdge->getEnd() == BB) {
      continue;
    }

    const GraphPtr &PredState = getPredOrNullState(Edge);
    if (!PredState)
      return true;
  }

  return false;
}

bool FunctionAnalysis::allBottomOrNull(const BasicBlock *BB) {
  for (const_pred_iterator PI = pred_begin(BB), E = pred_end(BB); PI != E;
       ++PI) {
    const BasicBlock *PredBB = *PI;

    auto Edge = BasicBlockEdge(PredBB, BB);

    if (IgnoredEdge && IgnoredEdge->getStart() == PredBB && IgnoredEdge->getEnd() == BB) {
      continue;
    }

    const GraphPtr &PredState = getPredOrNullState(Edge);

    if (PredState && !PredState->isBottom())
      return false;
  }

  return true;
}

GraphPtr FunctionAnalysis::merge(const BasicBlock *BB) {
  if (hasNoPredecessors(BB)) {
    return Initial->clone();
  }
  GraphPtr Ret;
  for (const_pred_iterator PI = pred_begin(BB), E = pred_end(BB); PI != E;
       ++PI) {
    const BasicBlock *PredBB = *PI;

    auto Edge = BasicBlockEdge(PredBB, BB);

    // Handle ignored edge
    if (IgnoredEdge && IgnoredEdge->getStart() == PredBB && IgnoredEdge->getEnd() == BB) {
      continue;
    }

    const GraphPtr &PredState = getPredOrInitialState(Edge);
    if (PredState->isBottom()) {
      continue;
    }

    if (Ret == nullptr) { Ret = PredState->clone(); }
    else                { Ret = Graph::merge(*Ret, *PredState); }
  }

  // No predecessor state contributed to the final merge state, this means all
  // the predecessors were bottom, so just set the final merge state to bottom
  if (Ret == nullptr) {
    Ret = Graph::createBottom(Q);
  }

  return std::move(Ret);;
}

GraphPtr FunctionAnalysis::getCurrentState(BasicBlockEdge Edge,
                                           const Instruction &I) {
  if (auto BI = dyn_cast<BranchInst>(&I)) {
    if (BI->isConditional()) {
      assert(BI->getNumSuccessors() == 2
             && "Conditional branch should only have a true and false branch");
      // The first successor is the true branch, if it's the same as the branch
      // target we're assuming the condition is true
      GraphPtr CurrentState = MutableState->clone();
      bool B = BI->getSuccessor(0) == Edge.getEnd();
//errs() << "BEFORE Refine\n\n\n";
//CurrentState->dump();
      CurrentState->refineBool(BI->getCondition(), B);
//errs() << "\n\nAFTER Refine\n\n\n";
//CurrentState->dump();

      return std::move(CurrentState);
    }
  }

  return MutableState->clone();
}

bool FunctionAnalysis::isRecursive(const Function *F) const {
  if (CurrentFunction == F) {
    return true;
  }
  if (ParentAnalysis == nullptr) {
    return false;
  }
  return ParentAnalysis->isRecursive(F);
}

void FunctionAnalysis::handleDefaultDeleteCall(const Instruction *I) {
  MutableState->handleDefaultDeleteCall(I);
}

void FunctionAnalysis::handleUnknownCall(const Instruction *I) {
  // TODO: Refactor this better?
  MutableState->handleUnknownCall(I);
}

void FunctionAnalysis::handleCall(const Instruction *CI, const Function *F) {
  if (isRecursive(F)) {
    handleUnknownCall(CI);
    return;
  }

  /*
  if (F->getName().contains("default_delete")) {
  }
  */

  // TODO: Add an external ignore function list
  // This is for ninja, because these functions for some reason use VA args
  if (F->getName() == "_Z5FatalPKcz"
      || F->getName() == "_Z7WarningPKcz"
      || F->getName() == "_Z5ErrorPKcz"
      || F->getName() == "_Z5debugiPKcz"
      || F->getName() == "swprintf"
      || F->getName() == "debug_thread_error"
      || F->getName() == "_Z24exit_without_destructorsi"
      || F->getName() == "_ZSt19__throw_logic_errorPKc") {
    //llvm::errs() << "UNREACHABLE CALL?\n";
    //CI->getParent()->dump();
    return;
  }

  if (F->empty()) {
    handleUnknownCall(CI);
    return;
  }

  if (F->getName() == "_ZNSt3setIcSt4lessIcESaIcEEC2ERKS3_"
      || F->getName() == "_ZNSt3setIdSt4lessIdESaIdEEC2ERKS3_" ) {
    handleUnknownCall(CI);
    return;
  }

  //if (F->getName().contains("default_delete")) {
  //  return;
  //}
/*
  if (CurrentFunction->getName() == "_ZNK8Sequence7PolySNP9WallStatsEv") {
    if (F->getName() == "_ZNSt3setINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEN8Sequence9uniqueSeqESaIS5_EE6insertERKS5_") {
      return;
    }
    else if (F->getName() == "_ZNSt3setINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEN8Sequence9uniqueSeqESaIS5_EED2Ev") {
      return;
    }
  }
  if (CurrentFunction->getName() == "_ZNK8Sequence7PolySNP25DepaulisVeuilleStatisticsEv") {
    if (F->getName() == "_ZNSt3setINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEN8Sequence9uniqueSeqESaIS5_EE6insertIN9__gnu_cxx17__normal_iteratorIPKS5_St6vectorIS5_S8_EEEEEvT_SI_") {
      return;
    }
    else if (F->getName().startswith("_ZSt8count_ifIN9__gnu_cxx17__normal_iteratorIPKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESt6vectorIS")) {
      return;
    }
    else if (F->getName() == "_ZNSt3setINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEN8Sequence9uniqueSeqESaIS5_EED2Ev") {
      return;
    }
  }
  if (F->getName() == "_ZN6google8protobuf8internal10LogMessage6FinishEv"
      || F->getName() == "_ZN6google8protobuf8internal12MapKeySorter7SortKeyERKNS0_7MessageEPKNS0_10ReflectionEPKNS0_15FieldDescriptorE") {
    return;
  }
*/

  // This function call is unreachable, ignore
  if (auto UnI = dyn_cast_or_null<UnreachableInst>(F->getEntryBlock().getTerminator())) {
    return;
  }

  CallSite CS(const_cast<Instruction *>(CI));

  if (F->arg_size() != CS.getNumArgOperands()) {
    if (!F->isVarArg()) {
      if (F->arg_size() != CS.getNumArgOperands()) {
        F->dump();
        errs() << F->arg_size() << " vs. " << CS.getNumArgOperands() << '\n';
      }
    }
    assert(F->isVarArg());
    handleUnknownCall(CI);
    return;
  }
  assert(F->arg_size() == CS.getNumArgOperands());
  unsigned I = 0;
  for (const Argument &A : F->args()) {
    const Type *T = A.getType();
    NodePtr Orig = MutableState->getMapping(CS.getArgOperand(I));
    NodePtr N = Orig->copy();
        // Need to create edges that include this copy now
        for (auto CopyN : N->getCopyNodes()) {
          CopyN->addCopyEdge(N);
        }
        /*
        for (auto WeakN : N->getWeakNodes()) {
          WeakN->addWeakEdge(N);
        }
        */
        N->clearWeakEdges();

        //if (MutableState->GlobalTracker.hasNode(Orig)) {
        //  MutableState->GlobalTracker.addNode(N);
        //}
        Node::addCopyEdge(Orig, N);
        /*
        for (auto &Entry : MutableState->CallAliases) {
          auto &Tracker = Entry.second;
          if (Tracker.hasNode(Orig)) {
            Tracker.addNode(N);
          }
        }
        */
    if (T != N->getType()) {
      N = MutableState->getPointerElementOp(N.get()); // getSeqPointee
     if (!N->hasStructSubStruct()) {
         // probably a base
         /*
         errs() << "Types don't match without substruct\n";
         errs() << "T requesteed: "; T->dump();
         N->dump();
         */
     }
     else {
       N = N->getStructSubStruct();
     }
     //assert(N->getType()->getPointerTo() == T);
     N = PointerNode::createPointee(N);
    }
    //assert(T == N->getType());
    MutableState->addMapping(&A, N);
    ++I;
  }

  FunctionAnalysis FA(Q, this, F, MutableState, FirstMethod);

  auto Result = FA.getResult();
  if (Result->isBottom()) {
    MutableState->markIsBottom();
    MutableState->eraseRelevant(F);
    return;
  }
  const Type *T = CI->getType();
  if (!T->isVoidTy()) {
    assert(Result->hasReturn());
  }
  if (Result->hasReturn()) {
    auto Return = Result->getReturn();
    if (Return->getType() != CI->getType()) {
        /*
      errs() << "Hmmm?\n";
      CI->dump();
      Return->getType()->dump();
      CI->getType()->dump();
      */
      Return->setType(CI->getType());
    }
    //assert(Return->getType() == CI->getType());
    MutableState = std::move(Result);
    MutableState->removeReturn();
    MutableState->addMapping(CI, Return->copy());
  }
  else {
    MutableState = std::move(Result);
  }
  MutableState->setFirstMethod(FirstMethod);

  MutableState->eraseRelevant(F);
}

void FunctionAnalysis::handlePHINode(const PHINode &I) {
  const BasicBlock *BB = I.getParent();
  NodeSetT S;
  for (unsigned i = 0; i < I.getNumIncomingValues(); ++i) {
    const BasicBlock *PredBB = I.getIncomingBlock(i);
    BasicBlockEdge Edge(PredBB, BB);
    auto &Pred = getPredOrInitialState(Edge);

    // In order for this to execute at least one branch is not bottom
    // Therefore N will always be initialized at the end of this function
    if (Pred->isBottom()) {
      continue;
    }

    const Value *V = I.getIncomingValue(i);
    NodePtr N = MutableState->getMapping(V);
    S.insert(N);
  }

  assert(S.size() > 0);
  MutableState->addMapping(&I, Node::unionAll(*MutableState, S));
}

void FunctionAnalysis::handleExitTerminator(const Instruction &I) {
  if (isa<UnreachableInst>(I)) {
    llvm::errs() << "  >>>> unreachableinst\n";
    I.getParent()->dump();
  }

  ExitStates[&I] = std::move(MutableState);
}

void FunctionAnalysis::handleTerminator(const Instruction &I) {
  const BasicBlock *BB = I.getParent();

#if DEBUG_FUNCTION_ANALYSIS
      dbgs() << "WORKLIST: BB " << BB << " finished\n";
#endif

  if (I.getNumSuccessors() == 0) {
    return handleExitTerminator(I);
  }

  for (int i=0; i<I.getNumSuccessors(); i++) {
    const BasicBlock *SuccBB = I.getSuccessor(i);
    // Skip unreachable blocks
    if (const UnreachableInst* UI = dyn_cast_or_null<UnreachableInst>(SuccBB->getTerminator())) {
      continue;
    }
    BasicBlockEdge Edge(BB, SuccBB);
    auto &PreviousState = getPredOrNullState(Edge);
    GraphPtr CurrentState = getCurrentState(Edge, I);

    if (PreviousState.get() != nullptr) {
      if (!(PreviousState->equivalent(*CurrentState, CurrentFunction))) {
        addToWorklist(SuccBB);
      }
    }
    else {
        //assert(PreviousState.get() == nullptr);
      addToWorklist(SuccBB);
    }

    States[Edge] = std::move(CurrentState);
  }
}

GraphPtr FunctionAnalysis::getResult() {
  if (ExitStates.size() == 0) {
    CurrentFunction->dump();
  }

  assert(ExitStates.size() > 0 && "Function analysis does not exit");

  const Type *ReturnTy = CurrentFunction->getReturnType();
  GraphPtr Result;
  bool IsVoid = false;
  if (ReturnTy->isVoidTy()) {
    IsVoid = true;
  }

  //NodeToNodeSetT StateToPartial;
  bool Initialized = false;
  for (auto &Entry : ExitStates) {
    if (isa<UnreachableInst>(Entry.first)) {
      continue;
    }

    const Value *ReturnValue;
    if (auto ResumeI = dyn_cast<ResumeInst>(Entry.first)) {
      ReturnValue = ResumeI->getValue();
    }
    else {
      auto RI = cast<ReturnInst>(Entry.first);
      ReturnValue = RI->getReturnValue();
    }

    GraphPtr &State = Entry.second;

#if DEBUG_FUNCTION_ANALYSIS
    dbgs() << "EXITSTATE: " << *Entry.first << "\n";
#endif

    if (State->isBottom()) {
      continue;
    }

    if (!Initialized) {
      Result = State->clone();
      if (!IsVoid) {
          assert(ReturnValue);
        Result->addReturn(ReturnValue);
      }
      Initialized = true;
    }
    else {
      GraphPtr Next = State->clone();
      if (!IsVoid) {
          assert(ReturnValue);
        Next->addReturn(ReturnValue);
      }
      Result = Graph::merge(*Result, *Next);
    }
  }
  if (!Initialized) {
    // TODO2018: This shouldn't be hit
    // CurrentFunction->print(errs());
    Result = Graph::createBottom(Q);
    Initialized = true;
  }
  assert(Initialized && "Result requires one valid exit state");
  //Result->verify();

  // WriteGraph(llvm::errs(), &*Result);

  return std::move(Result);
}

void FunctionAnalysis::run() {
#if DEBUG_FUNCTION_ANALYSIS
  dbgs() << "RUN: Function " << CurrentFunction->getName() << '\n';
#endif
  //dbgs() << "RUN-START: Function " << CurrentFunction->getName() << '\n';

  if (CurrentFunction->empty()) {
    CurrentFunction->print(errs());
  }
  assert(!CurrentFunction->empty());

  std::deque<BasicBlock::const_iterator> InstWorklist;

  addToWorklist(&CurrentFunction->getEntryBlock());

  while (!(Worklist.empty() && InstWorklist.empty())) {
    if (InstWorklist.empty()) {
      size_t MaxTries = getWorklistSize();
      size_t Tries = 0;
      const BasicBlock *BB = getFromWorklist();

      while (shouldWait(BB)) {
        addToWorklist(BB);
        BB = getFromWorklist();
        ++Tries;
        if (Tries == MaxTries) {
          break;
        }
      }

#if DEBUG_FUNCTION_ANALYSIS
      dbgs() << "WORKLIST: BB " << BB << " started\n";
#endif

      if (Tries == MaxTries && allBottomOrNull(BB)) {
        MutableState = Graph::createBottom(Q);
      }
      else {
        MutableState = std::move(merge(BB));
      }
      MutableState->setFirstMethod(FirstMethod);

      if (MutableState->isBottom()) {
        // Skip all analysis and just handle the last instruction
        InstWorklist.push_back(getLastIter(BB));
      }
      else {
        InstWorklist.push_back(getFirstIter(BB));
      }
      continue;
    }

    auto Iter = InstWorklist.back();
    InstWorklist.pop_back();
    const Instruction &I = *Iter;
    //assert(!isa<UnreachableInst>(I));

    // I.print(errs()); errs() << '\n';
    // errs() << CurrentFunction->getName() << " " << I << '\n'; // TODO2018: REMOVE

    if (!MutableState->isBottom()) {
    bool IsVTableInst = Q->C.isVTableInst(&I);
    if (isCallSite(&I)) {
      if (!IsVTableInst) {
          //auto CI = cast<CallInst>(&I);
        if (!Q->isIgnoredInst(&I)) {
          CallSite CS(const_cast<Instruction *>(&I));

          const Function *Callee = CS.getCalledFunction();
          if (!Callee) {
            const CallInst *CallI = dyn_cast<CallInst>(&I);
            const ConstantExpr *CVCE = nullptr;
            if (CallI) {
              CVCE = dyn_cast<ConstantExpr>(CallI->getCalledValue());
            }
            if (CVCE) {
              Instruction *CVI = const_cast<ConstantExpr *>(CVCE)->getAsInstruction();
              if (const BitCastInst *BCI = dyn_cast<BitCastInst>(CVI)) {
                if (const Function *BCFunction = dyn_cast<Function>(BCI->getOperand(0))) {
                  Callee = BCFunction;
                }
              }
              CVI->deleteValue();
            }

            if (Callee && Callee->getName().contains("default_delete")) {
              handleDefaultDeleteCall(&I);
            }
            else {
              handleUnknownCall(&I);
            }
          }
          else {
            handleCall(&I, Callee);
          }
        }
      }
      else {
          //auto CI = cast<CallInst>(&I);
        CallSite CS(const_cast<Instruction *>(&I));
        //assert(CS.getNumArgOperands() == 1);

        // Get the actual subtype of the this argument
        auto Arg0 = CS.getArgOperand(0);
        auto N = MutableState->getMapping(Arg0);
        N = MutableState->getPointerElementOp(N.get()); // getSeqPointee
        if (N->hasStructSubStruct()) {
          N = N->getStructSubStruct();
        }
        const StructType *CurrentType = cast<StructType>(N->getType());
        auto Callee = Q->C.getVTableEntry(&I, CurrentType);
        // This function is a noop if it does not return a function
        if (Callee) {
          handleCall(&I, Callee);
          /*
          llvm_unreachable("REMOVE ARGUMENTS");
          bool IsThis = true;
          assert(Callee->getArgumentList().size() == CI->getNumArgOperands());
          for (const Argument &A : Callee->args()) {
            if (IsThis) {
                auto StructTy = cast<StructType>(A.getType()->getSequentialElementType());
                //Arguments[&A] = PointerNode::createPointee(N);
              IsThis = false;
            }
            else {
              llvm_unreachable("TODO remaining args");
            }
          }
          */
        }
        else {
          handleUnknownCall(&I);
        }
      }
    }
    else if (!IsVTableInst) {
      if (isa<PHINode>(&I)) {
        handlePHINode(cast<PHINode>(I));
      }
      else if (isa<LandingPadInst>(I)) {
        MutableState->markIsBottom();
      }
      else if (!I.isTerminator()) {
        if (!Q->isIgnoredInst(&I)) {
          // Normal transfer function
           // MutableState->verify();
          // errs() << CurrentFunction->getName() << " " << I << '\n'; // TODO2018: REMOVE
          MutableState->visit(const_cast<Instruction &>(I));
          // errs() << "after\n";
          // MutableState->verify();
        }
      }
    }
    }

    if (I.isTerminator()) {
      handleTerminator(cast<Instruction>(I));
    }
    else {
      InstWorklist.push_back(++Iter);
    }
  }
  //dbgs() << "RUN-END  : Function " << CurrentFunction->getName() << '\n';
}
