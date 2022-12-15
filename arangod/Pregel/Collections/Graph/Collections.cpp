#include "Collections.h"

#include "Pregel/Collections/CollectionFactory.h"
#include "VocBase/vocbase.h"

using namespace arangodb::pregel::collections::graph;

auto GraphCollections::convertToShards(
    EdgeCollectionRestrictions const& restrictions) const
    -> std::unordered_map<VertexShardID, std::vector<EdgeShardID>> {
  auto restrictionsPerShard =
      std::unordered_map<VertexShardID, std::vector<EdgeShardID>>{};
  for (auto const& [vertexCollectionId, edgeCollectionIds] :
       restrictions.items) {
    auto vertexCollection = vertexCollections.find(vertexCollectionId);
    if (vertexCollection == std::nullopt) {
      continue;
    }
    for (auto const& vertexShard : vertexCollection.value()->shards()) {
      for (auto const& edgeCollectionId : edgeCollectionIds) {
        auto edgeCollection = edgeCollections.find(edgeCollectionId);
        if (edgeCollection == std::nullopt) {
          continue;
        }
        for (auto const& edgeShard : edgeCollection.value()->shards()) {
          restrictionsPerShard[vertexShard].emplace_back(edgeShard);
        }
      }
    }
  }
  return restrictionsPerShard;
}

auto GraphCollections::from(GraphCollectionNames const& names,
                            TRI_vocbase_t& vocbase)
    -> ResultT<GraphCollections> {
  auto collectionFactory = CollectionFactory{vocbase};
  auto vertexCollections = collectionFactory.create(names.vertexCollections);
  if (vertexCollections.fail()) {
    return vertexCollections.result();
  }
  auto edgeCollections = collectionFactory.create(names.edgeCollections);
  if (edgeCollections.fail()) {
    return edgeCollections.result();
  }
  return GraphCollections{.vertexCollections = vertexCollections.get(),
                          .edgeCollections = edgeCollections.get()};
}

auto GraphCollections::all() const -> collections::Collections {
  auto allCollections = vertexCollections;
  allCollections.insert(edgeCollections);
  return allCollections;
}
