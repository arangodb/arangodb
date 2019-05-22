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
struct BaseOptions;
class Graph;
}  // namespace graph

namespace aql {

// @brief This is a pure virtual super-class for all AQL graph operations
//        It does the generally required:
//        * graph info parsing
//        * traverser-engine creation
//        * option preparation
//        * Smart Graph Handling

class GraphNode : public ExecutionNode {
 public:
  /// @brief constructor with a vocbase and a collection name
  GraphNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase, AstNode const* direction,
            AstNode const* graph, std::unique_ptr<graph::BaseOptions> options);

  GraphNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base);

 protected:
  /// @brief Internal constructor to clone the node.
  GraphNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
            std::vector<std::unique_ptr<Collection>> const& edgeColls,
            std::vector<std::unique_ptr<Collection>> const& vertexColls,
            std::vector<TRI_edge_direction_e> const& directions,
            std::unique_ptr<graph::BaseOptions> options);

 public:
  virtual ~GraphNode();

  void toVelocyPackHelper(arangodb::velocypack::Builder& nodes, unsigned flags) const override;

  /// @brief the cost of a graph node
  CostEstimate estimateCost() const override;

  /// @brief flag, if smart traversal (enterprise edition only!) is done
  bool isSmart() const { return _isSmart; }

  /// @brief return the database
  TRI_vocbase_t* vocbase() const { return _vocbase; }

  /// @brief return the vertex out variable
  Variable const* vertexOutVariable() const { return _vertexOutVariable; }

  /// @brief checks if the vertex out variable is used
  bool usesVertexOutVariable() const { return _vertexOutVariable != nullptr; }

  /// @brief set the vertex out variable
  void setVertexOutput(Variable const* outVar) { _vertexOutVariable = outVar; }

  /// @brief return the edge out variable
  Variable const* edgeOutVariable() const { return _edgeOutVariable; }

  /// @brief checks if the edge out variable is used
  bool usesEdgeOutVariable() const { return _edgeOutVariable != nullptr; }

  /// @brief set the edge out variable
  void setEdgeOutput(Variable const* outVar) { _edgeOutVariable = outVar; }

  /// @brief Compute the shortest path options containing the expressions
  ///        MUST! be called after optimization and before creation
  ///        of blocks.
  virtual void prepareOptions() = 0;

  graph::BaseOptions* options() const;

  /// @brief Get the AstNode containing the temporary variable
  AstNode* getTemporaryRefNode() const;

  /// @brief Get the temporary variable
  Variable const* getTemporaryVariable() const;

  /// @brief Add a traverser engine Running on a DBServer to this node.
  ///        The block will communicate with them (CLUSTER ONLY)
  void addEngine(traverser::TraverserEngineID const&, ServerID const&);

  /// @brief enhance the TraversalEngine with all necessary info
  ///        to be send to DBServers (CLUSTER ONLY)
  void enhanceEngineInfo(arangodb::velocypack::Builder&) const;

  /// @brief Returns a reference to the engines. (CLUSTER ONLY)
  std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines() const;

  std::vector<std::unique_ptr<aql::Collection>> const& edgeColls() const {
    return _edgeColls;
  }

  std::vector<std::unique_ptr<aql::Collection>> const& vertexColls() const {
    return _vertexColls;
  }

  virtual void getConditionVariables(std::vector<Variable const*>&) const;

  /// @brief return any of the collections
  Collection const* collection() const;

 private:
  void addEdgeCollection(std::string const& n, TRI_edge_direction_e dir);

 protected:
  /// @brief the database
  TRI_vocbase_t* _vocbase;

  /// @brief vertex output variable
  Variable const* _vertexOutVariable;

  /// @brief vertex output variable
  Variable const* _edgeOutVariable;

  /// @brief our graph...
  graph::Graph const* _graphObj;

  /// @brief Temporary pseudo variable for the currently traversed object.
  Variable const* _tmpObjVariable;

  /// @brief Reference to the pseudo variable
  AstNode* _tmpObjVarNode;

  /// @brief Pseudo string value node to hold the last visited vertex id.
  AstNode* _tmpIdNode;

  /// @brief input graphInfo only used for serialization & info
  arangodb::velocypack::Builder _graphInfo;

  /// @brief the edge collection names
  std::vector<std::unique_ptr<aql::Collection>> _edgeColls;

  /// @brief the vertex collection names
  std::vector<std::unique_ptr<aql::Collection>> _vertexColls;

  /// @brief The directions edges are followed
  std::vector<TRI_edge_direction_e> _directions;

  /// @brief Options for traversals
  std::unique_ptr<graph::BaseOptions> _options;

  /// @brief Pseudo string value node to hold the last visited vertex id.
  /// @brief Flag if the options have been build.
  /// Afterwards this class is not copyable anymore.
  bool _optionsBuilt;

  /// @brief The list of traverser engines grouped by server.
  std::unordered_map<ServerID, traverser::TraverserEngineID> _engines;

  /// @brief flag, if graph is smart (enterprise edition only!)
  bool _isSmart;
};

}  // namespace aql
}  // namespace arangodb
#endif
