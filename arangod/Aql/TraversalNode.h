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
#include "Aql/Condition.h"
#include "Aql/Graphs.h"
#include "VocBase/Traverser.h"

namespace arangodb {
namespace aql {

class SimpleTraverserExpression
    : public arangodb::traverser::TraverserExpression {
 public:
  arangodb::aql::AstNode* compareToNode;

  arangodb::aql::Expression* expression;

  SimpleTraverserExpression(bool isEdgeAccess,
                            arangodb::aql::AstNodeType comparisonType,
                            arangodb::aql::AstNode const* varAccess,
                            arangodb::aql::AstNode* compareToNode)
      : arangodb::traverser::TraverserExpression(isEdgeAccess, comparisonType,
                                                 varAccess),
        compareToNode(compareToNode),
        expression(nullptr) {}

  SimpleTraverserExpression(arangodb::aql::Ast* ast, arangodb::basics::Json const& j);

  ~SimpleTraverserExpression();

  void toJson(arangodb::basics::Json& json, TRI_memory_zone_t* zone) const;

  void toVelocyPack(arangodb::velocypack::Builder&) const;
};

/// @brief class TraversalNode
class TraversalNode : public ExecutionNode {
  friend class ExecutionBlock;
  friend class TraversalCollectionBlock;

  /// @brief constructor with a vocbase and a collection name
 public:
  TraversalNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
                AstNode const* direction, AstNode const* start,
                AstNode const* graph);

  TraversalNode(ExecutionPlan* plan, arangodb::basics::Json const& base);

  ~TraversalNode() {
    delete _condition;

    for (auto& it : _expressions) {
      for (auto& it2 : it.second) {
        delete it2;
      }
    }
  }

  /// @brief Internal constructor to clone the node.
 private:
  TraversalNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
                std::vector<std::string> const& edgeColls,
                Variable const* inVariable, std::string const& vertexId,
                std::vector<TRI_edge_direction_e> directions, uint64_t minDepth,
                uint64_t maxDepth);

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
      result.emplace_back(condVar);
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
      result.emplace(condVar);
    }
    if (usesInVariable()) {
      result.emplace(_inVariable);
    }
  }

  /// @brief getVariablesSetHere
  std::vector<Variable const*> getVariablesSetHere() const override final {
    std::vector<Variable const*> vars;
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

  /// @brief Fill the traversal options with all values known to this node or
  ///        with default values.
  void fillTraversalOptions(arangodb::traverser::TraverserOptions& opts) const;

  std::vector<std::string> const edgeColls() const { return _edgeColls; }

  /// @brief remember the condition to execute for early traversal abortion.
  void setCondition(Condition* condition);

  /// @brief return the condition for the node
  Condition* condition() const { return _condition; }

  /// @brief which variable? -1 none, 0 Edge, 1 Vertex, 2 path
  int checkIsOutVariable(size_t variableId) const;

  /// @brief check whether an access is inside the specified range
  bool isInRange(uint64_t thisIndex, bool isEdge) const {
    if (isEdge) {
      return (thisIndex < _maxDepth);
    }
    return (thisIndex <= _maxDepth);
  }

  /// @brief check whecher min..max actualy span a range
  bool isRangeValid() const { return _maxDepth >= _minDepth; }

  /// @brief Remember a simple comparator filter
  void storeSimpleExpression(bool isEdgeAccess, size_t indexAccess,
                             AstNodeType comparisonType,
                             AstNode const* varAccess, AstNode* compareToNode);

  /// @brief Returns a regerence to the simple traverser expressions
  std::unordered_map<
      size_t, std::vector<arangodb::traverser::TraverserExpression*>> const*
  expressions() const {
    return &_expressions;
  }

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

  /// @brief input graphJson only used for serialisation & info
  arangodb::basics::Json _graphJson;

  /// @brief The minimal depth included in the result
  uint64_t _minDepth;

  /// @brief The maximal depth searched and included in the result
  uint64_t _maxDepth;

  /// @brief The directions edges are followed
  std::vector<TRI_edge_direction_e> _directions;

  /// @brief the edge collection cid
  std::vector<std::string> _edgeColls;

  /// @brief our graph...
  Graph const* _graphObj;

  /// @brief early abort traversal conditions:
  Condition* _condition;

  /// @brief variables that are inside of the condition
  std::vector<Variable const*> _conditionVariables;

  /// @brief store a simple comparator filter
  /// one vector of TraverserExpressions per matchdepth (size_t)
  std::unordered_map<size_t,
                     std::vector<arangodb::traverser::TraverserExpression*>>
      _expressions;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
