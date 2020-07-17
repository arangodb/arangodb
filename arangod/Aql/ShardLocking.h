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

#include "Aql/ExecutionNodeId.h"
#include "Aql/QuerySnippet.h"
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
class QueryContext;

/*
 * This class is responsible to ensure all shards that participate in a query
 * get locked with the correct lock type.
 * During instantiation on the coordinator every ExecutionNode is passed
 * through this class which adapts locking accordingly.
 * As a side-effect this class can expose which servers are going
 * to participate in this query, and it can also expose a mapping
 * of participating shards => leaders.
 */

class ShardLocking {
 private:
  static std::set<ShardID> const EmptyShardList;
  static std::unordered_set<ShardID> const EmptyShardListUnordered;
  // @brief Information about a single snippet, if this snippet is restricted to a certain set of shards
  struct SnippetInformation {
    SnippetInformation() : isRestricted(false), restrictedShards({}) {}
    // Flag if this snippet is restricted at all
    bool isRestricted;
    // The list of shards this snippet is restricted to.
    // Invariant isRestricted == false <=>  restrictedShards.empty()
    // Invariant isRestricted == true  <=> !restrictedShards.empty()
    std::unordered_set<ShardID> restrictedShards;
  };

  // @brief the information about the locking for a single collection.
  //  will be modified during the instantiation of the plan on coordinator.
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
  // @brief prepare a shardlocking for the new query.
  explicit ShardLocking(QueryContext& query) : _query(query) {}

  // @brief Every ExectionNode that is send to a Database server needs to be passed through this method
  // this class will check if a collection (or more) is used, and will adapt the locking.
  // The given snippetId is used to determin in which snippet this node is used.
  // This will also check for shard restrictions on the given node.
  void addNode(ExecutionNode const* node, size_t snippetId, bool pushToSingleServer);

  // @brief we need to send the lock information to a database server.
  // This is the function that serializes this information for the given server.
  // NOTE: There is only one locking, but there can be many snippets on this
  // server. The handed in Builder needs to be an Open Object.
  void serializeIntoBuilder(ServerID const& server,
                            arangodb::velocypack::Builder& builder) const;

  // The list of servers that will participate in this query as leaders for at least one shard.
  // Only these servers need to be informed by the coordinator.
  // Note: As a side effec this will create the ShardMapping on the first call.
  // This function needs to be called before you can get any shardInformation below.
  std::vector<ServerID> getRelevantServers();

  // The list of all collections used within this query.
  // Only shards of these collections are locked!
  std::vector<Collection const*> getUsedCollections() const;

  // Get the shards for the given collection, that hat their leader on the given server.
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

  // Get a full mapping of ShardID => LeaderID.
  // This will stay constant during this query, and a query could be aborted in case of failovers.
  std::unordered_map<ShardID, ServerID> const& getShardMapping();

  // Get the shards of the given collection within the given snippet.
  // This will honor shard restrictions on the given snippet.
  // All shards will be returned, there will be no filtering on the server.
  std::unordered_set<ShardID> const& shardsForSnippet(aql::QuerySnippet::Id snippetId,
                                                      Collection const* col);

 private:
  // Adjust locking level of a single collection
  void updateLocking(Collection const* col, AccessMode::Type const& accessType,
                     size_t snippetId, std::unordered_set<std::string> const& restrictedShards,
                     bool useAsSatellite);

 private:
  QueryContext& _query;

  std::unordered_map<Collection const*, CollectionLockingInformation> _collectionLocking;

  std::unordered_map<ServerID, std::unordered_map<Collection const*, std::set<ShardID>>> _serverToCollectionToShard;

  std::unordered_map<ServerID, std::unordered_map<AccessMode::Type, std::unordered_set<ShardID>>> _serverToLockTypeToShard;

  std::unordered_map<ShardID, ServerID> _shardMapping;
};

}  // namespace aql
}  // namespace arangodb

#endif
