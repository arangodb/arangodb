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

/// @brief simplify some conditions in CalculationNodes
void simplifyConditionsRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                            OptimizerRule const&);

/// @brief this rule replaces expressions of the type:
///   x.val == 1 || x.val == 2 || x.val == 3
//  with
//    x.val IN [1,2,3]
//  when the OR conditions are present in the same FILTER node, and refer to the
//  same (single) attribute.
void replaceOrWithInRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                         OptimizerRule const&);

void removeRedundantOrRule(Optimizer*, std::unique_ptr<ExecutionPlan>,
                           OptimizerRule const&);


}  // namespace arangodb::aql
