#include "Collections.h"

using namespace arangodb::pregel::collections;

auto Collections::shardsPerServer() const
    -> std::unordered_map<ServerID,
                          std::map<CollectionID, std::vector<ShardID>>> {
  auto result =
      std::unordered_map<ServerID,
                         std::map<CollectionID, std::vector<ShardID>>>{};
  for (auto const& [_, collection] : collections) {
    for (auto&& [server, map] : collection->shardsPerServer()) {
      for (auto&& [coll, shards] : map) {
        result[server][coll] = shards;
      }
    }
  }
  return result;
}

auto Collections::shards() const -> std::vector<ShardID> {
  auto result = std::vector<ShardID>{};
  for (auto const& [_, collection] : collections) {
    auto shards = collection->shards();
    result.insert(result.end(), shards.begin(), shards.end());
  }
  return result;
}

auto Collections::find(CollectionID const& id) const
    -> std::optional<std::shared_ptr<Collection>> {
  auto found = collections.find(id);
  if (found == collections.end()) {
    return std::nullopt;
  }
  return {found->second};
}

auto Collections::planIds() const
    -> std::unordered_map<CollectionID, std::string> {
  auto result = std::unordered_map<CollectionID, std::string>{};
  for (auto const& [_, collection] : collections) {
    auto planIds = collection->planIds();
    result.insert(planIds.begin(), planIds.end());
  }
  return result;
}

auto Collections::insert(Collections const& other) -> void {
  collections.insert(other.collections.begin(), other.collections.end());
}
