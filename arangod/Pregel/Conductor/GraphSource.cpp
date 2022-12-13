#include "GraphSource.h"

#include "Cluster/ServerState.h"
#include "Pregel/Collections/Graph/Collections.h"
#include "VocBase/vocbase.h"
#include "fmt/ranges.h"

using namespace arangodb::pregel::conductor;

auto GraphSettings::isShardingCorrect(
    std::shared_ptr<collections::Collection> const& collection) -> Result {
  if (!collection->isSmart()) {
    std::vector<std::string> eKeys = collection->shardKeys();

    if (eKeys.size() != 1 || eKeys[0] != shardKeyAttribute) {
      return Result(
          TRI_ERROR_BAD_PARAMETER,
          fmt::format("Edge collection '{}' needs to be only sharded by "
                      "shardKeyAttribute parameter ('{}'), or use "
                      "SmartGraphs. The current shardKeys are: {}",
                      collection->name(), shardKeyAttribute,
                      eKeys.empty() ? "undefined"
                                    : "'" + fmt::format("{}", eKeys) + "'"));
    }
  }
  return Result{};
}

auto GraphSettings::getSource(TRI_vocbase_t& vocbase)
    -> ResultT<PregelGraphSource> {
  auto graphCollectionNames = graphSource.collectionNames(vocbase);
  if (graphCollectionNames.fail()) {
    return graphCollectionNames.result();
  }
  auto graphCollections = collections::graph::GraphCollections::from(
      graphCollectionNames.get(), vocbase);
  if (graphCollections.fail()) {
    return graphCollections.result();
  }

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

  if (!ServerState::instance()->ROLE_SINGLE) {
    for (auto const& [_, collection] :
         graphCollections->edgeCollections.collections) {
      if (auto result = isShardingCorrect(collection); result.fail()) {
        return result;
      }
    }
  }

  auto edgeCollectionRestrictions = graphSource.restrictions(vocbase);
  if (edgeCollectionRestrictions.fail()) {
    return edgeCollectionRestrictions.result();
  }

  return {PregelGraphSource{
      .edgeCollectionRestrictions = graphCollections.get().convertToShards(
          std::move(edgeCollectionRestrictions).get()),
      .vertexShards =
          graphCollections.get().vertexCollections.shardsPerServer(),
      .edgeShards = graphCollections.get().edgeCollections.shardsPerServer(),
      .allShards = allCollections.shards(),
      .planIds = allCollections.planIds()}};
}
