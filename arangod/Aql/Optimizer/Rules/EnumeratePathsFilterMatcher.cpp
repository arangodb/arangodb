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
#include <variant>

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Expression.h"
#include "Aql/EnumeratePathsNode.h"
#include "Aql/Quantifier.h"
#include "Aql/Optimizer/Rules/ExpressionMatcher.h"

#include "Basics/ErrorT.h"
#include "Basics/overload.h"

using EN = arangodb::aql::ExecutionNode;

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

      auto pathsNode = ExecutionNode::castTo<EnumeratePathsNode*>(node);
      TRI_ASSERT(pathsNode != nullptr);

      // ENUMERATE_PATHS has one output variable `p`,
      // the conditions that are supported are ones that
      // access p.vertices[*] or p.edges[*], or both
      auto pathVar = &pathsNode->pathOutVariable();
      auto const& varsValidInPathEnumeration = pathsNode->getVarsValid();

      (void)pathVar;
      (void)varsValidInPathEnumeration;
      /*      auto matches = matchSupportedConditions(
          pathVar, varsValidInPathEnumeration, _condition);
      if (not matches.empty()) {
        LOG_DEVEL << "OPTIMIZER RULE FIRES";
        }*/

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
