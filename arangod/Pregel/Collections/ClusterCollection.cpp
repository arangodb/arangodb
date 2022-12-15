#include "ClusterCollection.h"
#include "Cluster/ClusterInfo.h"

using namespace arangodb::pregel::collections;

auto ClusterCollection::name() const -> std::string_view {
  return virtualCollection->name();
}

auto ClusterCollection::shards() const -> std::vector<ShardID> {
  auto shards = std::vector<ShardID>{};
  for (auto const& collection : collections) {
    std::shared_ptr<std::vector<ShardID>> shardIDs =
        clusterInfo.getShardList(std::to_string(collection->id().id()));
    shards.insert(shards.end(), shardIDs->begin(), shardIDs->end());
  }
  return shards;
}

auto ClusterCollection::shardsPerServer() const
    -> std::unordered_map<ServerID,
                          std::map<CollectionID, std::vector<ShardID>>> {
  auto result =
      std::unordered_map<ServerID,
                         std::map<CollectionID, std::vector<ShardID>>>{};
  for (auto const& collection : collections) {
    std::shared_ptr<std::vector<ShardID>> shardIDs =
        clusterInfo.getShardList(std::to_string(collection->id().id()));
    for (auto const& shard : *shardIDs) {
      std::shared_ptr<std::vector<ServerID> const> servers =
          clusterInfo.getResponsibleServer(shard);
      result[(*servers)[0]][collection->name()].emplace_back(shard);
    }
  }
  return result;
}

auto ClusterCollection::planIds() const
    -> std::unordered_map<CollectionID, std::string> {
  auto result = std::unordered_map<CollectionID, std::string>{};
  for (auto const& collection : collections) {
    result[collection->name()] = std::to_string(collection->planId().id());
  }
  return result;
}

auto ClusterCollection::isSystem() const -> bool {
  return virtualCollection->system();
}

auto ClusterCollection::isDeleted() const -> bool {
  return virtualCollection->deleted() ||
         virtualCollection->status() == TRI_VOC_COL_STATUS_DELETED;
}

auto ClusterCollection::hasAccessRights(auth::Level requested) -> bool {
  ExecContext const& exec = ExecContext::current();
  for (auto const& collection : collections) {
    if (exec.isSuperuser() ||
        exec.canUseCollection(collection->name(), requested)) {
      return true;
    }
  }
  return false;
}

auto ClusterCollection::isSmart() const -> bool {
  return virtualCollection->isSmart();
}

auto ClusterCollection::shardKeys() const -> std::vector<std::string> {
  auto keys = std::vector<std::string>{};
  for (auto const& collection : collections) {
    auto collectionKeys = collection->shardKeys();
    keys.insert(keys.end(), collectionKeys.begin(), collectionKeys.end());
  }
  return keys;
}
