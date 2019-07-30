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

  struct CollectionLockingInformation {
    void mergeShards(std::shared_ptr<std::vector<ShardID>> const& shards);

    AccessMode::Type lockType{AccessMode::Type::NONE};
    std::unordered_set<ShardID> usedShards;
  };

 public:
  ShardLocking(Query* query) : _query(query) { TRI_ASSERT(_query != nullptr); }

  void addNode(ExecutionNode const* node);

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

 private:
  // Adjust locking level of a single collection
  void updateLocking(Collection const* col, AccessMode::Type const& accessType,
                     std::unordered_set<std::string> const& restrictedShards);

 private:
  Query* _query;

  std::unordered_map<Collection const*, CollectionLockingInformation> _collectionLocking;

  std::unordered_map<ServerID, std::unordered_map<Collection const*, std::set<ShardID>>> _serverToCollectionToShard;

  std::unordered_map<ServerID, std::unordered_map<AccessMode::Type, std::unordered_set<ShardID>>> _serverToLockTypeToShard;
};

}  // namespace aql
}  // namespace arangodb

#endif