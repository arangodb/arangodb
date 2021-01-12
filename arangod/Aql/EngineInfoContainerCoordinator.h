////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_ENGINE_INFO_CONTAINER_COORDINATOR_H
#define ARANGOD_AQL_ENGINE_INFO_CONTAINER_COORDINATOR_H 1

#include "Aql/types.h"
#include "Aql/ExecutionEngine.h"

#include <stack>
#include <string>
#include <unordered_set>

namespace arangodb {
class Result;

namespace aql {

class AqlItemBlockManager;
class ExecutionNode;
class QueryContext;
class QueryRegistry;

class EngineInfoContainerCoordinator {
 private:
  struct EngineInfo {
   public:
    EngineInfo(EngineId id, ExecutionNodeId idOfRemoteNode);

    // This container is not responsible for nodes, they are managed by the AST
    // somewhere else.
    ~EngineInfo() = default;

#if (_MSC_VER != 0)
#pragma warning(disable : 4521)  // stfu wintendo.
#endif
    EngineInfo(EngineInfo&) = delete;
    EngineInfo(EngineInfo const& other) = delete;
    EngineInfo(EngineInfo&& other) noexcept;

    void addNode(ExecutionNode* en);

    Result buildEngine(Query& query, MapRemoteToSnippet const& dbServerQueryIds,
                       bool isfirst, std::unique_ptr<ExecutionEngine>& engine) const;

    EngineId engineId() const;

   private:
    // The generated id how we can find this query part
    // in the coordinators registry.
    EngineId const _id;

    // The nodes belonging to this plan.
    std::vector<ExecutionNode*> _nodes;

    // id of the remote node this plan collects data from
    ExecutionNodeId _idOfRemoteNode;
  };

 public:
  EngineInfoContainerCoordinator();

  ~EngineInfoContainerCoordinator();

  // Insert a new node into the last engine on the stack
  void addNode(ExecutionNode* node);

  // Open a new snippet, which is connected to the given remoteNode id
  void openSnippet(ExecutionNodeId idOfRemoteNode);

  // Close the currently open snippet.
  // This will finalizes the EngineInfo from the given information
  // This will intentionally NOT insert the Engines into the query
  // registry for easier cleanup
  // Returns the queryId of the closed snippet
  QueryId closeSnippet();

  // Build the Engines on the coordinator
  //   * Creates the ExecutionBlocks
  //   * Injects all Parts but the First one into QueryRegistry
  //   Return the first engine which is not added in the Registry
  Result buildEngines(Query& query, AqlItemBlockManager& mgr,
                      MapRemoteToSnippet const& dbServerQueryIds,
                      SnippetList& coordSnippets) const;

 private:
  // @brief List of EngineInfos to distribute across the cluster
  std::vector<EngineInfo> _engines;

  std::stack<size_t, std::vector<size_t>> _engineStack;
};

}  // namespace aql
}  // namespace arangodb

#endif
