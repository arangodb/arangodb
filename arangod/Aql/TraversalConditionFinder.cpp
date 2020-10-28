////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/Quantifier.h"
#include "Aql/Query.h"
#include "Aql/TraversalNode.h"
#include "Graph/TraverserOptions.h"
#include "Logger/LogMacros.h"

#include "Basics/StaticStrings.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;
using EN = arangodb::aql::ExecutionNode;

static AstNodeType BuildSingleComparatorType(AstNode const* condition) {
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
  int64_t val = quantifier->getIntValue(true);
  TRI_ASSERT(val != Quantifier::ANY);
  if (val == Quantifier::NONE) {
    auto it = Ast::NegatedOperators.find(type);
    if (it == Ast::NegatedOperators.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unsupported operator type");
    }
    type = it->second;
  }
  return type;
}

static AstNode* BuildExpansionReplacement(Ast* ast, AstNode const* condition, AstNode* tmpVar) {
  AstNodeType type = BuildSingleComparatorType(condition);

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

static bool IsSupportedNode(Variable const* pathVar, AstNode const* node) {
  // do a quick first check for all comparisons
  switch (node->type) {
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
    case NODE_TYPE_OPERATOR_BINARY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_NE:
    case NODE_TYPE_OPERATOR_BINARY_LT:
    case NODE_TYPE_OPERATOR_BINARY_LE:
    case NODE_TYPE_OPERATOR_BINARY_GT:
    case NODE_TYPE_OPERATOR_BINARY_GE:
    case NODE_TYPE_OPERATOR_BINARY_IN:
    case NODE_TYPE_OPERATOR_BINARY_NIN: {
      // the following types of expressions are not supported
      //   p.edges[0]._from  op  whatever attribute access
      //   whatever attribute access  op  p.edges[0]._from
      AstNode const* lhs = node->getMember(0);
      AstNode const* rhs = node->getMember(1);

      if (lhs->isAttributeAccessForVariable(pathVar, true)) {
        // p.xxx  op  whatever
        if (rhs->type != NODE_TYPE_VALUE && rhs->type != NODE_TYPE_ARRAY &&
            rhs->type != NODE_TYPE_OBJECT && rhs->type != NODE_TYPE_REFERENCE) {
          return false;
        }
      } else if (rhs->isAttributeAccessForVariable(pathVar, true)) {
        // whatever  op  p.xxx
        if (lhs->type != NODE_TYPE_VALUE && lhs->type != NODE_TYPE_ARRAY &&
            lhs->type != NODE_TYPE_OBJECT && lhs->type != NODE_TYPE_REFERENCE) {
          return false;
        }
      }
      break;
    }
    default: {
      // intentionally no other cases defined...
      // we'll simply fall through to the next switch..case statement
      break;
    }
  }

  switch (node->type) {
    case NODE_TYPE_VARIABLE:
    case NODE_TYPE_OPERATOR_UNARY_PLUS:
    case NODE_TYPE_OPERATOR_UNARY_MINUS:
    case NODE_TYPE_OPERATOR_UNARY_NOT:
    case NODE_TYPE_OPERATOR_BINARY_AND:
    case NODE_TYPE_OPERATOR_BINARY_OR:
    case NODE_TYPE_OPERATOR_BINARY_PLUS:
    case NODE_TYPE_OPERATOR_BINARY_MINUS:
    case NODE_TYPE_OPERATOR_BINARY_TIMES:
    case NODE_TYPE_OPERATOR_BINARY_DIV:
    case NODE_TYPE_OPERATOR_BINARY_MOD:
    case NODE_TYPE_OPERATOR_BINARY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_NE:
    case NODE_TYPE_OPERATOR_BINARY_LT:
    case NODE_TYPE_OPERATOR_BINARY_LE:
    case NODE_TYPE_OPERATOR_BINARY_GT:
    case NODE_TYPE_OPERATOR_BINARY_GE:
    case NODE_TYPE_OPERATOR_BINARY_IN:
    case NODE_TYPE_OPERATOR_BINARY_NIN:
    case NODE_TYPE_ATTRIBUTE_ACCESS:
    case NODE_TYPE_BOUND_ATTRIBUTE_ACCESS:
    case NODE_TYPE_INDEXED_ACCESS:
    case NODE_TYPE_EXPANSION:
    case NODE_TYPE_ITERATOR:
    case NODE_TYPE_VALUE:
    case NODE_TYPE_ARRAY:
    case NODE_TYPE_OBJECT:
    case NODE_TYPE_OBJECT_ELEMENT:
    case NODE_TYPE_REFERENCE:
    case NODE_TYPE_NOP:
    case NODE_TYPE_RANGE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
    case NODE_TYPE_QUANTIFIER:
      return true;
    case NODE_TYPE_FCALL:
    case NODE_TYPE_FCALL_USER:
      // These may be possible in the future
      return false;
    case NODE_TYPE_OPERATOR_NARY_OR:
    case NODE_TYPE_OPERATOR_NARY_AND:
      // If we get here the astNode->normalize() did not work
      TRI_ASSERT(false);
      return false;
    default:
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      LOG_TOPIC("ebe25", ERR, arangodb::Logger::FIXME)
          << "Traversal optimizer encountered node: " << node->getTypeString();
#endif
      return false;
  }
}

static bool checkPathVariableAccessFeasible(Ast* ast, AstNode* parent, size_t testIndex,
                                            TraversalNode* tn, Variable const* pathVar,
                                            bool& conditionIsImpossible, size_t& swappedIndex,
                                            int64_t& indexedAccessDepth) {
  AstNode* node = parent->getMemberUnchecked(testIndex);
  if (!IsSupportedNode(pathVar, node)) {
    return false;
  }
  // We need to walk through each branch and validate:
  // 1. It does not contain unsupported types
  // 2. Only one contains var
  // 3. The one with var matches pattern:
  //   A) var.vertices[n] (.*)
  //   B) var.edges[n] (.*)
  //   C) var.vertices[*] (.*) (ALL|NONE) (.*)
  //   D) var.edges[*] (.*) (ALL|NONE) (.*)

  auto unusedWalker = [](AstNode const* n) {};
  bool isEdge = false;
  // We define that depth == UINT64_MAX is "ALL depths"
  uint64_t depth = UINT64_MAX;
  AstNode* parentOfReplace = nullptr;
  size_t replaceIdx = 0;
  bool notSupported = false;

  // We define that patternStep >= 6 is complete Match.
  unsigned char patternStep = 0;

  auto supportedGuard = [&notSupported, pathVar](AstNode const* n) -> bool {
    // cppcheck-suppress knownConditionTrueFalse
    if (notSupported) {
      return false;
    }
    if (!IsSupportedNode(pathVar, n)) {
      notSupported = true;
      return false;
    }
    return true;
  };

  auto searchPattern = [&patternStep, &isEdge, &depth, &pathVar, &notSupported,
                        &parentOfReplace, &replaceIdx,
                        &indexedAccessDepth](AstNode* node) -> AstNode* {
    if (notSupported) {
      // Short circuit, this condition cannot be fulfilled.
      return node;
    }
    switch (patternStep) {
      case 1:
        // we have var.<this-here>
        // Only vertices || edges supported
        if (node->type != NODE_TYPE_ATTRIBUTE_ACCESS) {
          // Incorrect type
          notSupported = true;
          return node;
        }
        if (node->stringEquals(StaticStrings::GraphQueryEdges)) {
          isEdge = true;
        } else if (node->stringEquals(StaticStrings::GraphQueryVertices)) {
          isEdge = false;
        } else {
          notSupported = true;
          return node;
        }
        patternStep++;
        return node;
      case 2: {
        switch (node->type) {
          case NODE_TYPE_VALUE: {
            // we have var.edges[<this-here>]
            if (node->value.type != VALUE_TYPE_INT || node->value.value._int < 0) {
              // Only positive indexed access allowed
              notSupported = true;
              return node;
            }
            depth = static_cast<uint64_t>(node->value.value._int);
            break;
          }
          case NODE_TYPE_ITERATOR:
          case NODE_TYPE_REFERENCE:
            // This Node type is ok. it does not convey any information
            break;
          default:
            // Other types cannot be optimized
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
            LOG_TOPIC("fcdf3", ERR, arangodb::Logger::FIXME)
                << "Failed type: " << node->getTypeString();
            node->dump(0);
#endif
            notSupported = true;
            return node;
        }
        patternStep++;
        break;
      }
      case 3:
        if (depth != UINT64_MAX) {
          // We are in depth pattern.
          // The first Node we encount HAS to be indexed Access
          if (node->type != NODE_TYPE_INDEXED_ACCESS) {
            notSupported = true;
            return node;
          }
          // This completes this pattern. All good
          // Search for the parent having this node.
          patternStep = 6;
          parentOfReplace = node;

          // we need to know the depth at which a filter condition will
          // access a path. Otherwise there are too many results
          TRI_ASSERT(node->numMembers() == 2);
          AstNode* indexVal = node->getMemberUnchecked(1);
          if (indexVal->isIntValue()) {
            indexedAccessDepth = indexVal->getIntValue() + (isEdge ? 1 : 0);
          } else {  // should cause the caller to not remove a filter
            indexedAccessDepth = INT64_MAX;
          }
          return node;
        }
        if (node->type == NODE_TYPE_EXPANSION) {
          // Check that the expansion [*] contains no inline expression;
          // members 2, 3 and 4 correspond to FILTER, LIMIT and RETURN,
          // respectively.
          TRI_ASSERT(node->numMembers() == 5);
          if (node->getMemberUnchecked(2)->type != NODE_TYPE_NOP ||
              node->getMemberUnchecked(3)->type != NODE_TYPE_NOP ||
              node->getMemberUnchecked(4)->type != NODE_TYPE_NOP) {
            notSupported = true;
            return node;
          }

          // We continue in this pattern, all good
          patternStep++;
          parentOfReplace = node;
          return node;
        }
        // if we get here we are in the expansion operator.
        // We simply pipe this one through
        break;
      case 4: {
        if (node->type == NODE_TYPE_QUANTIFIER) {
          // We are in array case. We need to wait for a quantifier
          // This means we have path.edges[*] on the right hand side
          int64_t val = node->getIntValue(true);
          if (val == Quantifier::ANY) {
            // Nono optimize for ANY
            notSupported = true;
            return node;
          }
          // This completes this pattern. All good
          // Search for the parent having this node.
          patternStep = 5;
        }
        // if we get here we are in the expansion operator.
        // We simply pipe this one through
        break;
      }
      case 5:
      case 6: {
        for (size_t idx = 0; idx < node->numMembers(); ++idx) {
          if (node->getMemberUnchecked(idx) == parentOfReplace) {
            if (patternStep == 5) {
              if (idx != 0) {
                // We found a right hand side expansion of y ALL == p.edges[*]
                // We cannot optimize this
                notSupported = true;
                return node;
              }
            }
            parentOfReplace = node;
            replaceIdx = idx;
            // Ok finally done.
            patternStep++;
            break;
          }
        }
      }
      default:
        // Just fall through
        break;
    }
    if (node->type == NODE_TYPE_REFERENCE || node->type == NODE_TYPE_VARIABLE) {
      // we are on the bottom of the tree. Check if it is our pathVar
      auto variable = static_cast<Variable*>(node->getData());
      if (pathVar == variable) {
        // We found pathVar
        if (patternStep != 0) {
          // found it twice. Abort
          notSupported = true;
          return node;
        }
        ++patternStep;
      }
    }
    return node;
  };

  // Check branches:
  size_t numMembers = node->numMembers();
  for (size_t i = 0; i < numMembers; ++i) {
    Ast::traverseAndModify(node->getMemberUnchecked(i), supportedGuard,
                           searchPattern, unusedWalker);
    if (notSupported) {
      return false;
    }
    if (patternStep == 5) {
      // The first item is direct child of the parent.
      // Use parent to replace
      // This is only the case on Expansion being
      // the node we have to replace.
      TRI_ASSERT(parentOfReplace->type == NODE_TYPE_EXPANSION);
      if (parentOfReplace != node->getMemberUnchecked(0)) {
        // We found a right hand side of x ALL == p.edges[*]
        // Cannot optimize
        return false;
      }
      parentOfReplace = node;
      replaceIdx = 0;
      patternStep++;
    }
    if (patternStep == 6) {
      if (parentOfReplace == node) {
        parentOfReplace = parent;
        replaceIdx = testIndex;
      } else {
        TRI_ASSERT(parentOfReplace == node->getMemberUnchecked(i));
        parentOfReplace = node;
        replaceIdx = i;
      }
      patternStep++;
    }
    if (patternStep == 7) {
      swappedIndex = i;
      patternStep++;
    }
  }

  if (patternStep < 8) {
    // We found sth. that is not matching the pattern complete.
    // => Do not optimize
    return false;
  }

  // If we get here we can optimize this condition
  // As we modify the condition we need to clone it
  auto tempNode = tn->getTemporaryRefNode();
  TRI_ASSERT(parentOfReplace != nullptr);
  if (depth == UINT64_MAX) {
    // Global Case
    auto replaceNode =
        BuildExpansionReplacement(ast, parentOfReplace->getMemberUnchecked(replaceIdx), tempNode);
    parentOfReplace->changeMember(replaceIdx, replaceNode);
    // NOTE: We have to reload the NODE here, because we may have replaced
    // it entirely
    tn->registerGlobalCondition(isEdge, parent->getMemberUnchecked(testIndex));
  } else {
    conditionIsImpossible = !tn->isInRange(depth, isEdge);
    if (conditionIsImpossible) {
      return false;
    }
    // edit in-place; TODO replace instead?
    TEMPORARILY_UNLOCK_NODE(parentOfReplace);
    // Point Access
    parentOfReplace->changeMember(replaceIdx, tempNode);
    // NOTE: We have to reload the NODE here, because we may have replaced
    // it entirely
    tn->registerCondition(isEdge, depth, parent->getMemberUnchecked(testIndex));
  }
  return true;
}

TraversalConditionFinder::TraversalConditionFinder(ExecutionPlan* plan, bool* planAltered)
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
    case EN::K_SHORTEST_PATHS:
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

      if (pathVar == nullptr) {
        // This traverser does not have a path output.
        // So the user cannot filter based on it
        // => No optimization
        break;
      }

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

      auto originalFilterConditions = std::make_unique<Condition>(_plan->getAst());
      for (size_t i = andNode->numMembers(); i > 0; --i) {
        // Whenever we do not support a of the condition we have to throw it out

        auto cond = andNode->getMemberUnchecked(i - 1);
        // We now iterate over all condition-parts  we found, and check if we
        // can optimize them
        varsUsedByCondition.clear();
        Ast::getReferencedVariables(cond, varsUsedByCondition);

        if (varsUsedByCondition.find(pathVar) == varsUsedByCondition.end()) {
          // For now we only! optimize filter conditions on the path
          // So we skip all FILTERS not referencing the path
          andNode->removeMemberUnchecked(i - 1);
          continue;
        }

        // now we validate that there is no illegal variable used.
        // illegal are
        // * edgeVar and vertexVar. Those are not set
        // during runtime
        // * variables issued after the traversal
        bool illegalVarFound = false;
        for (auto const& var : varsUsedByCondition) {
          if (var == edgeVar || var == vertexVar ||
              varsValidInTraversal.find(var) == varsValidInTraversal.end()) {
            illegalVarFound = true;
            break;
          }
        }

        if (illegalVarFound) {
          // we found a variable created after the
          // traversal. Cannot optimize this condition
          andNode->removeMemberUnchecked(i - 1);
          continue;
        }

        AstNode* cloned = andNode->getMember(i - 1)->clone(_plan->getAst());
        int64_t indexedAccessDepth = -1;

        size_t swappedIndex = 0;
        // If we get here we can optimize this condition
        if (!checkPathVariableAccessFeasible(_plan->getAst(), andNode, i - 1,
                                             node, pathVar, conditionIsImpossible,
                                             swappedIndex, indexedAccessDepth)) {
          andNode->removeMemberUnchecked(i - 1);
          if (conditionIsImpossible) {
            // If we get here we cannot fulfill the condition
            // So clear
            andNode->clearMembers();
            break;
          }

        } else {
          TRI_ASSERT(!conditionIsImpossible);

          // remember the original filter conditions if we can remove them later
          if (indexedAccessDepth == -1) {
            originalFilterConditions->andCombine(cloned);
          } else if ((uint64_t)indexedAccessDepth <= options->maxDepth) {
            // if we had an  index access then indexedAccessDepth
            // is in [0..maxDepth], if the depth is not a concrete value
            // then indexedAccessDepth would be INT64_MAX
            originalFilterConditions->andCombine(cloned);

            if ((int64_t)options->minDepth < indexedAccessDepth &&
                !isTrueOnNull(cloned, pathVar)) {
              // do not return paths shorter than the deepest path access
              // Unless the condition evaluates to true on `null`.
              options->minDepth = indexedAccessDepth;
            }
          }  // otherwise do not remove the filter statement
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
      bool isEmpty = _condition->isEmpty();
      if (!isEmpty) {
        // Check if it only contains an empty n-ary-and within the n-ary-or
        auto node = _condition->root();
        TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_NARY_OR);
        switch (node->numMembers()) {
          case 0:
            isEmpty = true;
            break;
          case 1:
            node = node->getMemberUnchecked(0);
            // We have a DNF, so only n-ary And allowed here
            TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_NARY_AND);
            // If this is empty the condition actually is empty.
            isEmpty = (node->numMembers() == 0);
            break;
          default:
            isEmpty = false;
        }
      }

      if (!isEmpty) {
        originalFilterConditions->normalize();
        node->setCondition(std::move(originalFilterConditions));
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

bool TraversalConditionFinder::isTrueOnNull(AstNode* node, Variable const* pathVar) const {
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
                                 _plan->getAst()->query(),
                                 rcache);
  ctxt.setVariableValue(pathVar, {});
  AqlValue res = tmpExp.execute(&ctxt, mustDestroy);
  AqlValueGuard guard(res, mustDestroy);
  return res.toBoolean();
}
