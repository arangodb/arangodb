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

#include "EnumeratePathsFilterMatcher.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumeratePathsNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/Quantifier.h"
#include "Aql/TypedAstNodes.h"
#include "Basics/overload.h"
#include "Logger/LogMacros.h"

#include <format>
#include <optional>
#include <string>

namespace arangodb::aql {
using EN = ExecutionNode;
#define LOG_ENUMERATE_PATHS_OPTIMIZER_RULE LOG_DEVEL_IF(false)

namespace {

struct Match {
  AstNode const* map;
  AstNode const* attributeAccess;
  AstNode const* rhs;
  /// @brief expression inside `[* FILTER ...]`, or nullptr when absent
  AstNode const* inlineFilter;
  /// @brief CURRENT variable belonging to the matched expansion only
  Variable const* iteratorVar;
  AstNodeType comparisonType;
  std::string accessedAttribute;
};

/// @brief Replace references to the expansion's own CURRENT (`iteratorVar`)
/// with `tmpVar`. Nested expansions introduce a different CURRENT variable and
/// must remain untouched — matching is by Variable* identity, not by name.
AstNode* replaceIteratorReference(AstNode* node, Variable const* iteratorVar,
                                  AstNode* tmpVar) {
  auto func = [&](AstNode* n) -> AstNode* {
    if (n->type == NODE_TYPE_REFERENCE || n->type == NODE_TYPE_VARIABLE) {
      if (static_cast<Variable const*>(n->getData()) == iteratorVar) {
        return tmpVar;
      }
    }
    return n;
  };
  return Ast::traverseAndModify(node, func);
}

/// @brief Map ARRAY_* operators to the corresponding binary operator; for
/// NONE negate so the per-element implication stays `!B || cmp`.
AstNodeType buildSingleComparatorType(AstNode const* condition) {
  TRI_ASSERT(condition->numMembers() == 3);
  AstNodeType type = NODE_TYPE_ROOT;

  switch (condition->type) {
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
      type = NODE_TYPE_OPERATOR_BINARY_EQ;
      break;
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
      type = NODE_TYPE_OPERATOR_BINARY_NE;
      break;
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
      type = NODE_TYPE_OPERATOR_BINARY_LT;
      break;
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
      type = NODE_TYPE_OPERATOR_BINARY_LE;
      break;
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
      type = NODE_TYPE_OPERATOR_BINARY_GT;
      break;
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
      type = NODE_TYPE_OPERATOR_BINARY_GE;
      break;
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
      type = NODE_TYPE_OPERATOR_BINARY_IN;
      break;
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
      type = NODE_TYPE_OPERATOR_BINARY_NIN;
      break;
    default:
      TRI_ASSERT(false) << "unsupported array operator type";
      return NODE_TYPE_ROOT;
  }
  auto quantifier = condition->getMemberUnchecked(2);
  TRI_ASSERT(quantifier->type == NODE_TYPE_QUANTIFIER);
  TRI_ASSERT(!Quantifier::isAny(quantifier));
  if (Quantifier::isNone(quantifier)) {
    type = Ast::negateOperator(type);
  }
  return type;
}

/// @brief Lightweight check that an inline FILTER may be pulled into the path
/// enumeration. Mirrors TraversalConditionFinder::isSupportedInlineFilter:
/// plain `[* FILTER ...]` only, no path-variable access, no nested ARRAY_FILTER
/// quantifier. Nested expansions are allowed; their CURRENT stays distinct.
bool isSupportedInlineFilter(Variable const* pathVar, AstNode const* filter) {
  ast::ArrayFilterNode arrayFilter{filter};
  if (arrayFilter.getQuantifier()->type != NODE_TYPE_NOP) {
    return false;
  }
  bool supported = true;
  Ast::traverseReadOnly(
      arrayFilter.getFilter(),
      [&](AstNode const* n) -> bool {
        if (!supported) {
          return false;
        }
        // Filter is evaluated per edge/vertex and must not look at the path.
        if ((n->type == NODE_TYPE_REFERENCE || n->type == NODE_TYPE_VARIABLE) &&
            static_cast<Variable const*>(n->getData()) == pathVar) {
          supported = false;
          return false;
        }
        if (n->type == NODE_TYPE_FCALL_USER) {
          supported = false;
          return false;
        }
        if (n->type == NODE_TYPE_FCALL) {
          auto* func = static_cast<Function const*>(n->getData());
          if (!func->hasFlag(Function::Flags::Deterministic)) {
            supported = false;
            return false;
          }
        }
        return true;
      },
      [](AstNode const*) {});
  return supported;
}

// Currently supported conditions are of the form
//
//  pathVariable.vertices[* ...] ALL|NONE op $literal_value
//  pathVariable.edges[* ...] ALL|NONE op $literal_value
//
// Optionally with an inline FILTER on the expansion:
//
//  pathVariable.edges[* FILTER B(CURRENT)].attr ALL op y
//
// which becomes the per-element condition `!B(edge) || edge.attr op y`
// (for NONE the comparator is negated first).
//
// The map in RETURN / attribute access after the expansion must not access
// variables that are invalid inside the path enumeration.
auto matchExpression(Ast* ast, AstNode const* expression,
                     Variable const* pathVar) -> std::optional<Match> {
  switch (expression->type) {
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
      break;
    default:
      LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << std::format(
          "iterating andNode, bailing not binary array op, but {}",
          expression->getTypeString());
      return std::nullopt;
  }

  auto quantifier = expression->getMemberUnchecked(2);
  if (quantifier == nullptr || quantifier->type != NODE_TYPE_QUANTIFIER ||
      Quantifier::isAny(quantifier) || Quantifier::isAtLeast(quantifier)) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE
        << "iterating andNode, bailing quantifier not ALL/NONE";
    return std::nullopt;
  }

  auto lhs = expression->getMemberUnchecked(0);
  if (lhs->type != NODE_TYPE_EXPANSION) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE
        << std::format("iterating andNode, bailing lhs not EXPANSION");
    return std::nullopt;
  }

  ast::ExpansionNode expansion{lhs};

  // Inline LIMIT is unsupported (same as before).
  if (expansion.getLimit()->type != NODE_TYPE_NOP) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << std::format(
        "iterating andNode, bailing lhs member 3 (LIMIT) not NOP");
    return std::nullopt;
  }

  AstNode const* inlineFilter = nullptr;
  AstNode const* filterMember = expansion.getFilter();
  if (filterMember->type == NODE_TYPE_ARRAY_FILTER) {
    // Validate the FILTER as a whole here and do not recurse into it for
    // path-pattern matching — nested expansions inside the FILTER must not be
    // mistaken for the path access being optimized (COR-918).
    if (!isSupportedInlineFilter(pathVar, filterMember)) {
      LOG_ENUMERATE_PATHS_OPTIMIZER_RULE
          << "iterating andNode, bailing unsupported inline FILTER";
      return std::nullopt;
    }
    inlineFilter = ast::ArrayFilterNode{filterMember}.getFilter();
  } else if (filterMember->type != NODE_TYPE_NOP) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << std::format(
        "iterating andNode, bailing lhs member 2 not NOP/FILTER");
    return std::nullopt;
  }

  if (expansion.getExpression()->type != NODE_TYPE_ATTRIBUTE_ACCESS &&
      expansion.getProjection()->type == NODE_TYPE_NOP) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << std::format(
        "iterating andNode, bailing lhs member 1 not ATTRIBUTE_ACCESS");
    return std::nullopt;
  }

  auto map = expansion.getProjection()->clone(ast);

  auto rhsValue = expression->getMemberUnchecked(1);
  if (rhsValue->type != NODE_TYPE_VALUE && rhsValue->type != NODE_TYPE_ARRAY &&
      rhsValue->type != NODE_TYPE_OBJECT) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << std::format(
        "iterating andNode, bailing rhs not a constant, but a {}",
        rhsValue->getTypeString());
    return std::nullopt;
  }

  auto iterator = expansion.getIterator();
  Variable const* current = iterator.getVariable();

  auto attributeAccess = iterator.getExpression();
  if (attributeAccess->type != NODE_TYPE_ATTRIBUTE_ACCESS) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << std::format(
        "iterating andNode, bailing iterator member 1 not an attribute "
        "access, but a {}",
        attributeAccess->getTypeString());
    return std::nullopt;
  }

  if (!attributeAccess->isAttributeAccessForVariable(pathVar, true)) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << std::format(
        "iterating andNode, bailing attribute access is not accessing "
        "the path variable");
    return std::nullopt;
  }

  auto accessedAttribute = attributeAccess->getStringView();
  if (not(accessedAttribute == "vertices" or accessedAttribute == "edges")) {
    LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << std::format(
        "iterating andNode, bailing iterator member accessed attribute "
        "is {}"
        " not `vertices` or `edges`",
        accessedAttribute);
    return std::nullopt;
  }

  AstNodeType comparisonType = buildSingleComparatorType(expression);
  if (comparisonType == NODE_TYPE_ROOT) {
    return std::nullopt;
  }

  return Match{.map = map,
               .attributeAccess = expansion.getExpression(),
               .rhs = rhsValue,
               .inlineFilter = inlineFilter,
               .iteratorVar = current,
               .comparisonType = comparisonType,
               .accessedAttribute = std::string{accessedAttribute}};
}

// Assemble a filter that can be applied to a single vertex or edge.
// Without an inline FILTER this is `map(tmp) op rhs` (or attribute access).
// With an inline FILTER B, this is `!B(tmp) || (map(tmp) op rhs)`.
auto assembleCondition(Ast* ast, AstNode* tmpVar, Match const& match)
    -> AstNode const* {
  AstNode* comparison = nullptr;
  if (match.map->type != NODE_TYPE_NOP) {
    auto mapClone = match.map->clone(ast);
    auto lhs = replaceIteratorReference(mapClone, match.iteratorVar, tmpVar);
    comparison =
        ast->createNodeBinaryOperator(match.comparisonType, lhs, match.rhs);
  } else {
    AstNode* access = match.attributeAccess->clone(ast);
    // inject tmpVar as the base of the attribute access (replacing CURRENT)
    access->changeMember(0, tmpVar);
    comparison =
        ast->createNodeBinaryOperator(match.comparisonType, access, match.rhs);
  }

  if (match.inlineFilter == nullptr) {
    return comparison;
  }

  // p.edges[* FILTER B(CURRENT)].attr ALL|NONE op y
  //   =>  !B(edge) || edge.attr op y   (NONE: comparator already negated)
  AstNode* filterExpression = replaceIteratorReference(
      match.inlineFilter->clone(ast), match.iteratorVar, tmpVar);
  return ast->createNodeBinaryOperator(
      NODE_TYPE_OPERATOR_BINARY_OR,
      ast->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT,
                                   filterExpression),
      comparison);
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
  if (match->inlineFilter != nullptr) {
    Ast::getReferencedVariables(match->inlineFilter, variablesReferenced);
  }
  // The expansion's CURRENT is replaced with the temporary and must not be
  // treated as an external dependency of the pushed condition.
  variablesReferenced.erase(match->iteratorVar);

  auto const& variablesValid = enumeratePathsNode->getVarsValid();

  if (!std::includes(std::begin(variablesValid), std::end(variablesValid),
                     std::begin(variablesReferenced),
                     std::end(variablesReferenced))) {
    return false;
  }

  auto* tmpVar = enumeratePathsNode->getTemporaryRefNode();
  auto condition = assembleCondition(ast, tmpVar, *match);

  if (match->accessedAttribute == "vertices") {
    enumeratePathsNode->registerGlobalVertexCondition(condition);
    return true;
  } else if (match->accessedAttribute == "edges") {
    enumeratePathsNode->registerGlobalEdgeCondition(condition);
    return true;
  } else {
    TRI_ASSERT(false) << std::format(
        "only matching on `vertices` or `edges` is expected, found {}",
        match->accessedAttribute);
    return false;
  }
}

}  // namespace

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
          << std::format("Unsupported node type {}.", node->getTypeString());
    }
  }
  return false;
}

auto EnumeratePathsFilterMatcher::enterSubquery(ExecutionNode* node1,
                                                ExecutionNode* node2) -> bool {
  return false;
}

}  // namespace arangodb::aql
