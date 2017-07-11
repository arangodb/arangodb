////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRulesFeature.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/OptimizerRules.h"
#include "Basics/Exceptions.h"
#include "Cluster/ServerState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::aql;

// @brief list of all rules
std::map<int, OptimizerRule> OptimizerRulesFeature::_rules;

// @brief lookup from rule name to rule level
std::unordered_map<std::string, int> OptimizerRulesFeature::_ruleLookup;

constexpr bool CreatesAdditionalPlans = true;
constexpr bool DoesNotCreateAdditionalPlans = false;
constexpr bool CanBeDisabled = true;
constexpr bool CanNotBeDisabled = false;

OptimizerRulesFeature::OptimizerRulesFeature(
    application_features::ApplicationServer* server)
    : application_features::ApplicationFeature(server, "OptimizerRules") {
  setOptional(false);
  startsAfter("EngineSelector");
  startsAfter("Aql");
  startsAfter("Cluster");
}

void OptimizerRulesFeature::prepare() {
  addRules();
}

/// @brief register a rule
void OptimizerRulesFeature::registerRule(std::string const& name, RuleFunction func,
                                         OptimizerRule::RuleLevel level, bool canCreateAdditionalPlans,
                                         bool canBeDisabled, bool isHidden) {
  if (_ruleLookup.find(name) != _ruleLookup.end()) {
    // duplicate rule names are not allowed
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                    "duplicate optimizer rule name");
  }

  _ruleLookup.emplace(name, level);

  if (_rules.find(level) != _rules.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                    "duplicate optimizer rule level");
  }

  _rules.emplace(level, OptimizerRule(name, func, level, canCreateAdditionalPlans, canBeDisabled, isHidden));
}

/// @brief set up the optimizer rules once and forever
void OptimizerRulesFeature::addRules() {
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
                     OptimizerRule::specializeCollectRule_pass1, CreatesAdditionalPlans, CanNotBeDisabled);

  // inline subqueries one level higher
  registerRule("inline-subqueries", inlineSubqueriesRule,
               OptimizerRule::inlineSubqueriesRule_pass1, DoesNotCreateAdditionalPlans, CanBeDisabled);

  // move calculations up the dependency chain (to pull them out of
  // inner loops etc.)
  registerRule("move-calculations-up", moveCalculationsUpRule,
               OptimizerRule::moveCalculationsUpRule_pass1, DoesNotCreateAdditionalPlans, CanBeDisabled);

  // move filters up the dependency chain (to make result sets as small
  // as possible as early as possible)
  registerRule("move-filters-up", moveFiltersUpRule, OptimizerRule::moveFiltersUpRule_pass1,
               DoesNotCreateAdditionalPlans, CanBeDisabled);

  // remove redundant calculations
  registerRule("remove-redundant-calculations", removeRedundantCalculationsRule,
               OptimizerRule::removeRedundantCalculationsRule_pass1, DoesNotCreateAdditionalPlans, CanBeDisabled);

  /// "Pass 2": try to remove redundant or unnecessary nodes
  // remove filters from the query that are not necessary at all
  // filters that are always true will be removed entirely
  // filters that are always false will be replaced with a NoResults node
  registerRule("remove-unnecessary-filters", removeUnnecessaryFiltersRule,
               OptimizerRule::removeUnnecessaryFiltersRule_pass2, DoesNotCreateAdditionalPlans, CanBeDisabled);

  // remove calculations that are never necessary
  registerRule("remove-unnecessary-calculations",
               removeUnnecessaryCalculationsRule,
               OptimizerRule::removeUnnecessaryCalculationsRule_pass2, DoesNotCreateAdditionalPlans, CanBeDisabled);

  // remove redundant sort blocks
  registerRule("remove-redundant-sorts", removeRedundantSortsRule,
               OptimizerRule::removeRedundantSortsRule_pass2, DoesNotCreateAdditionalPlans, CanBeDisabled);

  /// "Pass 3": interchange EnumerateCollection nodes in all possible ways
  ///           this is level 500, please never let new plans from higher
  ///           levels go back to this or lower levels!
  registerRule("interchange-adjacent-enumerations",
               interchangeAdjacentEnumerationsRule,
               OptimizerRule::interchangeAdjacentEnumerationsRule_pass3, CreatesAdditionalPlans, CanBeDisabled);

  // "Pass 4": moving nodes "up" (potentially outside loops) (second try):
  // move calculations up the dependency chain (to pull them out of
  // inner loops etc.)
  registerRule("move-calculations-up-2", moveCalculationsUpRule,
               OptimizerRule::moveCalculationsUpRule_pass4, DoesNotCreateAdditionalPlans, CanBeDisabled);

  // move filters up the dependency chain (to make result sets as small
  // as possible as early as possible)
  registerRule("move-filters-up-2", moveFiltersUpRule, OptimizerRule::moveFiltersUpRule_pass4,
               DoesNotCreateAdditionalPlans, CanBeDisabled);

  // merge filters into traversals
  registerRule("optimize-traversals", optimizeTraversalsRule,
               OptimizerRule::optimizeTraversalsRule_pass6, DoesNotCreateAdditionalPlans, CanBeDisabled);
  
  // optimize unneccessary filters already applied by the traversal
  registerRule("remove-filter-covered-by-traversal", removeFiltersCoveredByTraversal,
               OptimizerRule::removeFiltersCoveredByTraversal_pass6, DoesNotCreateAdditionalPlans, CanBeDisabled);
  
  // optimize unneccessary filters already applied by the traversal. Only ever does something if previous
  // rule remove all filters using the path variable
  registerRule("remove-redundant-path-var", removeTraversalPathVariable,
               OptimizerRule::removeTraversalPathVariable_pass6, DoesNotCreateAdditionalPlans, CanBeDisabled);

  // prepare traversal info
  registerHiddenRule("prepare-traversals", prepareTraversalsRule,
                     OptimizerRule::prepareTraversalsRule_pass6, DoesNotCreateAdditionalPlans, CanNotBeDisabled);

  /// "Pass 5": try to remove redundant or unnecessary nodes (second try)
  // remove filters from the query that are not necessary at all
  // filters that are always true will be removed entirely
  // filters that are always false will be replaced with a NoResults node
  registerRule("remove-unnecessary-filters-2", removeUnnecessaryFiltersRule,
               OptimizerRule::removeUnnecessaryFiltersRule_pass6, DoesNotCreateAdditionalPlans, CanBeDisabled);

  // remove redundant sort node
  registerRule("remove-redundant-sorts-2", removeRedundantSortsRule,
               OptimizerRule::removeRedundantSortsRule_pass5, DoesNotCreateAdditionalPlans, CanBeDisabled);

  // remove unused INTO variable from COLLECT, or unused aggregates
  registerRule("remove-collect-variables", removeCollectVariablesRule,
               OptimizerRule::removeCollectVariablesRule_pass5, DoesNotCreateAdditionalPlans, CanBeDisabled);

  // remove unused out variables for data-modification queries
  registerRule("remove-data-modification-out-variables",
               removeDataModificationOutVariablesRule,
               OptimizerRule::removeDataModificationOutVariablesRule_pass5, DoesNotCreateAdditionalPlans, CanBeDisabled);

  // propagate constant attributes in FILTERs
  registerRule("propagate-constant-attributes", propagateConstantAttributesRule,
               OptimizerRule::propagateConstantAttributesRule_pass5, DoesNotCreateAdditionalPlans, CanBeDisabled);

  /// "Pass 6": use indexes if possible for FILTER and/or SORT nodes
  // try to replace simple OR conditions with IN
  registerRule("replace-or-with-in", replaceOrWithInRule,
               OptimizerRule::replaceOrWithInRule_pass6, DoesNotCreateAdditionalPlans, CanBeDisabled);

  // try to remove redundant OR conditions
  registerRule("remove-redundant-or", removeRedundantOrRule,
               OptimizerRule::removeRedundantOrRule_pass6, DoesNotCreateAdditionalPlans, CanBeDisabled);

  // try to find a filter after an enumerate collection and find indexes
  registerRule("use-indexes", useIndexesRule, OptimizerRule::useIndexesRule_pass6, DoesNotCreateAdditionalPlans, CanBeDisabled);

  // try to remove filters which are covered by index ranges
  registerRule("remove-filter-covered-by-index",
               removeFiltersCoveredByIndexRule,
               OptimizerRule::removeFiltersCoveredByIndexRule_pass6, DoesNotCreateAdditionalPlans, CanBeDisabled);

  // try to find sort blocks which are superseeded by indexes
  registerRule("use-index-for-sort", useIndexForSortRule,
               OptimizerRule::useIndexForSortRule_pass6, DoesNotCreateAdditionalPlans, CanBeDisabled);
  
  // sort in-values in filters (note: must come after
  // remove-filter-covered-by-index rule)
  registerRule("sort-in-values", sortInValuesRule, OptimizerRule::sortInValuesRule_pass6,
               DoesNotCreateAdditionalPlans, CanBeDisabled);
  
  // remove calculations that are never necessary
  registerRule("remove-unnecessary-calculations-2",
               removeUnnecessaryCalculationsRule,
               OptimizerRule::removeUnnecessaryCalculationsRule_pass6, DoesNotCreateAdditionalPlans, CanBeDisabled);

  // finally, push calculations as far down as possible
  registerRule("move-calculations-down", moveCalculationsDownRule,
               OptimizerRule::moveCalculationsDownRule_pass9, DoesNotCreateAdditionalPlans, CanBeDisabled);

  // patch update statements
  registerRule("patch-update-statements", patchUpdateStatementsRule,
               OptimizerRule::patchUpdateStatementsRule_pass9, DoesNotCreateAdditionalPlans, CanBeDisabled);
  
  // patch update statements
  OptimizerRulesFeature::registerRule("geo-index-optimizer", geoIndexRule,
                                      OptimizerRule::applyGeoIndexRule, false, true);

  if (arangodb::ServerState::instance()->isCoordinator()) {
    // distribute operations in cluster
    registerRule("scatter-in-cluster", scatterInClusterRule,
                 OptimizerRule::scatterInClusterRule_pass10, DoesNotCreateAdditionalPlans, CanNotBeDisabled);

    registerRule("distribute-in-cluster", distributeInClusterRule,
                 OptimizerRule::distributeInClusterRule_pass10, DoesNotCreateAdditionalPlans, CanNotBeDisabled);

    // distribute operations in cluster
    registerRule("distribute-filtercalc-to-cluster",
                 distributeFilternCalcToClusterRule,
                 OptimizerRule::distributeFilternCalcToClusterRule_pass10, DoesNotCreateAdditionalPlans, CanBeDisabled);

    registerRule("distribute-sort-to-cluster", distributeSortToClusterRule,
                 OptimizerRule::distributeSortToClusterRule_pass10, DoesNotCreateAdditionalPlans, CanBeDisabled);

    registerRule("remove-unnecessary-remote-scatter",
                 removeUnnecessaryRemoteScatterRule,
                 OptimizerRule::removeUnnecessaryRemoteScatterRule_pass10, DoesNotCreateAdditionalPlans, CanBeDisabled);

    registerRule("undistribute-remove-after-enum-coll",
                 undistributeRemoveAfterEnumCollRule,
                 OptimizerRule::undistributeRemoveAfterEnumCollRule_pass10, DoesNotCreateAdditionalPlans, CanBeDisabled);

#ifdef USE_ENTERPRISE
    registerRule("remove-satellite-joins",
                 removeSatelliteJoinsRule,
                 OptimizerRule::removeSatelliteJoinsRule_pass10, DoesNotCreateAdditionalPlans, CanBeDisabled);
#endif
  }
  
  // finally add the storage-engine specific rules
  addStorageEngineRules();
}

void OptimizerRulesFeature::addStorageEngineRules() {
  StorageEngine* engine = EngineSelectorFeature::ENGINE; 
  TRI_ASSERT(engine != nullptr); // Engine not loaded. Startup broken
  engine->addOptimizerRules();
}

/// @brief translate a list of rule ids into rule names
std::vector<std::string> OptimizerRulesFeature::translateRules(
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
char const* OptimizerRulesFeature::translateRule(int rule) {
  auto it = _rules.find(rule);

  if (it != _rules.end()) {
    return (*it).second.name.c_str();
  }

  return nullptr;
}

/// @brief look up the ids of all disabled rules
std::unordered_set<int> OptimizerRulesFeature::getDisabledRuleIds(
    std::vector<std::string> const& names) {
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
