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
/// @author Julia Puget
/// @author Koichi Nakata
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionPlan.h"

namespace arangodb::aql {
class Optimizer;

/// @brief remove redundant sorts
/// this rule modifies the plan in place:
/// - sorts that are covered by earlier sorts will be removed
void removeRedundantSortsRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                              OptimizerRule const&);

/// @brief make sort node aware of limit to enable internal optimizations
void sortLimitRule(Optimizer*, std::unique_ptr<aql::ExecutionPlan>,
                   OptimizerRule const&);

/// @brief parallelize Gather nodes (cluster-only)
void parallelizeGatherRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                           OptimizerRule const&);

//// @brief reduces a sorted gather to an unsorted gather if only one shard is
/// involved
void decayUnnecessarySortedGather(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                  OptimizerRule const&);

}  // namespace arangodb::aql