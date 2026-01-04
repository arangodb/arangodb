////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "Aql/ExecutionNode/DistributeNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExecutionNode/GatherNode.h"

namespace arangodb::aql {
class Optimizer;

//// @brief create a DistributeNode for the given ExecutionNode
DistributeNode* createDistributeNodeFor(ExecutionPlan& plan,
                                        ExecutionNode* node);

//// @brief create a gather node matching the given DistributeNode
GatherNode* createGatherNodeFor(ExecutionPlan& plan, DistributeNode* node);

//// @brief enclose a node in DISTRIBUTE/GATHER
DistributeNode* insertDistributeGatherSnippet(ExecutionPlan& plan,
                                              ExecutionNode* at,
                                              SubqueryNode* snode);

/// @brief distribute operations in cluster - send each incoming row to every
/// remote client precisely once. This happens in queries like:
///
///   FOR x IN coll1 [SOME FILTER/CALCULATION STATEMENTS] REMOVE f(x) in coll2
///
/// where coll2 is sharded by _key, but not if it is sharded by anything else.
/// The collections coll1 and coll2 do not have to be distinct for this.
void distributeInClusterRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                             OptimizerRule const&);

#ifdef USE_ENTERPRISE
ExecutionNode* distributeInClusterRuleSmart(ExecutionPlan*, SubqueryNode* snode,
                                            ExecutionNode* node,
                                            bool& wasModified);
#endif

/// @brief move collect to the DB servers in cluster
void collectInClusterRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                          OptimizerRule const&);

void distributeFilterCalcToClusterRule(Optimizer*,
                                       std::unique_ptr<ExecutionPlan>,
                                       OptimizerRule const&);

void distributeSortToClusterRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                 OptimizerRule const&);

/// @brief try to get rid of a RemoteNode->ScatterNode combination which has
/// only a SingletonNode and possibly some CalculationNodes as dependencies
void removeUnnecessaryRemoteScatterRule(Optimizer*,
                                        std::unique_ptr<ExecutionPlan>,
                                        OptimizerRule const&);

/// @brief try to restrict fragments to a single shard if possible
void restrictToSingleShardRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                               OptimizerRule const&);

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
void undistributeRemoveAfterEnumCollRule(Optimizer*,
                                         std::unique_ptr<ExecutionPlan>,
                                         OptimizerRule const&);

}  // namespace arangodb::aql