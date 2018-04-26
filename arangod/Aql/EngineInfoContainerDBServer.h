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

#include "Aql/types.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/AccessMode.h"

#include <stack>

namespace arangodb {

class ClusterComm;
class Result;

namespace aql {

struct Collection;
class ExecutionNode;
class GraphNode;
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
    explicit EngineInfo(size_t idOfRemoteNode);
    ~EngineInfo();

#if (_MSC_VER != 0)
#pragma warning( disable : 4521) // stfu wintendo.
#endif
    EngineInfo(EngineInfo&) = delete;
    EngineInfo(EngineInfo const& other) = delete;
    EngineInfo(EngineInfo const&& other);

    void connectQueryId(QueryId id);

    void serializeSnippet(Query* query, ShardID id,
                          velocypack::Builder& infoBuilder,
                          bool isResponsibleForInit) const;

    Collection const* collection() const;

    void collection(Collection* col);

    void addNode(ExecutionNode* node);

   private:
    std::vector<ExecutionNode*> _nodes;
    size_t _idOfRemoteNode;   // id of the remote node
    QueryId _otherId;         // Id of query engine before this one
    Collection* _collection;  // The collection used to connect to this engine
  };

  struct DBServerInfo {
   public:
    DBServerInfo();
    ~DBServerInfo();

   public:
    void addShardLock(AccessMode::Type const& lock, ShardID const& id);

    void addEngine(std::shared_ptr<EngineInfo> info, ShardID const& id);

    void buildMessage(Query* query, velocypack::Builder& infoBuilder) const;

    void addTraverserEngine(GraphNode* node,
                            TraverserEngineShardLists&& shards);

    void combineTraverserEngines(ServerID const& serverID,
                                 arangodb::velocypack::Slice const ids);

   private:
    void injectTraverserEngines(VPackBuilder& infoBuilder) const;

    void injectQueryOptions(Query* query,
                            velocypack::Builder& infoBuilder) const;

   private:
    // @brief Map of LockType to ShardId
    std::unordered_map<AccessMode::Type, std::vector<ShardID>> _shardLocking;

    // @brief Map of all EngineInfos with their shards
    std::unordered_map<std::shared_ptr<EngineInfo>, std::vector<ShardID>>
        _engineInfos;

    // @brief List of all information required for traverser engines
    std::vector<std::pair<GraphNode*, TraverserEngineShardLists>>
        _traverserEngineInfos;
  };

 public:
  EngineInfoContainerDBServer();

  ~EngineInfoContainerDBServer();

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
  Result buildEngines(Query* query,
                      std::unordered_map<std::string, std::string>& queryIds,
                      std::unordered_set<std::string> const& restrictToShards,
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
  void cleanupEngines(
      std::shared_ptr<ClusterComm> cc, int errorCode, std::string const& dbname,
      std::unordered_map<std::string, std::string>& queryIds) const;

  // Insert a GraphNode that needs to generate TraverserEngines on
  // the DBServers. The GraphNode itself will retain on the coordinator.
  void addGraphNode(Query* query, GraphNode* node);

 private:
  void handleCollection(Collection const* col,
                        AccessMode::Type const& accessType,
                        bool updateCollection);

  // @brief Helper to create DBServerInfos and sort collections/shards into
  // them
  std::map<ServerID, EngineInfoContainerDBServer::DBServerInfo>
  createDBServerMapping(std::unordered_set<std::string> const& restrictToShards,
                        std::unordered_set<ShardID>& lockedShards) const;

  // @brief Helper to inject the TraverserEngines into the correct infos
  void injectGraphNodesToMapping(
      Query* query, std::unordered_set<std::string> const& restrictToShards,
      std::map<ServerID, EngineInfoContainerDBServer::DBServerInfo>&
          dbServerMapping) const;

#ifdef USE_ENTERPRISE
  void prepareSatellites(
      std::map<ServerID, DBServerInfo>& dbServerMapping,
      std::unordered_set<std::string> const& restrictToShards) const;

  void resetSatellites() const;
#endif

 private:
  // @brief Reference to the last inserted EngineInfo, used for back linking of
  // QueryIds
  std::stack<std::shared_ptr<EngineInfo>> _engineStack;

  // @brief List of EngineInfos to distribute accross the cluster
  std::unordered_map<Collection const*,
                     std::vector<std::shared_ptr<EngineInfo>>>
      _engines;

  // @brief Mapping of used collection names to lock type required
  std::unordered_map<Collection const*, AccessMode::Type> _collections;

#ifdef USE_ENTERPRISE
  // @brief List of all satellite collections
  std::unordered_set<Collection const*> _satellites;
#endif

  // @brief List of all graphNodes that need to create TraverserEngines on
  // DBServers
  std::vector<GraphNode*> _graphNodes;
};

}  // aql
}  // arangodb
#endif
