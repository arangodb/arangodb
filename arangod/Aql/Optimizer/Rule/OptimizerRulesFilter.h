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

/// @brief remove all unnecessary filters
/// this rule modifies the plan in place:
/// - filters that are always true are removed completely
/// - filters that are always false will be replaced by a NoResults node
void removeUnnecessaryFiltersRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                  OptimizerRule const&);

/// @brief move filters up in the plan
/// this rule modifies the plan in place
/// filters are moved as far up in the plan as possible to make result sets
/// as small as possible as early as possible
/// filters are not pushed beyond limits
void moveFiltersUpRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                       OptimizerRule const&);

/// @brief fuse filter conditions that follow each other
void fuseFiltersRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                     OptimizerRule const&);

/// @brief move filters into EnumerateCollection nodes
void moveFiltersIntoEnumerateRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                  OptimizerRule const&);

}  // namespace arangodb::aql