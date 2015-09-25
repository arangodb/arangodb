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

using namespace triagens::aql;
using EN = triagens::aql::ExecutionNode;

bool ConditionFinder::before (ExecutionNode* en) {
  if (! _filterVariables.empty() && en->canThrow()) {
    // we already found a FILTER and
    // something that can throw is not safe to optimize
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
      _sortExpression = nullptr;
      break;

    case EN::SINGLETON:
    case EN::NORESULTS:
    case EN::ILLEGAL:
      // in all these cases we better abort
      return true;

    case EN::FILTER: {
       std::vector<Variable const*>&& inVar = en->getVariablesUsedHere();
       TRI_ASSERT(inVar.size() == 1);
       // register which variable is used in a FILTER
       _filterVariables.emplace(inVar[0]->id);
       break;
     }
    
    case EN::SORT: {
       std::vector<Variable const*>&& inVar = en->getVariablesUsedHere();
       TRI_ASSERT(inVar.size() == 1);
       // register which variable is used in a SORT
       _sortVariables.emplace(inVar[0]->id);
       break;
     }

    case EN::CALCULATION: {
      auto outvar = en->getVariablesSetHere();
      TRI_ASSERT(outvar.size() == 1);

      if (_filterVariables.find(outvar[0]->id) != _filterVariables.end()) {
        // a variable used in a FILTER
        auto expressionNode = static_cast<CalculationNode const*>(en)->expression()->node();

        if (_condition == nullptr) {
          // did not have any expression before. now save what we found
          _condition = new Condition(_plan->getAst());
        }

        TRI_ASSERT(_condition != nullptr);
        _condition->andCombine(expressionNode);
      }
      else if (_sortVariables.find(outvar[0]->id) != _sortVariables.end()) {
        // a variable used in a SORT
        auto expressionNode = static_cast<CalculationNode const*>(en)->expression()->node();

        // only store latest SORT condition
        if (_sortExpression == nullptr) {
          // did not have any expression before. now save what we found
          _sortExpression = expressionNode;
        }
      }
      break;
    }

    case EN::ENUMERATE_COLLECTION: {
      auto node = static_cast<EnumerateCollectionNode const*>(en);
      if (_changes->find(node->id()) != _changes->end()) {
        std::cout << "Already optimized " << node->id() << std::endl;
        break;
      }
      _condition->normalize(_plan);
      std::vector<Index const*> usedIndexes;
      _condition->findIndexes(node, usedIndexes);
      std::cout << node->id() << " Number of indexes used: " << usedIndexes.size() << std::endl;
      if (usedIndexes.size() != 0) {
        // We either cann find indexes for everything or findIndexes will clear out usedIndexes
        std::unique_ptr<ExecutionNode> newNode(new IndexNode(
          _plan, 
          _plan->nextId(), 
          node->vocbase(), 
          node->collection(), 
          node->outVariable(), 
          usedIndexes, 
          _condition,
          false
        ));

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

