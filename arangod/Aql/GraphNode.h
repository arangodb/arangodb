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

#ifndef ARANGOD_AQL_GRAPH_NODE_H
#define ARANGOD_AQL_GRAPH_NODE_H 1

#include "Aql/ExecutionNode.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/TraverserEngineRegistry.h"

namespace arangodb {

namespace graph {
class BaseOptions;
}

namespace aql {

class Graph;

// @brief This is a pure virtual super-class for all AQL graph operations
//        It does the generally required:
//        * graph info parsing
//        * traverser-engine creation
//        * option preparation
//        * Smart Graph Handling

class GraphNode : public ExecutionNode {
 public:
  /// @brief constructor with a vocbase and a collection name
  GraphNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
            std::unique_ptr<graph::BaseOptions>& options);

  GraphNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base);

  virtual ~GraphNode();

  /// @brief Compute the shortest path options containing the expressions
  ///        MUST! be called after optimization and before creation
  ///        of blocks.
  virtual void prepareOptions() = 0;

 protected:
  /// @brief the database
  TRI_vocbase_t* _vocbase;

  /// @brief vertex output variable
  Variable const* _vertexOutVariable;

  /// @brief vertex output variable
  Variable const* _edgeOutVariable;

  /// @brief our graph...
  Graph const* _graphObj;

  /// @brief Temporary pseudo variable for the currently traversed object.
  Variable const* _tmpObjVariable;

  /// @brief Reference to the pseudo variable
  AstNode* _tmpObjVarNode;

  /// @brief Pseudo string value node to hold the last visted vertex id.
  AstNode* _tmpIdNode;

  /// @brief Options for traversals
  std::unique_ptr<graph::BaseOptions> _options;

  /// @brief Flag if the options have been build.
  /// Afterwards this class is not copyable anymore.
  bool _optionsBuild;

  /// @brief The list of traverser engines grouped by server.
  std::unordered_map<ServerID, traverser::TraverserEngineID> _engines;

  /// @brief flag, if graph is smart (enterprise edition only!)
  bool _isSmart;
};

} //namespace aql
} // namespace arangodb
#endif
