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
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "TraversalConditionFinder.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/GraphOptimizerRules.h"
#include "Aql/Quantifier.h"
#include "Aql/Query.h"
#include "Aql/TraversalNode.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "Graph/TraverserOptions.h"
#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;
using EN = arangodb::aql::ExecutionNode;

namespace {
AstNode* conditionWithInlineCalculations(ExecutionPlan const* plan,
                                         AstNode* cond) {
  auto func = [&](AstNode* node) -> AstNode* {
    if (node->type == NODE_TYPE_REFERENCE) {
      auto variable = static_cast<Variable*>(node->getData());

      if (variable != nullptr) {
        auto setter = plan->getVarSetBy(variable->id);

        if (setter != nullptr && setter->getType() == EN::CALCULATION) {
          auto s = ExecutionNode::castTo<CalculationNode*>(setter);
          auto filterExpression = s->expression();
          AstNode* inNode = filterExpression->nodeForModification();
          if (inNode->isDeterministic() && inNode->isSimple()) {
            return inNode;
          }
        }
      }
    }
    return node;
  };

  return Ast::traverseAndModify(cond, func);
}

enum class OptimizationCase { PATH, EDGE, VERTEX, NON_OPTIMIZABLE };

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
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unsupported operator type");
  }
  auto quantifier = condition->getMemberUnchecked(2);
  TRI_ASSERT(quantifier->type == NODE_TYPE_QUANTIFIER);
  TRI_ASSERT(!Quantifier::isAny(quantifier));
  if (Quantifier::isNone(quantifier)) {
    auto it = Ast::NegatedOperators.find(type);
    if (it == Ast::NegatedOperators.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unsupported operator type");
    }
    type = it->second;
  }
  return type;
}

AstNode* buildExpansionReplacement(Ast* ast, AstNode const* condition,
                                   AstNode* tmpVar) {
  AstNodeType type = buildSingleComparatorType(condition);

  auto replaceReference = [&tmpVar](AstNode* node) -> AstNode* {
    if (node->type == NODE_TYPE_REFERENCE) {
      return tmpVar;
    }
    return node;
  };

  // Now we need to traverse down and replace the reference
  auto lhs = condition->getMemberUnchecked(0);
  auto rhs = condition->getMemberUnchecked(1);
  // We can only optimize if path.edges[*] is on the left hand side
  TRI_ASSERT(lhs->type == NODE_TYPE_EXPANSION);
  TRI_ASSERT(lhs->numMembers() >= 2);
  // This is the part appended to each element in the expansion.
  lhs = lhs->getMemberUnchecked(1);

  // We have to take the return-value if LHS already is the refence.
  // otherwise the point will not be relocated.
  lhs = Ast::traverseAndModify(lhs, replaceReference);
  return ast->createNodeBinaryOperator(type, lhs, rhs);
}

bool checkPathVariableAccessFeasible(Ast* ast, ExecutionPlan* plan,
                                     AstNode* parent, size_t testIndex,
                                     TraversalNode* tn, Variable const* pathVar,
                                     bool& conditionIsImpossible,
                                     int64_t& indexedAccessDepth) {
  auto pathAccess = arangodb::aql::maybeExtractPathAccess(ast, pathVar, parent, testIndex);
  if (!pathAccess.has_value()) {
    return false;
  }
  // If we get here we can optimize this condition
  // As we modify the condition we need to clone it
  auto tempNode = tn->getTemporaryRefNode();
  TRI_ASSERT(pathAccess->parentOfReplace != nullptr);
  if (pathAccess->isAllAccess()) {
    // Global Case
    auto replaceNode = buildExpansionReplacement(
        ast,
        pathAccess->parentOfReplace->getMemberUnchecked(pathAccess->replaceIdx),
        tempNode);
    pathAccess->parentOfReplace->changeMember(pathAccess->replaceIdx,
                                              replaceNode);
    ///////////
    // NOTE: We have to reload the NODE here, because we may have replaced
    // it entirely
    ///////////
    auto cond = conditionWithInlineCalculations(
        plan, parent->getMemberUnchecked(testIndex));
    tn->registerGlobalCondition(pathAccess->isEdgeAccess(), cond);
  } else {
    if (pathAccess->getDepth() < 0) {
      return false;
    }
    if (!tn->isInRange(pathAccess->getDepth(), pathAccess->isEdgeAccess())) {
      conditionIsImpossible = true;
      return false;
    }
    // edit in-place; TODO replace instead?
    TEMPORARILY_UNLOCK_NODE(pathAccess->parentOfReplace);
    // Point Access
    pathAccess->parentOfReplace->changeMember(pathAccess->replaceIdx, tempNode);

    auto cond = conditionWithInlineCalculations(
        plan, parent->getMemberUnchecked(testIndex));

    // NOTE: We have to reload the NODE here, because we may have replaced
    // it entirely
    tn->registerCondition(pathAccess->isEdgeAccess(), pathAccess->getDepth(), cond);
  }
  return true;
}

}  // namespace

TraversalConditionFinder::TraversalConditionFinder(ExecutionPlan* plan,
                                                   bool* planAltered)
    : _plan(plan),
      _condition(std::make_unique<Condition>(plan->getAst())),
      _planAltered(planAltered) {}

bool TraversalConditionFinder::before(ExecutionNode* en) {
  if (!_condition->isEmpty() && !en->isDeterministic()) {
    // we already found a FILTER and
    // something that is not deterministic is not safe to optimize

    _filterVariables.clear();
    // What about _condition?
    return true;
  }

  switch (en->getType()) {
    case EN::ENUMERATE_LIST:
    case EN::COLLECT:
    case EN::SCATTER:
    case EN::DISTRIBUTE:
    case EN::GATHER:
    case EN::REMOTE:
    case EN::SUBQUERY:
    case EN::INDEX:
    case EN::RETURN:
    case EN::SORT:
    case EN::ENUMERATE_COLLECTION:
    case EN::LIMIT:
    case EN::SHORTEST_PATH:
    case EN::ENUMERATE_PATHS:
    case EN::ENUMERATE_IRESEARCH_VIEW:
    case EN::WINDOW: {
      // in these cases we simply ignore the intermediate nodes, note
      // that we have taken care of nodes that could throw exceptions
      // above.
      break;
    }

    case EN::INSERT:
    case EN::REMOVE:
    case EN::REPLACE:
    case EN::UPDATE:
    case EN::UPSERT: {
      // modification invalidates the filter expression we already found
      _condition = std::make_unique<Condition>(_plan->getAst());
      _filterVariables.clear();
      break;
    }

    case EN::SINGLETON:
    case EN::NORESULTS: {
      // in all these cases we better abort
      return true;
    }

    case EN::FILTER: {
      // register which variable is used in a FILTER
      _filterVariables.emplace(
          ExecutionNode::castTo<FilterNode const*>(en)->inVariable()->id);
      break;
    }

    case EN::CALCULATION: {
      auto calcNode = ExecutionNode::castTo<CalculationNode const*>(en);
      Variable const* outVar = calcNode->outVariable();
      if (_filterVariables.find(outVar->id) != _filterVariables.end()) {
        // This calculationNode is directly part of a filter condition
        // So we have to iterate through it.
        TRI_IF_FAILURE("ConditionFinder::variableDefinition") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        _condition->andCombine(calcNode->expression()->node());
      }
      break;
    }

    case EN::TRAVERSAL: {
      auto node = ExecutionNode::castTo<TraversalNode*>(en);
      if (_condition->isEmpty()) {
        // No condition, no optimize
        break;
      }
      auto options = node->options();
      auto const& varsValidInTraversal = node->getVarsValid();

      bool conditionIsImpossible = false;
      auto vertexVar = node->vertexOutVariable();
      auto edgeVar = node->edgeOutVariable();
      auto pathVar = node->pathOutVariable();

      _condition->normalize();

      TRI_IF_FAILURE("ConditionFinder::normalizePlan") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      // _condition is now in disjunctive normal form
      auto orNode = _condition->root();
      TRI_ASSERT(orNode->type == NODE_TYPE_OPERATOR_NARY_OR);
      if (orNode->numMembers() != 1) {
        // Multiple OR statements.
        // => No optimization
        break;
      }

      auto andNode = orNode->getMemberUnchecked(0);
      TRI_ASSERT(andNode->type == NODE_TYPE_OPERATOR_NARY_AND);
      // edit in-place; TODO: replace node instead
      TEMPORARILY_UNLOCK_NODE(andNode);
      VarSet varsUsedByCondition;

      auto coveredCondition = std::make_unique<Condition>(_plan->getAst());

      // Method to identify which optimization case we need to take care of.
      // We can only optimize if we have a single variable (vertex / edge /
      // path) per condition.
      auto identifyCase = [&]() -> OptimizationCase {
        OptimizationCase result = OptimizationCase::NON_OPTIMIZABLE;
        for (auto const& var : varsUsedByCondition) {
          if (varsValidInTraversal.find(var) == varsValidInTraversal.end()) {
            // Found a variable that is not in the scope
            return OptimizationCase::NON_OPTIMIZABLE;
          }
          if (var == edgeVar) {
            if (result != OptimizationCase::NON_OPTIMIZABLE) {
              return OptimizationCase::NON_OPTIMIZABLE;
            }
            result = OptimizationCase::EDGE;
          } else if (var == vertexVar) {
            if (result != OptimizationCase::NON_OPTIMIZABLE) {
              return OptimizationCase::NON_OPTIMIZABLE;
            }
            result = OptimizationCase::VERTEX;
          } else if (var == pathVar) {
            if (result != OptimizationCase::NON_OPTIMIZABLE) {
              return OptimizationCase::NON_OPTIMIZABLE;
            }
            result = OptimizationCase::PATH;
          }
        }
        return result;
      };

      for (size_t i = andNode->numMembers(); i > 0; --i) {
        // Whenever we do not support a of the condition we have to throw it out

        auto cond = andNode->getMemberUnchecked(i - 1);
        // We now iterate over all condition-parts  we found, and check if we
        // can optimize them
        varsUsedByCondition.clear();
        Ast::getReferencedVariables(cond, varsUsedByCondition);
        OptimizationCase usedCase = identifyCase();

        switch (usedCase) {
          case OptimizationCase::NON_OPTIMIZABLE:
            // we found a variable created after the
            // traversal. Cannot optimize this condition
            andNode->removeMemberUnchecked(i - 1);
            break;
          case OptimizationCase::PATH: {
            AstNode* cloned = andNode->getMember(i - 1)->clone(_plan->getAst());
            int64_t indexedAccessDepth = -1;

            // If we get here we can optimize this condition
            if (!checkPathVariableAccessFeasible(
                    _plan->getAst(), _plan, andNode, i - 1, node, pathVar,
                    conditionIsImpossible, indexedAccessDepth)) {
              if (conditionIsImpossible) {
                // If we get here we cannot fulfill the condition
                // So clear
                andNode->clearMembers();
                break;
              }
              andNode->removeMemberUnchecked(i - 1);
            } else {
              TRI_ASSERT(!conditionIsImpossible);

              // remember the original filter conditions if we can remove them
              // later
              if (indexedAccessDepth == -1) {
                coveredCondition->andCombine(cloned);
              } else if ((uint64_t)indexedAccessDepth <= options->maxDepth) {
                // if we had an  index access then indexedAccessDepth
                // is in [0..maxDepth], if the depth is not a concrete value
                // then indexedAccessDepth would be INT64_MAX
                coveredCondition->andCombine(cloned);

                if ((int64_t)options->minDepth < indexedAccessDepth &&
                    !isTrueOnNull(cloned, pathVar)) {
                  // do not return paths shorter than the deepest path access
                  // unless the condition evaluates to true on `null`.
                  options->minDepth = indexedAccessDepth;
                }
              }  // otherwise do not remove the filter statement
            }
            break;
          }
          case OptimizationCase::VERTEX:
          case OptimizationCase::EDGE: {
            // We have the Vertex or Edge variable in the statement
            AstNode* expr = andNode->getMemberUnchecked(i - 1);

            // check if the filter condition can be executed on a DB server,
            // deterministically
            if (expr != nullptr &&
                expr->canBeUsedInFilter(
                    _plan->getAst()->query().vocbase().isOneShard())) {
              // Only do register this condition in case it can be executed
              // inside a TraversalNode. We need to check here and abort in
              // cases which are just not allowed, e.g. execution of user
              // defined JavaScript method or V8 based methods.

              auto conditionToOptimize =
                  conditionWithInlineCalculations(_plan, expr);

              // Create a clone before we modify the Condition
              AstNode* cloned = conditionToOptimize->clone(_plan->getAst());
              // Retain original condition, as covered by this Node
              coveredCondition->andCombine(cloned);
              node->registerPostFilterCondition(conditionToOptimize);
            }
            break;
          }
        }

        if (conditionIsImpossible) {
          // Abort iteration through nodes
          break;
        }
      }

      if (conditionIsImpossible) {
        // condition is always false
        for (auto const& x : node->getParents()) {
          auto noRes = new NoResultsNode(_plan, _plan->nextId());
          _plan->registerNode(noRes);
          _plan->insertDependency(x, noRes);
          *_planAltered = true;
        }
        break;
      }
      if (!coveredCondition->isEmpty()) {
        coveredCondition->normalize();
        node->setCondition(std::move(coveredCondition));
        // We restart here with an empty condition.
        // All Filters that have been collected thus far
        // depend on sth issued by this traverser or later
        // specifically they cannot be used by any earlier traversal
        _condition = std::make_unique<Condition>(_plan->getAst());
        *_planAltered = true;
      }
      break;
    }

    default: {
      // should not reach this point
      TRI_ASSERT(false);
    }
  }
  return false;
}

bool TraversalConditionFinder::enterSubquery(ExecutionNode*, ExecutionNode*) {
  return false;
}

bool TraversalConditionFinder::isTrueOnNull(AstNode* node,
                                            Variable const* pathVar) const {
  VarSet vars;
  Ast::getReferencedVariables(node, vars);
  if (vars.size() > 1) {
    // More then one variable.
    // Too complex, would require to figure out all
    // possible values for all others vars and play them through
    // Do not opt.
    return true;
  }
  TRI_ASSERT(vars.size() == 1);
  TRI_ASSERT(vars.find(pathVar) != vars.end());

  TRI_ASSERT(_plan->getAst() != nullptr);

  bool mustDestroy = false;
  Expression tmpExp(_plan->getAst(), node);

  AqlFunctionsInternalCache rcache;
  FixedVarExpressionContext ctxt(_plan->getAst()->query().trxForOptimization(),
                                 _plan->getAst()->query(), rcache);
  ctxt.setVariableValue(pathVar, {});
  AqlValue res = tmpExp.execute(&ctxt, mustDestroy);
  AqlValueGuard guard(res, mustDestroy);
  return res.toBoolean();
}
