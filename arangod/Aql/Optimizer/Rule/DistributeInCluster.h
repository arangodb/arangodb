////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/ExecutionNode/GatherNode.h"
#include "Aql/ExecutionPlan.h"

namespace arangodb::aql {
class Optimizer;
class SubqueryNode;

//// @brief create a DistributeNode for the given ExecutionNode
DistributeNode* createDistributeNodeFor(ExecutionPlan& plan,
                                        ExecutionNode* node);

//// @brief create a gather node matching the given DistributeNode
GatherNode* createGatherNodeFor(ExecutionPlan& plan, DistributeNode* node);

//// @brief enclose a node in DISTRIBUTE/GATHER
DistributeNode* insertDistributeGatherSnippet(ExecutionPlan& plan,
                                              ExecutionNode* at,
                                              SubqueryNode* snode);

/// @brief distribute operations in cluster
void distributeInClusterRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                             OptimizerRule const&);

}  // namespace arangodb::aql
