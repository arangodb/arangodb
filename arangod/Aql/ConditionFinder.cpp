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

#include "Aql/ConditionFinder.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/IndexNode.h"
#include "Aql/SortCondition.h"
#include "Aql/SortNode.h"

using namespace triagens::aql;
using EN = triagens::aql::ExecutionNode;
    
bool ConditionFinder::isInnerLoop (ExecutionNode const* node) const {
  while (node != nullptr) {
    if (! node->hasDependency()) {
      return false;
    }

    node = node->getFirstDependency();
    TRI_ASSERT(node != nullptr);

    auto type = node->getType();

    if (type == EN::ENUMERATE_COLLECTION ||
        type == EN::INDEX_RANGE ||
        type == EN::INDEX ||
        type == EN::ENUMERATE_LIST) {
      // we are contained in an outer loop
      return true;

      // future potential optimization: check if the outer loop has 0 or 1 
      // iterations. in this case it is still possible to remove the sort
    }
  }

  return false;
}

bool ConditionFinder::before (ExecutionNode* en) {
  if (! _variableDefinitions.empty() && en->canThrow()) {
    // we already found a FILTER and
    // something that can throw is not safe to optimize
    _filters.clear();
    _sorts.clear();
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
    case EN::INDEX_RANGE:
    case EN::INSERT:
    case EN::REMOVE:
    case EN::REPLACE:
    case EN::UPDATE:
    case EN::UPSERT:
    case EN::RETURN:
      // in these cases we simply ignore the intermediate nodes, note
      // that we have taken care of nodes that could throw exceptions
      // above.
      break;
    
    case EN::LIMIT:           
      // LIMIT invalidates the sort expression we already found
      _sorts.clear();
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
       _filters.emplace(invars[0]->id);
       break;
     }
    
    case EN::SORT: {
       // register which variables are used in a SORT
       if (_sorts.empty()) {
         for (auto& it : static_cast<SortNode const*>(en)->getElements()) {
           _sorts.emplace_back(std::make_pair((it.first)->id, it.second));
         }
       }
       break;
     }

    case EN::CALCULATION: {
      auto outvars = en->getVariablesSetHere();
      TRI_ASSERT(outvars.size() == 1);

      _variableDefinitions.emplace(outvars[0]->id, static_cast<CalculationNode const*>(en)->expression()->node());
      break;
    }

    case EN::ENUMERATE_COLLECTION: {
      auto node = static_cast<EnumerateCollectionNode const*>(en);
      if (_changes->find(node->id()) != _changes->end()) {
        // already optimized this node
        break;
      }

      auto const& varsValid = node->getVarsValid();
      std::unordered_set<Variable const*> varsUsed;

      std::unique_ptr<Condition> condition(new Condition(_plan->getAst()));

      for (auto& it : _variableDefinitions) {
        if (_filters.find(it.first) != _filters.end()) {
          // a variable used in a FILTER

          // now check if all variables from the FILTER condition are still valid here
          Ast::getReferencedVariables(it.second, varsUsed);
          bool valid = true;
          for (auto& it2 : varsUsed) {
            if (varsValid.find(it2) == varsValid.end()) {
              valid = false;
            }
          }

          if (valid) {
            condition->andCombine(it.second);
          }
        }
      }

      condition->normalize(_plan);
    
      std::unique_ptr<SortCondition> sortCondition; 
      if (! isInnerLoop(en)) {
        // we cannot optimize away a sort if we're in an inner loop ourselves
        sortCondition.reset(new SortCondition(_sorts, _variableDefinitions));
      } 
      else {
        sortCondition.reset(new SortCondition);
      }

      if (condition->isEmpty() && sortCondition->isEmpty()) {
        // no filter conditions left
        break;
      }

      std::vector<Index const*> usedIndexes;
      if (condition->findIndexes(node, usedIndexes, sortCondition.get())) {
        bool reverse = false;
        if (sortCondition->isUnidirectional()) {
          reverse = sortCondition->isDescending();
        }

        TRI_ASSERT(! usedIndexes.empty());
          // We either can find indexes for everything or findIndexes will clear out usedIndexes
        std::unique_ptr<ExecutionNode> newNode(new IndexNode(
          _plan, 
          _plan->nextId(), 
          node->vocbase(), 
          node->collection(), 
          node->outVariable(), 
          usedIndexes, 
          condition.get(),
          reverse
        ));
        condition.release();

        // We keep this nodes change
        _changes->emplace(node->id(), newNode.get());
        newNode.release();
      }

      break;
    }
  }
  return false;
}

bool ConditionFinder::enterSubquery (ExecutionNode*, ExecutionNode*) {
  return false;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

