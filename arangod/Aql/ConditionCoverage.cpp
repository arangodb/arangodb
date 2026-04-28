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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ConditionCoverage.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Functions.h"
#include "Aql/Variable.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/debugging.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"

namespace arangodb::aql {
namespace {

/// @brief checks if the current condition is covered by the other
bool canRemove(ExecutionPlan const* plan, ConditionPart const& me,
               AstNode const* andNode, bool allowArrayExpansion) {
  TRI_ASSERT(andNode != nullptr);
  TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);

  std::pair<Variable const*, std::vector<basics::AttributeName>> result;

  size_t const n = andNode->numMembers();

  auto normalize = [plan](AstNode const* node) -> std::string {
    if (node->type == NODE_TYPE_REFERENCE) {
      auto setter =
          plan->getVarSetBy(static_cast<Variable const*>(node->getData())->id);
      if (setter != nullptr &&
          setter->getType() == ExecutionNode::CALCULATION) {
        auto cn = ExecutionNode::castTo<CalculationNode const*>(setter);
        // use expression node instead
        node = cn->expression()->node();
      }
    }
    // return string representation
    return node->toString();
  };

  try {
    std::string attrName;
    for (size_t i = 0; i < n; ++i) {
      auto operand = andNode->getMemberUnchecked(i);

      if (operand->isComparisonOperator() ||
          (allowArrayExpansion && operand->isArrayComparisonOperator())) {
        auto lhs = operand->getMember(0);
        auto rhs = operand->getMember(1);

        if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
            (allowArrayExpansion && lhs->type == NODE_TYPE_EXPANSION)) {
          clearAttributeAccess(result);

          if (lhs->isAttributeAccessForVariable(result, allowArrayExpansion) &&
              result.first == me.variable) {
            attrName.clear();
            TRI_AttributeNamesToString(result.second, attrName);
            if (attrName == me.attributeName) {
              if (rhs->isConstant()) {
                ConditionPart indexCondition(result.first, result.second,
                                             operand, ATTRIBUTE_LEFT, nullptr);

                if (me.isCoveredBy(indexCondition, false)) {
                  return true;
                }
              }
              // non-constant condition
              else if (me.operatorType == operand->type &&
                       normalize(me.valueNode) == normalize(rhs)) {
                return true;
              }
            }
          }
        }

        if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS ||
            rhs->type == NODE_TYPE_EXPANSION) {
          clearAttributeAccess(result);

          if (rhs->isAttributeAccessForVariable(result, allowArrayExpansion) &&
              result.first == me.variable) {
            attrName.clear();
            TRI_AttributeNamesToString(result.second, attrName);
            if (attrName == me.attributeName) {
              if (lhs->isConstant()) {
                ConditionPart indexCondition(result.first, result.second,
                                             operand, ATTRIBUTE_RIGHT, nullptr);

                if (me.isCoveredBy(indexCondition, true)) {
                  return true;
                }
              }
              // non-constant condition
              else {
                auto opType = operand->type;
                if (Ast::isReversibleOperator(opType)) {
                  opType = Ast::reverseOperator(opType);
                }
                if (me.operatorType == opType &&
                    normalize(me.valueNode) == normalize(lhs)) {
                  return true;
                }
              }
            }
          }
        }
      }
    }
  } catch (std::exception const& ex) {
    // simply ignore any errors (except trace-logging) and return false.
    // this is not an error, but just means we cannot compare the two
    // conditions for equality because there is no implemented way for
    // comparing some of their components.
    LOG_TOPIC("9f37b", TRACE, Logger::QUERIES)
        << "caught exception in canRemove(): " << ex.what();
  }

  return false;
}

}  // namespace

containers::HashSet<size_t> collectOverlappingMembersForTraversal(
    ExecutionPlan const* plan, Variable const* variable, AstNode const* andNode,
    AstNode const* otherAndNode, bool isPathCondition) {
  containers::HashSet<size_t> toRemove;

  for (size_t i = 0; i < andNode->numMembers(); ++i) {
    auto operand = andNode->getMemberUnchecked(i);

    bool allowOps = operand->isComparisonOperator();
    if (isPathCondition) {
      allowOps = allowOps || operand->isArrayComparisonOperator();
    }

    if (!allowOps) {
      continue;
    }

    if (isConditionCoveredBy(plan, variable, operand, otherAndNode)) {
      toRemove.emplace(i);
    }
  }

  return toRemove;
}

bool isConditionCoveredBy(ExecutionPlan const* plan, Variable const* variable,
                          AstNode const* condition,
                          AstNode const* otherAndNode) {
  TRI_ASSERT(condition != nullptr);
  TRI_ASSERT(otherAndNode != nullptr);
  TRI_ASSERT(otherAndNode->type == NODE_TYPE_OPERATOR_NARY_AND);

  if (!condition->isComparisonOperator() &&
      !condition->isArrayComparisonOperator()) {
    return false;
  }

  constexpr bool allowIndexedAccessInArray = true;
  std::pair<Variable const*, std::vector<basics::AttributeName>> result;

  auto lhs =
      const_cast<AstNode*>(plan->resolveVariableAlias(condition->getMember(0)));
  auto rhs =
      const_cast<AstNode*>(plan->resolveVariableAlias(condition->getMember(1)));

  auto tryRemove = [&](AstNode const* node,
                       AttributeSideType sideType) -> bool {
    if (node->type != NODE_TYPE_ATTRIBUTE_ACCESS &&
        node->type != NODE_TYPE_EXPANSION) {
      return false;
    }
    clearAttributeAccess(result);
    if (!node->isAttributeAccessForVariable(result,
                                            allowIndexedAccessInArray) ||
        result.first != variable) {
      return false;
    }
    ConditionPart current(variable, result.second, condition, sideType,
                          nullptr);
    return canRemove(plan, current, otherAndNode, allowIndexedAccessInArray);
  };

  return tryRemove(lhs, ATTRIBUTE_LEFT) || tryRemove(rhs, ATTRIBUTE_RIGHT);
}

bool extractSingleAndNodes(AstNode const* root, AstNode const* condition,
                           AstNode const*& andNode,
                           AstNode const*& conditionAndNode) {
  if (root == nullptr || condition == nullptr) {
    return false;
  }

  TRI_ASSERT(root->type == NODE_TYPE_OPERATOR_NARY_OR);
  TRI_ASSERT(condition->type == NODE_TYPE_OPERATOR_NARY_OR);

  if (condition->numMembers() != 1 && root->numMembers() != 1) {
    return false;
  }

  andNode = root->getMemberUnchecked(0);
  conditionAndNode = condition->getMemberUnchecked(0);

  TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);
  TRI_ASSERT(conditionAndNode->type == NODE_TYPE_OPERATOR_NARY_AND);

  return true;
}

AstNode* rebuildConditionWithoutMembers(
    Ast* ast, AstNode const* andNode,
    containers::HashSet<size_t> const& toRemove) {
  if (toRemove.empty()) {
    return const_cast<AstNode*>(andNode);
  }

  AstNode* newNode = nullptr;
  for (size_t i = 0; i < andNode->numMembers(); ++i) {
    if (toRemove.find(i) == toRemove.end()) {
      auto what = andNode->getMemberUnchecked(i);
      if (newNode == nullptr) {
        // the only node so far
        newNode = what;
      } else {
        // AND-combine with existing node
        newNode = ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND,
                                                newNode, what);
      }
    }
  }

  return newNode;
}

}  // namespace arangodb::aql