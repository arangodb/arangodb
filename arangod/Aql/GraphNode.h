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

#include "Aql/Collection.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Graphs.h"
#include "Cluster/TraverserEngineRegistry.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/TraverserOptions.h"

/// NOTE: This Node is purely virtual and is used to unify graph parsing for
///       Traversal and ShortestPath node. It shall never be part of any plan
///       nor will their be a Block to implement it.

namespace arangodb {

namespace aql {

class GraphNode : public ExecutionNode {
 protected:
  /// @brief Constructor for a new node parsed from AQL
  GraphNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
            traverser::BaseTraverserOptions* options);

  /// @brief Deserializer for node from VPack
  GraphNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base);

  /// @brief Internal constructor to clone the node.
  GraphNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
            std::vector<std::unique_ptr<aql::Collection>> const& edgeColls,
            std::vector<std::unique_ptr<aql::Collection>> const& vertexColls,
            std::vector<TRI_edge_direction_e> const& directions,
            traverser::BaseTraverserOptions* options);

 public:
  virtual ~GraphNode() {}

  /// @brief flag if smart search can be used (Enterprise only)
  bool isSmart() const {
    return _isSmart;
  }

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


  std::vector<std::unique_ptr<aql::Collection>> const& edgeColls() const {
    return _edgeColls;
  }

  std::vector<std::unique_ptr<aql::Collection>> const& reverseEdgeColls() const {
    return _reverseEdgeColls;
  }

  std::vector<std::unique_ptr<aql::Collection>> const& vertexColls() const {
    return _vertexColls;
  }

  bool allDirectionsEqual() const;

  AstNode* getTemporaryRefNode() const;

  Variable const* getTemporaryVariable() const;
 
  void enhanceEngineInfo(arangodb::velocypack::Builder&) const;

  virtual traverser::BaseTraverserOptions* options() const = 0;

  virtual void getConditionVariables(std::vector<Variable const*>&) const {};

  /// @brief estimate the cost of this graph node
  double estimateCost(size_t& nrItems) const final;

  /// @brief Compute the traversal options containing the expressions
  ///        MUST! be called after optimization and before creation
  ///        of blocks.
  virtual void prepareOptions() = 0;

  /// @brief Add a traverser engine Running on a DBServer to this node.
  ///        The block will communicate with them (CLUSTER ONLY)
  void addEngine(traverser::TraverserEngineID const&, ServerID const&);
  
  /// @brief Returns a reference to the engines. (CLUSTER ONLY)
  std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines()
      const {
    TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
    return &_engines;
  }

 protected:

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Export to VelocyPack
  ////////////////////////////////////////////////////////////////////////////////

  void baseToVelocyPackHelper(arangodb::velocypack::Builder&, bool) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Helper function to parse all collection names / directions
  ////////////////////////////////////////////////////////////////////////////////

  virtual void addEdgeColl(std::string const& name, TRI_edge_direction_e dir);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Helper function to parse the graph and direction nodes
  ////////////////////////////////////////////////////////////////////////////////

  void parseGraphAstNodes(AstNode const* direction, AstNode const* graph);

  ////////////////////////////////////////////////////////////////////////////////
  /// SECTION Shared subclass variables
  ////////////////////////////////////////////////////////////////////////////////
 protected:
  /// @brief the database
  TRI_vocbase_t* _vocbase;

  /// @brief vertex output variable
  Variable const* _vertexOutVariable;

  /// @brief edge output variable
  Variable const* _edgeOutVariable;

  /// @brief input graphInfo only used for serialisation & info
  arangodb::velocypack::Builder _graphInfo;

  /// @brief The directions edges are followed
  std::vector<TRI_edge_direction_e> _directions;

  /// @brief the edge collections
  std::vector<std::unique_ptr<aql::Collection>> _edgeColls;

  /// @brief the reverse edge collections (ShortestPathOnly)
  std::vector<std::unique_ptr<aql::Collection>> _reverseEdgeColls;

  /// @brief the vertex collection names
  std::vector<std::unique_ptr<aql::Collection>> _vertexColls;

  /// @brief our graph...
  Graph const* _graphObj;

  /// @brief Temporary pseudo variable for the currently traversed object.
  Variable const* _tmpObjVariable;

  /// @brief Reference to the pseudo variable
  AstNode* _tmpObjVarNode;

  /// @brief Pseudo string value node to hold the last visted vertex id.
  AstNode* _tmpIdNode;

  /// @brief The hard coded condition on _from
  /// NOTE: Created by sub classes, as it differs for class
  AstNode* _fromCondition;

  /// @brief The hard coded condition on _to
  /// NOTE: Created by sub classes, as it differs for class
  AstNode* _toCondition;

  /// @brief Flag if options are already prepared. After
  ///        this flag was set the node cannot be cloned
  ///        any more.
  bool _optionsBuild;

  /// @brief The list of traverser engines grouped by server.
  std::unordered_map<ServerID, traverser::TraverserEngineID> _engines;

  /// @brief flag, if traversal is smart (enterprise edition only!)
  bool _isSmart;

  std::unique_ptr<traverser::BaseTraverserOptions> _options;

};
}
}
#endif
