#ifndef LLVM_ANALYSIS_IMMUTABILITY_FUNCTION_ANALYSIS_H
#define LLVM_ANALYSIS_IMMUTABILITY_FUNCTION_ANALYSIS_H

#include "Graph.h"
#include "ImmutabilityAnalysis.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/Dominators.h>

#include <deque>

namespace llvm {
namespace immutability {

unsigned getNumPredecessors(const BasicBlock *BB);
bool hasNoSuccessors(const BasicBlock *BB);

extern unsigned DELETENumCalls;

class FunctionAnalysis {
public:
  typedef DenseMap<const Argument *, NodePtr> ArgumentsTy;

private:
  Query *Q;
  FunctionAnalysis *ParentAnalysis;
  const Function *CurrentFunction;
  const Function *FirstMethod;
  const BasicBlockEdge *IgnoredEdge;

  GraphPtr Initial;
  GraphPtr Null;

  std::deque<const BasicBlock *> Worklist;
  GraphPtr MutableState;
  DenseMap<BasicBlockEdge, GraphPtr> States;
  DenseMap<const Instruction *, GraphPtr> ExitStates;

  void addToWorklist(const BasicBlock *BB);
  const BasicBlock *getFromWorklist();
  size_t getWorklistSize() const;

  const GraphPtr &getPredOrNullState(BasicBlockEdge Edge);
  const GraphPtr &getPredOrInitialState(BasicBlockEdge Edge);

  bool shouldWait(const BasicBlock *BB);
  bool allBottomOrNull(const BasicBlock *BB);
  GraphPtr merge(const BasicBlock *BB);
  GraphPtr getCurrentState(BasicBlockEdge Edge, const Instruction &I);

  bool isRecursive(const Function *F) const;
  void handleDefaultDeleteCall(const Instruction *I);
  void handleUnknownCall(const Instruction *I);
  void handleCall(const Instruction *I, const Function *F);
  void handlePHINode(const PHINode &I);

  void handleExitTerminator(const Instruction &I);
  void handleTerminator(const Instruction &I);

  //void computeResult();

  void run();
  unsigned getDepth() {
      if (ParentAnalysis == nullptr) {
          return 1;
      }
      else {
          return ParentAnalysis->getDepth() + 1;
      }
  }
  void getStack(std::vector<StringRef> &Names) {
      Names.push_back(CurrentFunction->getName());
      if (ParentAnalysis == nullptr) {
          return;
      }
      else {
          ParentAnalysis->getStack(Names);
      }
  }
public:
  FunctionAnalysis(Query *Q,
                   FunctionAnalysis *P,
                   const Function *F,
                   const GraphPtr &I,
                   const Function *FM,
                   const BasicBlockEdge *E=nullptr)
      : Q(Q), ParentAnalysis(P), CurrentFunction(F), IgnoredEdge(E),
        FirstMethod(FM) {

    if (ParentAnalysis == nullptr)
      DELETENumCalls = 0;
    ++DELETENumCalls;
    if (DELETENumCalls >= 2000) {
      /*
      errs() << "  > WARNING NumCalls: " << DELETENumCalls << '\n';
    std::vector<StringRef> Names;
    getStack(Names);
    unsigned i = 0;
    for (auto I = Names.rbegin(), IE = Names.rend(); I != IE; ++I) {
      for (unsigned j = 0; j < i; ++j) {
        llvm::errs() << "  ";
      }
      llvm::errs() << *I << '\n';
      ++i;
    }
      */
    }
    Initial = I->clone();
    run();
  }

  // Debug only
  GraphPtr &getExitState(const Instruction *I) {
    assert(ExitStates.count(I) > 0 && "Invalid exit state");
    return ExitStates[I];
  }

  GraphPtr getResult();

    /*
private:
  ImmutabilityAnalysis &IA;
  const Function *Current;
  FunctionAnalyzer *Parent;

  GraphPtr Initial;

  DenseMap<const BasicBlock *, GraphPtr> MutableGraphs;
  DenseMap<const BasicBlock *, GraphPtr> NodeGraphs;
  DenseMap<BasicBlockEdge, GraphPtr> EdgeGraphs;

  DenseMap<const ReturnInst *, GraphPtr> Result;
  std::deque<const BasicBlock *> BBWorkList;

public:
  FunctionAnalyzer(ImmutabilityAnalysis &IA, FunctionAnalyzer *P, const Function *F, const GraphPtr &IG)
  : IA(IA), Parent(P), Current(F) {
    Initial = IG->clone();
    // errs() << "\033[32mFunction[Start] " << F->getName()
    //        << "\033[0m\n";
    analyze();
    // errs() << "\033[32mFunction[End] " << F->getName()
    //        << "\033[0m\n";
  }

  DenseMap<const ReturnInst *, GraphPtr> &getResult() { return Result; }

private:
  void doPHINode(const PHINode &I) {
    const BasicBlock *BB = I.getParent();
    GraphPtr &G = MutableGraphs[BB];
    bool Initialized = false;
    Node *N = nullptr;

    // TODO: bad hack
    if (I.getType()->isPointerTy()) {
      Node *N = G->addNode(Node::createUninitializedFromType(I.getType()));
      G->setMapping(&I, N);
      return;
    }

    for (unsigned i = 0; i < I.getNumIncomingValues(); ++i) {
      const BasicBlock *IncBB = I.getIncomingBlock(i);
      auto Key = BasicBlockEdge(IncBB, BB);
      const Value *Val = I.getIncomingValue(i);

      Node *M;
      if (const ConstantInt *CI = dyn_cast<ConstantInt>(Val)) {
        if (!N) {
          N = G->addNode(IntNode::createInitializedFromConstantInt(CI));
        }
        else {
          IntNodePtr Tmp = IntNode::createInitializedFromConstantInt(CI);
          cast<IntNode>(N)->mergeWith(*Tmp.get());
        }
      }
      else if (EdgeGraphs.count(Key) > 0) {
        if (EdgeGraphs[Key]->isBottom()) { continue; }
        auto Tmp = EdgeGraphs[Key]->getIntMapping(Val);
        if (!N) {
          N = G->addNode(Tmp->clone(true));
        }
        else {
          cast<IntNode>(N)->mergeWith(*Tmp);
        }
      }
      else if (NodeGraphs.count(IncBB) > 0) {
        if (NodeGraphs[IncBB]->isBottom()) { continue; }
        auto Tmp = NodeGraphs[IncBB]->getIntMapping(Val);
        if (!N) {
          N = G->addNode(Tmp->clone(true));
        }
        else {
          cast<IntNode>(N)->mergeWith(*Tmp);
        }
      }
      else if (IntegerType *IT = dyn_cast<IntegerType>(Val->getType())) {
        if (!N) {
          N = G->addNode(IntNode::createUninitialized(IT));
        }
        else {
          IntNodePtr Tmp = IntNode::createUninitialized(IT);
          cast<IntNode>(N)->mergeWith(*Tmp.get());
        }
      }
      else if (const Instruction *Ins = dyn_cast<Instruction>(Val)) {
        // For some reason this exception handling isn't marked impossible yet
        const Instruction *Begin = &*(Ins->getParent()->begin());
        if (isa<LandingPadInst>(Begin)) {
            continue;
        }

        // Now we make a worst-case guess
        continue;
      }
      else {
        //assert(false && "Unexpected PHI Node");
      }
    }

    if (!N) {
      N = G->addNode(Node::createUninitializedFromType(I.getType()));
    }

    G->setMapping(&I, N);
  }

  void doConditionalBranch(GraphPtr &G, const BranchInst *BI) {
    assert(BI->getNumSuccessors() == 2);
    const BasicBlock *BB = BI->getParent();
    BasicBlock *TrueBB = BI->getSuccessor(0);
    BasicBlock *FalseBB = BI->getSuccessor(1);
    auto TrueEdge = BasicBlockEdge(BB, TrueBB);
    auto FalseEdge = BasicBlockEdge(BB, FalseBB);
    const Value *Condition = BI->getCondition();

    assert(NodeGraphs.count(BB) == 0
           && "End of conditional branch should have edges");

    GraphPtr FalseG = G->clone();
    GraphPtr TrueG = std::move(G);

    bool NeedUpdate = false;
    TrueG->setTrue(Condition);
    if (EdgeGraphs.count(TrueEdge) == 0) {
      NeedUpdate = true;
    }
    else {
      if (!Graph::equivalent(*TrueG, *EdgeGraphs[TrueEdge])) {
        NeedUpdate = true;
      }
    }
    EdgeGraphs[TrueEdge] = std::move(TrueG);
    if (NeedUpdate) {
      ensureWorkList(TrueBB);
    }

    NeedUpdate = false;
    FalseG->setFalse(Condition);
    if (EdgeGraphs.count(FalseEdge) == 0) {
      NeedUpdate = true;
    }
    else {
      if (!Graph::equivalent(*FalseG, *EdgeGraphs[FalseEdge])) {
        NeedUpdate = true;
      }
    }
    EdgeGraphs[FalseEdge] = std::move(FalseG);

    if (NeedUpdate) {
      ensureWorkList(FalseBB);
    }
  }

  void doTerminator(const Instruction &I) {
    const BasicBlock *BB = I.getParent();
    GraphPtr &G = MutableGraphs[BB];

    // Handle the branch
    if (const BranchInst *BI = dyn_cast<BranchInst>(&I)) {
      if (BI->isConditional()) {
        doConditionalBranch(G, BI);
        return;
      }
    }
    else if (const ReturnInst *RI = dyn_cast<ReturnInst>(&I)) {
      assert(RI->getNumSuccessors() == 0);
      if (Result.count(RI) > 0) {
        Result.erase(RI);
      }
      Result[RI] = std::move(G);
      // Don't need to modify the worklist here
      return;
    }

    bool NeedUpdate = false;
    if (NodeGraphs.count(BB) == 0) {
      NeedUpdate = true;
    }
    else {
      if (!Graph::equivalent(*G, *NodeGraphs[BB])) {
        NeedUpdate = true;
      }
    }
    NodeGraphs[BB] = std::move(G);
    if (NeedUpdate) {
      ensureWorkListAllSucc(BB);
    }
  }

  void doCallSite(CallSite CS) {
    const Instruction *I = CS.getInstruction();
    if (isa<DbgDeclareInst>(I)
        || isa<DbgValueInst>(I)
        || isa<DbgInfoIntrinsic>(I)) {
      return;
    }
    // TODO: This needs more handling
    if (isa<IntrinsicInst>(I)) { return; }

    const BasicBlock *BB = I->getParent();

    if (auto CI = dyn_cast<CallInst>(I)) {
      if (CI->isInlineAsm()) {
        if (!CI->getType()->isVoidTy()) {
          MutableGraphs[BB]->setMapping(I,
            MutableGraphs[BB]->addNode(Node::createUninitializedFromType(I->getType())));
        }
        return;
      }
    }

    const Function *F = CS.getCalledFunction();

    GraphPtr &G = MutableGraphs[BB];
    // TODO: Need to handle default args
    if (F == nullptr) {
      const Value *V = CS.getCalledValue();
      if (G->isMapped(V) && !(G->getSeqMapping(V)->isOnlyUninitialized())) {
        F = G->getFunctionFromValue(V);
      }
    }

    if (F == nullptr && IA.getClassQuery().isVTableInst(I)) {
      const StructType *T = IA.getCurrentType();
      F = IA.getClassQuery().getVTableEntry(I, T);
    }

    if (!F || F->empty()) {
        DenseSet<Node *> Reachable;
        G->populateReachableThis(Reachable);
        for (auto &Arg : CS.args()) {
          const Value *V = Arg.get();

          if (Node *ArgN = G->getMappingOrNull(V)) {
            DenseSet<Node *> ArgReachable;
            G->populateReachable(ArgReachable, ArgN);
            for (auto AN : ArgReachable) {
              if (Reachable.count(AN) > 0) {
                const DebugLoc &DL = I->getDebugLoc();
                errs() << "\033[1;31mInvalid call @ "
                       << DL.getLine() << ':' << DL.getCol()
                       << "\033[0m\n";
              }
            }
          }
        }
        auto Ty = I->getType();
        if (Ty->isVoidTy()) { return; }
        G->setMapping(I, G->addNode(Node::createUninitializedFromType(Ty)));
        return;
    }

    // TODO: THIS IS WRONG
    if (isRecursive(F)) {
      if (I->getType()->isVoidTy()) { return; }
      auto Ty = I->getType();
      G->setMapping(I, G->addNode(Node::createUninitializedFromType(Ty)));
      return;
    }
    assert(!isRecursive(F) && "TODO: Recursive");

    // Setup GraphPtr
    auto Iter = F->arg_begin();
    for (auto &Arg : CS.args()) {
      const Value *V = Arg.get();
      const Argument *A = &*Iter;
      ++Iter;

      G->setArg(A, V);
    }

    if (Iter != F->arg_end()) {
      errs() << "\033[33mWarning: function passes arg through varargs\033[0m\n";
    }
    FunctionAnalyzer FA(IA, this, F, G);

    if (!F->getReturnType()->isVoidTy()) {
      //assert(FA.getResult().size() == 1); // TODO: replaced for int merging
      bool First = true;
      for (auto &KV : FA.getResult()) {
        const ReturnInst *RI = KV.first;
        Value *V = RI->getReturnValue();
        // TODO: This needs to be merged later
        //if (!V) { break; } // void return
        if (First) {
          Node *N = G->addNode(KV.second->getMapping(V)->clone(true));
          G->setMapping(I, N);
        }
        else {
          cast<IntNode>(G->getMapping(I))->unionWith(*cast<IntNode>(KV.second->getMapping(V)));
        }
      }
    }
  }

  void analyze() {
    std::deque<BasicBlock::const_iterator> InstWorkList;

    BBWorkList.push_back(&Current->getEntryBlock());

    while (!(BBWorkList.empty() && InstWorkList.empty())) {
      if (InstWorkList.empty()) {
        const BasicBlock *BB = BBWorkList.front();
        BBWorkList.pop_front();
        MutableGraphs[BB] = std::move(merge(BB));
        if (MutableGraphs[BB]->isBottom()) {
          // Skip all analysis and just propagate the bottom
          // TODO: Ask if this is right
          InstWorkList.push_back(--(BB->end()));
        }
        else if (isa<LandingPadInst>(BB->begin())) {
          // Ignore any exception handling code
          MutableGraphs[BB]->setIsBottom();
          InstWorkList.push_back(--(BB->end()));
        }
        else {
          InstWorkList.push_back(BB->begin());
        }
        continue;
      }

      auto Iter = InstWorkList.back();
      InstWorkList.pop_back();

      const Instruction &I = *Iter;

      bool IsVTableInst = IA.getClassQuery().isVTableInst(&I);

      // Instructions are straight-line, so modify the graph
      if (isa<CallInst>(&I) || isa<InvokeInst>(&I)) {
        doCallSite(CallSite(const_cast<Instruction *>(&I)));
      }
      else if (isa<PHINode>(&I)) {
        assert(!IsVTableInst);
        doPHINode(*cast<PHINode>(&I));
      }
      else {
        MutableGraphs[I.getParent()]->visit(const_cast<Instruction &>(I));
      }

      if (I.isTerminator()) {
        assert(!IsVTableInst);
        doTerminator(I);
      }
      else {
        InstWorkList.push_back(++Iter);
      }
    }
  }
    */
};

}
}

#endif
