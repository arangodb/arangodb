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

#include "Aql/TraversalConditionFinder.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/TraversalNode.h"

using namespace triagens::aql;
using EN = triagens::aql::ExecutionNode;
    
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
      if (_changes->find(node->id()) != _changes->end()) {
        // already optimized this node
        break;
      }

      std::unique_ptr<Condition> condition(new Condition(_plan->getAst()));

      bool foundCondition = false;
      for (auto& it : _variableDefinitions) {
        if (_filters.find(it.first) != _filters.end()) {
          // a variable used in a FILTER
          foundCondition = true;
        }
      }

      for (auto& it : _variableDefinitions) {
        auto f = _filters.find(it.first);
        if (f != _filters.end()) {
          // a variable used in a FILTER
          //it.second
          //auto inVar = f.first;
          auto outVar = node->getVariablesSetHere();
          if (outVar.size() != 1 || outVar[0]->id == f->first) {
            // now we know, this filter is used for our traversal node.
            auto cn = it.second;
            auto vars = cn->getVariablesUsedHere();
            for (auto var: vars) {
              // check whether its one of those we emit 
              int variableType = node->checkIsOutVariable(var->id);
              if (variableType >= 0) {
                if (variableType == 2) {
                  condition->andCombine(it.second->expression()->node());
                  foundCondition = true;
                  node->setCalculationNodeId(cn->id());
                }
              }
            }
          }
        }
      }
      bool const conditionIsImpossible = (foundCondition && condition->isEmpty());

      if (conditionIsImpossible) {
        break;
      }
      if (foundCondition) {
        node->setCondition(condition.release());
      }
      auto const& varsValid = node->getVarsValid();

      // remove all invalid variables from the condition
      /// TODO: we don't have a DNF?if (condition->removeInvalidVariables(varsValid)) {
      /// TODO: we don't have a DNF?  // removing left a previously non-empty OR block empty...
      /// TODO: we don't have a DNF?  // this means we can't use the index to restrict the results
      /// TODO: we don't have a DNF?  break;
      /// TODO: we don't have a DNF?}

      //if (condition->isEmpty()) {
        // no filter conditions left
        //break;
      // }

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

