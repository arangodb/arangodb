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
    EngineInfo(QueryId id, std::vector<ExecutionNode*>& nodes,
               size_t idOfRemoteNode);
    ~EngineInfo();

    EngineInfo(EngineInfo&) = delete;
    EngineInfo(EngineInfo const& other) = delete;
    EngineInfo(EngineInfo const&& other);

    void connectQueryId(QueryId id);

    ExecutionEngine* buildEngine(
        Query* query,
        QueryRegistry* queryRegistry,
        std::unordered_map<std::string, std::string>& queryIds) const;

   private:
    QueryId const _id;
    std::vector<ExecutionNode*> _nodes;
    size_t _idOfRemoteNode;  // id of the remote node
  };

 public:
  EngineInfoContainerCoordinator();

  ~EngineInfoContainerCoordinator();

  // Add a new query part to be executed in this location
  // This will have the following effect:
  // * It creates a new EngineInfo from the given information
  // * It adds the collections to the map
  // This intentionally copies the vector of nodes, the caller reuses
  // the given vector.
  QueryId addQuerySnippet(std::vector<ExecutionNode*>& nodes,
                          size_t idOfRemoteNode);

  // Build the Engines on the coordinator
  //   * Creates the ExecutionBlocks
  //   * Injects all Parts but the First one into QueryRegistery
  //   Return the first engine which is not added in the Registry
  ExecutionEngine* buildEngines(
      Query* query,
      QueryRegistry* registry,
      std::unordered_map<std::string, std::string>& queryIds);

 private:
  // @brief List of EngineInfos to distribute accross the cluster
  std::vector<EngineInfo> _engines;
};

class EngineInfoContainerDBServer {
 private:
  struct EngineInfo {
   public:
    EngineInfo(std::vector<ExecutionNode*>& nodes, size_t idOfRemoteNode);
    ~EngineInfo();

    EngineInfo(EngineInfo&) = delete;
    EngineInfo(EngineInfo const& other) = delete;
    EngineInfo(EngineInfo const&& other);

    void connectQueryId(QueryId id);

    void serializeSnippet(ShardID id, velocypack::Builder& infoBuilder);

   private:
    std::vector<ExecutionNode*> _nodes;
    size_t _idOfRemoteNode;  // id of the remote node
    QueryId _otherId;        // Id of query engine before this one
  };

  struct DBServerInfo {
   public:
    DBServerInfo();
    ~DBServerInfo();

   public:
    void addShardLock(AccessMode::Type const& lock, ShardID const& id);

    void addEngine(EngineInfo* info, ShardID const& id);

    void buildMessage(Query* query, velocypack::Builder& infoBuilder) const;

   private:
    void injectQueryOptions(Query* query,
                            velocypack::Builder& infoBuilder) const;

   private:
    // @brief Map of LockType to ShardId
    std::unordered_map<AccessMode::Type, std::vector<ShardID>> _shardLocking;

    // @brief Map of all EngineInfos with their shards
    std::unordered_map<EngineInfo*, std::vector<ShardID>> _engineInfos;
  };

 public:
  EngineInfoContainerDBServer();

  ~EngineInfoContainerDBServer();

  // Add a new query part to be executed in this location
  // This will have the following effect:
  // * It creates a new EngineInfo from the given information
  // * It adds the collections to the map
  // This intentionally copies the vector of nodes, the caller reuses
  // the given vector.
  void addQuerySnippet(std::vector<ExecutionNode*>& nodes,
                       size_t idOfRemoteNode);

  // Connects the last snippet in the list with the given QueryId.
  // This is only required for DBServer snippets and is used
  // to give them the query id on the Coordinator.
  // The coordinator needs to build a list of ids later on.
  void connectLastSnippet(QueryId id);

  // Build the Engines for the DBServer
  //   * Creates one Query-Entry with all locking information per DBServer
  //   * Creates one Query-Entry for each Snippet per Shard (multiple on the
  //   same DB)
  //   * Snippets DO NOT lock anything, locking is done in the overall query.
  //   * After this step DBServer-Collections are locked!
  void buildEngines(Query* query);

 private:
  // @brief Reference to the last inserted EngineInfo, used for back linking of
  // QueryIds
  EngineInfo* _lastEngine;

  // @brief List of EngineInfos to distribute accross the cluster
  std::unordered_map<Collection const*, std::vector<EngineInfo>> _engines;

  // @brief Mapping of used collection names to lock type required
  std::unordered_map<Collection const*, AccessMode::Type> _collections;

  // @brief List of all satellite collections
  std::unordered_set<Collection const*> _satellites;
};

}  // aql
}  // arangodb
#endif
