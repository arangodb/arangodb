#include "GraphSource.h"

#include "Pregel/Collections/CollectionFactory.h"
#include "Pregel/PregelOptions.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::conductor;

namespace {
template<class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
}  // namespace

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
  auto collectionFactory = collections::CollectionFactory{vocbase};
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

auto GraphSourceSettings::isShardingCorrect(
    std::shared_ptr<collections::Collection> const& collection) -> Result {
  if (!collection->isSmart()) {
    std::vector<std::string> eKeys = collection->shardKeys();

    if (eKeys.size() != 1 || eKeys[0] != shardKeyAttribute) {
      return Result(
          TRI_ERROR_BAD_PARAMETER,
          fmt::format("Edge collection needs to be sharded by "
                      "shardKeyAttribute parameter ('{}'), or use "
                      "SmartGraphs. The current shardKey is: {}",
                      shardKeyAttribute,
                      eKeys.empty() ? "undefined" : "'" + eKeys[0] + "'"));
    }
  }
  return Result{};
}

auto GraphSourceSettings::getSource(TRI_vocbase_t& vocbase)
    -> ResultT<PregelGraphSource> {
  auto graphCollectionNames = graphDataSource.collectionNames(vocbase);
  if (graphCollectionNames.fail()) {
    return graphCollectionNames.result();
  }
  auto graphCollections =
      GraphCollections::from(graphCollectionNames.get(), vocbase);
  if (graphCollections.fail()) {
    return graphCollections.result();
  }

  auto graphRestrictions = graphDataSource.graphRestrictions(vocbase);
  if (graphRestrictions.fail()) {
    return graphRestrictions.result();
  }
  auto allRestrictions =
      edgeCollectionRestrictions.add(std::move(graphRestrictions).get());

  auto allCollections = graphCollections.get().all();
  for (auto const& [_, collection] : allCollections.collections) {
    if (collection->isSystem()) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    fmt::format("Cannot use pregel on system collection {}",
                                collection->name()));
    }
    if (collection->isDeleted()) {
      return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, collection->name());
    }
    if (!collection->hasAccessRights(auth::Level::RW)) {
      return Result{TRI_ERROR_FORBIDDEN};
    }
    if (storeResults && !collection->hasAccessRights(auth::Level::RW)) {
      return Result{TRI_ERROR_FORBIDDEN};
    }
  }

  for (auto const& [_, collection] :
       graphCollections->edgeCollections.collections) {
    if (auto result = isShardingCorrect(collection); result.fail()) {
      return result;
    }
  }

  return {PregelGraphSource{
      .edgeCollectionRestrictions =
          graphCollections.get().convertToShards(allRestrictions),
      .vertexShards =
          graphCollections.get().vertexCollections.shardsPerServer(),
      .edgeShards = graphCollections.get().edgeCollections.shardsPerServer(),
      .allShards = allCollections.shards(),
      .planIds = allCollections.planIds()}};
}
