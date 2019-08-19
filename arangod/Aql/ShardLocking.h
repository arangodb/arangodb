////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SHARD_LOCKING_H
#define ARANGOD_AQL_SHARD_LOCKING_H 1

#include "Cluster/ClusterInfo.h"
#include "VocBase/AccessMode.h"

#include <set>

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace aql {

struct Collection;
class ExecutionNode;
class Query;

class ShardLocking {
 private:
  static std::set<ShardID> const EmptyShardList;
  static std::unordered_set<ShardID> const EmptyShardListUnordered;
  struct SnippetInformation {
    SnippetInformation() : isRestricted(false), restrictedShards({}) {}
    bool isRestricted;
    std::unordered_set<ShardID> restrictedShards;
  };

  struct CollectionLockingInformation {
    // Lock type used for this collection
    AccessMode::Type lockType{AccessMode::Type::NONE};
    // The list of All shards of this query respecting query limits,
    // it is possible that not all shards are used.
    std::unordered_set<ShardID> allShards{};
    // The list of specific shard information for snippets
    std::unordered_map<size_t, SnippetInformation> snippetInfo{};
    // Flag if the collection is used as a Satellite in any snippet.
    bool isSatellite = false;
  };

 public:
  ShardLocking(Query* query) : _query(query) { TRI_ASSERT(_query != nullptr); }

  void addNode(ExecutionNode const* node, size_t snippetId);

  void serializeIntoBuilder(ServerID const& server,
                            arangodb::velocypack::Builder& builder) const;

  std::vector<ServerID> getRelevantServers();

  std::vector<Collection const*> getUsedCollections() const;

  std::set<ShardID> const& getShardsForCollection(ServerID const& server,
                                                  Collection const* col) const {
    // NOTE: This function will not lazily update the Server list
    TRI_ASSERT(!_serverToCollectionToShard.empty());
    auto perServer = _serverToCollectionToShard.find(server);
    // This is guaranteed, every server has at least 1 shard
    TRI_ASSERT(perServer != _serverToCollectionToShard.end());
    auto shards = perServer->second.find(col);
    if (shards == perServer->second.end()) {
      return EmptyShardList;
    }
    return shards->second;
  }

  std::unordered_map<ShardID, ServerID> const& getShardMapping();

  std::unordered_set<ShardID> const& shardsForSnippet(size_t snippetId,
                                                      Collection const* col);

 private:
  // Adjust locking level of a single collection
  void updateLocking(Collection const* col, AccessMode::Type const& accessType,
                     size_t snippetId, std::unordered_set<std::string> const& restrictedShards,
                     bool useAsSatellite);

 private:
  Query* _query;

  std::unordered_map<Collection const*, CollectionLockingInformation> _collectionLocking;

  std::unordered_map<ServerID, std::unordered_map<Collection const*, std::set<ShardID>>> _serverToCollectionToShard;

  std::unordered_map<ServerID, std::unordered_map<AccessMode::Type, std::unordered_set<ShardID>>> _serverToLockTypeToShard;

  std::unordered_map<ShardID, ServerID> _shardMapping;
};

}  // namespace aql
}  // namespace arangodb

#endif