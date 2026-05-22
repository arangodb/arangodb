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

#include <cstdint>
#include <utility>
#include <vector>

#include "Aql/RegisterId.h"
#include "Aql/types.h"
#include "Containers/NodeHashMap.h"
#include "VectorIndex/VectorSearchStrategy.h"
#include "VocBase/Identifiers/LocalDocumentId.h"

#include <velocypack/SharedSlice.h>

namespace arangodb {

namespace transaction {
class Methods;
}

namespace aql {
class Expression;
class InputAqlItemRow;
class QueryContext;
struct Variable;
}  // namespace aql

namespace vector {

// FAISS uses int64_t labels; the static_assert in RocksDBVectorIndex.cpp
// guards this assumption against future faiss changes.
using VectorIndexLabelId = std::int64_t;

// Static per-search configuration. Built once by EnumerateNearVectorNode in
// createBlock and reused for every readBatch call within an executor.
struct VectorSearchConfig {
  SearchParameters searchParameters;
  std::size_t topK;  // = LIMIT + OFFSET

  // Optional pushed-down filter. When SearchStrategy::filter != kNone,
  // these must be populated.
  aql::Expression* filterExpression{nullptr};
  std::vector<std::pair<aql::VariableId, aql::RegisterId>> filterVarsToRegs;
  aql::Variable const* documentVariable{nullptr};

  // Selects the iterator family and capture behaviour at the storage
  // layer. Set based on the node's filter / projection coverage analysis.
  SearchStrategy strategy;
};

// Per-call runtime context. Filled by the executor immediately before
// each readBatch invocation; pointers reference executor-owned storage.
struct VectorSearchContext {
  std::vector<float>* inputs{nullptr};  // mutable: cosine normalization
  aql::InputAqlItemRow const* inputRow{nullptr};
  transaction::Methods* trx{nullptr};
  aql::QueryContext* queryContext{nullptr};
};

struct SearchResult {
  std::vector<VectorIndexLabelId> labels;
  std::vector<float> distances;
  // Per-survivor VPack: a storedValues array (when strategy.projection ==
  // kCovered) or a full document Object (kDocument). Empty unless
  // strategy.projection != kPassThroughId.
  containers::NodeHashMap<LocalDocumentId, velocypack::SharedSlice>
      capturedDocuments;
};

}  // namespace vector
}  // namespace arangodb
