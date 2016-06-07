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

#include "ConditionFinder.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/IndexNode.h"
#include "Aql/SortCondition.h"
#include "Aql/SortNode.h"

using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

bool ConditionFinder::before(ExecutionNode* en) {
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
    case EN::TRAVERSAL:
    case EN::SHORTEST_PATH:
      // in these cases we simply ignore the intermediate nodes, note
      // that we have taken care of nodes that could throw exceptions
      // above.
      break;

    case EN::LIMIT:
      // LIMIT invalidates the sort expression we already found
      _sorts.clear();
      _filters.clear();
      break;

    case EN::SINGLETON:
    case EN::NORESULTS:
    case EN::ILLEGAL:
      // in all these cases we better abort
      return true;

    case EN::FILTER: {
      std::vector<Variable const*> invars(en->getVariablesUsedHere());
      TRI_ASSERT(invars.size() == 1);
      // register which variable is used in a FILTER
      _filters.emplace(invars[0]->id);
      break;
    }

    case EN::SORT: {
      // register which variables are used in a SORT
      if (_sorts.empty()) {
        for (auto& it : static_cast<SortNode const*>(en)->getElements()) {
          _sorts.emplace_back((it.first)->id, it.second);
          TRI_IF_FAILURE("ConditionFinder::sortNode") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
        }
      }
      break;
    }

    case EN::CALCULATION: {
      auto outvars = en->getVariablesSetHere();
      TRI_ASSERT(outvars.size() == 1);

      _variableDefinitions.emplace(
          outvars[0]->id,
          static_cast<CalculationNode const*>(en)->expression()->node());
      TRI_IF_FAILURE("ConditionFinder::variableDefinition") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      break;
    }

    case EN::ENUMERATE_COLLECTION: {
      auto node = static_cast<EnumerateCollectionNode const*>(en);
      if (_changes->find(node->id()) != _changes->end()) {
        // already optimized this node
        break;
      }

      auto condition = std::make_unique<Condition>(_plan->getAst());

      bool foundCondition = false;
      for (auto& it : _variableDefinitions) {
        if (_filters.find(it.first) != _filters.end()) {
          // a variable used in a FILTER
          condition->andCombine(it.second);
          foundCondition = true;
        }
      }

      // normalize the condition
      condition->normalize(_plan);
      TRI_IF_FAILURE("ConditionFinder::normalizePlan") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      bool const conditionIsImpossible =
          (foundCondition && condition->isEmpty());

      if (conditionIsImpossible) {
        // condition is always false
        for (auto const& x : en->getParents()) {
          auto noRes = new NoResultsNode(_plan, _plan->nextId());
          _plan->registerNode(noRes);
          _plan->insertDependency(x, noRes);
          *_hasEmptyResult = true;
        }
        break;
      }

      auto const& varsValid = node->getVarsValid();

      // remove all invalid variables from the condition
      if (condition->removeInvalidVariables(varsValid)) {
        // removing left a previously non-empty OR block empty...
        // this means we can't use the index to restrict the results
        break;
      }

      if (condition->root() && condition->root()->canThrow()) {
        // something that can throw is not safe to optimize
        break;
      }

      std::unique_ptr<SortCondition> sortCondition;
      if (!en->isInInnerLoop()) {
        // we cannot optimize away a sort if we're in an inner loop ourselves
        sortCondition.reset(new SortCondition(_sorts, condition->getConstAttributes(node->outVariable(), false), _variableDefinitions));
      } else {
        sortCondition.reset(new SortCondition);
      }

      if (condition->isEmpty() && sortCondition->isEmpty()) {
        // no filter conditions left
        break;
      }

      std::vector<Transaction::IndexHandle> usedIndexes;
      auto canUseIndex =
          condition->findIndexes(node, usedIndexes, sortCondition.get());

      if (canUseIndex.first || canUseIndex.second) {
        bool reverse = false;
        if (canUseIndex.second && sortCondition->isUnidirectional()) {
          reverse = sortCondition->isDescending();
        }

        if (!canUseIndex.first) {
          // index cannot be used for filtering, but only for sorting
          // remove the condition now
          TRI_ASSERT(canUseIndex.second);
          condition.reset(new Condition(_plan->getAst()));
          condition->normalize(_plan);
        }

        TRI_ASSERT(!usedIndexes.empty());

        // We either can find indexes for everything or findIndexes will clear
        // out usedIndexes
        std::unique_ptr<ExecutionNode> newNode(new IndexNode(
            _plan, _plan->nextId(), node->vocbase(), node->collection(),
            node->outVariable(), usedIndexes, condition.get(), reverse));
        condition.release();
        TRI_IF_FAILURE("ConditionFinder::insertIndexNode") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        // We keep this node's change
        _changes->emplace(node->id(), newNode.get());
        newNode.release();
      }

      break;
    }
  }
  return false;
}

bool ConditionFinder::enterSubquery(ExecutionNode*, ExecutionNode*) {
  return false;
}
