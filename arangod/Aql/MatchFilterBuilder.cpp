////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "MatchFilterBuilder.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"

#include <memory>

namespace arangodb::aql {
namespace {

int directionFilterBits(MatchEdgeDirection direction) {
  switch (direction) {
    case MatchEdgeDirection::kInbound:
      return 1;
    case MatchEdgeDirection::kOutbound:
      return 2;
    case MatchEdgeDirection::kAny:
      return 3;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "invalid direction for match expression");
}

}  // namespace

MatchFilterBuilder::MatchFilterBuilder(ExecutionPlan& plan, Ast* ast)
    : _plan(plan), _ast(ast) {}

AstNode* MatchFilterBuilder::createPropertyAccess(Variable const* variable,
                                                  std::string_view property) {
  char const* registered = _ast->resources().registerString(property);
  return _ast->createNodeAttributeAccess(
      _ast->createNodeReference(variable),
      std::string_view(registered, property.size()));
}

std::tuple<CalculationNode*, FilterNode*>
MatchFilterBuilder::createPropertiesFilter(
    Variable const* variable,
    std::vector<MatchPropertyConstraint> const& properties,
    std::optional<MatchExpressionRef> const& additionalFilter,
    std::unordered_map<VariableId, Variable const*> const& subst) {
  AstNode* root = nullptr;
  if (additionalFilter.has_value()) {
    root = Ast::replaceVariables(const_cast<AstNode*>(additionalFilter->node),
                                 subst);
  }

  for (auto const& property : properties) {
    auto access = createPropertyAccess(variable, property.key);
    auto value =
        Ast::replaceVariables(const_cast<AstNode*>(property.value.node), subst);
    auto operatorEq = _ast->createNodeBinaryOperator(
        NODE_TYPE_OPERATOR_BINARY_EQ, access, value);
    if (root) {
      root = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, root,
                                            operatorEq);
    } else {
      root = operatorEq;
    }
  }

  if (root == nullptr) {
    root = _ast->createNodeValueBool(true);
  }

  Variable const* filterVar = _ast->variables()->createTemporaryVariable();
  CalculationNode* calc = _plan.createNode<CalculationNode>(
      &_plan, _plan.nextId(), std::make_unique<Expression>(_ast, root),
      filterVar);
  FilterNode* filter =
      _plan.createNode<FilterNode>(&_plan, _plan.nextId(), filterVar);
  filter->addDependency(calc);
  return std::make_tuple(calc, filter);
}

std::tuple<CalculationNode*, FilterNode*>
MatchFilterBuilder::createVertexEdgeFilter(Variable const* leftVertex,
                                           Variable const* edge,
                                           Variable const* rightVertex,
                                           MatchEdgeDirection direction) {
  AstNode* root = nullptr;
  int const bits = directionFilterBits(direction);

  if (bits & 2) {
    auto leftVertexId = createPropertyAccess(leftVertex, "_id");
    auto rightVertexId = createPropertyAccess(rightVertex, "_id");
    auto edgeFrom = createPropertyAccess(edge, "_from");
    auto edgeTo = createPropertyAccess(edge, "_to");
    auto first = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                leftVertexId, edgeFrom);
    auto second = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                 edgeTo, rightVertexId);
    root = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, first,
                                          second);
  }
  if (bits & 1) {
    auto leftVertexId = createPropertyAccess(leftVertex, "_id");
    auto rightVertexId = createPropertyAccess(rightVertex, "_id");
    auto edgeFrom = createPropertyAccess(edge, "_from");
    auto edgeTo = createPropertyAccess(edge, "_to");
    auto first = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                leftVertexId, edgeTo);
    auto second = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                 edgeFrom, rightVertexId);
    auto andNode = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND,
                                                  first, second);
    if (root) {
      root = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_OR, root,
                                            andNode);
    } else {
      root = andNode;
    }
  }

  Variable const* filterVar = _ast->variables()->createTemporaryVariable();
  CalculationNode* calc = _plan.createNode<CalculationNode>(
      &_plan, _plan.nextId(), std::make_unique<Expression>(_ast, root),
      filterVar);
  FilterNode* filter =
      _plan.createNode<FilterNode>(&_plan, _plan.nextId(), filterVar);
  filter->addDependency(calc);
  return std::make_tuple(calc, filter);
}

}  // namespace arangodb::aql
