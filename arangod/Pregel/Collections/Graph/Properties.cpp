#include "Properties.h"

using namespace arangodb::pregel::collections::graph;

auto EdgeCollectionRestrictions::add(EdgeCollectionRestrictions others) const
    -> EdgeCollectionRestrictions {
  auto newItems = items;
  for (auto const& [vertexCollection, edgeCollections] : others.items) {
    for (auto const& edgeCollection : edgeCollections) {
      newItems[vertexCollection].emplace_back(edgeCollection);
    }
  }
  return {newItems};
}
