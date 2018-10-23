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

#ifndef ARANGOD_AQL_AQL_OPTIMIZER_RULE_H
#define ARANGOD_AQL_AQL_OPTIMIZER_RULE_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace aql {
class ExecutionPlan;
class Optimizer;
struct OptimizerRule;

/// @brief type of an optimizer rule function, the function gets an
/// optimizer, an ExecutionPlan, and the current rule. it has
/// to append one or more plans to the resulting deque. This must
/// include the original plan if it ought to be kept. The rule has to
/// set the level of the appended plan to the largest level of rule
/// that ought to be considered as done to indicate which rule is to be
/// applied next.
typedef std::function<void(Optimizer*, std::unique_ptr<ExecutionPlan>, OptimizerRule const*)>
    RuleFunction;

/// @brief type of an optimizer rule
struct OptimizerRule {
  /// @brief optimizer rules
  enum RuleLevel : int {
    // List all the rules in the system here:
    // lower level values mean earlier rule execution

    // note that levels must be unique
    initial = 100,

    // "Pass 1": moving nodes "up" (potentially outside loops):
    // ========================================================
    replaceNearWithinFulltext,

    inlineSubqueriesRule,

    // split and-combined filters into multiple smaller filters
    splitFiltersRule,
    
    /// simplify some conditions in CalculationNodes
    simplifyConditionsRule,

    // move calculations up the dependency chain (to pull them out of
    // inner loops etc.)
    moveCalculationsUpRule,

    // move filters up the dependency chain (to make result sets as small
    // as possible as early as possible)
    moveFiltersUpRule,

    // remove calculations that are repeatedly used in a query
    removeRedundantCalculationsRule,

    // "Pass 2": try to remove redundant or unnecessary nodes
    // ======================================================

    // remove filters from the query that are not necessary at all
    // filters that are always true will be removed entirely
    // filters that are always false will be replaced with a NoResults node
    removeUnnecessaryFiltersRule,

    // remove calculations that are never necessary
    removeUnnecessaryCalculationsRule,
    
    // determine the "right" type of CollectNode and
    // add a sort node for each COLLECT (may be removed later)
    specializeCollectRule,

    // remove redundant sort blocks
    removeRedundantSortsRule,

    // push limits into subqueries and simplify them
    optimizeSubqueriesRule,

    // "Pass 3": interchange EnumerateCollection nodes in all possible ways
    //           this is level 500, please never let new plans from higher
    //           levels go back to this or lower levels!
    // ======================================================

    interchangeAdjacentEnumerationsRule,

    // "Pass 4": moving nodes "up" (potentially outside loops) (second try):
    // ======================================================

    // move calculations up the dependency chain (to pull them out of
    // inner loops etc.)
    moveCalculationsUpRule2,

    // move filters up the dependency chain (to make result sets as small
    // as possible as early as possible)
    moveFiltersUpRule2,

    /// "Pass 5": try to remove redundant or unnecessary nodes (second try)
    // remove filters from the query that are not necessary at all
    // filters that are always true will be removed entirely
    // filters that are always false will be replaced with a NoResults node
    // ======================================================

    // remove redundant sort blocks
    removeRedundantSortsRule2,

    // remove SORT RAND() if appropriate
    removeSortRandRule,

    // remove INTO for COLLECT if appropriate
    removeCollectVariablesRule,

    // propagate constant attributes in FILTERs
    propagateConstantAttributesRule,

    // remove unused out variables for data-modification queries
    removeDataModificationOutVariablesRule,

    /// "Pass 6": use indexes if possible for FILTER and/or SORT nodes
    // ======================================================

    // replace simple OR conditions with IN
    replaceOrWithInRule,

    // remove redundant OR conditions
    removeRedundantOrRule,

    // remove FILTER and SORT if there are geoindexes
    applyGeoIndexRule,

    // replace FULLTEXT with index
    applyFulltextIndexRule,

    useIndexesRule,

    // try to remove filters covered by index ranges
    removeFiltersCoveredByIndexRule,

    removeUnnecessaryFiltersRule2,

    // try to find sort blocks which are superseeded by indexes
    useIndexForSortRule,

    // sort values used in IN comparisons of remaining filters
    sortInValuesRule,

    // merge filters into graph traversals
    optimizeTraversalsRule,
    // remove redundant filters statements
    removeFiltersCoveredByTraversal,

    // remove calculations that are redundant
    // needs to run after filter removal
    removeUnnecessaryCalculationsRule2,

#ifdef USE_IRESEARCH
    // move filters and sort conditions into views and remove them
    handleArangoSearchViewsRule,
#endif

    // remove now obsolete path variables
    removeTraversalPathVariable,
    prepareTraversalsRule,

    // when we have single document operations, fill in special cluster
    // handling.
    substituteSingleDocumentOperations,

    /// Pass 9: push down calculations beyond FILTERs and LIMITs
    moveCalculationsDownRule,
    
    /// Pass 9: fuse filter conditions
    fuseFiltersRule,

    /// Pass 9: patch update statements
    patchUpdateStatementsRule,

    /// "Pass 10": final transformations for the cluster

    // optimize queries in the cluster so that the entire query
    // gets pushed to a single server
    optimizeClusterSingleShardRule,

    // make operations on sharded collections use distribute
    distributeInClusterRule,

    // try to find candidates for shard-local joins in the cluster
    optimizeClusterJoinsRule,

    // make operations on sharded collections use scatter / gather / remote
    scatterInClusterRule,

#ifdef USE_IRESEARCH
    // FIXME order-???
    // make operations on sharded IResearch views use scatter / gather / remote
    scatterIResearchViewInClusterRule,
#endif

    // move FilterNodes & Calculation nodes in between
    // scatter(remote) <-> gather(remote) so they're
    // distributed to the cluster nodes.
    distributeFilternCalcToClusterRule,

    // move SortNodes into the distribution.
    // adjust gathernode to also contain the sort criteria.
    distributeSortToClusterRule,

    // try to get rid of a RemoteNode->ScatterNode combination which has
    // only a SingletonNode and possibly some CalculationNodes as dependencies
    removeUnnecessaryRemoteScatterRule,

#ifdef USE_ENTERPRISE
    // remove any superflous satellite collection joins...
    // put it after Scatter rule because we would do
    // the work twice otherwise
    removeSatelliteJoinsRule,
#endif

    // recognize that a RemoveNode can be moved to the shards
    undistributeRemoveAfterEnumCollRule,

    // push collect operations to the db servers
    collectInClusterRule,

    // try to restrict fragments to a single shard if possible
    restrictToSingleShardRule,

    // simplify an EnumerationCollectionNode that fetches an
    // entire document to a projection of this document
    reduceExtractionToProjectionRule,
  };

  std::string name;
  RuleFunction func;
  RuleLevel const level;
  bool const canCreateAdditionalPlans;
  bool const canBeDisabled;
  bool const isHidden;

  OptimizerRule() = delete;

  OptimizerRule(std::string const& name, RuleFunction const& func, RuleLevel level,
        bool canCreateAdditionalPlans, bool canBeDisabled, bool isHidden)
      : name(name),
        func(func),
        level(level),
        canCreateAdditionalPlans(canCreateAdditionalPlans),
        canBeDisabled(canBeDisabled),
        isHidden(isHidden) {}

};

} // namespace aql
} // namespace arangodb

#endif
