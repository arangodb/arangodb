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

#include "Aql/ExecutionPlan.h"
#include "Aql/IResearchViewOptimizerRules.h"
#include "Aql/OptimizerRules.h"
#include "Basics/Exceptions.h"
#include "Cluster/ServerState.h"
#include "OptimizerRulesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

#include <velocypack/StringRef.h>

using namespace arangodb::application_features;

namespace arangodb {
namespace aql {

// @brief list of all rules
std::map<int, OptimizerRule> OptimizerRulesFeature::_rules;

// @brief lookup from rule name to rule level
std::unordered_map<std::string, std::pair<int, bool>> OptimizerRulesFeature::_ruleLookup;

OptimizerRulesFeature::OptimizerRulesFeature(application_features::ApplicationServer& server)
    : application_features::ApplicationFeature(server, "OptimizerRules") {
  setOptional(false);
  startsAfter("V8Phase");
  startsAfter("Aql");
}

void OptimizerRulesFeature::prepare() { addRules(); }

void OptimizerRulesFeature::unprepare() {
  _ruleLookup.clear();
  _rules.clear();
}

/// @brief register a rule
void OptimizerRulesFeature::registerRule(std::string const& name, RuleFunction func,
                                         OptimizerRule::RuleLevel level,
                                         std::underlying_type<OptimizerRule::Flags>::type flags) {
  // duplicate rules are not allowed
  TRI_ASSERT(_ruleLookup.find(name) == _ruleLookup.end());
  TRI_ASSERT(_rules.find(level) == _rules.end());

  OptimizerRule rule(name, func, level, flags);

  if (rule.isClusterOnly() && !ServerState::instance()->isCoordinator()) {
    // cluster-only rule... however, we are not a coordinator, so we can just
    // ignore this rule
    return;
  }

  _ruleLookup.emplace(name, std::make_pair(level, rule.canBeDisabled()));
  _rules.emplace(level, std::move(rule));
}

/// @brief set up the optimizer rules once and forever
void OptimizerRulesFeature::addRules() {
  // List all the rules in the system here:
  // lower level values mean earlier rule execution

  // note that levels must be unique

  // determine the "right" type of CollectNode and
  // add a sort node for each COLLECT (may be removed later)
  // this rule cannot be turned off (otherwise, the query result might be
  // wrong!)
  registerRule("specialize-collect", specializeCollectRule, OptimizerRule::specializeCollectRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanCreateAdditionalPlans, OptimizerRule::Flags::Hidden));

  // inline subqueries one level higher
  registerRule("inline-subqueries", inlineSubqueriesRule, OptimizerRule::inlineSubqueriesRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // move calculations up the dependency chain (to pull them out of
  // inner loops etc.)
  registerRule("move-calculations-up", moveCalculationsUpRule, OptimizerRule::moveCalculationsUpRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // move filters up the dependency chain (to make result sets as small
  // as possible as early as possible)
  registerRule("move-filters-up", moveFiltersUpRule, OptimizerRule::moveFiltersUpRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // simplify conditions
  registerRule("simplify-conditions", simplifyConditionsRule, OptimizerRule::simplifyConditionsRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // remove redundant calculations
  registerRule("remove-redundant-calculations", removeRedundantCalculationsRule,
               OptimizerRule::removeRedundantCalculationsRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  /// "Pass 2": try to remove redundant or unnecessary nodes

  // remove filters from the query that are not necessary at all
  // filters that are always true will be removed entirely
  // filters that are always false will be replaced with a NoResults node
  registerRule("remove-unnecessary-filters", removeUnnecessaryFiltersRule,
               OptimizerRule::removeUnnecessaryFiltersRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // remove calculations that are never necessary
  registerRule("remove-unnecessary-calculations", removeUnnecessaryCalculationsRule,
               OptimizerRule::removeUnnecessaryCalculationsRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // remove redundant sort blocks
  registerRule("remove-redundant-sorts", removeRedundantSortsRule,
               OptimizerRule::removeRedundantSortsRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // push limits into subqueries and simplify them
  registerRule("optimize-subqueries", optimizeSubqueriesRule, 
               OptimizerRule::optimizeSubqueriesRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  /// "Pass 3": interchange EnumerateCollection nodes in all possible ways
  ///           this is level 500, please never let new plans from higher
  ///           levels go back to this or lower levels!
  registerRule("interchange-adjacent-enumerations", interchangeAdjacentEnumerationsRule,
               OptimizerRule::interchangeAdjacentEnumerationsRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanCreateAdditionalPlans, OptimizerRule::Flags::CanBeDisabled));

  // "Pass 4": moving nodes "up" (potentially outside loops) (second try):
  // move calculations up the dependency chain (to pull them out of
  // inner loops etc.)
  registerRule("move-calculations-up-2", moveCalculationsUpRule,
               OptimizerRule::moveCalculationsUpRule2,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // move filters up the dependency chain (to make result sets as small
  // as possible as early as possible)
  registerRule("move-filters-up-2", moveFiltersUpRule, 
               OptimizerRule::moveFiltersUpRule2,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // merge filters into traversals
  registerRule("optimize-traversals", optimizeTraversalsRule, 
               OptimizerRule::optimizeTraversalsRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // optimize unneccessary filters already applied by the traversal
  registerRule("remove-filter-covered-by-traversal", removeFiltersCoveredByTraversal,
               OptimizerRule::removeFiltersCoveredByTraversal,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // optimize unneccessary filters already applied by the traversal. Only ever
  // does something if previous rules remove all filters using the path variable
  registerRule("remove-redundant-path-var", removeTraversalPathVariable,
               OptimizerRule::removeTraversalPathVariable,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // prepare traversal info
  registerRule("prepare-traversals", prepareTraversalsRule,
               OptimizerRule::prepareTraversalsRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::Hidden));

  /// "Pass 5": try to remove redundant or unnecessary nodes (second try)
  // remove filters from the query that are not necessary at all
  // filters that are always true will be removed entirely
  // filters that are always false will be replaced with a NoResults node
  registerRule("remove-unnecessary-filters-2", removeUnnecessaryFiltersRule,
               OptimizerRule::removeUnnecessaryFiltersRule2,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // remove redundant sort node
  registerRule("remove-redundant-sorts-2", removeRedundantSortsRule,
               OptimizerRule::removeRedundantSortsRule2,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // remove unused INTO variable from COLLECT, or unused aggregates
  registerRule("remove-collect-variables", removeCollectVariablesRule,
               OptimizerRule::removeCollectVariablesRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // remove unused out variables for data-modification queries
  registerRule("remove-data-modification-out-variables", removeDataModificationOutVariablesRule,
               OptimizerRule::removeDataModificationOutVariablesRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // propagate constant attributes in FILTERs
  registerRule("propagate-constant-attributes", propagateConstantAttributesRule,
               OptimizerRule::propagateConstantAttributesRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  /// "Pass 6": use indexes if possible for FILTER and/or SORT nodes
  // try to replace simple OR conditions with IN
  registerRule("replace-or-with-in", replaceOrWithInRule, 
               OptimizerRule::replaceOrWithInRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // try to remove redundant OR conditions
  registerRule("remove-redundant-or", removeRedundantOrRule, OptimizerRule::removeRedundantOrRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // try to find a filter after an enumerate collection and find indexes
  registerRule("use-indexes", useIndexesRule, 
               OptimizerRule::useIndexesRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // try to remove filters which are covered by index ranges
  registerRule("remove-filter-covered-by-index", removeFiltersCoveredByIndexRule,
               OptimizerRule::removeFiltersCoveredByIndexRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // try to find sort blocks which are superseeded by indexes
  registerRule("use-index-for-sort", useIndexForSortRule, 
               OptimizerRule::useIndexForSortRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // sort in-values in filters (note: must come after
  // remove-filter-covered-by-index rule)
  registerRule("sort-in-values", sortInValuesRule, 
               OptimizerRule::sortInValuesRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // remove calculations that are never necessary
  registerRule("remove-unnecessary-calculations-2", removeUnnecessaryCalculationsRule,
               OptimizerRule::removeUnnecessaryCalculationsRule2,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // fuse multiple adjacent filters into one
  registerRule("fuse-filters", fuseFiltersRule, 
               OptimizerRule::fuseFiltersRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // finally, push calculations as far down as possible
  registerRule("move-calculations-down", moveCalculationsDownRule,
               OptimizerRule::moveCalculationsDownRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // patch update statements
  registerRule("patch-update-statements", patchUpdateStatementsRule,
               OptimizerRule::patchUpdateStatementsRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  registerRule("replace-function-with-index", replaceNearWithinFulltext,
               OptimizerRule::replaceNearWithinFulltext,
               OptimizerRule::makeFlags());
  
  // move filters and sort conditions into views
  registerRule("handle-arangosearch-views", arangodb::iresearch::handleViewsRule,
               OptimizerRule::handleArangoSearchViewsRule,
               OptimizerRule::makeFlags());

  registerRule("late-document-materialization", arangodb::iresearch::lateDocumentMaterializationRule,
               OptimizerRule::lateDocumentMaterializationRule,
               OptimizerRule::makeFlags());

  // remove FILTER DISTANCE(...) and SORT DISTANCE(...)
  registerRule("geo-index-optimizer", geoIndexRule,
               OptimizerRule::applyGeoIndexRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // make sort node aware of subsequent limit statements for internal optimizations
  registerRule("sort-limit", sortLimitRule,
               OptimizerRule::applySortLimitRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

#if 0
#ifdef USE_ENTERPRISE
  // must be the first cluster optimizer rule
  registerRule("cluster-one-shard", clusterOneShardRule,
               OptimizerRule::clusterOneShardRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled, OptimizerRule::Flags::ClusterOnly));
#endif
#endif

  registerRule("optimize-cluster-single-document-operations", substituteClusterSingleDocumentOperationsRule,
               OptimizerRule::substituteSingleDocumentOperations,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled, OptimizerRule::Flags::ClusterOnly));

  // distribute operations in cluster
  registerRule("scatter-in-cluster", scatterInClusterRule, 
               OptimizerRule::scatterInClusterRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::ClusterOnly));

  registerRule("distribute-in-cluster", distributeInClusterRule,
               OptimizerRule::distributeInClusterRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::ClusterOnly));

  registerRule("collect-in-cluster", collectInClusterRule, 
               OptimizerRule::collectInClusterRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled, OptimizerRule::Flags::ClusterOnly));

  // distribute operations in cluster
  registerRule("distribute-filtercalc-to-cluster", distributeFilternCalcToClusterRule,
               OptimizerRule::distributeFilternCalcToClusterRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled, OptimizerRule::Flags::ClusterOnly));

  registerRule("distribute-sort-to-cluster", distributeSortToClusterRule,
               OptimizerRule::distributeSortToClusterRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled, OptimizerRule::Flags::ClusterOnly));

  registerRule("remove-unnecessary-remote-scatter", removeUnnecessaryRemoteScatterRule,
               OptimizerRule::removeUnnecessaryRemoteScatterRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled, OptimizerRule::Flags::ClusterOnly));

  registerRule("undistribute-remove-after-enum-coll", undistributeRemoveAfterEnumCollRule,
               OptimizerRule::undistributeRemoveAfterEnumCollRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled, OptimizerRule::Flags::ClusterOnly));

#ifdef USE_ENTERPRISE
  registerRule("remove-satellite-joins", removeSatelliteJoinsRule,
               OptimizerRule::removeSatelliteJoinsRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled, OptimizerRule::Flags::ClusterOnly));
  
  registerRule("smart-joins", smartJoinsRule,
               OptimizerRule::smartJoinsRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled, OptimizerRule::Flags::ClusterOnly));
#endif

  // distribute view queries in cluster
  registerRule("scatter-arangosearch-view-in-cluster", arangodb::iresearch::scatterViewInClusterRule,
               OptimizerRule::scatterIResearchViewInClusterRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::ClusterOnly));

  registerRule("restrict-to-single-shard", restrictToSingleShardRule,
               OptimizerRule::restrictToSingleShardRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled, OptimizerRule::Flags::ClusterOnly));

  // finally add the storage-engine specific rules
  addStorageEngineRules();
}

void OptimizerRulesFeature::addStorageEngineRules() {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);  // Engine not loaded. Startup broken
  engine->addOptimizerRules();
}

/// @brief translate a list of rule ids into rule names
std::vector<std::string> OptimizerRulesFeature::translateRules(std::vector<int> const& rules) {
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
std::unordered_set<int> OptimizerRulesFeature::getDisabledRuleIds(std::vector<std::string> const& names) {
  std::unordered_set<int> disabled;

  // lookup ids of all disabled rules
  for (auto const& name : names) {
    if (name.empty()) {
      continue;
    }
    if (name[0] == '-') {
      disableRule(name, disabled);
    } else {
      enableRule(name, disabled);
    }
  }

  return disabled;
}

void OptimizerRulesFeature::disableRule(std::string const& name,
                                        std::unordered_set<int>& disabled) {
  TRI_ASSERT(name[0] == '-');
  char const* p = name.data() + 1;
  size_t size = name.size() - 1;

  if (arangodb::velocypack::StringRef(p, size) == "all") {
    // disable all rules
    for (auto const& it : _rules) {
      if (it.second.canBeDisabled()) {
        disabled.emplace(it.first);
      }
    }
  } else {
    // disable a specific rule
    auto it = _ruleLookup.find(p);

    if (it != _ruleLookup.end() && (*it).second.second) {
      // can be disabled
      disabled.emplace((*it).second.first);
    }
  }
}

void OptimizerRulesFeature::enableRule(std::string const& name,
                                       std::unordered_set<int>& disabled) {
  char const* p = name.data();
  size_t size = name.size();

  if (name[0] == '+') {
    ++p;
    --size;
  }

  if (arangodb::velocypack::StringRef(p, size) == "all") {
    // enable all rules
    disabled.clear();
  } else {
    auto it = _ruleLookup.find(p);

    if (it != _ruleLookup.end()) {
      disabled.erase((*it).second.first);
    }
  }
}

}  // namespace aql
}  // namespace arangodb
