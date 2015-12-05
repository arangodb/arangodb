////////////////////////////////////////////////////////////////////////////////
/// @brief Condition finder, used to build up the Condition object
///
/// @file 
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/JsonHelper.h"
#include "Aql/Ast.h"
#include "Aql/TraversalConditionFinder.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/TraversalNode.h"

using namespace triagens::aql;
using EN = triagens::aql::ExecutionNode;

bool checkPathVariableAccessFeasible (CalculationNode const* cn,
                                      TraversalNode* tn,
                                      Variable const* var,
                                      bool &conditionIsImpossible,
                                      Ast* ast) {
  auto node = cn->expression()->node();

  if (node->containsNodeType(NODE_TYPE_OPERATOR_BINARY_OR) ||
      node->containsNodeType(NODE_TYPE_OPERATOR_BINARY_IN) ||
      node->containsNodeType(NODE_TYPE_OPERATOR_BINARY_NIN)) {
    return false;
  }

  std::vector<AstNode const*> currentPath;
  std::vector<std::vector<AstNode const*>> paths;

  node->findVariableAccess(currentPath, paths, var);

  for (auto const& onePath : paths) {
    size_t len = onePath.size();
    bool isEdgeAccess = false;

    if (onePath[len - 2]->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      isEdgeAccess   = strcmp(onePath[len - 2]->getStringValue(),    "edges") == 0;

      if (!isEdgeAccess && strcmp(onePath[len - 2]->getStringValue(), "vertices") != 0) {
        /* We can't catch all cases in which this error would occur, so we don't throw here.
           std::string message("TRAVERSAL: path only knows 'edges' and 'vertices', not ");
           message += onePath[len - 2]->getStringValue();
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
      conditionIsImpossible = ! tn->isInRange(indexAccessNode->value.value._int, isEdgeAccess);
    }
    else if ((onePath[len - 3]->type == NODE_TYPE_ITERATOR) &&
             (onePath[len - 4]->type == NODE_TYPE_EXPANSION)){
      // we now need to check for p.edges[*] which becomes a fancy structure
      return false;
    }
    else {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
bool extractSimplePathAccesses (AstNode const* node,
                                TraversalNode* tn,
                                Ast* ast) {

  std::vector<AstNode const*> currentPath;
  std::vector<std::vector<AstNode const*>> paths;
  std::vector<std::vector<AstNode const*>> clonePath;

  node->findVariableAccess(currentPath, paths, tn->pathOutVariable());

  for (auto onePath : paths) {
    size_t len = onePath.size();
    bool isEdgeAccess = false;
    size_t attrAccessTo = 0;

    if (onePath[len - 2]->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      isEdgeAccess   = strcmp(onePath[len - 2]->getStringValue(), "edges") == 0;
    }
    // we now need to check for p.edges[n] whether n is >= 0
    if (onePath[len - 3]->type == NODE_TYPE_INDEXED_ACCESS) {
      auto indexAccessNode = onePath[len - 3]->getMember(1);
      attrAccessTo = indexAccessNode->value.value._int;
    }

    AstNode const* compareNode = nullptr;
    AstNode const* accessNodeBranch = nullptr;
    for (auto oneNode : onePath) {

      if (compareNode != nullptr && accessNodeBranch == nullptr) {
        accessNodeBranch = oneNode;
      }

      if ((oneNode->type == NODE_TYPE_OPERATOR_BINARY_EQ)  ||
          (oneNode->type == NODE_TYPE_OPERATOR_BINARY_NE ) ||
          (oneNode->type == NODE_TYPE_OPERATOR_BINARY_LT ) ||
          (oneNode->type == NODE_TYPE_OPERATOR_BINARY_LE ) ||
          (oneNode->type == NODE_TYPE_OPERATOR_BINARY_GT ) ||
          (oneNode->type == NODE_TYPE_OPERATOR_BINARY_GE )
          //  || As long as we need to twist the access, this is impossible:
          // (oneNode->type == NODE_TYPE_OPERATOR_BINARY_IN ) ||
          // (oneNode->type == NODE_TYPE_OPERATOR_BINARY_NIN))
          ){
        compareNode = oneNode;
      }
    }

    if (compareNode != NULL) {
      AstNode const * pathAccessNode;
      AstNode const * filterByNode;
      bool flipOperator = false;

      if (compareNode->getMember(0) == accessNodeBranch) {
        pathAccessNode = accessNodeBranch;
        filterByNode = compareNode->getMember(1);
      }
      else {
        flipOperator = (compareNode->type == NODE_TYPE_OPERATOR_BINARY_LT ) ||
          (compareNode->type == NODE_TYPE_OPERATOR_BINARY_LE ) ||
          (compareNode->type == NODE_TYPE_OPERATOR_BINARY_GT ) ||
          (compareNode->type == NODE_TYPE_OPERATOR_BINARY_GE );

        pathAccessNode = accessNodeBranch;
        filterByNode = compareNode->getMember(0);
      }

      if (accessNodeBranch->isSimple() && filterByNode->isDeterministic()) {

        currentPath.clear();
        clonePath.clear();
        filterByNode->findVariableAccess(currentPath, clonePath, tn->pathOutVariable());
        if (clonePath.size() > 0) {
          // Path variable access on the RHS? can't do that.
          continue;
        }

        AstNode *newNode = pathAccessNode->clone(ast);

        // since we just copied one path, we should only find one. 
        currentPath.clear();
        clonePath.clear();
        newNode->findVariableAccess(currentPath, clonePath, tn->pathOutVariable());
        if (clonePath.size() != 1) {
          continue;
        }
        auto len = clonePath[0].size();
        if (len < 4) {
          continue;
        }

        AstNode* firstRefNode = (AstNode*) clonePath[0][len - 4];
        TRI_ASSERT(firstRefNode->type == NODE_TYPE_ATTRIBUTE_ACCESS);

        // replace the path variable access by a variable access to edge/vertex (then current to the iteration)
        auto varRefNode = new AstNode(NODE_TYPE_REFERENCE);
        ast->query()->addNode(varRefNode);
        varRefNode->setData(isEdgeAccess ? tn->edgeOutVariable(): tn->vertexOutVariable());
        firstRefNode->changeMember(0, varRefNode);

        auto expressionOperator = compareNode->type;
        if (flipOperator) {
          if (expressionOperator == NODE_TYPE_OPERATOR_BINARY_LT ) {
            expressionOperator = NODE_TYPE_OPERATOR_BINARY_GT;
          }
          else if (expressionOperator == NODE_TYPE_OPERATOR_BINARY_LE ) {
            expressionOperator = NODE_TYPE_OPERATOR_BINARY_GE;
          }
          else if (expressionOperator == NODE_TYPE_OPERATOR_BINARY_GT ) { 
            expressionOperator = NODE_TYPE_OPERATOR_BINARY_LT;
          }
          else if (expressionOperator == NODE_TYPE_OPERATOR_BINARY_GE ) {
            expressionOperator = NODE_TYPE_OPERATOR_BINARY_LE;
          }
        }
        tn->storeSimpleExpression(isEdgeAccess,
                                  attrAccessTo,
                                  expressionOperator,
                                  newNode,
                                  filterByNode);
      }
    }
  }

  return true;
}

bool TraversalConditionFinder::before (ExecutionNode* en) {
  if (! _variableDefinitions.empty() && en->canThrow()) {
    // we already found a FILTER and
    // something that can throw is not safe to optimize
    _filters.clear();
    return true;
  }

  switch (en->getType()) {
    case EN::ENUMERATE_LIST:
    case EN::AGGREGATE:
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

      _variableDefinitions.emplace(outvars[0]->id, static_cast<CalculationNode const*>(en));
      TRI_IF_FAILURE("ConditionFinder::variableDefinition") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      break;
    }

    case EN::TRAVERSAL: {
      auto node = static_cast<TraversalNode *>(en);

      std::unique_ptr<Condition> condition(new Condition(_plan->getAst()));

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

            // check whether variables that are not in scope of the condition are used:
            varsUsedByCondition.clear();
            Ast::getReferencedVariables(cn->expression()->node(), varsUsedByCondition); 
            bool unknownVariableFound = false;
            for (auto conditionVar: varsUsedByCondition) {
              bool found = false;
              for (auto traversalKnownVar : varsValidInTraversal ) {
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

            for (auto conditionVar: varsUsedByCondition) {
              // check whether conditionVar is one of those we emit
              int variableType = node->checkIsOutVariable(conditionVar->id);
              if (variableType >= 0) {
                if ((variableType == 2) &&
                    checkPathVariableAccessFeasible(cn, node, conditionVar, conditionIsImpossible, _plan->getAst()))
                  {
                    condition->andCombine(it.second->expression()->node()->clone(_plan->getAst()));
                    foundCondition = true;
                    node->setCalculationNodeId(cn->id());
                  }
                if (conditionIsImpossible)
                  break;
              }
            }
          }
        }
        if (conditionIsImpossible)
          break;
      }

      if (!conditionIsImpossible) {
        conditionIsImpossible = !node->isRangeValid();
      }

      // TODO: we can't execute if we condition->normalize(_plan); in generateCodeNode
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
        extractSimplePathAccesses(condition->root(), node, _plan->getAst());
        node->setCondition(condition.release());
        *_planAltered = true;
      }

      break;
    }
  }
  return false;
}

bool TraversalConditionFinder::enterSubquery (ExecutionNode*, ExecutionNode*) {
  return false;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

