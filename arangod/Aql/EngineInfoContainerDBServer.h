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

#ifndef ARANGOD_AQL_ENGINE_INFO_CONTAINER_DBSERVER_H
#define ARANGOD_AQL_ENGINE_INFO_CONTAINER_DBSERVER_H 1

#include "Basics/Common.h"

#include "Aql/ExecutionNode.h"
#include "Aql/types.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/AccessMode.h"

#include <stack>

namespace arangodb {

class ClusterComm;
class Result;
class CollectionNameResolver;

namespace aql {

struct Collection;
class GraphNode;
class ScatterNode;
class Query;

class EngineInfoContainerDBServer {
 private:
  // @brief Local struct to create the
  // information required to build traverser engines
  // on DB servers.
  struct TraverserEngineShardLists {
    explicit TraverserEngineShardLists(size_t length) {
      // Make sure they all have a fixed size.
      edgeCollections.resize(length);
    }

    ~TraverserEngineShardLists() {}

    // Mapping for edge collections to shardIds.
    // We have to retain the ordering of edge collections, all
    // vectors of these in one run need to have identical size.
    // This is because the conditions to query those edges have the
    // same ordering.
    std::vector<std::vector<ShardID>> edgeCollections;

    // Mapping for vertexCollections to shardIds.
    std::unordered_map<std::string, std::vector<ShardID>> vertexCollections;

#ifdef USE_ENTERPRISE
    std::set<ShardID> inaccessibleShards;
#endif
  };

  struct EngineInfo {
   public:
    explicit EngineInfo(size_t idOfRemoteNode) noexcept;
    EngineInfo(EngineInfo&& other) noexcept;
    ~EngineInfo();

#if (_MSC_VER != 0)
#pragma warning(disable : 4521)  // stfu wintendo.
#endif
    void addNode(ExecutionNode* node);

    void connectQueryId(QueryId id) noexcept { _otherId = id; }

    Collection const* collection() const noexcept;
    void collection(Collection* col) noexcept { _collection = col; }

    void serializeSnippet(Query& query, const ShardID& id, velocypack::Builder& infoBuilder,
                          bool isResponsibleForInitializeCursor) const;

    void serializeSnippet(ServerID const& serverId, Query& query,
                          std::vector<ShardID> const& shards, VPackBuilder& infoBuilder,
                          bool isResponsibleForInitializeCursor) const;

    /// @returns type of the "main node" if applicable,
    ///    'ExecutionNode::MAX_NODE_TYPE_VALUE' otherwise
    ExecutionNode::NodeType type() const noexcept { return _type; }

    LogicalView const* view() const noexcept;

   private:
    EngineInfo(EngineInfo&) = delete;
    EngineInfo(EngineInfo const& other) = delete;

    std::vector<ExecutionNode*> _nodes;
    size_t _idOfRemoteNode;  // id of the remote node
    QueryId _otherId;        // Id of query engine before this one
    union {
      Collection* _collection;  // The collection used to connect to this engine
      LogicalView const* _view;  // The view used to connect to this engine
    };
    ShardID _restrictedShard;  // The shard this snippet is restricted to
    ExecutionNode::NodeType _type{ExecutionNode::MAX_NODE_TYPE_VALUE};  // type of the "main node"
  };

  struct DBServerInfo {
   public:
    void addShardLock(AccessMode::Type const& lock, ShardID const& id);

    void addEngine(std::shared_ptr<EngineInfo> info, ShardID const& id);

    void setShardAsResponsibleForInitializeCursor(ShardID const& id);

    void buildMessage(ServerID const& serverId, EngineInfoContainerDBServer const& context,
                      Query& query, velocypack::Builder& infoBuilder) const;

    void addTraverserEngine(GraphNode* node, TraverserEngineShardLists&& shards);

    void combineTraverserEngines(ServerID const& serverID,
                                 arangodb::velocypack::Slice const ids);

   private:
    void injectTraverserEngines(VPackBuilder& infoBuilder) const;

    // @brief Map of LockType to ShardId
    std::unordered_map<AccessMode::Type, std::vector<ShardID>> _shardLocking;

    // @brief Map of all EngineInfos with their shards
    std::unordered_map<std::shared_ptr<EngineInfo>, std::vector<ShardID>> _engineInfos;

    std::unordered_set<ShardID> _shardsResponsibleForInitializeCursor;

    // @brief List of all information required for traverser engines
    std::vector<std::pair<GraphNode*, TraverserEngineShardLists>> _traverserEngineInfos;
  };

  struct CollectionInfo {
    void mergeShards(std::shared_ptr<std::vector<ShardID>> const& shards);

    AccessMode::Type lockType{AccessMode::Type::NONE};
    std::vector<std::shared_ptr<EngineInfo>> engines;
    std::vector<LogicalView const*> views;
    std::unordered_set<ShardID> usedShards;
    std::vector<ExecutionNode*> scatters;  // corresponding scatters
  };

 public:
  explicit EngineInfoContainerDBServer(Query* query) noexcept;

  // Insert a new node into the last engine on the stack
  // If this Node contains Collections, they will be added into the map
  // for ShardLocking
  void addNode(ExecutionNode* node);

  // Open a new snippet, which is connected to the given remoteNode id
  void openSnippet(size_t idOfRemoteNode);

  // Closes the given snippet and connects it
  // to the given queryid of the coordinator.
  void closeSnippet(QueryId id);

  // Build the Engines for the DBServer
  //   * Creates one Query-Entry for each Snippet per Shard (multiple on the
  //   same DB)
  //   * All snippets know all locking information for the query.
  //   * Only the first snippet is responsible to lock.
  //   * After this step DBServer-Collections are locked!
  //
  //   Error Case: It is guaranteed that for all snippets created during
  //   this methods a shutdown request is send to all DBServers.
  //   In case the network is broken and this shutdown request is lost
  //   the DBServers will clean up their snippets after a TTL.
  Result buildEngines(MapRemoteToSnippet& queryIds,
                      std::unordered_set<ShardID>& lockedShards) const;

  /**
   * @brief Will send a shutdown to all engines registered in the list of
   * queryIds.
   * NOTE: This function will ignore all queryids where the key is not of
   * the expected format
   * they may be leftovers from Coordinator.
   * Will also clear the list of queryIds after return.
   *
   * @param cc The ClusterComm
   * @param errorCode error Code to be send to DBServers for logging.
   * @param dbname Name of the database this query is executed in.
   * @param queryIds A map of QueryIds of the format: (remoteNodeId:shardId) ->
   * queryid.
   */
  void cleanupEngines(std::shared_ptr<ClusterComm> cc, int errorCode,
                      std::string const& dbname, MapRemoteToSnippet& queryIds) const;

  // Insert a GraphNode that needs to generate TraverserEngines on
  // the DBServers. The GraphNode itself will retain on the coordinator.
  void addGraphNode(GraphNode* node);

 private:
  /**
   * @brief Take care of this collection, set the lock state accordingly
   *        and maintain the list of used shards for this collection.
   *
   * @param col The collection that should be used
   * @param accessType The lock-type of this collection
   * @param restrictedShards The list of shards that can be relevant in this query (a subset of the collection shards)
   * @param scatter The corresponding scatter node, nullptr means no scatter
   */
  CollectionInfo& handleCollection(Collection const* col, AccessMode::Type const& accessType,
                                   ScatterNode* scatter = nullptr,
                                   std::unordered_set<std::string> const& restrictedShards = {});

  /**
   * @brief Update the collection on the last open engine. Used for communication
   *
   * @param col Collection to be inserted
   */
  void updateCollection(Collection const* col);

  // @brief Helper to create DBServerInfos and sort collections/shards into
  // them
  std::map<ServerID, EngineInfoContainerDBServer::DBServerInfo> createDBServerMapping(
      std::unordered_set<ShardID>& lockedShards) const;

  // @brief Helper to inject the TraverserEngines into the correct infos
  void injectGraphNodesToMapping(std::map<ServerID, DBServerInfo>& dbServerMapping) const;

#ifdef USE_ENTERPRISE
  void prepareSatellites(std::map<ServerID, DBServerInfo>& dbServerMapping) const;

  void resetSatellites() const;
#endif

 private:
  struct ViewInfo {
    std::vector<std::shared_ptr<EngineInfo>> engines;  // list of the engines associated with a view
    std::vector<ScatterNode*> scatters;  // list of the scatters associated with a view
  };

  // @brief The query that is executed. We are not responsible for it
  Query* _query;

  // @brief Reference to the last inserted EngineInfo, used for back linking of
  // QueryIds
  std::stack<std::shared_ptr<EngineInfo>> _engineStack;

  // @brief A map of Collection => Info required for distribution
  std::unordered_map<Collection const*, CollectionInfo> _collectionInfos;

  // @brief A map of LogicalView => Info required for distribution
  // std::map ~25-30% is faster than std::unordered_map for small number of elements
  std::map<LogicalView const*, ViewInfo> _viewInfos;

#ifdef USE_ENTERPRISE
  // @brief List of all satellite collections
  std::unordered_set<Collection const*> _satellites;
#endif

  // @brief List of all graphNodes that need to create TraverserEngines on
  // DBServers
  std::vector<GraphNode*> _graphNodes;
};

}  // namespace aql
}  // namespace arangodb
#endif
