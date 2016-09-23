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

#include "TraversalConditionFinder.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Quantifier.h"
#include "Aql/TraversalNode.h"

using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

static AstNode* createGlobalCondition(Ast* ast, AstNode const* condition) {
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
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  auto quantifier = condition->getMemberUnchecked(2);
  TRI_ASSERT(quantifier->type == NODE_TYPE_QUANTIFIER);
  int val = quantifier->getIntValue(true);
  TRI_ASSERT(val != Quantifier::ANY);
  if (val == Quantifier::NONE) {
    auto it = Ast::NegatedOperators.find(type);
    if (it == Ast::NegatedOperators.end()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    type = it->second;
  }
  auto left = condition->getMemberUnchecked(0);
  TRI_ASSERT(left->numMembers() >= 2);
  // This is the part appended to each element in the expansion.
  left = left->getMemberUnchecked(1);
  auto right = condition->getMemberUnchecked(1);
  return ast->createNodeBinaryOperator(type, left, right);
}

static bool matchesArrayAccessPattern(AstNode const* testee, 
                                     Variable const* findme,
                                     bool& isEdge,
                                     VariableId& tmpVar) {
  // The search pattern is:
  // expansion{levels: 1} -> iterator -2> attributeAccess -> reference
  // Where reference has to be equal to var
  
  if (testee->type != NODE_TYPE_EXPANSION) {
    return false;
  }
  TRI_ASSERT(testee->numMembers() == 5);
  auto levels = testee->getIntValue(true);
  if (levels != 1) {
    // This expression is too complicated for now.
    return false;
  }
  if (testee->getMemberUnchecked(2)->type != NODE_TYPE_NOP ||
      testee->getMemberUnchecked(3)->type != NODE_TYPE_NOP ||
      testee->getMemberUnchecked(4)->type != NODE_TYPE_NOP) {
    // Some complex transformation in subqueries happening.
    // Do not optimize now.
    return false;
  }
  testee = testee->getMemberUnchecked(0); 
  TRI_ASSERT(testee->type == NODE_TYPE_ITERATOR); // Expansion always has iterator
  TRI_ASSERT(testee->numMembers() == 2);
  auto varNode = testee->getMemberUnchecked(0);
  auto v = static_cast<Variable*>(varNode->getData());
  tmpVar = v->id;
  testee = testee->getMemberUnchecked(1);
  if (testee->type != NODE_TYPE_ATTRIBUTE_ACCESS) {
    return false;
  }

  // Ok up to here, Check if it is edges or vertices
  if (testee->stringEquals("edges", false)) {
    // Ok this could be an edge access
    isEdge = true;
  } else if (testee->stringEquals("vertices", false)) {
    // Ok this could be a vertex access
    isEdge = false;
  } else {
    // This is indexed access on sth. completely different.
    return false;
  }

  // Advance to the Variable
  TRI_ASSERT(testee->numMembers() == 1);
  testee = testee->getMemberUnchecked(0);
  if (testee->type != NODE_TYPE_REFERENCE &&
      testee->type != NODE_TYPE_VARIABLE  // Do we actually allow this case?
    ) {
    return false;
  }

  // Check if it really is the variable
  auto variable = static_cast<Variable*>(testee->getData());
  TRI_ASSERT(variable != nullptr);
  if (variable->id == findme->id) {
    return true;
  }

  return false;
}

static bool checkPathVariableAccessFeasible(CalculationNode const* cn,
                                            TraversalNode* tn,
                                            Variable const* var,
                                            bool& conditionIsImpossible) {
  auto node = cn->expression()->node();

  if (node->containsNodeType(NODE_TYPE_OPERATOR_BINARY_OR)) {
    return false;
  }

  std::vector<AstNode const*> currentPath;
  std::vector<std::vector<AstNode const*>> paths;

  node->findVariableAccess(currentPath, paths, var);

  for (auto const& onePath : paths) {
    size_t len = onePath.size();
    bool isEdgeAccess = false;

    for (auto const & node : onePath) {
      if (node->type == NODE_TYPE_FCALL) {
        return false;
      }
      if (node->type == NODE_TYPE_OPERATOR_BINARY_IN ||
          node->type == NODE_TYPE_OPERATOR_BINARY_NIN) {
        if (!node->getMember(0)->isAttributeAccessForVariable(var, true)) {
          return false;
        }
      }
    }

    if (onePath[len - 2]->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      isEdgeAccess = onePath[len - 2]->stringEquals("edges", false);

      if (!isEdgeAccess &&
          !onePath[len - 2]->stringEquals("vertices", false)) {
        /* We can't catch all cases in which this error would occur, so we don't
           throw here.
           std::string message("TRAVERSAL: path only knows 'edges' and
           'vertices', not ");
           message += onePath[len - 2]->getString();
           THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, message);
        */
        return false;
      }
    }

    // we now need to check for p.edges[n] whether n is >= 0
    if (onePath[len - 3]->type == NODE_TYPE_INDEXED_ACCESS) {
      auto indexAccessNode = onePath[len - 3]->getMember(1);
      if ((indexAccessNode->type != NODE_TYPE_VALUE) ||
          (indexAccessNode->value.type != VALUE_TYPE_INT) ||
          (indexAccessNode->value.value._int < 0)) {
        return false;
      }

      conditionIsImpossible =
          !tn->isInRange(indexAccessNode->value.value._int, isEdgeAccess);

    } else if ((onePath[len - 3]->type == NODE_TYPE_ITERATOR) &&
               (onePath[len - 4]->type == NODE_TYPE_EXPANSION)) {
      // we now need to check for p.edges[*] which becomes a fancy structure
      return false;
    } else {
      return false;
    }
  }

  return true;
}

static bool matchesPathAccessPattern(AstNode const* testee,
                                     Variable const* findme, size_t& idx,
                                     bool& isEdge) {
  // The search pattern is:
  // indexedAccess -> attributeAccess -> reference
  // Where reference has to be equal to var

  // Testee has to be IndexedAccess
  if (testee->type != NODE_TYPE_INDEXED_ACCESS) {
    return false;
  }
  TRI_ASSERT(testee->numMembers() == 2);

  // Ok up to here, read the idx already.
  AstNode const* idxNode = testee->getMemberUnchecked(1);
  idx = idxNode->value.value._int;

  // Advance to the AttributeAccess
  testee = testee->getMemberUnchecked(0);
  if (testee->type != NODE_TYPE_ATTRIBUTE_ACCESS) {
    return false;
  }
  // Ok up to here, Check if it is edges or vertices
  if (testee->stringEquals("edges", false)) {
    // Ok this could be an edge access
    isEdge = true;
  } else if (testee->stringEquals("vertices", false)) {
    // Ok this could be a vertex access
    isEdge = false;
  } else {
    // This is indexed access on sth. completely different.
    return false;
  }


  // Advance to the Variable
  TRI_ASSERT(testee->numMembers() == 1);
  testee = testee->getMemberUnchecked(0);
  if (testee->type != NODE_TYPE_REFERENCE &&
      testee->type != NODE_TYPE_VARIABLE  // Do we actually allow this case?
    ) {
    return false;
  }

  // Check if it really is the variable
  auto variable = static_cast<Variable*>(testee->getData());
  TRI_ASSERT(variable != nullptr);
  if (variable->id == findme->id) {
    return true;
  }

  return false;
}

static void transformCondition(AstNode const* node, Variable const* pvar,
                               Ast* ast, TraversalNode* tn) {
  TRI_ASSERT(node->type == NODE_TYPE_OPERATOR_NARY_OR);
  // We do not support OR conditions for pruning
  TRI_ASSERT(node->numMembers() == 1);

  node = node->getMemberUnchecked(0);

  AstNode* result = node->clone(ast);

  AstNode* varRefNode = tn->getTemporaryRefNode();

  size_t const n = result->numMembers();

  for (size_t i = 0; i < n; ++i) {
    AstNode* baseCondition = result->getMemberUnchecked(i);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    switch (baseCondition->type) {
      case NODE_TYPE_OPERATOR_BINARY_EQ:
      case NODE_TYPE_OPERATOR_BINARY_NE:
      case NODE_TYPE_OPERATOR_BINARY_LT:
      case NODE_TYPE_OPERATOR_BINARY_LE:
      case NODE_TYPE_OPERATOR_BINARY_GT:
      case NODE_TYPE_OPERATOR_BINARY_GE:
      case NODE_TYPE_OPERATOR_BINARY_IN:
      case NODE_TYPE_OPERATOR_BINARY_NIN:
      case NODE_TYPE_INDEXED_ACCESS:
        TRI_ASSERT(baseCondition->numMembers() == 2);
        break;
      case NODE_TYPE_ATTRIBUTE_ACCESS:
        TRI_ASSERT(baseCondition->numMembers() == 1);
        break;
      case NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
      case NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
      case NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
      case NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
      case NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
      case NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
      case NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
      case NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
        TRI_ASSERT(baseCondition->numMembers() == 3);
        break;
      default:
        TRI_ASSERT(false);
        break;
    }
#endif


    if (!baseCondition->isSimple()) {
      // We would need v8 for this condition.
      // This will not be used.
      continue;
    }

    auto op = baseCondition->type;

    AstNode* top = baseCondition;
    bool isEdge = false;

    if (op >= NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ &&
        op <= NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN) {
      // We have to handle this differently iff the left side is path access
      AstNode* testee = baseCondition->getMemberUnchecked(0);
      VariableId toReplace;

      if (matchesArrayAccessPattern(testee, pvar, isEdge, toReplace)) {
        auto quantifier = baseCondition->getMemberUnchecked(2);
        TRI_ASSERT(quantifier->type == NODE_TYPE_QUANTIFIER);
        int val = quantifier->getIntValue(true);
        if (val == Quantifier::ANY) {
          // Nono optimize for ANY
          continue;
        }
        AstNode* newCondition = createGlobalCondition(ast, baseCondition);
        std::unordered_map<VariableId, Variable const*> replacements;
        replacements.emplace(toReplace, tn->getTemporaryVariable());

        newCondition = ast->replaceVariables(newCondition, replacements);
        tn->registerGlobalCondition(isEdge, newCondition);
        result->changeMember(i, newCondition);
        continue;
      }
    }

    size_t idx = 0;

    if (op == NODE_TYPE_ATTRIBUTE_ACCESS) {
      // We only have a single attribute access. Identical to attr == true
      AstNode* testee = baseCondition;
      while(true) {
        if (testee->numMembers() == 0) {
          // Ok we barked up the wrong tree. Give up
          break;
        }
        if (matchesPathAccessPattern(testee, pvar, idx, isEdge)) {
          // On top level we may change a different member:
          // cond == p.edges[x] 
          // Otherwise we always switch the 0 member.
          top->changeMember(0, varRefNode); 
          tn->registerCondition(isEdge, idx, baseCondition);
          break;
        }
        top = testee;
        testee = testee->getMemberUnchecked(0);
      }

      continue;
    }

    bool foundVar = false;
    TRI_ASSERT(baseCondition->numMembers() >= 2);
    for (size_t i = 0; i < 2; ++i) {
      bool firstRun = true;
      AstNode* testee = baseCondition->getMemberUnchecked(i);
      while(true) {
        if (testee->numMembers() == 0) {
          // Ok we barked up the wrong tree. Give up
          break;
        }
        if (matchesPathAccessPattern(testee, pvar, idx, isEdge)) {
          // We only find one!
          TRI_ASSERT(!foundVar);
          foundVar = true;
          // On top level we may change a different member:
          // cond == p.edges[x] 
          // Otherwise we always switch the 0 member.
          top->changeMember(firstRun ? i : 0, varRefNode); 
          tn->registerCondition(isEdge, idx, baseCondition);
          break;
        }
        top = testee;
        testee = testee->getMemberUnchecked(0);
        firstRun = false;
      }
      if (foundVar) {
        // We have an access. Can only be one.
        break;
      }
    }
  }
}

bool TraversalConditionFinder::before(ExecutionNode* en) {
  if (!_variableDefinitions.empty() && en->canThrow()) {
    // we already found a FILTER and
    // something that can throw is not safe to optimize
    _filters.clear();
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
    case EN::INSERT:
    case EN::REMOVE:
    case EN::REPLACE:
    case EN::UPDATE:
    case EN::UPSERT:
    case EN::RETURN:
    case EN::SORT:
    case EN::ENUMERATE_COLLECTION:
    case EN::LIMIT:
    case EN::SHORTEST_PATH:
      // in these cases we simply ignore the intermediate nodes, note
      // that we have taken care of nodes that could throw exceptions
      // above.
      break;

    case EN::SINGLETON:
    case EN::NORESULTS:
    case EN::ILLEGAL:
      // in all these cases we better abort
      return true;

    case EN::FILTER: {
      std::vector<Variable const*>&& invars = en->getVariablesUsedHere();
      TRI_ASSERT(invars.size() == 1);
      // register which variable is used in a FILTER
      _filters.emplace(invars[0]->id, en);
      break;
    }

    case EN::CALCULATION: {
      auto outvars = en->getVariablesSetHere();
      TRI_ASSERT(outvars.size() == 1);

      _variableDefinitions.emplace(outvars[0]->id,
                                   static_cast<CalculationNode const*>(en));
      TRI_IF_FAILURE("ConditionFinder::variableDefinition") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      break;
    }

    case EN::TRAVERSAL: {
      auto node = static_cast<TraversalNode*>(en);

      auto condition = std::make_unique<Condition>(_plan->getAst());

      bool foundCondition = false;
      auto const& varsValidInTraversal = node->getVarsValid();
      std::unordered_set<Variable const*> varsUsedByCondition;

      bool conditionIsImpossible = false;

      for (auto& it : _variableDefinitions) {
        auto f = _filters.find(it.first);
        if (f != _filters.end()) {
          // a variable used in a FILTER
          auto outVar = node->getVariablesSetHere();
          if (outVar.size() != 1 || outVar[0]->id == f->first) {
            // now we know, this filter is used for our traversal node.
            auto cn = it.second;

            // check whether variables that are not in scope of the condition
            // are used:
            varsUsedByCondition.clear();
            Ast::getReferencedVariables(cn->expression()->node(),
                                        varsUsedByCondition);
            bool unknownVariableFound = false;
            for (auto const& conditionVar : varsUsedByCondition) {
              bool found = false;
              for (auto const& traversalKnownVar : varsValidInTraversal) {
                if (conditionVar->id == traversalKnownVar->id) {
                  found = true;
                  break;
                }
              }
              if (!found) {
                unknownVariableFound = true;
                break;
              }
            }
            if (unknownVariableFound) {
              continue;
            }

            for (auto const& conditionVar : varsUsedByCondition) {
              // check whether conditionVar is one of those we emit
              int variableType = node->checkIsOutVariable(conditionVar->id);
              if (variableType >= 0) {
                if ((variableType == 2) &&
                    checkPathVariableAccessFeasible(cn, node, conditionVar,
                                                    conditionIsImpossible)) {
                  condition->andCombine(
                      it.second->expression()->node()->clone(_plan->getAst()));
                  foundCondition = true;
                }
                if (conditionIsImpossible) {
                  break;
                }
              }
            }
          }
        }
        if (conditionIsImpossible) {
          break;
        }
      }

      // TODO: we can't execute if we condition->normalize(_plan); in
      // generateCodeNode
      if (!conditionIsImpossible) {
        // right now we're not clever enough to find impossible conditions...
        conditionIsImpossible = (foundCondition && condition->isEmpty());
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
      if (foundCondition) {
        condition->normalize();
        TRI_IF_FAILURE("ConditionFinder::normalizePlan") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        transformCondition(condition->root(), node->pathOutVariable(), _plan->getAst(), node);
        node->setCondition(condition.release());
        *_planAltered = true;
      }

      break;
    }
  }
  return false;
}

bool TraversalConditionFinder::enterSubquery(ExecutionNode*, ExecutionNode*) {
  return false;
}
