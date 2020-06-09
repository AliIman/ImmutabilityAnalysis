#ifndef LLVM_ANALYSIS_IMMUTABILITY_GRAPH_H
#define LLVM_ANALYSIS_IMMUTABILITY_GRAPH_H

#include "Query.h"
#include "Node.h"
#include "NodeAliasTracker.h"
#include "MemoryAliases.h"

#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/InstVisitor.h"

namespace llvm {
namespace immutability {

class Graph;
typedef std::unique_ptr<Graph> GraphPtr;

class Graph : public InstVisitor<Graph> {
private:
  const Function *FirstMethod = nullptr;

  Query *Q;

  bool IsBottom;

  DenseMap<const Value *, NodePtr> Mapping;
  DenseMap<Node *, const Value *> ReverseMapping;

  MemoryAliases MemAliases;

  /*
  TypeNodesMap KnownPointees;
  TypeNodesMap UnknownPointees;

  TypeElementNodesMap KnownCompositeElementPointees;
  TypeElementNodesMap UnknownCompositeElementPointees;
  */

  NodePtr Return;

  NodePtr createKnownPointee(const Type *T);

public:
  Graph() : Q(nullptr), IsBottom(false) {
  }
  Graph(Query *Q) : Q(Q), IsBottom(false) {
  }

  /* Note: this makes the code super fragile, you have to set it properly from
   *       *both* places in FunctionAnalysis.cpp
   */
  void setFirstMethod(const Function *FM) {
    assert(FM->hasName());
    FirstMethod = FM;
  }

  static GraphPtr createEmptyExceptThis(Query *Q,
                                        const Argument *A,
                                        const StructType *T) {
    GraphPtr G = make_unique<Graph>(Q);
    auto StructArg = cast<StructType>(A->getType()->getPointerElementType());
    NodePtr N = Node::createThisFromType(T);
    if (T != StructArg) {
      auto Indices = Q->C.getSupertypeIndices(T, StructArg);
      NodePtr SubN = N;
      for (auto I : Indices) {
        N = G->getStructElementOp(N.get(), I);
        assert(N->isThis());
      }
      N->setStructSubStruct(SubN);
    }
    assert(N->isThis());
    NodePtr This = PointerNode::createPointee(N);
    This->setIsThis();
    G->addMapping(A, This);
    return std::move(G);
  }

  // Both of these should only be for testing
  std::vector<Node *> getWeakEdges(const Node *N) const;
  std::vector<std::shared_ptr<Node>> getWeakEdgesShared(const Node *N) const;
  bool isWeakEdge(NodePtr N1, NodePtr N2) const;

  void weakUpdate(NodePtr N, NodeSetT &S);

  /*
  NodeAliasTracker *getNewNodeAliasTracker() {
    auto Tracker = make_unique<NodeAliasTracker>();
    auto Ret = Tracker.get();
    Trackers.push_back(std::move(Tracker));
    return Ret;
  }
  */

  static GraphPtr createEmpty(Query *Q) {
    return make_unique<Graph>(Q);
  }
  static GraphPtr createBottom(Query *Q) {
    GraphPtr G = make_unique<Graph>(Q);
    G->markIsBottom();
    return std::move(G);
  }

  void removeAllExcept(const Argument *A) {
    NodePtr N = getMapping(A);
    // TODO TOP WEAK
    Mapping.clear();
    ReverseMapping.clear();
    Return.reset();
    addMapping(A, N);
  }
  void fixupThis(const Argument *A) {
    std::shared_ptr<Node> N = getMapping(A);
    NodeSetT S;
    Node::getReachable(S, N);
    for (const NodePtr &M : S) {
      M->setIsThis();
      for (const NodePtr &O : getWeakEdgesShared(M.get())) {
        Node::addWeakEdge(M, O);
      }
    }
    MemAliases.clear();
  }
  void changeThis(const Argument *NewThis) {
    assert(Mapping.size() == 1);
    auto &Entry = *Mapping.begin();
    const Value *OldThis = Entry.first;
    NodePtr N = Entry.second;
    assert(N->isPointer());
    auto StructArg = cast<StructType>(NewThis->getType()->getPointerElementType());
    if (NewThis->getType() != N->getType()) {
      N = getPointerElementOp(N.get());
      NodePtr SubN;
      if (!N->hasStructSubStruct()) {
        SubN = N;
      }
      else {
        N = N->getStructSubStruct();
        SubN = N;
      }
      if (N->getType() != StructArg) {
        auto Indices = Q->C.getSupertypeIndices(cast<StructType>(N->getType()), StructArg);
        for (auto I : Indices) {
          N = getStructElementOp(N.get(), I);
        }
        N->setStructSubStruct(SubN);
      }
      N = PointerNode::createPointee(N);
      N->setIsThis();
    }
    if (NewThis->getType() != N->getType()) {
      // TODO this can happen if it's a base type
      // N->getType() IS A BASE TYPE
      //NewThis->getType()->dump();
      //N->setType(NewThis->getType());
    }
    //assert(NewThis->getType() == N->getType());
    Mapping.clear();
    ReverseMapping.clear();
    addMapping(NewThis, N);
  }

  bool hasReturn() const {
    return Return != nullptr;
  }
  NodePtr getReturn() {
    assert(hasReturn());
    return Return;
  }
  void addReturn(const Value *V) {
    assert(!Return);
    Return = getMapping(V);
  }
  void removeReturn() {
    Return = nullptr;
  }

  void eraseRelevant(const Function *F);

  static void canonicalization(Graph &GraphA, Graph &GraphB,
                               NodeSetT &VisitedA, NodeSetT &VisitedB,
                               NodePtr NodeA, NodePtr NodeB);
  static void canonicalize(Graph &A, Graph &B);
  static void checkCanonicalized(const Graph &GraphA, const Graph &GraphB,
                                 NodeSetT &VisitedA, NodeSetT &VisitedB,
                                 NodePtr NodeA, NodePtr NodeB);
  static void checkCanonicalized(const Graph &A, const Graph &B);

  void propagateThisEdges(const NodePtr &Src, const NodePtr &Dst);
  void addCopyEdges(const NodePtr &Src, const NodePtr &Dst);

  NodeSetT getReachableDirect() {
    NodeSetT Reachable;
    for (auto &Entry : Mapping) {
      NodePtr N = Entry.second;
      Node::getReachable(Reachable, N);
    }
    return Reachable;
  }

  // Equivalent.cpp
  bool equivalent(NodePtr ThisN, NodePtr OtherN, NodeSetT &Checked,
                  NodeToNodeSetT &ThisToOther, Graph &Other) const;
  static bool equivalentAllEdges(NodeToNodeSetT &ThisToOther, const Graph &This, const Graph &Other);
  bool equivalent(const Graph &Other, const Function *F) const;

  // MoreSpecific.cpp
  bool moreSpecific(NodePtr ThisN, NodePtr OtherN, NodeSetT &Checked,
                  NodeToNodeSetT &ThisToOther, Graph &Other) const;
  static bool moreSpecificAllEdges(NodeToNodeSetT &ThisToOther, const Graph &This, const Graph &Other);
  bool moreSpecific(const Graph &Other, const Function *F) const;

  // Clone.cpp
  GraphPtr clone() const;
  NodePtr clone(const NodePtr &N, NodeToNodeSetT &ThisToClone) const;
  static void cloneAllEdges(NodeToNodeSetT &ThisToClone);

  // Merge.cpp
  struct ResultOrigin {
    NodePtr NodeA;
    NodePtr NodeB;
  };
  typedef DenseMap<Node *, ResultOrigin> ResultOriginMapT;

  static GraphPtr merge(const Graph &A, const Graph &B);

  static NodePtr mergeUnique(const NodePtr &N,
                             bool isA,
                             NodeToNodeSetT &SrcToClone,
                             NodeToNodeSetT &UniqueToResult,
                             ResultOriginMapT &ResultOriginMap,
                             const Graph &G,
                             Graph &Result);
  static NodePtr merge(const NodePtr &ANode, const NodePtr &BNode,
                       NodeToNodeSetT &AToResult, NodeToNodeSetT &BToResult,
                       ResultOriginMapT &ResultOriginMap,
                       const Graph &A, const Graph &B,
                       Graph &Result);
  static void mergeAllEdges(NodeToNodeSetT &AToResult,
                            NodeToNodeSetT &BToResult); // TODO: DELETE DELETE DELETE
  static void mergeAllEdges(Graph &Result,
                            NodeToNodeSetT &AToResult,
                            NodeToNodeSetT &BToResult,
                            ResultOriginMapT &ResultOriginMap);
  static void mergeRemoveCopyEdges(NodeToNodeSetT &SrcToResult);
  static void mergeRemoveWrongSeqStructCopyEdges(NodeToNodeSetT &SrcToResult);
  static void mergeAddWeakEdges(NodeToNodeSetT &SrcToResult);

  // TODO: For Debug only
  bool hasMapping(const Value *V) const {
    if (isa<ConstantInt>(V)) {
      return true;
    }
    else if (isa<ConstantPointerNull>(V)) {
      return true;
    }
    else if (isa<ConstantExpr>(V)) {
      return true;
    }
    else if (isa<GlobalVariable>(V)) {
      return true;
    }
    return Mapping.count(V) > 0;
  }
  void addMapping(const Value *V, NodePtr N) {
    Mapping[V] = N;
    ReverseMapping[N.get()] = V;
  }

  NodePtr getGEPResult(const GetElementPtrInst &I);
  void addConstantExpr(const ConstantExpr *CE);

  NodePtr getMapping(const Value *V);

  bool isBottom() const {
    return IsBottom;
  }
  void markIsBottom() {
    IsBottom = true;
  }

  void update(NodePtr &DstN, NodePtr &SrcN);

  void refineInt(const Value *V, ConstantRange CR);
  void refineBool(const Value *V, bool B);

  void handleDefaultDeleteCall(const Instruction *I);
  void handleUnknownCall(const Instruction *I);
  NodePtr createUniquePointees(const Type *T);

  void getAllPointees(NodeSetT &S, NodePtr N);

  void dump() const;
  void dot(StringRef Filename) const;
  void dot(std::string &ClassName, unsigned IterationNum, StringRef MethodName) const;

  std::shared_ptr<Node> freshOp(const PointerType *T);
  void updateOp(const std::shared_ptr<Node> &To,
                const std::shared_ptr<Node> &From,
                const Instruction *I = nullptr);

  std::shared_ptr<Node> getPointerElementOp(Node *N);
  std::shared_ptr<Node> getStructElementOp(Node *N, unsigned Index);


  // Transfer functions
  //// Binary operations
  void visitBinaryOperator(BinaryOperator &I);
  //// Vector operations
  void visitExtractElementInst(ExtractElementInst &I);
  //// Aggregate operations
  void visitExtractValueInst(ExtractValueInst &I);
  //// Memory access and addressing operations
  void visitAllocaInst(AllocaInst &I);
  void visitLoadInst(LoadInst &I);
  void visitStoreInst(StoreInst &I);
  void visitGetElementPtrInst(GetElementPtrInst &I);
  //// Conversion operations
  void visitCastInst(CastInst &I);
  void visitTruncInst(TruncInst &I);
  void visitZExtInst(ZExtInst &I);
  void visitSExtInst(SExtInst &I);
  void visitBitCastInst(BitCastInst &I);
  //// Other operations
  void visitICmpInstSequential(ICmpInst &I);
  void visitICmpInst(ICmpInst &I);
  void visitFCmpInst(FCmpInst &I);
  void visitSelectInst(SelectInst &I);
  void visitLandingPadInst(LandingPadInst &I);

  void visitMemCpyInst(MemCpyInst &I);
};

}

/*
template <> struct GraphTraits<immutability::Graph *> {
  using NodeRef = immutability::Node *;
  using ChildIteratorType = immutability::Node *;
  using nodes_iterator = pointer_iterator<immutability::Node *>;
  static nodes_iterator nodes_begin(immutability::Graph *G) { return nullptr; };
  static nodes_iterator nodes_end(immutability::Graph *G) { return nullptr; };
  static ChildIteratorType child_begin(NodeRef N) { return N; };
  static ChildIteratorType child_end(NodeRef N) { return N; };
};
*/

}

#endif
