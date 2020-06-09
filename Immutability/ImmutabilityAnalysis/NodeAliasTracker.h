/* #ifndef LLVM_ANALYSIS_IMMUTABILITY_NODE_ALIAS_TRACKER_H */
/* #define LLVM_ANALYSIS_IMMUTABILITY_NODE_ALIAS_TRACKER_H */

/* #include "Node.h" */

/* namespace llvm { */
/* namespace immutability { */

/* class Node; */

/* class NodeAliasTracker { */
/*   typedef DenseMap<const Type *, NodeWeakSetT> TypeNodesMap; */

/*   TypeNodesMap TypeToNodes; */
/*   NodeWeakSetT AliasSet; */
/*   NodeWeakSetT WeakSet; */
/* public: */
/*   NodeAliasTracker(); */
/*   bool hasNode(NodePtr N) const { */
/*     return AliasSet.count(N); */
/*   } */
/*   bool hasWeakNode(NodePtr N) const { */
/*     return WeakSet.count(N); */
/*   } */
/*   void addNode(NodePtr N); */
/*   void addWeakNode(NodePtr N) { */
/*     WeakSet.insert(N); */
/*   } */
/*   NodeSetT getMatchingNodes(const Type *T); */
/*   void addWeakEdges(NodePtr N); */
/*   void clear() { */
/*     TypeToNodes.clear(); */
/*     AliasSet.clear(); */
/*     WeakSet.clear(); */
/*   } */
/*   size_t size() const { */
/*     return AliasSet.size(); */
/*   } */
/*   const NodeWeakSetT &getAliasSet() const { */
/*     return AliasSet; */
/*   } */
/*   const NodeWeakSetT &getWeakSet() const { */
/*     return WeakSet; */
/*   } */
/*   void dump() const; */

/*   //void addNodeAndWeakEdges(NodePtr N); // TODO: remove */
/* }; */

/* } */
/* } */

/* #endif */
