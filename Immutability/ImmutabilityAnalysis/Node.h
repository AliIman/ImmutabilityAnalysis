#ifndef LLVM_ANALYSIS_IMMUTABILITY_NODE_H
#define LLVM_ANALYSIS_IMMUTABILITY_NODE_H

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallSet.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/ConstantRange.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>

namespace llvm {

namespace immutability {

class Graph;

class Node;
class FloatingPointNode;
class FunctionNode;
class IntNode;
class PointerNode;
class StructNode;
typedef std::shared_ptr<Node> NodePtr;
typedef std::shared_ptr<FloatingPointNode> FloatingPointNodePtr;
typedef std::shared_ptr<FunctionNode> FunctionNodePtr;
typedef std::shared_ptr<IntNode> IntNodePtr;
typedef std::shared_ptr<PointerNode> PointerNodePtr;
typedef std::shared_ptr<StructNode> StructNodePtr;
typedef std::weak_ptr<Node> NodeWeakPtr;
typedef std::set<NodeWeakPtr, std::owner_less<NodeWeakPtr>> NodeWeakSetT;

//typedef DenseMap<const Node *, NodePtr> NodeToNodeT;
typedef DenseMap<const Node *, std::set<NodePtr>> NodeToNodeSetT;

typedef std::set<NodePtr> NodeSetT;

class Node {
public:
  enum NodeKind {
    NK_FLOATING_POINT,
    NK_FUNCTION,
    NK_INT, // REMOVE
    NK_POINTER,
    NK_STRUCT,
    NK_SEQUENTIAL,
    NK_INTEGER,
  };
  enum SeqNullKind {
    SEQNK_BOTTOM,
    SEQNK_NULL,
    SEQNK_NOT_NULL,
    SEQNK_MAYBE_NULL,
  };
  typedef NodeSetT EdgeT;
  typedef NodeWeakSetT EdgeWeakT;
private:
  const NodeKind Kind;
protected:
  const Type *Ty;
  bool IsThis;
  bool IsRead;
  EdgeWeakT CopyEdges;
  EdgeT ThisEdges;
  EdgeWeakT WeakEdges;

  Node(const Node &N)
      : Kind(N.Kind), Ty(N.Ty), IsThis(N.IsThis), IsRead(N.IsRead),
        CopyEdges(N.CopyEdges), ThisEdges(N.ThisEdges),
        WeakEdges(N.WeakEdges) {
  }

public:
  Node(NodeKind K, const Type *T)
      : Kind(K), Ty(T), IsThis(false), IsRead(false) {
  }

  /*
  static NodeKind getKind(const Type *T) {
    if (T->isIntegerTy()) {
      return NK_INTEGER;
    }
    else if (T->isPointerTy()) {
      return NK_POINTER;
    }
    else if (T->isStructTy() || T->isArrayTy() || T->isVectorTy()) {
      return NK_COMPOSITE;
    }
    else if (T->isFunctionTy()) {
      return NK_FUNCTION;
    }
    else if (T->isHalfTy() || T->isFloatTy() || T->isDoubleTy()) {
      return NK_FLOATING_POINT;
    }
    else {
      llvm_unreachable("LLVM type does not correspond to a node");
    }
  }
  */

  NodeKind getKind() const {
    return Kind;
  }
  bool isFloatingPoint() const {
    return Kind == NK_FLOATING_POINT;
  }
  bool isFunction() const {
    return Kind == NK_FUNCTION;
  }
  bool isInt() const {
    return Kind == NK_INT;
  }
  bool isPointer() const {
    return Kind == NK_POINTER;
  }
  bool isComposite() const {
    return Kind == NK_SEQUENTIAL || Kind == NK_STRUCT;
  }
  bool isSequential() const {
    return Kind == NK_SEQUENTIAL;
  }
  bool isStruct() const {
    return Kind == NK_STRUCT;
  }

  const Type *getType() const {
    return Ty;
  }
  void setType(const Type *T) {
    Ty = T;
  }

  void clearThis() {
    IsThis = false;
    IsRead = false;
  }
  bool isThis() const {
    return IsThis;
  }
  bool isRead() const {
    return IsRead;
  }

  const CompositeType *getCompositeType() {
    return cast<CompositeType>(Ty);
  }
  const SequentialType *getSequentialType() {
    return cast<SequentialType>(Ty);
  }
  const StructType *getStructType() {
    return cast<StructType>(Ty);
  }
  const PointerType *getPointerType() {
    return cast<PointerType>(Ty);
  }

  unsigned getCompositeNumElements() {
    if (isStruct()) {
      return getStructNumElements();
    }
    else if (isSequential()) {
      return getSequentialNumElements();
    }
    else {
      llvm_unreachable("Unknown composite type");
    }
  }
  unsigned getSequentialNumElements() {
    return getSequentialType()->getNumElements();
  }
  unsigned getStructNumElements() {
    return getStructType()->getNumElements();
  }

  Type *getCompositeElementType(unsigned I) {
    if (isStruct()) {
      return getStructElementType(I);
    }
    else if (isSequential()) {
      return getSequentialElementType();
    }
    else {
      llvm_unreachable("Unknown composite type");
    }
  }
  Type *getSequentialElementType() {
    return getSequentialType()->getElementType();
  }
  Type *getStructElementType(unsigned I) {
    return getStructType()->getElementType(I);
  }
  Type *getPointerElementType() {
    return getPointerType()->getElementType();
  }
  bool hasCompositeElement(unsigned I) const {
    if (isStruct()) {
      return hasStructElement(I);
    }
    else if (isSequential()) {
      return hasSequentialElement(I);
    }
    else {
      llvm_unreachable("Unknown composite type");
    }
  }
  std::shared_ptr<Node> getCompositeElement(unsigned I) const {
    if (isStruct()) {
      return getStructElement(I);
    }
    else if (isSequential()) {
      return getSequentialElement(I);
    }
    else {
      llvm_unreachable("Unknown composite type");
    }
  }
  void setCompositeElement(unsigned I, const std::shared_ptr<Node> &N) {
    if (isStruct()) {
      return setStructElement(I, N);
    }
    else if (isSequential()) {
      return setSequentialElement(I, N);
    }
    else {
      llvm_unreachable("Unknown composite type");
    }
  }
  //virtual void dump() const = 0;
  //virtual void dumpAll(unsigned Indent) const = 0;

  static NodePtr createTopFromType(const Type *T);
  static NodePtr createThisFromType(const Type *T);

  bool isUnique() const { return WeakEdges.size() <= 1; }
  bool isShared() const { return WeakEdges.size() > 1; }

  bool hasCopyEdges() const {
    return CopyEdges.size() > 0;
  }
  bool isCopyEdge(NodePtr &N) const {
    return CopyEdges.count(N) > 0;
  }
  bool isThisEdge(NodePtr &N) const {
    return ThisEdges.count(N) > 0;
  }
  bool isWeakEdge(NodePtr &N) const {
    return WeakEdges.count(N) > 0;
  }
  unsigned getNumWeakEdges() const { return WeakEdges.size(); }

  const EdgeWeakT& getCopyEdges() const { return CopyEdges; }
  const EdgeT& getThisEdges() const { return ThisEdges; }
  const EdgeWeakT& getWeakEdges() const { return WeakEdges; }

  void clearThisEdges() {
    ThisEdges.clear();
  }
  void clearWeakEdges() {
    WeakEdges.clear();
  }

  void addCopyEdge(NodePtr N) { CopyEdges.insert(N); }
  void addThisEdge(NodePtr N) { ThisEdges.insert(N); }
  void addWeakEdge(NodePtr N) { WeakEdges.insert(N); }

  void removeCopyEdge(NodePtr N) { CopyEdges.erase(N); }
  void removeWeakEdge(NodePtr N) { WeakEdges.erase(N); }

  void setIsThis() {
    IsThis = true;
  }
  void setIsRead() {
    if (IsThis) {
      IsRead = true;
    }
  }

  const ConstantRange &getIntConstantRange() const;
  void setIntConstantRange(ConstantRange CR);

  bool hasPointerPointee() const;
  bool isSeqMaybeNull() const;
  bool isSeqNull() const;
  bool isSeqOnlyNull() const;
  NodePtr getPointerPointee() const;
  std::shared_ptr<Node> getPointerElement() const;
  void setPointerElement(const std::shared_ptr<Node> &N);
  SeqNullKind getSeqNullKind() const;
  void setSeqNullKind(SeqNullKind K);
  void setSeqPointee(NodePtr N);
  void markSeqOnlyNull();

  unsigned getStructNumFields() const;
  bool isStructFieldUninitialized(unsigned Index) const; // TODO: remove
  /*
  bool hasStructField(unsigned Index) const;
  NodePtr getStructField(unsigned Index) const;
  */
  bool hasStructElement(unsigned Index) const;
  NodePtr getStructElement(unsigned Index) const;
  void setStructField(unsigned Index, NodePtr N);
  void setStructElement(unsigned Index, const std::shared_ptr<Node> &N);
  bool hasStructSubStruct() const;
  NodePtr getStructSubStruct() const;
  void setStructSubStruct(NodePtr N);

  bool hasSequentialElement(unsigned Index) const;
  std::shared_ptr<Node> getSequentialElement(unsigned Index) const;
  void setSequentialElement(unsigned I, const std::shared_ptr<Node> &N);


  void zero();
  void top();

  void unionWith(const Node &N) {
    if (IsThis && N.IsThis) { IsThis = true; }
    else                    { IsThis = false; }
    if (IsRead && N.IsRead) { IsRead = true; }
    else                    { IsRead = false; }
  }

  static void addCopyEdge(NodePtr N, NodePtr M);
  static void addWeakEdge(NodePtr N, NodePtr M);
  static void removeCopyEdge(NodePtr N, NodePtr M);
  static void removeWeakEdge(NodePtr N, NodePtr M);
  static void removeAllCopyEdges(NodePtr N);

  static bool isCopyEdge(NodePtr N, NodePtr M) {
    return N->isCopyEdge(M) && M->isCopyEdge(N);
  }
  static bool isCopyEdgeSet(NodeSetT &S) {
    for (NodePtr N : S) {
      bool OnlyOneSame = false;
      for (NodePtr M : S) {
        if (N == M) {
          if (!OnlyOneSame) {
            OnlyOneSame = true;
          }
          else {
            return false;
          }
        }
        else {
          if (!N->isCopyEdge(M)) {
            return false;
          }
        }
      }
    }
    return true;
  }
  static bool isWeakEdge(NodePtr N, NodePtr M) {
    return N->isWeakEdge(M) && M->isWeakEdge(N);
  }
  static bool isWeakEdgeSet(NodeSetT &S) {
    for (NodePtr N : S) {
      for (NodePtr M : S) {
        if (N != M) {
          if (!N->isWeakEdge(M)) {
            return false;
          }
        }
      }
    }
    return true;
  }

  NodePtr copy() const;

  static void getReachableValues(NodeSetT &S, NodePtr N);

  static void getReachableInclWeak(NodeSetT &S, NodePtr N);
  static void getReachable(NodeSetT &S, NodePtr N);
  static NodeSetT getReachable(NodePtr N);

  NodeSetT getCopyNodes() const {
    NodeSetT S;
    for (auto NW : getCopyEdges()) {
      if (auto N = NW.lock()) {
        S.insert(N);
      }
    }
    return S;
  }

  NodeSetT getWeakNodes() const {
    NodeSetT S;
    for (auto NW : getWeakEdges()) {
      if (auto N = NW.lock()) {
        S.insert(N);
      }
    }
    return S;
  }

  static NodePtr unionAll(Graph &G, NodeSetT &S);

  void dump(unsigned Indent=0) const;

  void clearCopyThisWeakEdges() {
    CopyEdges.clear();
    ThisEdges.clear();
    WeakEdges.clear();
  }
  void clearEdges();
private:

  void dumpSingleNode() const;
};

class FloatingPointNode : public Node {
public:
  FloatingPointNode(const Type *T) : Node(NK_FLOATING_POINT, T) {}
  FloatingPointNode(const FloatingPointNode &N) : Node(N) {}

  static bool classof(const Node *N) { return N->getKind() == NK_FLOATING_POINT; }

  //void dumpAll(unsigned Indent) const override { dump(); errs() << '\n'; }

  static FloatingPointNodePtr createUninitialized(const Type *T) {
    return std::make_shared<FloatingPointNode>(T);
  }
};

class FunctionNode : public Node {
  bool Uninitialized;
  SmallSet<const Function *, 16> Functions;
public:
  //FunctionNode() : Node(NK_FUNCTION), Uninitialized(true) {}
  explicit FunctionNode(const Function *F) : Node(NK_FUNCTION, F->getType()), Uninitialized(true) {
    Functions.insert(F);
  }
  explicit FunctionNode(const Type *T) : Node(NK_FUNCTION, T), Uninitialized(true) {}
  FunctionNode(const FunctionNode &N)
  : Node(N), Uninitialized(N.Uninitialized), Functions(N.Functions) {}

  static bool classof(const Node *N) { return N->getKind() == NK_FUNCTION; }

  const Function *getSingleFunction() {
    assert(!Uninitialized && "Cannot get single function from top");
    assert(Functions.size() == 1 && "Cannot get a single function when there's not one function");
    return *Functions.begin();
  }

  static FunctionNodePtr createUninitialized(const Type *T) {
    auto N = std::make_shared<FunctionNode>(T);
    N->Uninitialized = true;
    return std::move(N);
  }

  static FunctionNodePtr createInitialized(const Function *F) {
    return std::make_shared<FunctionNode>(F);
  }
};

class IntNode : public Node {
private:
  ConstantRange Range;
public:
  IntNode(const IntNode &N) : Node(N), Range(N.Range) {}
  explicit IntNode(const IntegerType *T)
      : Node(NK_INT, T), Range(T->getBitWidth(), true) {}

  const ConstantRange& getConstantRange() const { return Range; }
  void setConstantRange(ConstantRange CR) { Range = CR; }

  static bool classof(const Node *N) { return N->getKind() == NK_INT; }

  static IntNodePtr createUninitialized(const IntegerType *T) {
    return std::make_shared<IntNode>(T);
  }

  static IntNodePtr createRange(const IntegerType *T, ConstantRange CR) {
    assert(T->getBitWidth() == CR.getBitWidth()
           && "Integer bitwidths need to match");
    auto N = std::make_shared<IntNode>(T);
    N->Range = CR;
    return std::move(N);
  }

  static IntNodePtr createFromConstant(const ConstantInt *CI) {
    auto N = std::make_shared<IntNode>(CI->getType());
    N->Range = ConstantRange(CI->getValue());
    return std::move(N);
  }

  IntNodePtr cloneNoEdges() {
    return std::make_shared<IntNode>(*this);
  }

  bool isUninitialized() const {
    return Range.isFullSet();
  }

  bool isFalse() const {
    if (Range.isSingleElement()) {
      if (Range.getLower().getBoolValue() == false) {
        return true;
      }
    }
    return false;
  }
  bool isTrue() const {
    if (Range.isSingleElement()) {
      if (Range.getLower().getBoolValue() == true) {
        return true;
      }
    }
    return false;
  }
  void mergeWith(const IntNode &N) { // TODO REMOVE
    if (N.IsRead) {
      IsRead = true;
    }
    Range = Range.unionWith(N.Range);
  }
  void strongUpdate(const IntNode &N) {
    // TODO: Should this copy the edges, what about read?
    IsRead = N.IsRead;
    Range = N.Range;
  }
  void unionWith(const IntNode &N) { // TODO REMOVE
    if (N.IsRead) {
      IsRead = true;
    }
    Range = Range.unionWith(N.Range);
  }
  void intersect(ConstantRange CR) {
    Range = Range.intersectWith(CR);
  }

  void refine(ConstantRange CR) {
    if (Range.contains(CR)) {
      Range = CR;
    }
    else {
      Range = ConstantRange(Range.getBitWidth(), false);
    }
  }

  unsigned getBitWidth() const {
    return Range.getBitWidth();
  }
};

class PointerNode : public Node {
private:
  /*
  bool IsNull;
  bool IsOnlyNull;
  */
  mutable NodePtr Pointee;

  mutable NodePtr PointeeZero;
  mutable NodePtr PointeeNonZero;
  mutable NodePtr PointeeAny;
  SeqNullKind NullKind;
public:
  PointerNode(const Type *T)
      : Node(NK_POINTER, T), NullKind(SEQNK_MAYBE_NULL) {
      assert(isa<SequentialType>(T) || isa<PointerType>(T));
  }
  PointerNode(const PointerNode &N)
      : Node(N), NullKind(N.NullKind), Pointee(N.Pointee) {
  }

  SeqNullKind getNullKind() const {
    return NullKind;
  }
  void setNullKind(SeqNullKind K) {
    NullKind = K;
  }

  static SeqNullKind join(SeqNullKind X, SeqNullKind Y) {
    if (X == Y) {
      return X;
    }
    else if (X == SEQNK_MAYBE_NULL || Y == SEQNK_MAYBE_NULL) {
      return SEQNK_MAYBE_NULL;
    }
    else if (X == SEQNK_BOTTOM || Y == SEQNK_BOTTOM) {
      if (X != SEQNK_BOTTOM) {
        return X;
      }
      else {
        return Y;
      }
    }
    else {
      return SEQNK_MAYBE_NULL;
    }
  }

  static bool classof(const Node *N) { return N->getKind() == NK_POINTER; }

  bool isMaybeNull() const {
    return NullKind == SEQNK_MAYBE_NULL;
  }
  //const SetTy& getPointsToSet() const { return PointsToSet; }
  //bool hasPointsToElements() const { return PointsToSet.size() > 0; }
  //bool hasSinglePointsToElement() const { return PointsToSet.size() == 1; }
  //Node *getSinglePointsToElement() const {
  //  assert(hasSinglePointsToElement());
  //  return *PointsToSet.begin();
  //}
  //void addPointsToElement(Node *N) {
  //  clearUninitialized();
  //  PointsToSet.insert(N);
  //}
  //void addPointsToElementUnsafe(Node *N) const {
  //  const_cast<PointerNode *>(this)->addPointsToElement(N);
  //}
  //void removePointsToElement(Node *N) {
  //  PointsToSet.erase(N);
  //}
  //unsigned getNumPointsToElements() const { return PointsToSet.size(); }
  //void clearPointsToSet() { PointsToSet.clear(); }
  //void clearUninitialized() { Flags &= ~(UNINITIALIZED_FLAG); }
  //void clearAll() { Flags = 0; clearPointsToSet(); }

  void clearPointee() { Pointee = nullptr; }
  bool hasPointee() const {
    return Pointee != nullptr;
  }
  bool isNull() const {
    return NullKind == SEQNK_NULL || NullKind == SEQNK_MAYBE_NULL;
  }
  bool isOnlyNull() const {
    return NullKind == SEQNK_NULL;
  }
  /*
  bool isOnlyUninitialized() const {
    if (isNull()) {
      return false;
    }
    return isUninitialized();
  }
  */
  NodePtr getPointee() const {
    assert(hasPointee());
    /* if (isThis()) { assert(Pointee->isThis()); } */
    /* // assert(!IsNull); */
    /* if (isNull()) { */
    /*   // BIG TODO: Re-enable this warning */
    /*   // errs() << "WARNING: possible null dereference\n"; */
    /* } */
    /* if (isTop()) { */
    /*     //errs() << "TODO getPointee\n"; */
    /* } */
    /* if (!hasPointee()) { */
    /*   auto ElementTy = Ty->getSequentialElementType(); */
    /*   Pointee = Node::createTopFromType(ElementTy); */
    /*   if (IsThis) { */
    /*     Pointee->markIsThis(); */
    /*   } */
    /*   for (auto CN : getCopyEdges()) { */
    /*     if (auto N = CN.lock()) { */
    /*       N->setSeqPointee(Pointee); */
    /*     } */
    /*   } */
    /* } */
    /* for (auto WeakN : getWeakNodes()) { */
    /*     if (WeakN->hasSeqPointee()) { */
    /*       auto WeakPointee = cast<PointerNode>(WeakN.get())->Pointee; */
            /* if (!Node::isWeakEdge(Pointee, WeakPointee)) { */
            /*     dump(); */
            /*     errs() << "and\n"; */
            /*     WeakN->dump(); */
            /* } */
          /* if (!Node::isWeakEdge(Pointee, WeakPointee)) { */
          /*     errs() << this << " -> " << Pointee.get() << '\n'; */
          /*     errs() << "without weak\n"; */
          /*     errs() << WeakN.get() << " - > " << WeakPointee.get() << '\n'; */
          /* } */
          //assert(Node::isWeakEdge(Pointee, WeakPointee));
    /*     } */
    /* } */
    return Pointee;
  }
  std::shared_ptr<Node> getElement() const {
    return Pointee;
  }
  void setElement(const std::shared_ptr<Node> &N) {
    Pointee = N;
  }
  void setPointee(NodePtr N) {
    Pointee = N;
  }
  /*
  void setIsNull(bool B) {
    IsNull = B;
  }
  */
  void markOnlyNull() {
    NullKind = SEQNK_NULL;
    Pointee = nullptr;
  }

  void strongUpdate(const PointerNode &N) {
    llvm_unreachable("TODO");
    //Flags = N.Flags;
    //PointsToSet = N.PointsToSet;
  }

  static PointerNodePtr createNull(const Type *T) {
    assert(isa<SequentialType>(T) || isa<PointerType>(T));
    PointerNodePtr P = std::make_shared<PointerNode>(T);
    P->markOnlyNull();
    return std::move(P);
  }

  static PointerNodePtr createUninitialized(const Type *T) {
    assert(isa<SequentialType>(T) || isa<PointerType>(T));
    PointerNodePtr P = std::make_shared<PointerNode>(T);
    return std::move(P);
  }

  static PointerNodePtr createPointee(NodePtr N) {
    PointerNodePtr P = std::make_shared<PointerNode>(N->getType()->getPointerTo());
    P->Pointee = N;
    P->NullKind = SEQNK_NOT_NULL;
    return std::move(P);
  }
};

class SequentialNode : public Node {
  std::vector<std::shared_ptr<Node>> Elements;
public:
  explicit SequentialNode(const SequentialType *T)
      : Node(NK_SEQUENTIAL, T), Elements(T->getNumElements()) {
  }
 SequentialNode(const SequentialNode &N)
   : Node(N), Elements(N.Elements) {
  }
  static bool classof(const Node *N) { return N->getKind() == NK_SEQUENTIAL; }

  bool hasElement(unsigned Index) const {
    return Elements[Index] != nullptr;
  }
  std::shared_ptr<Node> getElement(unsigned Index) const {
    return Elements[Index];
  }
  void setElement(unsigned Index, const std::shared_ptr<Node> &Element) {
    Elements[Index] = Element;
  }

  static std::shared_ptr<SequentialNode> createUninitialized(const SequentialType *T) {
    return std::make_shared<SequentialNode>(T);
  }
};

class StructNode : public Node {
  mutable std::vector<NodePtr> Fields;
  NodePtr SubStruct;
public:
  StructNode(const StructType *T)
      : Node(NK_STRUCT, T), Fields(T->getNumElements()) {
  }

  StructNode(const StructNode &N)
      : Node(N), Fields(N.Fields), SubStruct(N.SubStruct) {
  }

  static bool classof(const Node *N) { return N->getKind() == NK_STRUCT; }

  unsigned getNumFields() const { return Fields.size(); }
  bool isFieldUninitialized(unsigned Index) const { // TODO: remove
    assert(Index < getNumFields());
    return Fields[Index] == nullptr;
  }
  bool hasElement(unsigned Index) const {
      if (Index >= getNumFields()) {
          return false; // THIS IS BECAUSE OF BASE TYPES
      }
    assert(Index < getNumFields());
    return Fields[Index] != nullptr;
  }
  NodePtr getField(unsigned Index) const {
    errs() << "TODO: Remove getField\n";
    /*
    if (isFieldUninitialized(Index)) {
      for (auto CN : getCopyNodes()) {
        if (CN->hasStructField(Index)) {
          auto CopyField = CN->getStructField(Index);
          Fields[Index] = CopyField;
          if (IsThis) {
            Fields[Index]->setIsThis();
          }
          return Fields[Index];
        }
      }

      auto ElementTy = Ty->getStructElementType(Index);
      Fields[Index] = Node::createTopFromType(ElementTy);
      if (IsThis) {
        Fields[Index]->setIsThis();
      }
      for (auto CN : getCopyNodes()) {
        CN->setStructField(Index, Fields[Index]);
      }
      // Carry any weak edges
      for (auto WN : getWeakNodes()) {
        if (WN->hasStructField(Index)) {
          Node::addWeakEdge(WN->getStructField(Index), Fields[Index]);
        }
      }
    }
    */
    return Fields[Index];
  }
  NodePtr getElement(unsigned Index) const {
    return Fields[Index];
  }
  void setElement(unsigned Index, const std::shared_ptr<Node> &N) {
    Fields[Index] = N;
  }
  void setField(unsigned Index, NodePtr N) {
    Fields[Index] = N;
  }

  void clearFields() {
    for (unsigned Index = 0; Index < getNumFields(); ++Index) {
      Fields[Index] = nullptr;
    }
  }

  bool hasSubStruct() const {
    return SubStruct != nullptr;
  }
  NodePtr getSubStruct() const {
    assert(hasSubStruct());
    return SubStruct;
  }
  void setSubStruct(NodePtr N) {
    SubStruct = N;
  }

  static StructNodePtr createUninitialized(const StructType *T) {
    return std::make_shared<StructNode>(T);
  }
};

__attribute__((always_inline))
inline
bool hasSingleNode(NodeToNodeSetT &Map, const Node *Key) {
  assert(Map.count(Key) > 0 && "Node key not mapped");
  return Map[Key].size() == 1;
}

__attribute__((always_inline))
inline
NodePtr getSingleNode(NodeToNodeSetT &Map, const Node *Key) {
  assert(Map.count(Key) > 0 && "Node key not mapped");
  assert(Map[Key].size() == 1 && "Node does not map to single node");
  return *(Map[Key].begin());
}

}
}

#endif
