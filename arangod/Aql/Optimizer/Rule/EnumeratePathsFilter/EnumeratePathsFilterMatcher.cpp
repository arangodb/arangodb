////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "EnumeratePathsFilterMatcher.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumeratePathsNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Basics/overload.h"
#include "Logger/LogMacros.h"

#include <fmt/format.h>

#include <optional>
#include <string>

using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;
#define LOG_ENUMERATE_PATHS_OPTIMIZER_RULE LOG_DEVEL_IF(false)

namespace {

struct Match {
  AstNode const* map;
  AstNode const* attributeAccess;
  AstNode const* rhs;
  std::string accessedAttribute;
};

// Currently supported conditions are of the form
//
//  pathVariable.vertices[* RETURN ...] ALL == $literal_value
//
// or
//
//  pathVariable.edges[* RETURN ...] ALL == $literal_value
//
// such that the map in RETURN does not access *any* variables
// in the environment. This can be extended to variables that are
// valid in the path enumeration i.e. calculated before the enumeration
// is run.
//
// This function either returns std::nullopt, if it finds that the expression
// does not have the correct shape, or the `Match` struct which contains
// the *map*, which is the (RETURN ...) expression, the *rhs* which is the
// right hand side of the `ALL ==` operator, and the accessedAttribute, which
// can only be `vertices` or `edges`;
//
// The match function is not intended to check which variables are accessed
// inside `map`.
auto matchExpression(Ast* ast, AstNode const* expression,
                     Variable const* pathVar) -> std::optional<Match> {
  if (expression->type != NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE
        << fmt::format("iterating andNode, bailing not binary eq, but {}",
                       expression->getTypeString());
    return std::nullopt;
  }

  auto quantifier = expression->getMemberUnchecked(2);
  if ((quantifier == nullptr) or (not Quantifier::isAll(quantifier))) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
        "iterating andNode, bailing binary eq quantifier not ALL, but {}",
        quantifier->getIntValue(true));
    return std::nullopt;
  }

  auto lhs = expression->getMemberUnchecked(0);
  if (lhs->type != NODE_TYPE_EXPANSION) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE
        << fmt::format("iterating andNode, bailing lhs not EXPANSION");
    return std::nullopt;
  }
  if (lhs->getMemberUnchecked(2)->type != NODE_TYPE_NOP) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE
        << fmt::format("iterating andNode, bailing lhs member 2 not NOP");
    return std::nullopt;
  }
  if (lhs->getMemberUnchecked(3)->type != NODE_TYPE_NOP) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE
        << fmt::format("iterating andNode, bailing lhs member 3 not NOP");
    return std::nullopt;
  }

  if (lhs->getMemberUnchecked(1)->type != NODE_TYPE_ATTRIBUTE_ACCESS &&
      lhs->getMemberUnchecked(4)->type == NODE_TYPE_NOP) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
        "iterating andNode, bailing lhs member 1 not ATTRIBUTE_ACCESS");
    return std::nullopt;
  }

  auto map = lhs->getMemberUnchecked(4)->clone(ast);

  auto rhsValue = expression->getMemberUnchecked(1);
  if (rhsValue->type != NODE_TYPE_VALUE) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE
        << fmt::format("iterating andNode, bailing rhs not a Value, but a {}",
                       rhsValue->getTypeString());
    return std::nullopt;
  }

  auto iterator = lhs->getMemberUnchecked(0);
  if (iterator->type != NODE_TYPE_ITERATOR) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
        "iterating andNode, bailing lhs member 0 not an iterator, but a "
        "{}",
        rhsValue->getTypeString());
    return std::nullopt;
  }
  auto current = iterator->getMemberUnchecked(0);
  if (current->type != NODE_TYPE_VARIABLE) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
        "iterating andNode, bailing iterator member 0 not a variable, "
        "but a "
        "{}",
        rhsValue->getTypeString());
    return std::nullopt;
  }

  auto attributeAccess = iterator->getMemberUnchecked(1);
  if (attributeAccess->type != NODE_TYPE_ATTRIBUTE_ACCESS) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
        "iterating andNode, bailing iterator member 1 not an attribute "
        "access, but a {}",
        rhsValue->getTypeString());
    return std::nullopt;
  }

  if (!attributeAccess->isAttributeAccessForVariable(pathVar, true)) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
        "iterating andNode, bailing attribute access is not accessing "
        "the path variable");
    return std::nullopt;
  }

  auto accessedAttribute = attributeAccess->getStringView();
  if (not(accessedAttribute == "vertices" or accessedAttribute == "edges")) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
        "iterating andNode, bailing iterator member accessed attribute "
        "is {}"
        " not `vertices` or `edges`",
        accessedAttribute);
    return std::nullopt;
  }

  return Match{.map = map,
               .attributeAccess = lhs->getMemberUnchecked(1),
               .rhs = rhsValue,
               .accessedAttribute = std::string{accessedAttribute}};
}

// Assemble a filter that can be applied to a single vertex or edge.
// the *map* extracted in the matchExpression function is a function
// that has to evaluate equal to the *rhs* for *all* vertices/edges on
// a path.
//
// This function replaces the references to the variable CURRENT by
// the temporary variable used by the EnumeratePathsNode to evaluate
// vertex/edge conditions, and assembles a new expression with one
// "free" variable (the temporary) evaluating map == rhs
//
// TODO: Use the Expression wrapper class?
auto assembleCondition(Ast* ast, AstNode* tmpVar,
                       AstNode const* attributeAccess, AstNode const* map,
                       AstNode const* rhs) -> AstNode const* {
  if (map->type != NODE_TYPE_NOP) {
    auto mapClone = map->clone(ast);
    auto lhs =
        Ast::traverseAndModify(mapClone, [&tmpVar](AstNode* node) -> AstNode* {
          if (node->type == NODE_TYPE_REFERENCE) {
            return tmpVar;
          }
          return node;
        });
    return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, lhs,
                                         rhs);
  }

  AstNode* access = attributeAccess->clone(ast);
  // inject tmpVar
  access->changeMember(0, tmpVar);
  return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, access,
                                       rhs);
}

auto processFilter(Ast* ast, EnumeratePathsNode* enumeratePathsNode,
                   AstNode const* expression) -> bool {
  // ENUMERATE_PATHS has one output variable `pathVar`,
  // the conditions that are supported are ones that
  // access pathVar.vertices[*] or pathVar.edges[*], or both
  auto const* pathVariable = &enumeratePathsNode->pathOutVariable();

  auto match = matchExpression(ast, expression, pathVariable);
  if (!match.has_value()) {
    return false;
  }

  /* check that the condition does not reference variables that are not valid
     inside the path enumeration */
  auto variablesReferenced = VarSet{};
  Ast::getReferencedVariables(match->map, variablesReferenced);

  auto const& variablesValid = enumeratePathsNode->getVarsValid();

  if (!std::includes(std::begin(variablesValid), std::end(variablesValid),
                     std::begin(variablesReferenced),
                     std::end(variablesReferenced))) {
    return false;
  }

  auto* tmpVar = enumeratePathsNode->getTemporaryRefNode();
  auto condition = assembleCondition(ast, tmpVar, match->attributeAccess,
                                     match->map, match->rhs);

  if (match->accessedAttribute == "vertices") {
    enumeratePathsNode->registerGlobalVertexCondition(condition);
    return true;
  } else if (match->accessedAttribute == "edges") {
    enumeratePathsNode->registerGlobalEdgeCondition(condition);
    return true;
  } else {
    TRI_ASSERT(false) << fmt::format(
        "only matching on `vertices` or `edges` is expected, found {}",
        match->accessedAttribute);
    return false;
  }
}

}  // namespace

namespace arangodb::aql {

EnumeratePathsFilterMatcher::EnumeratePathsFilterMatcher(ExecutionPlan* plan)
    : _plan(plan) {}

auto EnumeratePathsFilterMatcher::before(ExecutionNode* node) -> bool {
  if (!_filterNodes.empty() && !node->isDeterministic()) {
    // found a FILTER and something that is not deterministic is not safe to
    // optimize

    _filterConditions.clear();
    _filterNodes.clear();
    return true;
  }

  switch (node->getType()) {
    case EN::ENUMERATE_LIST:
    case EN::COLLECT:
    case EN::SCATTER:
    case EN::DISTRIBUTE:
    case EN::GATHER:
    case EN::REMOTE:
    case EN::SUBQUERY:
    case EN::INDEX:
    case EN::JOIN:
    case EN::RETURN:
    case EN::SORT:
    case EN::ENUMERATE_COLLECTION:
    case EN::LIMIT:
    case EN::SHORTEST_PATH:
    case EN::TRAVERSAL:
    case EN::ENUMERATE_IRESEARCH_VIEW:
    case EN::WINDOW: {
      // the above node types can safely be ignored for the purposes
      // of this optimizer
    } break;

    case EN::INSERT:
    case EN::REMOVE:
    case EN::REPLACE:
    case EN::UPDATE:
    case EN::UPSERT: {
      // modification invalidates the filter expression
      _filterConditions.clear();
      _filterNodes.clear();
    } break;

    case EN::SINGLETON:
    case EN::NORESULTS: {
      return true;
    } break;

    case EN::FILTER: {
      // A FILTER node just tests a variable for truth(iness?);
      // the condition that is used to filter is calculated in an
      // CALCULATION_NODE before
      // So below we pick up the calculation for the filter (keep
      // in mind that the plan is traversed bottom to top.
      auto filterNode = ExecutionNode::castTo<FilterNode const*>(node);
      _filterNodes.emplace(filterNode->inVariable()->id, node);
    } break;

    case EN::CALCULATION: {
      auto calcNode = ExecutionNode::castTo<CalculationNode const*>(node);
      Variable const* outVar = calcNode->outVariable();
      if (_filterNodes.contains(outVar->id)) {
        _filterConditions.emplace(outVar->id, calcNode->expression()->node());
      }
    } break;

    case EN::ENUMERATE_PATHS: {
      if (_filterConditions.empty()) {
        // No conditions, no optimize
        break;
      }

      auto pathsNode = ExecutionNode::castTo<EnumeratePathsNode*>(node);
      TRI_ASSERT(pathsNode != nullptr);

      for (auto&& [variable, condition] : _filterConditions) {
        auto success = processFilter(_plan->getAst(), pathsNode, condition);

        _appliedChange |= success;

        // If a change was applied, this means we put a filter condition
        // into a path enumeration, the corresponding filter node can be removed
        if (success) {
          auto filterNode = _filterNodes.at(variable);
          _plan->unlinkNode(filterNode, false);
        }
      }
    } break;
    default: {
      // TODO: should this maybe just prevent the optimiser rule to fire
      // instead of crashing?
      ADB_PROD_ASSERT(false)
          << fmt::format("Unsupported node type {}.", node->getTypeString());
    }
  }
  return false;
}

auto EnumeratePathsFilterMatcher::enterSubquery(ExecutionNode* node1,
                                                ExecutionNode* node2) -> bool {
  return false;
}

}  // namespace arangodb::aql
