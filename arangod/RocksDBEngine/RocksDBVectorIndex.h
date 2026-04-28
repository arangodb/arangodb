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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <memory>
#include <type_traits>

#include "Containers/NodeHashMap.h"
#include "RocksDBIndex.h"
#include "VectorIndex/VectorIndexDefinition.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBVectorIndexBuilder.h"
#include "Transaction/Methods.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "Aql/Expression.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/QueryContext.h"
#include "Aql/RegisterId.h"
#include "Aql/Variable.h"

#include <faiss/IndexIVF.h>
#include <rocksdb/iterator.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace rocksdb {
class DB;
}  // namespace rocksdb

namespace arangodb {

using VectorIndexLabelId = faiss::idx_t;

enum class VectorIndexTrainingState : std::uint8_t {
  kUnusable,
  kTraining,
  kIngesting,
  kReady
};

std::string_view trainingStateToString(VectorIndexTrainingState state) noexcept;

namespace vector {

// Static configuration of a vector search. Owned by the executor's Infos
// (set once at createBlock) and passed by reference to readBatch.
struct SearchConfig {
  SearchParameters searchParameters;
  std::size_t topK;  // = LIMIT + OFFSET

  // Optional pushed-down filter. Either all four are populated or none.
  aql::Expression* filterExpression{nullptr};
  std::vector<std::pair<aql::VariableId, aql::RegisterId>> filterVarsToRegs;
  aql::Variable const* documentVariable{nullptr};

  // True when the FAISS layer should use the storedValues-only filter
  // iterator: the filter is expressible against storedValues AND no
  // consumer of this readBatch call needs a full document. That covers
  // (filter covered, projections covered) -- the executor produces
  // projection registers from the captured partial doc -- and (filter
  // covered, no projections) -- the executor passes labels through to a
  // downstream materializer. When false, the regular filter iterator
  // runs and loads the full document during filter eval.
  bool useStoredValuesIterator{false};

  // When set (and a filter is present), the filter iterator hands the
  // VPack object it built for filter eval back through SearchResult --
  // either the full document (regular iterator) or the partial doc
  // assembled from storedValues (storedValues iterator). The caller does
  // not care which: both are objects suitable for projection extraction.
  bool captureDocuments{false};
};

// Per-call execution context — what changes between readBatch invocations
// for the same configured EnumerateNearVectorNode.
struct SearchContext {
  std::vector<float>* inputs;  // mutable: cosine renormalises in place
  aql::InputAqlItemRow const* inputRow;
  transaction::Methods* trx;
  aql::QueryContext* queryContext;
};

struct SearchResult {
  std::vector<VectorIndexLabelId> labels;
  std::vector<float> distances;
  // Empty unless captureDocuments was set in SearchConfig.
  containers::NodeHashMap<LocalDocumentId, velocypack::Buffer<uint8_t>>
      capturedDocuments;
};

}  // namespace vector

class RocksDBVectorIndex final : public RocksDBIndex {
 public:
  RocksDBVectorIndex(IndexId iid, LogicalCollection& coll,
                     arangodb::velocypack::Slice info);
  ~RocksDBVectorIndex();

  IndexType type() const override { return Index::TRI_IDX_TYPE_VECTOR_INDEX; }

  bool isSorted() const override { return false; }

  bool canBeDropped() const override { return true; }

  bool hasSelectivityEstimate() const override { return false; }

  char const* typeName() const override { return "rocksdb-vector"; }

  bool matchesDefinition(VPackSlice const& /*unused*/) const override;

  void toVelocyPack(
      arangodb::velocypack::Builder& builder,
      std::underlying_type<Index::Serialize>::type flags) const override;
  vector::UserVectorIndexDefinition const& getDefinition() const noexcept {
    return _definition;
  }

  vector::SearchResult readBatch(vector::SearchConfig const& config,
                                 vector::SearchContext const& ctx);

  vector::UserVectorIndexDefinition const& getVectorIndexDefinition() override;

  bool isVectorIndexReady() const noexcept override;

  Result readDocumentVectorData(velocypack::Slice doc,
                                std::vector<float>& vector) const;

  std::shared_ptr<faiss::IndexIVF> const& faissIndex() const noexcept {
    return _faissIndex;
  }

  std::optional<std::size_t> resolvedNLists() const noexcept {
    if (auto const state = _trainingState.load();
        state == VectorIndexTrainingState::kIngesting ||
        state == VectorIndexTrainingState::kReady) {
      return _faissIndex->nlist;
    }
    return std::nullopt;
  }

  // Absolute minimum number of vectors required for training, might give false
  // positives with sparse indexes
  std::size_t trainingThreshold() const noexcept { return _trainingThreshold; }

  void applyTrainingResult(std::shared_ptr<faiss::IndexIVF> faissIndex,
                           vector::TrainedData trainedData);

  bool hasStoredValues() const noexcept;

  StoredValues const& storedValues() const override;

  Result prepareIndex(std::unique_ptr<rocksdb::Iterator> it,
                      rocksdb::Slice upper, RocksDBMethods* methods) override;

  void truncateCommit(TruncateGuard&& guard, TRI_voc_tick_t tick,
                      transaction::Methods* trx) override;

  bool setTrainingState(VectorIndexTrainingState expected,
                        VectorIndexTrainingState desired) noexcept;

  VectorIndexTrainingState trainingState() const noexcept {
    return _trainingState.load(std::memory_order_acquire);
  }

  /// @brief Clear trained data on build failure so that
  /// stale training state is not accidentally persisted.
  void resetTrainingState() noexcept;

 protected:
  ResultT<std::vector<float>> preModificationCheck(std::string_view operation,
                                                   velocypack::Slice doc) const;

  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& options, bool performChecks) override;

  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& /*options*/) override;

 private:
  vector::TrainedData loadTrainedData(velocypack::Slice info) const;

  vector::UserVectorIndexDefinition _definition;
  std::shared_ptr<faiss::IndexIVF> _faissIndex;
  vector::TrainedData _trainedData;
  StoredValues const _storedValues;

  std::size_t _trainingThreshold{0};
  std::atomic<VectorIndexTrainingState> _trainingState{
      VectorIndexTrainingState::kUnusable};
};

}  // namespace arangodb
