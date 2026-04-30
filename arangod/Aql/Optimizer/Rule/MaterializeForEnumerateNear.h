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

// Decide for each EnumerateNearVectorNode how its document output reaches
// downstream consumers. Three cases:
//   1. The vector index storedValues cover the downstream projections --
//      transfer the projections to the vector node and let it produce them
//      directly (kCovered).
//   2. A pushed-down filter already forces the iterator to load the doc
//      (FilterMode::kDocument) -- transfer the projections, capture the doc
//      the iterator loaded, and let the executor project from it (kDocument).
//   3. Neither applies -- insert a MaterializeRocksDBNode after the vector
//      node to translate the doc-id into the full document, and exclude the
//      vector node from scatter/gather so the materializer anchors the
//      cluster SCATTER/GATHER snippet.
void materializeForEnumerateNear(Optimizer*, std::unique_ptr<ExecutionPlan>,
                                 OptimizerRule const&);

}  // namespace arangodb::aql
