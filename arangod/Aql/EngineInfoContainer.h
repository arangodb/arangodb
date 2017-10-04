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

#ifndef ARANGOD_AQL_ENGINE_INFO_CONTAINER_H
#define ARANGOD_AQL_ENGINE_INFO_CONTAINER_H 1

#include "Basics/Common.h"

#include "Aql/types.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/AccessMode.h"

#include <stack>

namespace arangodb {
namespace aql {

class Collection;
class ExecutionEngine;
class ExecutionNode;
class Query;
class QueryRegistry;

class EngineInfoContainerCoordinator {
 private:
  struct EngineInfo {
   public:
    EngineInfo(QueryId id, size_t idOfRemoteNode);
    ~EngineInfo();

    EngineInfo(EngineInfo&) = delete;
    EngineInfo(EngineInfo const& other) = delete;
    EngineInfo(EngineInfo const&& other);

    void addNode(ExecutionNode* en);

    void buildEngine(Query* query, QueryRegistry* queryRegistry,
                     std::unordered_map<std::string, std::string>& queryIds,
                     std::unordered_set<ShardID> const* lockedShards) const;

    QueryId queryId() const {
      return _id;
    }

   private:

    // The generated id how we can find this query part 
    // in the coordinators registry.
    QueryId const _id;

    // The nodes belonging to this plan.
    std::vector<ExecutionNode*> _nodes;

    // id of the remote node this plan collects data from
    size_t _idOfRemoteNode;

  };

 public:
  EngineInfoContainerCoordinator();

  ~EngineInfoContainerCoordinator();

  // Insert a new node into the last engine on the stack
  void addNode(ExecutionNode* node);

  // Open a new snippet, which is connected to the given remoteNode id
  void openSnippet(size_t idOfRemoteNode);

  // Close the currently open snippet.
  // This will finallizes the EngineInfo from the given information
  // This will intentionally NOT insert the Engines into the query
  // registry for easier cleanup
  // Returns the queryId of the closed snippet
  QueryId closeSnippet();

  // Build the Engines on the coordinator
  //   * Creates the ExecutionBlocks
  //   * Injects all Parts but the First one into QueryRegistery
  //   Return the first engine which is not added in the Registry
  ExecutionEngine* buildEngines(
      Query* query, QueryRegistry* registry,
      std::unordered_map<std::string, std::string>& queryIds,
      std::unordered_set<ShardID> const* lockedShards) const;

 private:
  // @brief List of EngineInfos to distribute accross the cluster
  std::vector<EngineInfo> _engines;

  std::stack<size_t> _engineStack;
};

class EngineInfoContainerDBServer {
 private:
  struct EngineInfo {
   public:
    EngineInfo(size_t idOfRemoteNode);
    ~EngineInfo();

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

   private:
    void injectQueryOptions(Query* query,
                            velocypack::Builder& infoBuilder) const;

   private:
    // @brief Map of LockType to ShardId
    std::unordered_map<AccessMode::Type, std::vector<ShardID>> _shardLocking;

    // @brief Map of all EngineInfos with their shards
    std::unordered_map<std::shared_ptr<EngineInfo>, std::vector<ShardID>> _engineInfos;
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
  //   * Creates one Query-Entry with all locking information per DBServer
  //   * Creates one Query-Entry for each Snippet per Shard (multiple on the
  //   same DB)
  //   * Snippets DO NOT lock anything, locking is done in the overall query.
  //   * After this step DBServer-Collections are locked!
  void buildEngines(Query* query,
                    std::unordered_map<std::string, std::string>& queryIds,
                    std::unordered_set<ShardID>* lockedShards) const;

  // Insert a GraphNode that eeds to generate TraverserEngines on
  // the DBServers. The GraphNode itself will retain on the coordinator.
  void addGraphNode(GraphNode* node);

 private:

  void handleCollection(Collection const* col, bool isWrite);

 private:
  // @brief Reference to the last inserted EngineInfo, used for back linking of
  // QueryIds
  std::stack<std::shared_ptr<EngineInfo>> _engineStack;

  // @brief List of EngineInfos to distribute accross the cluster
  std::unordered_map<Collection const*, std::vector<std::shared_ptr<EngineInfo>>> _engines;

  // @brief Mapping of used collection names to lock type required
  std::unordered_map<Collection const*, AccessMode::Type> _collections;

  // @brief List of all satellite collections
  std::unordered_set<Collection const*> _satellites;

  // @brief List of all graphNodes that need to create TraverserEngines on DBServers
  std::vector<GraphNode*> _graphNodes;
};

}  // aql
}  // arangodb
#endif
