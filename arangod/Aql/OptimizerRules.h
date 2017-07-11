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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_OPTIMIZER_RULES_H
#define ARANGOD_AQL_OPTIMIZER_RULES_H 1

#include "Basics/Common.h"
#include "Aql/OptimizerRulesFeature.h"

namespace arangodb {
namespace aql {
class Optimizer;
class ExecutionNode;
class SubqueryNode;

/// @brief adds a SORT operation for IN right-hand side operands
void sortInValuesRule(Optimizer*, std::unique_ptr<ExecutionPlan>, OptimizerRule const*);

/// @brief remove redundant sorts
/// this rule modifies the plan in place:
/// - sorts that are covered by earlier sorts will be removed
void removeRedundantSortsRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                              OptimizerRule const*);

/// @brief remove all unnecessary filters
/// this rule modifies the plan in place:
/// - filters that are always true are removed completely
/// - filters that are always false will be replaced by a NoResults node
void removeUnnecessaryFiltersRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                  OptimizerRule const*);

/// @brief remove unused INTO variable from COLLECT, or unused aggregates
void removeCollectVariablesRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                OptimizerRule const*);

/// @brief propagate constant attributes in FILTERs
void propagateConstantAttributesRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                     OptimizerRule const*);

/// @brief move calculations up in the plan
/// this rule modifies the plan in place
/// it aims to move up calculations as far up in the plan as possible, to
/// avoid redundant calculations in inner loops
void moveCalculationsUpRule(Optimizer*, std::unique_ptr<ExecutionPlan>, OptimizerRule const*);

/// @brief move calculations down in the plan
/// this rule modifies the plan in place
/// it aims to move down calculations as far down in the plan as possible,
/// beyond FILTER and LIMIT statements
void moveCalculationsDownRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                              OptimizerRule const*);

/// @brief determine the "right" type of CollectNode and
/// add a sort node for each COLLECT (may be removed later)
/// this rule cannot be turned off (otherwise, the query result might be wrong!)
void specializeCollectRule(Optimizer*, std::unique_ptr<ExecutionPlan>, OptimizerRule const*);

/// @brief split and-combined filters and break them into smaller parts
void splitFiltersRule(Optimizer*, std::unique_ptr<ExecutionPlan>, OptimizerRule const*);

/// @brief move filters up in the plan
/// this rule modifies the plan in place
/// filters are moved as far up in the plan as possible to make result sets
/// as small as possible as early as possible
/// filters are not pushed beyond limits
void moveFiltersUpRule(Optimizer*, std::unique_ptr<ExecutionPlan>, OptimizerRule const*);

/// @brief remove redundant CalculationNodes
void removeRedundantCalculationsRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                     OptimizerRule const*);

/// @brief remove CalculationNodes and SubqueryNodes that are never needed
void removeUnnecessaryCalculationsRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                       OptimizerRule const*);

/// @brief useIndex, try to use an index for filtering
void useIndexesRule(Optimizer*, std::unique_ptr<ExecutionPlan>, OptimizerRule const*);

/// @brief try to use the index for sorting
void useIndexForSortRule(Optimizer*, std::unique_ptr<ExecutionPlan>, OptimizerRule const*);

/// @brief try to remove filters which are covered by indexes
void removeFiltersCoveredByIndexRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                     OptimizerRule const*);

/// @brief interchange adjacent EnumerateCollectionNodes in all possible ways
void interchangeAdjacentEnumerationsRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                         OptimizerRule const*);

/// @brief scatter operations in cluster - send all incoming rows to all remote
/// clients
void scatterInClusterRule(Optimizer*, std::unique_ptr<ExecutionPlan>, OptimizerRule const*);

/// @brief distribute operations in cluster - send each incoming row to every
/// remote client precisely once. This happens in queries like:
///
///   FOR x IN coll1 [SOME FILTER/CALCULATION STATEMENTS] REMOVE f(x) in coll2
///
/// where coll2 is sharded by _key, but not if it is sharded by anything else.
/// The collections coll1 and coll2 do not have to be distinct for this.
void distributeInClusterRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                             OptimizerRule const*);

#ifdef USE_ENTERPRISE
void distributeInClusterRuleSmartEdgeCollection(
    ExecutionPlan*,
    SubqueryNode* snode,
    ExecutionNode* node,
    ExecutionNode* originalParent,
    bool& wasModified);

/// @brief remove scatter/gather and remote nodes for satellite collections
void removeSatelliteJoinsRule(Optimizer*, std::unique_ptr<ExecutionPlan>, OptimizerRule const*);
#endif

void distributeFilternCalcToClusterRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                        OptimizerRule const*);

void distributeSortToClusterRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                 OptimizerRule const*);

/// @brief try to get rid of a RemoteNode->ScatterNode combination which has
/// only a SingletonNode and possibly some CalculationNodes as dependencies
void removeUnnecessaryRemoteScatterRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                        OptimizerRule const*);

/// @brief this rule removes Remote-Gather-Scatter/Distribute-Remote nodes from
/// plans arising from queries of the form:
///
///   FOR x IN coll [SOME FILTER/CALCULATION STATEMENTS] REMOVE x IN coll
///
/// when coll is any collection sharded by any attributes, and
///
///   FOR x IN coll [SOME FILTER/CALCULATION STATEMENTS] REMOVE x._key IN coll
///
/// when the coll is sharded by _key.
///
/// This rule dues not work for queries like:
///
///    FOR x IN coll [SOME FILTER/CALCULATION STATEMENTS] REMOVE y IN coll
///
/// [different variable names], or
///
///   FOR x IN coll1 [SOME FILTER/CALCULATION STATEMENTS] REMOVE x in coll2
///
///  when coll1 and coll2 are not the same, or
///
///   FOR x IN coll [SOME FILTER/CALCULATION STATEMENTS] REMOVE f(x) in coll
///
///  where f is some function.
///
void undistributeRemoveAfterEnumCollRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                         OptimizerRule const*);

/// @brief this rule replaces expressions of the type:
///   x.val == 1 || x.val == 2 || x.val == 3
//  with
//    x.val IN [1,2,3]
//  when the OR conditions are present in the same FILTER node, and refer to the
//  same (single) attribute.
void replaceOrWithInRule(Optimizer*, std::unique_ptr<ExecutionPlan>, OptimizerRule const*);

void removeRedundantOrRule(Optimizer*, std::unique_ptr<ExecutionPlan>, OptimizerRule const*);

/// @brief remove $OLD and $NEW variables from data-modification statements
/// if not required
void removeDataModificationOutVariablesRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                            OptimizerRule const*);

/// @brief patch UPDATE statement on single collection that iterates over the
/// entire collection to operate in batches
void patchUpdateStatementsRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                               OptimizerRule const*);

/// @brief optimizes away unused traversal output variables and
/// merges filter nodes into graph traversal nodes
void optimizeTraversalsRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                            OptimizerRule const* rule);
  
/// @brief removes filter nodes already covered by the traversal and removes unused variables
void removeFiltersCoveredByTraversal(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                                     OptimizerRule const* rule);

/// @brief removes redundant path variables, after applying
/// `removeFiltersCoveredByTraversal`. Should significantly reduce overhead
void removeTraversalPathVariable(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                                 OptimizerRule const* rule);

/// @brief prepares traversals for execution (hidden rule)
void prepareTraversalsRule(Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
                           OptimizerRule const* rule);

/// @brief moves simple subqueries one level higher
void inlineSubqueriesRule(Optimizer*, std::unique_ptr<ExecutionPlan>, OptimizerRule const*);
  
void geoIndexRule(aql::Optimizer* opt, std::unique_ptr<aql::ExecutionPlan> plan, aql::OptimizerRule const* rule);

}  // namespace aql
}  // namespace arangodb

#endif
