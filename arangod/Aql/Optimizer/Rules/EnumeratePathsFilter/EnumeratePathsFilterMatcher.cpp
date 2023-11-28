////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////
#include "EnumeratePathsFilterMatcher.h"
#include <optional>
#include <ostream>
#include <variant>

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Expression.h"
#include "Aql/EnumeratePathsNode.h"

#include "Basics/ErrorT.h"
#include "Basics/overload.h"

using EN = arangodb::aql::ExecutionNode;
#define LOG_ENUMERATE_PATHS_OPTIMIZER_RULE LOG_DEVEL_IF(false)

namespace arangodb::aql {

EnumeratePathsFilterMatcher::EnumeratePathsFilterMatcher(ExecutionPlan* plan)
    : _plan(plan), _condition(std::make_unique<Condition>(plan->getAst())) {}

auto EnumeratePathsFilterMatcher::before(ExecutionNode* node) -> bool {
  if (!_condition->isEmpty() && !node->isDeterministic()) {
    // found a FILTER and something that is not deterministic is not safe to
    // optimize

    _filterVariables.clear();
    _condition = std::make_unique<Condition>(_plan->getAst());
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
      _condition = std::make_unique<Condition>(_plan->getAst());
      _filterVariables.clear();
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
      _filterVariables.emplace(
          ExecutionNode::castTo<FilterNode const*>(node)->inVariable()->id);
    } break;

    case EN::CALCULATION: {
      auto calcNode = ExecutionNode::castTo<CalculationNode const*>(node);
      Variable const* outVar = calcNode->outVariable();
      if (_filterVariables.find(outVar->id) != _filterVariables.end()) {
        // This calculationNode is directly part of a filter condition
        // So we have to iterate through it.
        _condition->andCombine(calcNode->expression()->node());
      }
    } break;

    case EN::ENUMERATE_PATHS: {
      if (_condition->isEmpty()) {
        // No condition, no optimize
        break;
      }

      auto* ast = _plan->getAst();

      // TODO: Check that this does what one believes; that is: rewrite
      // a boolean expression into DNF
      _condition->normalize();

      auto pathsNode = ExecutionNode::castTo<EnumeratePathsNode*>(node);
      TRI_ASSERT(pathsNode != nullptr);

      // ENUMERATE_PATHS has one output variable `pathVar`,
      // the conditions that are supported are ones that
      // access pathVar.vertices[*] or pathVar.edges[*], or both
      auto pathVar = &pathsNode->pathOutVariable();
      //      auto const& varsValidInPathEnumeration =
      //      pathsNode->getVarsValid();

      if (_condition->root()->type != NODE_TYPE_OPERATOR_NARY_OR) {
        LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
            "expect normalised condition root to be n-ary or, found {}",
            _condition->root()->getTypeString());
        return false;
      }
      if (_condition->root()->numMembers() != 1) {
        LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
            "expecting condition root to have precisely one member, found {}",
            _condition->root()->numMembers());
        return false;
      }
      auto andNode = _condition->root()->getMemberUnchecked(0);

      if (andNode->type != NODE_TYPE_OPERATOR_NARY_AND) {
        LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
            "expecting andNode to be of type n-ary and, found {}",
            andNode->getTypeString());
        return false;
      }

      // Every condition that is in andNode has to be of the form
      // pathVariable.vertices[* RETURN ...] ALL == $literal_value
      // or
      // pathVariable.edges[* RETURN ...] ALL == $literal_value
      for (auto i = size_t{0}; i < andNode->numMembers(); ++i) {
        auto member = andNode->getMemberUnchecked(i);
        if (member->type != NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ) {
          LOG_ENUMERATE_PATHS_OPTIMIZER_RULE
              << fmt::format("iterating andNode, bailing not binary eq, but {}",
                             member->getTypeString());
          continue;
        }
        if (Quantifier::isAll(member->getMemberUnchecked(3))) {
          LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
              "iterating andNode, bailing binary eq quantifier not ALL");
          continue;
        }

        auto lhs = member->getMemberUnchecked(0);
        if (lhs->type != NODE_TYPE_EXPANSION) {
          LOG_ENUMERATE_PATHS_OPTIMIZER_RULE
              << fmt::format("iterating andNode, bailing lhs not EXPANSION");
          continue;
        }
        if (lhs->getMemberUnchecked(2)->type != NODE_TYPE_NOP) {
          LOG_ENUMERATE_PATHS_OPTIMIZER_RULE
              << fmt::format("iterating andNode, bailing lhs member 2 not NOP");
          continue;
        }
        if (lhs->getMemberUnchecked(3)->type != NODE_TYPE_NOP) {
          LOG_ENUMERATE_PATHS_OPTIMIZER_RULE
              << fmt::format("iterating andNode, bailing lhs member 3 not NOP");
          continue;
        }
        auto map = lhs->getMemberUnchecked(4)->clone(ast);

        auto rhsValue = member->getMemberUnchecked(1);
        if (rhsValue->type != NODE_TYPE_VALUE) {
          LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
              "iterating andNode, bailing rhs not a Value, but a {}",
              rhsValue->getTypeString());
          continue;
        }

        auto iterator = lhs->getMemberUnchecked(0);
        if (iterator->type != NODE_TYPE_ITERATOR) {
          LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
              "iterating andNode, bailing lhs member 0 not an iterator, but a "
              "{}",
              rhsValue->getTypeString());
          continue;
        }
        auto current = iterator->getMemberUnchecked(0);
        if (current->type != NODE_TYPE_VARIABLE) {
          LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
              "iterating andNode, bailing iterator member 0 not a variable, "
              "but a "
              "{}",
              rhsValue->getTypeString());
          continue;
        }

        auto attributeAccess = iterator->getMemberUnchecked(1);
        if (attributeAccess->type != NODE_TYPE_ATTRIBUTE_ACCESS) {
          LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
              "iterating andNode, bailing iterator member 1 not an attribute "
              "access, but a {}",
              rhsValue->getTypeString());
          continue;
        }

        if (!attributeAccess->isAttributeAccessForVariable(pathVar, true)) {
          LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
              "iterating andNode, bailing attribute access is not accessing "
              "the path variable");
        }

        auto accessedAttribute = attributeAccess->getStringView();
        if (not(accessedAttribute == "vertices" or
                accessedAttribute == "edges")) {
          LOG_ENUMERATE_PATHS_OPTIMIZER_RULE << fmt::format(
              "iterating andNode, bailing iterator member accessed attribute "
              "is {}"
              " not `vertices` or `edges`",
              accessedAttribute);

          continue;
        }

        auto* tmpVar = pathsNode->getTemporaryRefNode();
        map = Ast::traverseAndModify(map, [&tmpVar](AstNode* node) -> AstNode* {
          if (node->type == NODE_TYPE_REFERENCE) {
            return tmpVar;
          }
          return node;
        });

        auto condition = ast->createNodeBinaryOperator(
            NODE_TYPE_OPERATOR_BINARY_EQ, map, rhsValue);

        if (accessedAttribute == "vertices") {
          pathsNode->registerGlobalVertexCondition(condition);
          _appliedChange = true;
        } else if (accessedAttribute == "edges") {
          pathsNode->registerGlobalEdgeCondition(condition);
          _appliedChange = true;
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
