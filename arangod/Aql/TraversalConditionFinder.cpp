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

bool checkPathVariableAccessFeasible(CalculationNode const* cn,
                                     TraversalNode * tn,
                                     Variable const* var,
                                     bool &conditionIsImpossible,
                                     Ast* ast) {
  auto node = cn->expression()->node();
  std::vector<AstNode const*> currentPath;
  std::vector<std::vector<AstNode const*>> paths;
  std::vector<std::vector<AstNode const*>> clonePath;
                                 
  node->findVariableAccess(currentPath, paths, var);

  for (auto onePath : paths) {
    size_t len = onePath.size();
    bool isEdgeAccess = false;
    bool isVertexAccess = false;
    bool isAsterisc = false;
    size_t attrAccessTo = 0;

    if (onePath[len - 2]->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      isEdgeAccess   = strcmp(onePath[len - 2]->getStringValue(),    "edges") == 0;
      isVertexAccess = strcmp(onePath[len - 2]->getStringValue(), "vertices") == 0;

      if (!isEdgeAccess && ! isVertexAccess) {

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
      conditionIsImpossible = !tn->isInRange(indexAccessNode->value.value._int);
      attrAccessTo = indexAccessNode->value.value._int;
    }
    else if ((onePath[len - 3]->type == NODE_TYPE_ITERATOR) &&
             (onePath[len - 4]->type == NODE_TYPE_EXPANSION)){
      // we now need to check for p.edges[*] which becomes a fancy structure
      isAsterisc = true;
    }
    else {
      return false;
    }

    // OR? don't know howto continue.
    AstNode const * compareNode = nullptr;
    AstNode const * accessNodeBranch = nullptr;
    for (auto oneNode : onePath) {
      if (oneNode->type == NODE_TYPE_OPERATOR_BINARY_OR) {
        return false; 
      }
      if (compareNode != nullptr && accessNodeBranch == nullptr) {
        accessNodeBranch = oneNode;
      }

      if (oneNode->type == NODE_TYPE_OPERATOR_BINARY_EQ) {
        compareNode = oneNode;
      }
    }

    if (compareNode != NULL) {
      AstNode const * pathAccessNode;
      AstNode const * filterByNode;
      
      if (compareNode->getMember(0) == accessNodeBranch) {
        pathAccessNode = accessNodeBranch;
        filterByNode = compareNode->getMember(1);
      }
      else {
        pathAccessNode = accessNodeBranch;
        filterByNode = compareNode->getMember(0);
      }

      if (accessNodeBranch->isSimple() && filterByNode->type == NODE_TYPE_VALUE) {
        AstNode *newNode = accessNodeBranch->clone(ast);

        // since we just copied one path, we should only find one. 
        newNode->findVariableAccess(currentPath, clonePath, var);
        newNode->dump(20);
        if (isAsterisc) {

        }
        else {
          auto len = clonePath[0].size();
          /// todo len < 4
          AstNode* firstRefNode = (AstNode*) clonePath[0][len - 4];
          TRI_ASSERT(firstRefNode->type == NODE_TYPE_ATTRIBUTE_ACCESS);
          auto varRefNode = new AstNode(NODE_TYPE_REFERENCE);
          ast->query()->addNode(varRefNode);
          varRefNode->setData(isEdgeAccess ? tn->edgeOutVariable(): tn->vertexOutVariable());
          firstRefNode->changeMember(0, varRefNode);
          tn->storeSimpleExpression(isAsterisc,
                                    isEdgeAccess,
                                    attrAccessTo,
                                    NODE_TYPE_OPERATOR_BINARY_EQ,
                                    newNode,
                                    filterByNode);
          
          printf("\n      xxxxx: %s\n", newNode->toString().c_str());
          printf("\n      yyyy: %s\n", accessNodeBranch->toString().c_str());
          printf("\n      yyyy: %s\n", filterByNode->toString().c_str());
        }

      }
printf("\na: %s\n", compareNode->toString().c_str());
printf("\na: %s\n", accessNodeBranch->toString().c_str());

  triagens::basics::Json j(TRI_UNKNOWN_MEM_ZONE, accessNodeBranch->toJson(TRI_UNKNOWN_MEM_ZONE, true));
  printf("sanotuh %s\n", j.toString().c_str());

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
                    condition->andCombine(it.second->expression()->node());
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

