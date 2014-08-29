////////////////////////////////////////////////////////////////////////////////
/// @brief infrastructure for query optimizer
///
/// @file arangod/Aql/Optimizer.cpp
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                               the optimizer class
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
// @brief list of all rules
////////////////////////////////////////////////////////////////////////////////

std::map<int, Optimizer::Rule> Optimizer::_rules;

////////////////////////////////////////////////////////////////////////////////
// @brief lookup from rule name to rule level
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<std::string, int> Optimizer::_ruleLookup;

////////////////////////////////////////////////////////////////////////////////
// @brief constructor, this will initialize the rules database
////////////////////////////////////////////////////////////////////////////////

Optimizer::Optimizer () {
  if (_rules.empty()) {
    setupRules();
  }
}

////////////////////////////////////////////////////////////////////////////////
// @brief the actual optimization
////////////////////////////////////////////////////////////////////////////////

int Optimizer::createPlans (ExecutionPlan* plan,
                            std::vector<std::string> const& disabledRules) {
  int res;
  int leastDoneLevel = 0;

  TRI_ASSERT(! _rules.empty());
  int maxRuleLevel = _rules.rbegin()->first;

  // which optimizer rules are disabled?
  std::unordered_set<int> const&& disabledIds = getDisabledRuleIds(disabledRules);

  // _plans contains the previous optimisation result
  _plans.clear();
  _plans.push_back(plan, 0);

  // int pass = 1;
  while (leastDoneLevel < maxRuleLevel) {
    /*
    std::cout << "Entering pass " << pass << " of query optimization..." 
              << std::endl;
    */
    // This vector holds the plans we have created in this pass:
    PlanList newPlans;

    // Find variable usage for all old plans now:
    for (auto p : _plans.list) {
      if (! p->varUsageComputed()) {
        p->findVarUsage();
      }
    }

    // std::cout << "Have " << _plans.size() << " plans:" << std::endl;
    /*
    for (auto p : _plans.list) {
      p->show();
      std::cout << std::endl;
    }
    */

    // int count = 0;

    // For all current plans:
    while (_plans.size() > 0) {
      int level;
      auto p = _plans.pop_front(level);
      if (level == maxRuleLevel) {
        newPlans.push_back(p, level);  // nothing to do, just keep it
      }
      else {   // find next rule
        auto it = _rules.upper_bound(level);
        TRI_ASSERT(it != _rules.end());
       
        /* 
        std::cout << "Trying rule " << it->second.name << " with level "
                  << it->first << " on plan " << count++
                  << std::endl;
        */

        if (disabledIds.find(level) != disabledIds.end()) {
          // we picked a disabled rule
          level = it->first;

          newPlans.push_back(p, level);  // nothing to do, just keep it
          // now try next
          continue;
        }

        try {
          res = it->second.func(this, p, it->first, newPlans);
        }
        catch (...) {
          delete p;
          throw;
        }

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
      }

      // TODO: abort early here if we found a good-enough plan
      // a good-enough plan is probably every plan with costs below some
      // defined threshold. this requires plan costs to be calculated here
    }
    _plans.steal(newPlans);
    leastDoneLevel = maxRuleLevel;
    for (auto l : _plans.levelDone) {
      if (l < leastDoneLevel) {
        leastDoneLevel = l;
      }
    }
    // std::cout << "Least done level is " << leastDoneLevel << std::endl;

    // Stop if the result gets out of hand:
    if (_plans.size() >= maxNumberOfPlans) {
      break;
    }
  }

  estimatePlans();
  sortPlans();
  /*
  std::cout << "Optimisation ends with " << _plans.size() << " plans."
            << std::endl;
  for (auto p : _plans.list) {
    p->show();
    std::cout << "costing: " << p->getCost() << std::endl;
    std::cout << std::endl;
  }
  */

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief estimatePlans
////////////////////////////////////////////////////////////////////////////////

void Optimizer::estimatePlans () {
  for (auto p : _plans.list) {
    p->getCost();
    // this value is cached in the plan, so formally this step is
    // unnecessary, but for the sake of cleanliness...
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sortPlans
////////////////////////////////////////////////////////////////////////////////

void Optimizer::sortPlans () {
  std::sort(_plans.list.begin(), _plans.list.end(), [](ExecutionPlan* const& a, ExecutionPlan* const& b) -> bool {
    return a->getCost() < b->getCost();
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief look up the ids of all disabled rules
////////////////////////////////////////////////////////////////////////////////

std::unordered_set<int> Optimizer::getDisabledRuleIds (std::vector<std::string> const& names) const {
  std::unordered_set<int> disabled;

  disabled.reserve(names.size());

  // lookup ids of all disabled rules
  for (auto name : names) {
    if (name == "all") {
      // disable all rules
      for (auto it : _rules) {
        disabled.insert(it.first);
      }
      break;
    }
    else {
      // disable a specific rule
      auto it = _ruleLookup.find(name);
      if (it != _ruleLookup.end()) {
        disabled.insert((*it).second);
      }
    }
  }

  return disabled;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set up the optimizer rules once and forever
////////////////////////////////////////////////////////////////////////////////

void Optimizer::setupRules () {
  TRI_ASSERT(_rules.empty());

  // List all the rules in the system here:
  // lower level values mean earlier rule execution
  
  // note that levels must be unique

  //////////////////////////////////////////////////////////////////////////////
  // "Pass 1": moving nodes "up" (potentially outside loops):
  //           please use levels between 1 and 99 here
  //////////////////////////////////////////////////////////////////////////////

  // move calculations up the dependency chain (to pull them out of
  // inner loops etc.)
  registerRule("move-calculations-up", moveCalculationsUpRule, 10);

  // move filters up the dependency chain (to make result sets as small
  // as possible as early as possible)
  registerRule("move-filters-up", moveFiltersUpRule, 20);

  //////////////////////////////////////////////////////////////////////////////
  /// "Pass 2": try to remove redundant or unnecessary nodes
  ///           use levels between 101 and 199 for this
  //////////////////////////////////////////////////////////////////////////////

  // remove filters from the query that are not necessary at all
  // filters that are always true will be removed entirely
  // filters that are always false will be replaced with a NoResults node
  registerRule("remove-unnecessary-filters", removeUnnecessaryFiltersRule, 110);
  
  // remove calculations that are never necessary
  registerRule("remove-unnecessary-calculations", 
               removeUnnecessaryCalculationsRule, 120);

  // remove redundant sort blocks
  registerRule("remove-redundant-sorts", removeRedundantSorts, 130);

  //////////////////////////////////////////////////////////////////////////////
  /// "Pass 3": interchange EnumerateCollection nodes in all possible ways
  ///           this is level 500, please never let new plans from higher
  ///           levels go back to this or lower levels!
  //////////////////////////////////////////////////////////////////////////////

  registerRule("interchangeAdjacentEnumerations", 
               interchangeAdjacentEnumerations, 500);

  //////////////////////////////////////////////////////////////////////////////
  // "Pass 4": moving nodes "up" (potentially outside loops) (second try):
  //           please use levels between 501 and 599 here
  //////////////////////////////////////////////////////////////////////////////

  // move calculations up the dependency chain (to pull them out of
  // inner loops etc.)
  registerRule("move-calculations-up-2", moveCalculationsUpRule, 510);

  // move filters up the dependency chain (to make result sets as small
  // as possible as early as possible)
  registerRule("move-filters-up-2", moveFiltersUpRule, 520);

  //////////////////////////////////////////////////////////////////////////////
  /// "Pass 5": try to remove redundant or unnecessary nodes (second try)
  ///           use levels between 601 and 699 for this
  //////////////////////////////////////////////////////////////////////////////

  // remove filters from the query that are not necessary at all
  // filters that are always true will be removed entirely
  // filters that are always false will be replaced with a NoResults node
  registerRule("remove-unnecessary-filters-2", removeUnnecessaryFiltersRule, 610);
  
  // remove calculations that are never necessary
  registerRule("remove-unnecessary-calculations-2", 
               removeUnnecessaryCalculationsRule, 620);

  // remove redundant sort blocks
  registerRule("remove-redundant-sorts-2", removeRedundantSorts, 630);

  //////////////////////////////////////////////////////////////////////////////
  /// "Pass 6": use indexes if possible for FILTER and/or SORT nodes
  ///           use levels between 701 and 799 for this
  //////////////////////////////////////////////////////////////////////////////

  // try to find a filter after an enumerate collection and find an index . . . 
  registerRule("use-index-range", useIndexRange, 710);

  // try to find sort blocks which are superseeded by indexes
  //registerRule("use-index-for-sort", useIndexForSort, 720);

  //////////////////////////////////////////////////////////////////////////////
  /// END OF OPTIMISATIONS
  //////////////////////////////////////////////////////////////////////////////

}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

