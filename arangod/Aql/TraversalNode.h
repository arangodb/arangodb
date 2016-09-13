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

#ifndef ARANGOD_AQL_TRAVERSAL_NODE_H
#define ARANGOD_AQL_TRAVERSAL_NODE_H 1

#include "Aql/ExecutionNode.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/Graphs.h"
#include "Cluster/TraverserEngineRegistry.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/TraverserOptions.h"

namespace arangodb {
namespace aql {

/// @brief class TraversalNode
class TraversalNode : public ExecutionNode {

  class EdgeConditionBuilder {
    private:

      /// @brief reference to the outer traversal node
      TraversalNode const* _tn;

      /// @brief the conditions modifying the edge on this depth
      AstNode* _modCondition;

      /// @brief indicator if we have attached the _from or _to condition to _modCondition
      bool _containsCondition;

    public:
     explicit EdgeConditionBuilder(TraversalNode const*);

     EdgeConditionBuilder(TraversalNode const*, arangodb::velocypack::Slice const&);

     EdgeConditionBuilder(TraversalNode const*, EdgeConditionBuilder const*);

     ~EdgeConditionBuilder() {}

     EdgeConditionBuilder(EdgeConditionBuilder const&) = delete;

     EdgeConditionBuilder(EdgeConditionBuilder&&) = delete;

     void addConditionPart(AstNode const*);

      AstNode* getOutboundCondition();

      AstNode* getInboundCondition();

      void toVelocyPack(arangodb::velocypack::Builder&, bool) const;
  };

  friend class ExecutionBlock;
  friend class TraversalBlock;
  friend class RedundantCalculationsReplacer;

  /// @brief constructor with a vocbase and a collection name
 public:
  TraversalNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
                AstNode const* direction, AstNode const* start,
                AstNode const* graph,
                std::unique_ptr<traverser::TraverserOptions>& options);

  TraversalNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base);

  ~TraversalNode();

  /// @brief Internal constructor to clone the node.
 private:
  TraversalNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
                std::vector<std::unique_ptr<aql::Collection>> const& edgeColls,
                std::vector<std::unique_ptr<aql::Collection>> const& vertexColls,
                Variable const* inVariable, std::string const& vertexId,
                std::vector<TRI_edge_direction_e> const& directions,
                std::unique_ptr<traverser::TraverserOptions>& options);

 public:
  /// @brief return the type of the node
  NodeType getType() const override final { return TRAVERSAL; }

  /// @brief export to VelocyPack
  void toVelocyPackHelper(arangodb::velocypack::Builder&,
                          bool) const override final;

  /// @brief clone ExecutionNode recursively
  ExecutionNode* clone(ExecutionPlan* plan, bool withDependencies,
                       bool withProperties) const override final;

  /// @brief the cost of a traversal node
  double estimateCost(size_t&) const override final;

  /// @brief Test if this node uses an in variable or constant
  bool usesInVariable() const { return _inVariable != nullptr; }

  /// @brief getVariablesUsedHere
  std::vector<Variable const*> getVariablesUsedHere() const override final {
    std::vector<Variable const*> result;
    for (auto const& condVar : _conditionVariables) {
      if (condVar != _tmpObjVariable) {
        result.emplace_back(condVar);
      }
    }
    if (usesInVariable()) {
      result.emplace_back(_inVariable);
    }
    return result;
  }

  /// @brief getVariablesUsedHere
  void getVariablesUsedHere(
      std::unordered_set<Variable const*>& result) const override final {
    for (auto const& condVar : _conditionVariables) {
      if (condVar != _tmpObjVariable) {
        result.emplace(condVar);
      }
    }
    if (usesInVariable()) {
      result.emplace(_inVariable);
    }
  }

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final {
    std::vector<Variable const*> vars;
    
    size_t const numVars = 
      (_vertexOutVariable != nullptr ? 1 : 0) + 
      (_edgeOutVariable != nullptr ? 1 : 0) + 
      (_pathOutVariable != nullptr ? 1 : 0);

    vars.reserve(numVars);
    
    if (_vertexOutVariable != nullptr) {
      vars.emplace_back(_vertexOutVariable);
    }
    if (_edgeOutVariable != nullptr) {
      vars.emplace_back(_edgeOutVariable);
    }
    if (_pathOutVariable != nullptr) {
      vars.emplace_back(_pathOutVariable);
    }
    return vars;
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

  /// @brief checks if the path out variable is used
  bool usesPathOutVariable() const { return _pathOutVariable != nullptr; }

  /// @brief return the path out variable
  Variable const* pathOutVariable() const { return _pathOutVariable; }

  /// @brief set the path out variable
  void setPathOutput(Variable const* outVar) { _pathOutVariable = outVar; }

  /// @brief return the in variable
  Variable const* inVariable() const { return _inVariable; }

  std::string const getStartVertex() const { return _vertexId; }

  std::vector<std::unique_ptr<aql::Collection>> const& edgeColls() const {
    return _edgeColls;
  }

  std::vector<std::unique_ptr<aql::Collection>> const& vertexColls() const {
    return _vertexColls;
  }

  /// @brief remember the condition to execute for early traversal abortion.
  void setCondition(Condition* condition);

  /// @brief return the condition for the node
  Condition* condition() const { return _condition; }

  /// @brief which variable? -1 none, 0 Edge, 1 Vertex, 2 path
  int checkIsOutVariable(size_t variableId) const;

  /// @brief check whether an access is inside the specified range
  bool isInRange(uint64_t,  bool) const;

  /// @brief register a filter condition on a given search depth.
  ///        If this condition is not fulfilled a traversal will abort.
  ///        The condition will contain the local variable for it's accesses.
  void registerCondition(bool, size_t, AstNode const*);

  /// @brief register a filter condition for all search depths
  ///        If this condition is not fulfilled a traversal will abort.
  ///        The condition will contain the local variable for it's accesses.
  void registerGlobalCondition(bool, AstNode const*);

  bool allDirectionsEqual() const;

  traverser::TraverserOptions* options() const;

  AstNode* getTemporaryRefNode() const;

  Variable const* getTemporaryVariable() const;

  void getConditionVariables(std::vector<Variable const*>&) const;

  void enhanceEngineInfo(arangodb::velocypack::Builder&) const;

  /// @brief Compute the traversal options containing the expressions
  ///        MUST! be called after optimization and before creation
  ///        of blocks.
  void prepareOptions();

  /// @brief Add a traverser engine Running on a DBServer to this node.
  ///        The block will communicate with them (CLUSTER ONLY)
  void addEngine(traverser::TraverserEngineID const&, ServerID const&);

  
  /// @brief Returns a reference to the engines. (CLUSTER ONLY)
  std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines()
      const {
    TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
    return &_engines;
  }

 private:

#ifdef TRI_ENABLE_MAINTAINER_MODE
  void checkConditionsDefined() const;
#endif


 private:

  /// @brief the database
  TRI_vocbase_t* _vocbase;

  /// @brief vertex output variable
  Variable const* _vertexOutVariable;

  /// @brief vertex output variable
  Variable const* _edgeOutVariable;

  /// @brief vertex output variable
  Variable const* _pathOutVariable;

  /// @brief input variable only used if _vertexId is unused
  Variable const* _inVariable;

  /// @brief input vertexId only used if _inVariable is unused
  std::string _vertexId;

  /// @brief input graphInfo only used for serialization & info
  arangodb::velocypack::Builder _graphInfo;

  /// @brief The directions edges are followed
  std::vector<TRI_edge_direction_e> _directions;

  /// @brief the edge collection names
  std::vector<std::unique_ptr<aql::Collection>> _edgeColls;

  /// @brief the vertex collection names
  std::vector<std::unique_ptr<aql::Collection>> _vertexColls;

  /// @brief our graph
  Graph const* _graphObj;

  /// @brief early abort traversal conditions:
  Condition* _condition;

  /// @brief variables that are inside of the condition
  std::unordered_set<Variable const*> _conditionVariables;

  /// @brief Options for traversals
  std::unique_ptr<traverser::TraverserOptions> _options;

  /// @brief Temporary pseudo variable for the currently traversed object.
  Variable const* _tmpObjVariable;

  /// @brief Reference to the pseudo variable
  AstNode* _tmpObjVarNode;

  /// @brief Pseudo string value node to hold the last visted vertex id.
  AstNode* _tmpIdNode;

  /// @brief The hard coded condition on _from
  AstNode* _fromCondition;

  /// @brief The hard coded condition on _to
  AstNode* _toCondition;

  /// @brief The global edge condition. Does not contain
  ///        _from and _to checks
  std::vector<AstNode const*> _globalEdgeConditions;

  /// @brief The global vertex condition
  std::vector<AstNode const*> _globalVertexConditions;

  /// @brief List of all depth specific conditions for edges
  std::unordered_map<size_t, std::unique_ptr<EdgeConditionBuilder>> _edgeConditions;

  /// @brief List of all depth specific conditions for vertices
  std::unordered_map<size_t, AstNode*> _vertexConditions;

  /// @brief Flag if options are already prepared. After
  ///        this flag was set the node cannot be cloned
  ///        any more.
  bool _optionsBuild;

  /// @brief The list of traverser engines grouped by server.
  std::unordered_map<ServerID, traverser::TraverserEngineID> _engines;

};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
