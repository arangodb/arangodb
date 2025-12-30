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

#include "Aql/ExecutionPlan.h"

namespace arangodb::aql {
class Optimizer;

/// @brief useIndex, try to use an index for filtering
void useIndexesRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                    OptimizerRule const&);

/// @brief try to use the index for sorting
void useIndexForSortRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                         OptimizerRule const&);

/// @brief try to remove filters which are covered by indexes
void removeFiltersCoveredByIndexRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                     OptimizerRule const&);

}  // namespace arangodb::aql
