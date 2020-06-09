#ifndef LLVM_ANALYSIS_IMMUTABILITY_MEMORY_ALIASES_H
#define LLVM_ANALYSIS_IMMUTABILITY_MEMORY_ALIASES_H

#include "Node.h"

namespace llvm {
namespace immutability {

class MemoryAliases {
public:
  enum class AliasKind {
    KNOWN,
    UNKNOWN,
    KNOWN_ELEMENT,
    UNKNOWN_ELEMENT,
  };

  typedef DenseMap<const Type *, NodeWeakSetT> TypeNodesMap;

  typedef std::pair<const CompositeType *, unsigned> ElementPair;
  typedef DenseMap<const Type*, DenseMap<ElementPair, NodeWeakSetT>>
    TypeElementNodesMap;

private:
  TypeNodesMap Known;
  TypeNodesMap Unknown;

  TypeElementNodesMap KnownElement;
  TypeElementNodesMap UnknownElement;

  DenseMap<const Node *, AliasKind> Kinds;
  DenseMap<const Node *, ElementPair> Elements;

public:
  void addKnown(const std::shared_ptr<Node> &N) {
    Known[N->getType()].insert(N);
    Kinds[N.get()] = AliasKind::KNOWN;
  }
  void addKnownElement(const std::shared_ptr<Node> &N, const ElementPair &Element) {
    KnownElement[N->getType()][Element].insert(N);
    Kinds[N.get()] = AliasKind::KNOWN_ELEMENT;
    Elements[N.get()] = Element;
  }
  void addUnknown(const std::shared_ptr<Node> &N) {
    Unknown[N->getType()].insert(N);
    Kinds[N.get()] = AliasKind::UNKNOWN;
  }
  void addUnknownElement(const std::shared_ptr<Node> &N, const ElementPair &Element) {
    UnknownElement[N->getType()][Element].insert(N);
    Kinds[N.get()] = AliasKind::UNKNOWN_ELEMENT;
    Elements[N.get()] = Element;
  }
  void addFrom(const std::shared_ptr<Node> &N, const MemoryAliases &OtherAliases, Node *Other) {
    if (OtherAliases.hasAliasKind(Other)) {
      auto AliasKind = OtherAliases.getAliasKind(Other);
      if (AliasKind == MemoryAliases::AliasKind::UNKNOWN) {
        addUnknown(N);
      }
      else if (AliasKind == MemoryAliases::AliasKind::KNOWN) {
        addKnown(N);
      }
      else {
        assert(OtherAliases.hasElement(Other));
        auto Element = OtherAliases.getElement(Other);
        if (AliasKind == MemoryAliases::AliasKind::UNKNOWN_ELEMENT) {
          addUnknownElement(N, Element);
        }
        else {
          addKnownElement(N, Element);
        }
      }
    }
  }

  bool hasAliasKind(const Node *N) const {
    return Kinds.count(N);
  }
  AliasKind getAliasKind(const Node *N) const {
    return Kinds.lookup(N);
  }
  bool hasElement(const Node *N) const {
    return Elements.count(N);
  }
  ElementPair getElement(const Node *N) const {
    return Elements.lookup(N);
  }

  std::vector<Node *> getWeakEdges(const Node *N) const;
  std::vector<std::shared_ptr<Node>> getWeakEdgesShared(const Node *N) const;

  void clear() {
    Known.clear();
    Unknown.clear();
    KnownElement.clear();
    UnknownElement.clear();
  }

  void dot(raw_ostream &O) const;

  static void CloneInto(MemoryAliases &New, const MemoryAliases &Old,
                        NodeToNodeSetT &ThisToClone);
};

}
}

#endif
