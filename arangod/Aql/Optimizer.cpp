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
#include "Aql/ExecutionEngine.h"
#include "Aql/OptimizerRules.h"
#include "Cluster/ServerState.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                               the optimizer class
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                            static initializations
// -----------------------------------------------------------------------------
        
triagens::basics::Mutex Optimizer::SetupLock;

////////////////////////////////////////////////////////////////////////////////
// @brief list of all rules
////////////////////////////////////////////////////////////////////////////////

std::map<int, Optimizer::Rule> Optimizer::_rules;

////////////////////////////////////////////////////////////////////////////////
// @brief lookup from rule name to rule level
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<std::string, int> Optimizer::_ruleLookup;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
// @brief constructor, this will initialize the rules database
////////////////////////////////////////////////////////////////////////////////

Optimizer::Optimizer (size_t maxNumberOfPlans) 
  : _maxNumberOfPlans(maxNumberOfPlans > 0 ? maxNumberOfPlans : DefaultMaxNumberOfPlans) {

  if (_rules.empty()) {
    setupRules();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
// @brief add a plan to the optimizer
////////////////////////////////////////////////////////////////////////////////

bool Optimizer::addPlan (ExecutionPlan* plan,
                         RuleLevel level,
                         bool wasModified) {
  TRI_ASSERT(plan != nullptr);

  _newPlans.push_back(plan, level);

  if (wasModified) {
    // register which rules modified / created the plan
    plan->addAppliedRule(_currentRule);
    plan->invalidateCost();
  }

  if (_newPlans.size() >= _maxNumberOfPlans) {
    return false;
  }
  
  return true; 
}

////////////////////////////////////////////////////////////////////////////////
// @brief the actual optimization
////////////////////////////////////////////////////////////////////////////////

int Optimizer::createPlans (ExecutionPlan* plan,
                            std::vector<std::string> const& rulesSpecification,
                            bool inspectSimplePlans) {
  if (! inspectSimplePlans &&
      ! ExecutionEngine::isCoordinator() && 
      plan->isDeadSimple()) {
    // the plan is so simple that any further optimizations would probably cost
    // more than simply executing the plan
    _plans.clear();
    _plans.push_back(plan, 0);
    estimatePlans();

    return TRI_ERROR_NO_ERROR;
  }

  bool runOnlyRequiredRules = false;
  int leastDoneLevel = 0;

  TRI_ASSERT(! _rules.empty());
  int maxRuleLevel = _rules.rbegin()->first;

  // which optimizer rules are disabled?
  std::unordered_set<int> const&& disabledIds = getDisabledRuleIds(rulesSpecification);


  // _plans contains the previous optimisation result
  _plans.clear();
  _plans.push_back(plan, 0);

  _newPlans.clear();

  while (leastDoneLevel < maxRuleLevel) {
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

      if (level >= maxRuleLevel) {
        _newPlans.push_back(p, level);  // nothing to do, just keep it
      }
      else {   // find next rule
        auto it = _rules.upper_bound(level);
        TRI_ASSERT(it != _rules.end());
       
        /* 
        std::cout << "Trying rule " << it->second.name << " with level "
                  << it->first << " on plan " << count++
                  << std::endl;
        */

        level = (*it).first;
        if ((runOnlyRequiredRules || disabledIds.find(level) != disabledIds.end()) &&
            (*it).second.canBeDisabled) {
          // we picked a disabled rule or we have reached the max number of plans
          // and just skip this rule
          level = it->first;

          _newPlans.push_back(p, level);  // nothing to do, just keep it
          // now try next
          continue;
        }

        _currentRule = level;

        int res;
        try {
          res = (*it).second.func(this, p, &(it->second));
        }
        catch (...) {
          delete p;
          throw;
        }

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
      }

      // future optimization: abort early here if we found a good-enough plan
      // a good-enough plan is probably every plan with costs below some
      // defined threshold. this requires plan costs to be calculated here
    }

    _plans.steal(_newPlans);
    leastDoneLevel = maxRuleLevel;
    for (auto l : _plans.levelDone) {
      if (l < leastDoneLevel) {
        leastDoneLevel = l;
      }
    }
    // std::cout << "Least done level is " << leastDoneLevel << std::endl;

    // Stop if the result gets out of hand:
    if (! runOnlyRequiredRules &&
        _plans.size() >= _maxNumberOfPlans) {
      // must still iterate over all REQUIRED remaining transformation rules 
      // because there are some rules which are required to make the query
      // work in cluster mode
      runOnlyRequiredRules = true;
    }
  }
  
  TRI_ASSERT(_plans.size() >= 1);

  estimatePlans();
  sortPlans();
#if 0
  // Only for debugging:
  std::cout << "Optimisation ends with " << _plans.size() << " plans."
            << std::endl;
  for (auto p : _plans.list) {
    p->show();
    std::cout << "costing: " << p->getCost() << std::endl;
    std::cout << std::endl;
  }
#endif

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief translate a list of rule ids into rule name
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Optimizer::translateRules (std::vector<int> const& rules) {
  std::vector<std::string> names;

  for (auto r : rules) {
    auto it = _rules.find(r);
    if (it != _rules.end()) {
      names.emplace_back((*it).second.name);
    }
  }
  return names;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

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

  // lookup ids of all disabled rules
  for (auto name : names) {
    if (name[0] == '-') {
      // disable rule
      if (name == "-all") {
        // disable all rules
        for (auto it : _rules) {
          disabled.insert(it.first);
        }
      }
      else {
        // disable a specific rule
        auto it = _ruleLookup.find(std::string(name.c_str() + 1));
        if (it != _ruleLookup.end()) {
          disabled.insert((*it).second);
        }
      }
    }
    else if (name[0] == '+') {
      // enable rule
      if (name == "+all") {
        // enable all rules
        disabled.clear();
      }
      else {
        auto it = _ruleLookup.find(std::string(name.c_str() + 1));
        if (it != _ruleLookup.end()) {
          disabled.erase((*it).second);
        }
      }
    }
  }

  return disabled;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set up the optimizer rules once and forever
////////////////////////////////////////////////////////////////////////////////

void Optimizer::setupRules () {
  MUTEX_LOCKER(SetupLock);
  
  if (! _rules.empty()) {
    // race condition... prevent duplicate registration of rules
    return;
  }

  // List all the rules in the system here:
  // lower level values mean earlier rule execution
  
  // note that levels must be unique

  //////////////////////////////////////////////////////////////////////////////
  // "Pass 1": moving nodes "up" (potentially outside loops):
  //           please use levels between 1 and 99 here
  //////////////////////////////////////////////////////////////////////////////

  // move calculations up the dependency chain (to pull them out of
  // inner loops etc.)
  registerRule("move-calculations-up",
               moveCalculationsUpRule,
               moveCalculationsUpRule_pass1, 
               true);

  // move filters up the dependency chain (to make result sets as small
  // as possible as early as possible)
  registerRule("move-filters-up",
               moveFiltersUpRule,
               moveFiltersUpRule_pass1,
               true);
  
  // remove redundant calculations
  registerRule("remove-redundant-calculations", 
               removeRedundantCalculationsRule,
               removeRedundantCalculationsRule_pass1, 
               true);

  //////////////////////////////////////////////////////////////////////////////
  /// "Pass 2": try to remove redundant or unnecessary nodes
  ///           use levels between 101 and 199 for this
  //////////////////////////////////////////////////////////////////////////////

  // remove filters from the query that are not necessary at all
  // filters that are always true will be removed entirely
  // filters that are always false will be replaced with a NoResults node
  registerRule("remove-unnecessary-filters",
               removeUnnecessaryFiltersRule,
               removeUnnecessaryFiltersRule_pass2,
               true);
  
  // remove calculations that are never necessary
  registerRule("remove-unnecessary-calculations", 
               removeUnnecessaryCalculationsRule,
               removeUnnecessaryCalculationsRule_pass2,
               true);

  // remove redundant sort blocks
  registerRule("remove-redundant-sorts",
               removeRedundantSorts,
               removeRedundantSorts_pass2,
               true);

  //////////////////////////////////////////////////////////////////////////////
  /// "Pass 3": interchange EnumerateCollection nodes in all possible ways
  ///           this is level 500, please never let new plans from higher
  ///           levels go back to this or lower levels!
  //////////////////////////////////////////////////////////////////////////////

  registerRule("interchange-adjacent-enumerations", 
               interchangeAdjacentEnumerations,
               interchangeAdjacentEnumerations_pass3,
               true);

  //////////////////////////////////////////////////////////////////////////////
  // "Pass 4": moving nodes "up" (potentially outside loops) (second try):
  //           please use levels between 501 and 599 here
  //////////////////////////////////////////////////////////////////////////////

  // move calculations up the dependency chain (to pull them out of
  // inner loops etc.)
  registerRule("move-calculations-up-2",
               moveCalculationsUpRule,
               moveCalculationsUpRule_pass4,
               true);

  // move filters up the dependency chain (to make result sets as small
  // as possible as early as possible)
  registerRule("move-filters-up-2",
               moveFiltersUpRule,
               moveFiltersUpRule_pass4,
               true);

  //////////////////////////////////////////////////////////////////////////////
  /// "Pass 5": try to remove redundant or unnecessary nodes (second try)
  ///           use levels between 601 and 699 for this
  //////////////////////////////////////////////////////////////////////////////

  // remove filters from the query that are not necessary at all
  // filters that are always true will be removed entirely
  // filters that are always false will be replaced with a NoResults node
  registerRule("remove-unnecessary-filters-2",
               removeUnnecessaryFiltersRule,
               removeUnnecessaryFiltersRule_pass5,
               true);
  
  // remove calculations that are never necessary
  registerRule("remove-unnecessary-calculations-2", 
               removeUnnecessaryCalculationsRule,
               removeUnnecessaryCalculationsRule_pass5,
               true);

  // remove redundant sort blocks
  registerRule("remove-redundant-sorts-2",
               removeRedundantSorts,
               removeRedundantSorts_pass5,
               true);

  //////////////////////////////////////////////////////////////////////////////
  /// "Pass 6": use indexes if possible for FILTER and/or SORT nodes
  ///           use levels between 701 and 799 for this
  //////////////////////////////////////////////////////////////////////////////

  // try to find a filter after an enumerate collection and find an index . . . 
  registerRule("use-index-range",
               useIndexRange,
               useIndexRange_pass6,
               true);

  // try to find sort blocks which are superseeded by indexes
  registerRule("use-index-for-sort",
               useIndexForSort,
               useIndexForSort_pass6,
               true);

  // try to replace simple OR conditions with IN
  registerRule("replace-OR-with-IN",
               replaceORwithIN,
               replaceORwithIN_pass6,
               true);

  if (ExecutionEngine::isCoordinator()) {
    // distribute operations in cluster
    registerRule("scatter-in-cluster",
                 scatterInCluster,
                 scatterInCluster_pass10,
                 false);
    
    registerRule("distribute-in-cluster",
                 distributeInCluster,
                 distributeInCluster_pass10,
                 false);

    // distribute operations in cluster
    registerRule("distribute-filtercalc-to-cluster",
                 distributeFilternCalcToCluster,
                 distributeFilternCalcToCluster_pass10,
                 true);

    registerRule("distribute-sort-to-cluster",
                 distributeSortToCluster,
                 distributeSortToCluster_pass10,
                 true);
    
    registerRule("remove-unnecessary-remote-scatter",
                 removeUnnecessaryRemoteScatter,
                 removeUnnecessaryRemoteScatter_pass10,
                 true);
    
    registerRule("undistribute-remove-after-enum-coll",
                 undistributeRemoveAfterEnumColl,
                 undistributeRemoveAfterEnumColl_pass10,
                 true);

  }
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

