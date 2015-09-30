////////////////////////////////////////////////////////////////////////////////
/// @brief Condition finder, used to build up the Condition object
///
/// @file arangod/Aql/ConditionFinder.h
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
#include "Aql/SortCondition.h"

using namespace triagens::aql;
using EN = triagens::aql::ExecutionNode;

bool ConditionFinder::before (ExecutionNode* en) {
  if (! _variableDefinitions.empty() && en->canThrow()) {
    // we already found a FILTER and
    // something that can throw is not safe to optimize
    _filters.clear();
    _sorts.clear();
    delete _condition;
    _condition = nullptr;
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
      
      if (_condition == nullptr) {
        // did not have any expression before. now save what we found
        _condition = new Condition(_plan->getAst());
      }

      if (_filters.find(outvars[0]->id) != _filters.end()) {
        // a variable used in a FILTER
        auto expressionNode = static_cast<CalculationNode const*>(en)->expression()->node();

        TRI_ASSERT(_condition != nullptr);
        _condition->andCombine(expressionNode);
      }
      break;
    }

    case EN::ENUMERATE_COLLECTION: {
      if (_condition == nullptr) {
        // No one used a filter up to now. Leave this node
        break;
      }

      auto node = static_cast<EnumerateCollectionNode const*>(en);
      if (_changes->find(node->id()) != _changes->end()) {
        std::cout << "Already optimized " << node->id() << std::endl;
        break;
      }

      TRI_ASSERT(_condition != nullptr);
      _condition->normalize(_plan);

      std::vector<Index const*> usedIndexes;
      SortCondition sortCondition(_sorts, _variableDefinitions);

      if (_condition->findIndexes(node, usedIndexes, sortCondition)) {
        TRI_ASSERT(! usedIndexes.empty());
        std::cout << node->id() << " Number of indexes used: " << usedIndexes.size() << std::endl;
          // We either cann find indexes for everything or findIndexes will clear out usedIndexes
        std::unique_ptr<ExecutionNode> newNode(new IndexNode(
          _plan, 
          _plan->nextId(), 
          node->vocbase(), 
          node->collection(), 
          node->outVariable(), 
          usedIndexes, 
          _condition
        ));

        // We handed over the condition to the created IndexNode
        _shouldFreeCondition = false;

        // We keep this nodes change
        _changes->emplace(node->id(), newNode.release());
      }

      break;
    }
  }
  return false;
}

bool ConditionFinder::enterSubquery (ExecutionNode* super, ExecutionNode* sub) {
  return false;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

