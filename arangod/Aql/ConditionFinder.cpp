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
  if (en->canThrow()) {
    // something that can throw is not safe to optimize
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
    case EN::SORT:
    case EN::INDEX_RANGE:
      // in these cases we simply ignore the intermediate nodes, note
      // that we have taken care of nodes that could throw exceptions
      // above.
      break;

    case EN::SINGLETON:
    case EN::INSERT:
    case EN::REMOVE:
    case EN::REPLACE:
    case EN::UPDATE:
    case EN::UPSERT:
    case EN::RETURN:
    case EN::NORESULTS:
    case EN::ILLEGAL:
      // in all these cases something is seriously wrong and we better abort

      // fall-through intentional...
    case EN::LIMIT:           
      // if we meet a limit node between a filter and an enumerate
      // collection, we abort . . .
      return true;

    case EN::FILTER: {
       std::vector<Variable const*>&& inVar = en->getVariablesUsedHere();
       TRI_ASSERT(inVar.size() == 1);
       // register which variable is used in a filter
       _varIds.emplace(inVar[0]->id);
       break;
     }

    case EN::CALCULATION: {
      auto outvar = en->getVariablesSetHere();
      TRI_ASSERT(outvar.size() == 1);

      if (_varIds.find(outvar[0]->id) == _varIds.end()) {
        // some non-interesting variable
        break;
      }

      auto expression = static_cast<CalculationNode const*>(en)->expression()->node();

      if (_condition == nullptr) {
        // did not have any expression before. now save what we found
        _condition = new Condition(_plan->getAst());
      }

      TRI_ASSERT(_condition != nullptr);
      _condition->andCombine(expression);
      break;
    }

    case EN::ENUMERATE_COLLECTION: {
      _condition->normalize(_plan);
      auto node = static_cast<EnumerateCollectionNode const*>(en);
      std::vector<Index*> usedIndexes;
      _condition->findIndexes(node, usedIndexes);
      std::cout << "Number of indexes used: " << usedIndexes.size() << std::endl;

      // TODO Build new IndexRangeNode
      /*
      std::unique_ptr<ExecutionNode> newNode(new IndexRangeNode(
        _plan, 
        _plan->nextId(), 
        node->vocbase(), 
        node->collection(), 
        node->outVariable(), 
        idx, 
        _condition,
        false
      ));

      size_t place = node->id();

      std::unordered_map<size_t, size_t>::iterator it = _changesPlaces.find(place);

      if (it == _changesPlaces.end()) {
        _changes.emplace_back(place, std::vector<ExecutionNode*>());
        it = _changesPlaces.emplace(place, _changes.size() - 1).first;
      }

      std::vector<ExecutionNode*>& vec = _changes[it->second].second;
      vec.emplace_back(newNode.release());
      */

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

