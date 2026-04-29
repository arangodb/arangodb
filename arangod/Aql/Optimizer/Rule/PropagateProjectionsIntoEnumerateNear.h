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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionPlan.h"

namespace arangodb::aql {
class Optimizer;

// Runs after optimizeProjectionsRule. Migrates the projections that
// optimizeProjectionsRule assigned to the MaterializeRocksDBNode parent of
// each EnumerateNearVectorNode onto the EnumerateNearVectorNode itself,
// chooses a kCovered/kDocument strategy, and drops the now-redundant
// materializer. Single-server only for now -- cluster mode keeps the
// materializer because scatterInClusterRule's exclude logic relies on it.
void propagateProjectionsIntoEnumerateNear(Optimizer*,
                                           std::unique_ptr<ExecutionPlan>,
                                           OptimizerRule const&);

}  // namespace arangodb::aql
