////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "OptimizerRulesFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/IResearchViewOptimizerRules.h"
#include "Aql/IndexNodeOptimizerRules.h"
#include "Aql/OptimizerRules.h"
#include "Basics/Exceptions.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

using namespace arangodb::application_features;

namespace arangodb {
namespace aql {

// @brief list of all rules, sorted by rule level
std::vector<OptimizerRule> OptimizerRulesFeature::_rules;

// @brief lookup from rule name to rule level
std::unordered_map<std::string_view, int> OptimizerRulesFeature::_ruleLookup;

OptimizerRulesFeature::OptimizerRulesFeature(Server& server)
    : ArangodFeature{server, *this}, _parallelizeGatherWrites(true) {
  setOptional(false);
  startsAfter<V8FeaturePhase>();

  startsAfter<AqlFeature>();
}

void OptimizerRulesFeature::collectOptions(
    std::shared_ptr<arangodb::options::ProgramOptions> options) {
  options
      ->addOption("--query.optimizer-rules",
                  "Enable or disable specific optimizer rules by default. "
                  "Specify the rule name prefixed with `-` for disabling, or "
                  "`+` for enabling.",
                  new arangodb::options::VectorParameter<
                      arangodb::options::StringParameter>(&_optimizerRules),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30600)
      .setLongDescription(R"(You can use this option to selectively enable or
disable AQL query optimizer rules by default. You can specify the option
multiple times.

For example, to turn off the rules `use-indexes-for-sort` and
`reduce-extraction-to-projection` by default, use the following:

```
--query.optimizer-rules "-use-indexes-for-sort" --query.optimizer-rules "-reduce-extraction-to-projection"
```

The purpose of this startup option is to be able to enable potential future
experimental optimizer rules, which may be shipped in a disabled-by-default
state.)");

  options
      ->addOption(
          "--query.parallelize-gather-writes",
          "Whether to enable write parallelization for gather nodes.",
          new arangodb::options::BooleanParameter(&_parallelizeGatherWrites))
      .setIntroducedIn(30600);
}

void OptimizerRulesFeature::prepare() {
  addRules();
  enableOrDisableRules();
}

void OptimizerRulesFeature::unprepare() {
  _ruleLookup.clear();
  _rules.clear();
}

OptimizerRule& OptimizerRulesFeature::ruleByLevel(int level) {
  // do a binary search in the sorted rules database
  auto it = std::lower_bound(_rules.begin(), _rules.end(), level);
  if (it != _rules.end() && (*it).level == level) {
    return (*it);
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "unable to find required OptimizerRule");
}

int OptimizerRulesFeature::ruleIndex(int level) {
  auto it = std::lower_bound(_rules.begin(), _rules.end(), level);
  if (it != _rules.end() && (*it).level == level) {
    return static_cast<int>(std::distance(_rules.begin(), it));
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "unable to find required OptimizerRule");
}

OptimizerRule& OptimizerRulesFeature::ruleByIndex(int index) {
  TRI_ASSERT(static_cast<size_t>(index) < _rules.size());
  if (static_cast<size_t>(index) < _rules.size()) {
    return _rules[index];
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "unable to find required OptimizerRule");
}

/// @brief register a rule
void OptimizerRulesFeature::registerRule(
    std::string_view name, RuleFunction func, OptimizerRule::RuleLevel level,
    std::underlying_type<OptimizerRule::Flags>::type flags) {
  // rules must only be added before start()
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(!_fixed);
#endif

  // duplicate rules are not allowed
  TRI_ASSERT(_ruleLookup.find(name) == _ruleLookup.end());

  LOG_TOPIC("18669", TRACE, Logger::AQL)
      << "adding optimizer rule '" << name << "' with level " << level;

  OptimizerRule rule(name, func, level, flags);

  if (rule.isClusterOnly() && !ServerState::instance()->isCoordinator()) {
    // cluster-only rule... however, we are not a coordinator, so we can just
    // ignore this rule
    return;
  }

  _ruleLookup.try_emplace(name, level);
  _rules.push_back(std::move(rule));
}

/// @brief set up the optimizer rules once and forever
void OptimizerRulesFeature::addRules() {
  // List all the rules in the system here:
  // lower level values mean earlier rule execution

  // note that levels must be unique

  registerRule("replace-function-with-index", replaceNearWithinFulltextRule,
               OptimizerRule::replaceNearWithinFulltext,
               OptimizerRule::makeFlags());

  // inline subqueries one level higher
  registerRule("inline-subqueries", inlineSubqueriesRule,
               OptimizerRule::inlineSubqueriesRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // simplify conditions
  registerRule("simplify-conditions", simplifyConditionsRule,
               OptimizerRule::simplifyConditionsRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // move calculations up the dependency chain (to pull them out of
  // inner loops etc.)
  registerRule("move-calculations-up", moveCalculationsUpRule,
               OptimizerRule::moveCalculationsUpRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // move filters up the dependency chain (to make result sets as small
  // as possible as early as possible)
  registerRule("move-filters-up", moveFiltersUpRule,
               OptimizerRule::moveFiltersUpRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // remove redundant calculations
  registerRule("remove-redundant-calculations", removeRedundantCalculationsRule,
               OptimizerRule::removeRedundantCalculationsRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // remove filters from the query that are not necessary at all
  // filters that are always true will be removed entirely
  // filters that are always false will be replaced with a NoResults node
  registerRule("remove-unnecessary-filters", removeUnnecessaryFiltersRule,
               OptimizerRule::removeUnnecessaryFiltersRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // remove calculations that are never necessary
  registerRule("remove-unnecessary-calculations",
               removeUnnecessaryCalculationsRule,
               OptimizerRule::removeUnnecessaryCalculationsRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // determine the "right" type of CollectNode and
  // add a sort node for each COLLECT (may be removed later)
  // this rule cannot be turned off (otherwise, the query result might be
  // wrong!)
  registerRule(
      "specialize-collect", specializeCollectRule,
      OptimizerRule::specializeCollectRule,
      OptimizerRule::makeFlags(OptimizerRule::Flags::CanCreateAdditionalPlans,
                               OptimizerRule::Flags::Hidden));

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
  registerRule(
      "interchange-adjacent-enumerations", interchangeAdjacentEnumerationsRule,
      OptimizerRule::interchangeAdjacentEnumerationsRule,
      OptimizerRule::makeFlags(OptimizerRule::Flags::CanCreateAdditionalPlans,
                               OptimizerRule::Flags::CanBeDisabled));

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

  // remove redundant sort node
  registerRule("remove-redundant-sorts-2", removeRedundantSortsRule,
               OptimizerRule::removeRedundantSortsRule2,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // remove unused INTO variable from COLLECT, or unused aggregates
  registerRule("remove-collect-variables", removeCollectVariablesRule,
               OptimizerRule::removeCollectVariablesRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // propagate constant attributes in FILTERs
  registerRule("propagate-constant-attributes", propagateConstantAttributesRule,
               OptimizerRule::propagateConstantAttributesRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // remove unused out variables for data-modification queries
  registerRule("remove-data-modification-out-variables",
               removeDataModificationOutVariablesRule,
               OptimizerRule::removeDataModificationOutVariablesRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  /// "Pass 6": use indexes if possible for FILTER and/or SORT nodes
  // try to replace simple OR conditions with IN
  registerRule("replace-or-with-in", replaceOrWithInRule,
               OptimizerRule::replaceOrWithInRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // try to remove redundant OR conditions
  registerRule("remove-redundant-or", removeRedundantOrRule,
               OptimizerRule::removeRedundantOrRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // remove FILTER DISTANCE(...) and SORT DISTANCE(...)
  registerRule("geo-index-optimizer", geoIndexRule,
               OptimizerRule::applyGeoIndexRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // try to find a filter after an enumerate collection and find indexes
  registerRule("use-indexes", useIndexesRule, OptimizerRule::useIndexesRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // try to remove filters which are covered by index ranges
  registerRule("remove-filter-covered-by-index",
               removeFiltersCoveredByIndexRule,
               OptimizerRule::removeFiltersCoveredByIndexRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  /// "Pass 5": try to remove redundant or unnecessary nodes (second try)
  // remove filters from the query that are not necessary at all
  // filters that are always true will be removed entirely
  // filters that are always false will be replaced with a NoResults node
  registerRule("remove-unnecessary-filters-2", removeUnnecessaryFiltersRule,
               OptimizerRule::removeUnnecessaryFiltersRule2,
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

  // merge filters into traversals
  registerRule("optimize-traversals", optimizeTraversalsRule,
               OptimizerRule::optimizeTraversalsRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // optimize unneccessary filters already applied by the traversal
  registerRule("remove-filter-covered-by-traversal",
               removeFiltersCoveredByTraversal,
               OptimizerRule::removeFiltersCoveredByTraversal,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // move search and scorers into views
  registerRule(
      "handle-arangosearch-views", arangodb::iresearch::handleViewsRule,
      OptimizerRule::handleArangoSearchViewsRule, OptimizerRule::makeFlags());

  // move constrained sort into views
  registerRule("arangosearch-constrained-sort",
               arangodb::iresearch::handleConstrainedSortInView,
               OptimizerRule::handleConstrainedSortInView,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // remove calculations that are never necessary
  registerRule("remove-unnecessary-calculations-2",
               removeUnnecessaryCalculationsRule,
               OptimizerRule::removeUnnecessaryCalculationsRule2,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // optimize unneccessary filters already applied by the traversal. Only ever
  // does something if previous rules remove all filters using the path variable
  registerRule("remove-redundant-path-var", removeTraversalPathVariable,
               OptimizerRule::removeTraversalPathVariable,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  registerRule("optimize-cluster-single-document-operations",
               substituteClusterSingleDocumentOperationsRule,
               OptimizerRule::substituteSingleDocumentOperations,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::ClusterOnly));

  // make sort node aware of subsequent limit statements for internal
  // optimizations
  registerRule("sort-limit", sortLimitRule, OptimizerRule::applySortLimitRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // finally, push calculations as far down as possible
  registerRule("move-calculations-down", moveCalculationsDownRule,
               OptimizerRule::moveCalculationsDownRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // fuse multiple adjacent filters into one
  registerRule("fuse-filters", fuseFiltersRule, OptimizerRule::fuseFiltersRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

#ifdef USE_ENTERPRISE
  // must be the first cluster optimizer rule
  registerRule("cluster-one-shard", clusterOneShardRule,
               OptimizerRule::clusterOneShardRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::ClusterOnly,
                                        OptimizerRule::Flags::EnterpriseOnly));
#endif

#ifdef USE_ENTERPRISE
  // must run before distribute-in-cluster and must not be disabled, as it is
  // necessary for distributing SmartGraph operations
  registerRule("cluster-lift-constant-for-disjoint-graph-nodes",
               clusterLiftConstantsForDisjointGraphNodes,
               OptimizerRule::clusterLiftConstantsForDisjointGraphNodes,
               OptimizerRule::makeFlags(OptimizerRule::Flags::ClusterOnly,
                                        OptimizerRule::Flags::EnterpriseOnly));
#endif

  registerRule("distribute-in-cluster", distributeInClusterRule,
               OptimizerRule::distributeInClusterRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::ClusterOnly));

#ifdef USE_ENTERPRISE
  registerRule("smart-joins", smartJoinsRule, OptimizerRule::smartJoinsRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::ClusterOnly,
                                        OptimizerRule::Flags::EnterpriseOnly));
#endif

  // distribute operations in cluster
  registerRule("scatter-in-cluster", scatterInClusterRule,
               OptimizerRule::scatterInClusterRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::ClusterOnly));

#ifdef USE_ENTERPRISE
  registerRule("distribute-offset-info-to-cluster",
               distributeOffsetInfoToClusterRule,
               OptimizerRule::distributeOffsetInfoToClusterRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::ClusterOnly,
                                        OptimizerRule::Flags::EnterpriseOnly));
#endif

  registerRule("distribute-filtercalc-to-cluster",
               distributeFilterCalcToClusterRule,
               OptimizerRule::distributeFilterCalcToClusterRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::ClusterOnly));

  registerRule("distribute-sort-to-cluster", distributeSortToClusterRule,
               OptimizerRule::distributeSortToClusterRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::ClusterOnly));

  // remove calculations that are never necessary
  registerRule("remove-unnecessary-calculations-3",
               removeUnnecessaryCalculationsRule,
               OptimizerRule::removeUnnecessaryCalculationsRule3,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::DisabledByDefault,
                                        OptimizerRule::Flags::Hidden));

  registerRule("remove-unnecessary-remote-scatter",
               removeUnnecessaryRemoteScatterRule,
               OptimizerRule::removeUnnecessaryRemoteScatterRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::ClusterOnly));

#ifdef USE_ENTERPRISE
  registerRule("scatter-satellite-graphs", scatterSatelliteGraphRule,
               OptimizerRule::scatterSatelliteGraphRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::ClusterOnly,
                                        OptimizerRule::Flags::EnterpriseOnly));

  registerRule("remove-satellite-joins", removeSatelliteJoinsRule,
               OptimizerRule::removeSatelliteJoinsRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::ClusterOnly,
                                        OptimizerRule::Flags::EnterpriseOnly));

  registerRule("remove-distribute-nodes", removeDistributeNodesRule,
               OptimizerRule::removeDistributeNodesRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::ClusterOnly,
                                        OptimizerRule::Flags::EnterpriseOnly));
#endif

  registerRule("undistribute-remove-after-enum-coll",
               undistributeRemoveAfterEnumCollRule,
               OptimizerRule::undistributeRemoveAfterEnumCollRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::ClusterOnly));

  registerRule("collect-in-cluster", collectInClusterRule,
               OptimizerRule::collectInClusterRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::ClusterOnly));

  registerRule("restrict-to-single-shard", restrictToSingleShardRule,
               OptimizerRule::restrictToSingleShardRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::ClusterOnly));

  registerRule("move-filters-into-enumerate", moveFiltersIntoEnumerateRule,
               OptimizerRule::moveFiltersIntoEnumerateRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  registerRule("optimize-count", optimizeCountRule,
               OptimizerRule::optimizeCountRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  registerRule("parallelize-gather", parallelizeGatherRule,
               OptimizerRule::parallelizeGatherRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::ClusterOnly));

  registerRule("decay-unnecessary-sorted-gather", decayUnnecessarySortedGather,
               OptimizerRule::decayUnnecessarySortedGatherRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::ClusterOnly));

#ifdef USE_ENTERPRISE
  registerRule("push-subqueries-to-dbserver", clusterPushSubqueryToDBServer,
               OptimizerRule::clusterPushSubqueryToDBServer,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::ClusterOnly,
                                        OptimizerRule::Flags::EnterpriseOnly));
#endif

  // apply late materialization for view queries
  registerRule("late-document-materialization-arangosearch",
               iresearch::lateDocumentMaterializationArangoSearchRule,
               OptimizerRule::lateDocumentMaterializationArangoSearchRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

  // apply late materialization for index queries
  registerRule("late-document-materialization", lateDocumentMaterializationRule,
               OptimizerRule::lateDocumentMaterializationRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled));

#ifdef USE_ENTERPRISE
  // apply late materialization for offset infos
  registerRule("late-materialization-offset-info",
               lateMaterialiationOffsetInfoRule,
               OptimizerRule::lateMaterialiationOffsetInfoRule,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::EnterpriseOnly));
#endif

  // add the storage-engine specific rules
  addStorageEngineRules();

  // Splice subqueries
  //
  // ***CAUTION***
  // TL;DR: This rule (if activated) *must* run last.
  //
  // It changes the structure of the query plan by "splicing", i.e. replacing
  // every SubqueryNode by a SubqueryStart and a SubqueryEnd node with the
  // subquery's nodes in between, resulting in a linear query plan. If an
  // optimizer rule runs after this rule, it has to be aware of
  // SubqueryStartNode and SubqueryEndNode and would likely be more complicated
  // to write. note: since 3.8 this rule cannot be turned off anymore. this also
  // means that any query execution plan created by 3.8 will not contain nodes
  // of type SUBQUERY after optimization. it is still possible that 3.8
  // encounters plan with SUBQUERY node types inside, e.g. if they come from 3.7
  // coordinators during rolling upgrades.
  registerRule("splice-subqueries", spliceSubqueriesRule,
               OptimizerRule::spliceSubqueriesRule, OptimizerRule::makeFlags());

  // allow nodes to asynchronously prefetch the next batch while processing the
  // current batch. this effectively allows parts of the query to run in
  // parallel, but as some internal details are currently not guaranteed to be
  // thread safe (e.g., TransactionState), this is currently disabled, and
  // should only be activated for experimental usage at one's own risk.
  registerRule("async-prefetch", asyncPrefetchRule,
               OptimizerRule::asyncPrefetch,
               OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled,
                                        OptimizerRule::Flags::DisabledByDefault,
                                        OptimizerRule::Flags::Hidden));

  // finally sort all rules by their level
  std::sort(_rules.begin(), _rules.end(),
            [](OptimizerRule const& lhs, OptimizerRule const& rhs) noexcept {
              return (lhs.level < rhs.level);
            });

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // make the rules database read-only from now on
  _fixed = true;
#endif
}

void OptimizerRulesFeature::addStorageEngineRules() {
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  engine.addOptimizerRules(*this);
}

/// @brief translate a list of rule ids into rule names
std::vector<std::string_view> OptimizerRulesFeature::translateRules(
    std::vector<int> const& rules) {
  std::vector<std::string_view> names;
  names.reserve(rules.size());

  for (auto const& rule : rules) {
    std::string_view name = translateRule(rule);

    if (!name.empty()) {
      names.emplace_back(name);
    }
  }
  return names;
}

/// @brief translate a single rule
std::string_view OptimizerRulesFeature::translateRule(int level) {
  // do a binary search in the sorted rules database
  auto it = std::lower_bound(_rules.begin(), _rules.end(), level);
  if (it != _rules.end() && (*it).level == level) {
    return (*it).name;
  }

  return std::string_view();
}

/// @brief translate a single rule
int OptimizerRulesFeature::translateRule(std::string_view name) {
  auto it = _ruleLookup.find(name);

  if (it != _ruleLookup.end()) {
    return (*it).second;
  }

  return -1;
}

void OptimizerRulesFeature::enableOrDisableRules() {
  // turn off or on specific optimizer rules, based on startup parameters
  for (auto const& name : _optimizerRules) {
    std::string_view n(name);
    if (!n.empty() && n.front() == '+') {
      // strip initial + sign
      n = n.substr(1);
    }

    if (n.empty()) {
      continue;
    }

    if (n.front() == '-') {
      n = n.substr(1);
      // disable
      if (n == "all") {
        for (auto& rule : _rules) {
          if (rule.hasFlag(OptimizerRule::Flags::CanBeDisabled)) {
            rule.flags |= OptimizerRule::makeFlags(
                OptimizerRule::Flags::DisabledByDefault);
          }
        }
      } else {
        int id = translateRule(n);
        if (id != -1) {
          auto& rule = ruleByLevel(id);
          if (rule.hasFlag(OptimizerRule::Flags::CanBeDisabled)) {
            rule.flags |= OptimizerRule::makeFlags(
                OptimizerRule::Flags::DisabledByDefault);
          }
        } else {
          LOG_TOPIC("6103b", WARN, Logger::QUERIES)
              << "cannot disable unknown optimizer rule '" << n << "'";
        }
      }
    } else {
      if (n == "all") {
        for (auto& rule : _rules) {
          rule.flags &= ~OptimizerRule::makeFlags(
              OptimizerRule::Flags::DisabledByDefault);
        }
      } else {
        int id = translateRule(n);
        if (id != -1) {
          auto& rule = ruleByLevel(id);
          rule.flags &= ~OptimizerRule::makeFlags(
              OptimizerRule::Flags::DisabledByDefault);
        } else {
          LOG_TOPIC("aa52f", WARN, Logger::QUERIES)
              << "cannot enable unknown optimizer rule '" << n << "'";
        }
      }
    }
  }
}

}  // namespace aql
}  // namespace arangodb
