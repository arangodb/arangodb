////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

#include <memory>

namespace arangodb::aql {
class DistributeNode;
class ExecutionNode;
class ExecutionPlan;
class GatherNode;
class Optimizer;
class SubqueryNode;
struct OptimizerRule;

/// @brief create a DistributeNode for the given ExecutionNode
DistributeNode* createDistributeNodeFor(ExecutionPlan& plan,
                                        ExecutionNode* node);

/// @brief create a GatherNode matching the given DistributeNode
GatherNode* createGatherNodeFor(ExecutionPlan& plan, DistributeNode* node);

/// @brief enclose a node in DISTRIBUTE/GATHER
DistributeNode* insertDistributeGatherSnippet(ExecutionPlan& plan,
                                              ExecutionNode* at,
                                              SubqueryNode* snode);

/// @brief distribute operations in cluster
void distributeInClusterRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                             OptimizerRule const&);

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

/// @brief remove Remote-Gather-Scatter/Distribute-Remote nodes for same-coll
/// remove operations
void undistributeRemoveAfterEnumCollRule(Optimizer*,
                                         std::unique_ptr<ExecutionPlan>,
                                         OptimizerRule const&);

}  // namespace arangodb::aql
