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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "Optimizer.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/OptimizerRules.h"
#include "Cluster/ServerState.h"

using namespace arangodb::aql;

static bool const CreatesAdditionalPlans = true;
static bool const DoesNotCreateAdditionalPlans = false;

arangodb::Mutex Optimizer::SetupLock;

// @brief list of all rules
std::map<int, Optimizer::Rule> Optimizer::_rules;

// @brief lookup from rule name to rule level
std::unordered_map<std::string, int> Optimizer::_ruleLookup;

// @brief constructor, this will initialize the rules database
Optimizer::Optimizer(size_t maxNumberOfPlans)
    : _maxNumberOfPlans(maxNumberOfPlans > 0 ? maxNumberOfPlans
                                             : DefaultMaxNumberOfPlans) {
  if (_rules.empty()) {
    setupRules();
  }
}

// @brief add a plan to the optimizer
bool Optimizer::addPlan(ExecutionPlan* plan, Rule const* rule, bool wasModified,
                        int newLevel) {
  TRI_ASSERT(plan != nullptr);

  if (newLevel > 0) {
    // use user-specified new level
    _newPlans.push_back(plan, newLevel);
  } else {
    // use rule's level
    _newPlans.push_back(plan, rule->level);
  }

  if (wasModified) {
    if (!rule->isHidden) {
      // register which rules modified / created the plan
      // hidden rules are excluded here
      plan->addAppliedRule(static_cast<int>(rule->level));
    }

    plan->clearVarUsageComputed();
    plan->invalidateCost();
    plan->findVarUsage();
  }

  if (_newPlans.size() >= _maxNumberOfPlans) {
    return false;
  }

  return true;
}

// @brief the actual optimization
int Optimizer::createPlans(ExecutionPlan* plan,
                           std::vector<std::string> const& rulesSpecification,
                           bool inspectSimplePlans) {
  if (!inspectSimplePlans &&
      !arangodb::ServerState::instance()->isCoordinator() &&
      plan->isDeadSimple()) {
    // the plan is so simple that any further optimizations would probably cost
    // more than simply executing the plan
    _plans.clear();
    try {
      _plans.push_back(plan, 0);
    } catch (...) {
      delete plan;
      throw;
    }

    estimatePlans();

    return TRI_ERROR_NO_ERROR;
  }

  bool runOnlyRequiredRules = false;
  int leastDoneLevel = 0;

  TRI_ASSERT(!_rules.empty());
  int maxRuleLevel = _rules.rbegin()->first;

  // which optimizer rules are disabled?
  std::unordered_set<int> disabledIds(getDisabledRuleIds(rulesSpecification));

  // _plans contains the previous optimization result
  _plans.clear();
  try {
    _plans.push_back(plan, 0);
  } catch (...) {
    delete plan;
    throw;
  }

  _newPlans.clear();

  while (leastDoneLevel < maxRuleLevel) {
    // std::cout << "Have " << _plans.size() << " plans:" << std::endl;
    // for (auto const& p : _plans.list) {
    //   p->show();
    //   std::cout << std::endl;
    // }

    // int count = 0;

    // For all current plans:
    while (_plans.size() > 0) {
      int level;
      auto p = _plans.pop_front(level);

      if (level >= maxRuleLevel) {
        _newPlans.push_back(p, level);  // nothing to do, just keep it
      } else {                          // find next rule
        auto it = _rules.upper_bound(level);
        TRI_ASSERT(it != _rules.end());

        // std::cout << "Trying rule " << it->second.name << " with level "
        //          << it->first << " on plan " << count++
        //          << std::endl;

        level = (*it).first;
        auto& rule = (*it).second;

        // skip over rules if we should
        // however, we don't want to skip those rules that will not create
        // additional plans
        if (((runOnlyRequiredRules && rule.canCreateAdditionalPlans) ||
             disabledIds.find(level) != disabledIds.end()) &&
            rule.canBeDisabled) {
          // we picked a disabled rule or we have reached the max number of
          // plans and just skip this rule

          _newPlans.push_back(p, level);  // nothing to do, just keep it

          if (!rule.isHidden) {
            ++_stats.rulesSkipped;
          }
          // now try next
          continue;
        }

        try {
          TRI_IF_FAILURE("Optimizer::createPlansOom") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }

          if (!p->varUsageComputed()) {
            p->findVarUsage();
          }

          // all optimizer rule functions must obey the following guidelines:
          // - the original plan passed to the rule function must be deleted if
          // and only
          //   if it has not been added (back) to the optimizer (using addPlan).
          // - if the rule throws, then the original plan will be deleted by the
          // optimizer.
          //   thus the rule must not have deleted the plan itself or add it
          //   back to the
          //   optimizer
          rule.func(this, p, &rule);

          if (!rule.isHidden) {
            ++_stats.rulesExecuted;
          }
        } catch (...) {
          if (!_newPlans.isContained(p)) {
            // only delete the plan if not yet contained in _newPlans
            delete p;
          }
          throw;
        }
      }

      // future optimization: abort early here if we found a good-enough plan
      // a good-enough plan is probably every plan with costs below some
      // defined threshold. this requires plan costs to be calculated here
    }

    _plans.steal(_newPlans);
    leastDoneLevel = maxRuleLevel;
    for (auto const& l : _plans.levelDone) {
      if (l < leastDoneLevel) {
        leastDoneLevel = l;
      }
    }
    // std::cout << "Least done level is " << leastDoneLevel << std::endl;

    // Stop if the result gets out of hand:
    if (!runOnlyRequiredRules && _plans.size() >= _maxNumberOfPlans) {
      // must still iterate over all REQUIRED remaining transformation rules
      // because there are some rules which are required to make the query
      // work in cluster mode etc
      runOnlyRequiredRules = true;
    }
  }

  _stats.plansCreated = _plans.size();

  TRI_ASSERT(_plans.size() >= 1);

  estimatePlans();
  sortPlans();
#if 0
  // Only for debugging:
  // std::cout << "Optimization ends with " << _plans.size() << " plans."
  //           << std::endl;
  // for (auto const& p : _plans.list) {
  //   p->show();
  //   std::cout << "costing: " << p->getCost() << std::endl;
  //   std::cout << std::endl;
  // }
#endif

  return TRI_ERROR_NO_ERROR;
}

/// @brief translate a list of rule ids into rule names
std::vector<std::string> Optimizer::translateRules(
    std::vector<int> const& rules) {
  std::vector<std::string> names;
  names.reserve(rules.size());

  for (auto const& rule : rules) {
    char const* name = translateRule(rule);

    if (name != nullptr) {
      names.emplace_back(std::string(name));
    }
  }
  return names;
}

/// @brief translate a single rule
char const* Optimizer::translateRule(int rule) {
  auto it = _rules.find(rule);

  if (it != _rules.end()) {
    return (*it).second.name.c_str();
  }

  return nullptr;
}

/// @brief estimatePlans
void Optimizer::estimatePlans() {
  for (auto& p : _plans.list) {
    if (!p->varUsageComputed()) {
      p->findVarUsage();
    }
    p->getCost();
    // this value is cached in the plan, so formally this step is
    // unnecessary, but for the sake of cleanliness...
  }
}

/// @brief sortPlans
void Optimizer::sortPlans() {
  std::sort(_plans.list.begin(), _plans.list.end(),
            [](ExecutionPlan* const& a, ExecutionPlan* const& b)
                -> bool { return a->getCost() < b->getCost(); });
}

/// @brief look up the ids of all disabled rules
std::unordered_set<int> Optimizer::getDisabledRuleIds(
    std::vector<std::string> const& names) const {
  std::unordered_set<int> disabled;

  // lookup ids of all disabled rules
  for (auto const& name : names) {
    if (name[0] == '-') {
      // disable rule
      if (name == "-all") {
        // disable all rules
        for (auto const& it : _rules) {
          disabled.emplace(it.first);
        }
      } else {
        // disable a specific rule
        auto it = _ruleLookup.find(name.c_str() + 1);

        if (it != _ruleLookup.end()) {
          disabled.emplace((*it).second);
        }
      }
    } else if (name[0] == '+') {
      // enable rule
      if (name == "+all") {
        // enable all rules
        disabled.clear();
      } else {
        auto it = _ruleLookup.find(name.c_str() + 1);

        if (it != _ruleLookup.end()) {
          disabled.erase((*it).second);
        }
      }
    }
  }

  return disabled;
}

/// @brief set up the optimizer rules once and forever
void Optimizer::setupRules() {
  MUTEX_LOCKER(mutexLocker, SetupLock);

  if (!_rules.empty()) {
    // race condition... prevent duplicate registration of rules
    return;
  }

// List all the rules in the system here:
// lower level values mean earlier rule execution

// note that levels must be unique

// "Pass 1": moving nodes "up" (potentially outside loops):
#if 0
  // rule not yet tested
  registerRule("split-filters",
               splitFiltersRule,
               splitFiltersRule_pass1, 
               true);
#endif

  // determine the "right" type of CollectNode and
  // add a sort node for each COLLECT (may be removed later)
  // this rule cannot be turned off (otherwise, the query result might be
  // wrong!)
  registerHiddenRule("specialize-collect", specializeCollectRule,
                     specializeCollectRule_pass1, CreatesAdditionalPlans, false);

  // inline subqueries one level higher
  registerRule("inline-subqueries", inlineSubqueriesRule,
               inlineSubqueriesRule_pass1, DoesNotCreateAdditionalPlans, true);

  // move calculations up the dependency chain (to pull them out of
  // inner loops etc.)
  registerRule("move-calculations-up", moveCalculationsUpRule,
               moveCalculationsUpRule_pass1, DoesNotCreateAdditionalPlans, true);

  // move filters up the dependency chain (to make result sets as small
  // as possible as early as possible)
  registerRule("move-filters-up", moveFiltersUpRule, moveFiltersUpRule_pass1,
               DoesNotCreateAdditionalPlans, true);

  // remove redundant calculations
  registerRule("remove-redundant-calculations", removeRedundantCalculationsRule,
               removeRedundantCalculationsRule_pass1, DoesNotCreateAdditionalPlans, true);

  /// "Pass 2": try to remove redundant or unnecessary nodes
  // remove filters from the query that are not necessary at all
  // filters that are always true will be removed entirely
  // filters that are always false will be replaced with a NoResults node
  registerRule("remove-unnecessary-filters", removeUnnecessaryFiltersRule,
               removeUnnecessaryFiltersRule_pass2, DoesNotCreateAdditionalPlans, true);

  // remove calculations that are never necessary
  registerRule("remove-unnecessary-calculations",
               removeUnnecessaryCalculationsRule,
               removeUnnecessaryCalculationsRule_pass2, DoesNotCreateAdditionalPlans, true);

  // remove redundant sort blocks
  registerRule("remove-redundant-sorts", removeRedundantSortsRule,
               removeRedundantSortsRule_pass2, DoesNotCreateAdditionalPlans, true);

  /// "Pass 3": interchange EnumerateCollection nodes in all possible ways
  ///           this is level 500, please never let new plans from higher
  ///           levels go back to this or lower levels!
  registerRule("interchange-adjacent-enumerations",
               interchangeAdjacentEnumerationsRule,
               interchangeAdjacentEnumerationsRule_pass3, CreatesAdditionalPlans, true);

  // "Pass 4": moving nodes "up" (potentially outside loops) (second try):
  // move calculations up the dependency chain (to pull them out of
  // inner loops etc.)
  registerRule("move-calculations-up-2", moveCalculationsUpRule,
               moveCalculationsUpRule_pass4, DoesNotCreateAdditionalPlans, true);

  // move filters up the dependency chain (to make result sets as small
  // as possible as early as possible)
  registerRule("move-filters-up-2", moveFiltersUpRule, moveFiltersUpRule_pass4,
               DoesNotCreateAdditionalPlans, true);

  // merge filters into traversals
  registerRule("optimize-traversals", optimizeTraversalsRule,
               optimizeTraversalsRule_pass6, DoesNotCreateAdditionalPlans, true);

  /// "Pass 5": try to remove redundant or unnecessary nodes (second try)
  // remove filters from the query that are not necessary at all
  // filters that are always true will be removed entirely
  // filters that are always false will be replaced with a NoResults node
  registerRule("remove-unnecessary-filters-2", removeUnnecessaryFiltersRule,
               removeUnnecessaryFiltersRule_pass6, DoesNotCreateAdditionalPlans, true);

  // remove redundant sort node
  registerRule("remove-redundant-sorts-2", removeRedundantSortsRule,
               removeRedundantSortsRule_pass5, DoesNotCreateAdditionalPlans, true);

  // SORT RAND() if appropriate
  registerRule("remove-sort-rand", removeSortRandRule, removeSortRandRule_pass5,
               DoesNotCreateAdditionalPlans, true);

  // remove unused INTO variable from COLLECT, or unused aggregates
  registerRule("remove-collect-variables", removeCollectVariablesRule,
               removeCollectVariablesRule_pass5, DoesNotCreateAdditionalPlans, true);

  // remove unused out variables for data-modification queries
  registerRule("remove-data-modification-out-variables",
               removeDataModificationOutVariablesRule,
               removeDataModificationOutVariablesRule_pass5, DoesNotCreateAdditionalPlans, true);

  // propagate constant attributes in FILTERs
  registerRule("propagate-constant-attributes", propagateConstantAttributesRule,
               propagateConstantAttributesRule_pass5, DoesNotCreateAdditionalPlans, true);

  /// "Pass 6": use indexes if possible for FILTER and/or SORT nodes
  // try to replace simple OR conditions with IN
  registerRule("replace-or-with-in", replaceOrWithInRule,
               replaceOrWithInRule_pass6, DoesNotCreateAdditionalPlans, true);

  // try to remove redundant OR conditions
  registerRule("remove-redundant-or", removeRedundantOrRule,
               removeRedundantOrRule_pass6, DoesNotCreateAdditionalPlans, true);

  // try to find a filter after an enumerate collection and find indexes
  registerRule("use-indexes", useIndexesRule, useIndexesRule_pass6, DoesNotCreateAdditionalPlans, true);

  // try to remove filters which are covered by index ranges
  registerRule("remove-filter-covered-by-index",
               removeFiltersCoveredByIndexRule,
               removeFiltersCoveredByIndexRule_pass6, DoesNotCreateAdditionalPlans, true);

  // try to find sort blocks which are superseeded by indexes
  registerRule("use-index-for-sort", useIndexForSortRule,
               useIndexForSortRule_pass6, DoesNotCreateAdditionalPlans, true);

  // sort in-values in filters (note: must come after
  // remove-filter-covered-by-index rule)
  registerRule("sort-in-values", sortInValuesRule, sortInValuesRule_pass6,
               DoesNotCreateAdditionalPlans, true);

  // remove calculations that are never necessary
  registerRule("remove-unnecessary-calculations-2",
               removeUnnecessaryCalculationsRule,
               removeUnnecessaryCalculationsRule_pass6, DoesNotCreateAdditionalPlans, true);

  // finally, push calculations as far down as possible
  registerRule("move-calculations-down", moveCalculationsDownRule,
               moveCalculationsDownRule_pass9, DoesNotCreateAdditionalPlans, true);

  // patch update statements
  registerRule("patch-update-statements", patchUpdateStatementsRule,
               patchUpdateStatementsRule_pass9, DoesNotCreateAdditionalPlans, true);

  if (arangodb::ServerState::instance()->isCoordinator()) {
    // distribute operations in cluster
    registerRule("scatter-in-cluster", scatterInClusterRule,
                 scatterInClusterRule_pass10, DoesNotCreateAdditionalPlans, false);

    registerRule("distribute-in-cluster", distributeInClusterRule,
                 distributeInClusterRule_pass10, DoesNotCreateAdditionalPlans, false);

    // distribute operations in cluster
    registerRule("distribute-filtercalc-to-cluster",
                 distributeFilternCalcToClusterRule,
                 distributeFilternCalcToClusterRule_pass10, DoesNotCreateAdditionalPlans, true);

    registerRule("distribute-sort-to-cluster", distributeSortToClusterRule,
                 distributeSortToClusterRule_pass10, DoesNotCreateAdditionalPlans, true);

    registerRule("remove-unnecessary-remote-scatter",
                 removeUnnecessaryRemoteScatterRule,
                 removeUnnecessaryRemoteScatterRule_pass10, DoesNotCreateAdditionalPlans, true);

    registerRule("undistribute-remove-after-enum-coll",
                 undistributeRemoveAfterEnumCollRule,
                 undistributeRemoveAfterEnumCollRule_pass10, DoesNotCreateAdditionalPlans, true);
  }
}
