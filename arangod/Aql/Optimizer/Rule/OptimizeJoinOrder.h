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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/Optimizer/Utils/JoinGraph.h"

#include <memory>

namespace arangodb::aql {
class ExecutionNode;
class ExecutionPlan;
class Optimizer;
struct OptimizerRule;

/// @brief build the join graph for the maximal run of adjacent collection
/// enumerations that starts at `firstEnumeration`. Walks upwards (towards the
/// root) collecting enumerations and classifying the filters between them.
/// Returns the graph and, via `next`, the first node that terminated the run
/// (or nullptr if the run reached the top of the plan). This is a read-only
/// analysis: the plan is not modified.
auto buildJoinGraph(ExecutionPlan const* plan, ExecutionNode* firstEnumeration,
                    ExecutionNode*& next) -> JoinGraph;

/// @brief the `optimize-join-order` optimizer rule.
void optimizeJoinOrder(Optimizer*, std::unique_ptr<ExecutionPlan>,
                       OptimizerRule const&);

}  // namespace arangodb::aql
