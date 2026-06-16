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
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>

#include "Basics/StaticStrings.h"
#include "RocksDBIndex.h"
#include "VectorIndex/VectorIndexDefinition.h"
#include "VectorIndex/VectorReadBatch.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBVectorIndexBuilder.h"
#include "VocBase/Identifiers/IndexId.h"

#include <faiss/IndexIVF.h>
#include <rocksdb/iterator.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace rocksdb {
class DB;
}  // namespace rocksdb

namespace arangodb {

enum class VectorIndexTrainingState : std::uint8_t {
  kUnusable,
  kTraining,
  kIngesting,
  kReady
};

std::string_view trainingStateToString(VectorIndexTrainingState state) noexcept;

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

  vector::SearchResult readBatch(vector::VectorSearchConfig const& config,
                                 vector::VectorSearchContext const& ctx);

  vector::UserVectorIndexDefinition const& getVectorIndexDefinition() override;

  bool isVectorIndexReady() const noexcept override;

  bool isLinearScanEnabled() const noexcept override;

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

  std::shared_ptr<faiss::IndexIVF> cloneFaissIndex();

  // Tuned nprobe if set, else the configured default. Read atomically so
  // autotune can update it while searches run concurrently.
  std::int64_t effectiveNProbe() const noexcept {
    auto const tuned = _liveNProbe.load(std::memory_order_acquire);
    return tuned != 0 ? tuned : _definition.defaultNProbe;
  }

  // Update the tuned nprobe: the persistence source and the atomic searches
  // read. Persist via persistMetadata() afterwards.
  void applyTunedNProbe(std::int64_t nprobe) noexcept {
    _trainedData.tunedNProbe = nprobe;
    _liveNProbe.store(nprobe, std::memory_order_release);
  }

  // Serialize the current trained data and format version into the index's
  // metadata slot in the VectorIndex column family.
  Result persistMetadata() const;

  bool hasStoredValues() const noexcept;

  StoredValues const& storedValues() const override;

  /// @brief On-disk format version for this index's list entries. Internal
  /// detail; never surfaced through toVelocyPack or the REST API.
  vector::VectorIndexFormatVersion formatVersion() const noexcept {
    return _formatVersion;
  }

  std::vector<std::vector<basics::AttributeName>> const& coveredFields()
      const override;

  Result prepareIndex(std::unique_ptr<rocksdb::Iterator> it,
                      rocksdb::Slice upper, RocksDBMethods* methods) override;

  void truncateCommit(TruncateGuard&& guard, TRI_voc_tick_t tick,
                      transaction::Methods* trx) override;

  bool setTrainingState(VectorIndexTrainingState expected,
                        VectorIndexTrainingState desired) noexcept;

  VectorIndexTrainingState trainingState() const noexcept {
    return _trainingState.load(std::memory_order_acquire);
  }

  void resetTrainingState() noexcept;

  void setTrainingError(std::string error) noexcept;

  std::string trainingError() const;

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
  // Read the stored metadata record into _trainedData and _formatVersion.
  void loadStoredMetadata(velocypack::Slice info);

  //  Helper functions for bruteForceSearch
  void captureDocument(
      vector::VectorSearchConfig const& config,
      vector::VectorSearchContext const& ctx,
      containers::NodeHashMap<LocalDocumentId, velocypack::SharedSlice>*
          captureSink,
      LocalDocumentId docId, velocypack::Slice docSlice);

  bool filterDocuments(vector::VectorSearchConfig const& config,
                       vector::VectorSearchContext const& ctx,
                       velocypack::Slice docSlice);

  float computeDistance(const vector::Vector& vec1, const vector::Vector& vec2,
                        bool isDescending);

  bool getNormalizedVectorFromDocument(const velocypack::Slice& docSlice,
                                       vector::Vector& vec);

  std::pair<vector::Labels, vector::Distances> bruteForceSearch(
      vector::Vector& searchVector, vector::VectorSearchConfig const& config,
      vector::VectorSearchContext const& ctx,
      containers::NodeHashMap<LocalDocumentId, velocypack::SharedSlice>*
          captureSink);

  vector::VectorIndexMetadata loadVectorIndexMetadata(
      velocypack::Slice info) const;

  vector::UserVectorIndexDefinition _definition;
  std::shared_ptr<faiss::IndexIVF> _faissIndex;
  vector::TrainedData _trainedData;
  vector::VectorIndexFormatVersion _formatVersion{
      vector::kCurrentVectorIndexFormatVersion};
  StoredValues const _storedValues;

  std::size_t _trainingThreshold{0};
  std::atomic<VectorIndexTrainingState> _trainingState{
      VectorIndexTrainingState::kUnusable};

  // Live nprobe consumed by search (0 = unset → fall back to defaultNProbe).
  // Atomic because on-demand autotune writes it while searches read it.
  std::atomic<std::int64_t> _liveNProbe{0};

  mutable std::mutex _trainingErrorMutex;
  // Placeholder used while the build manager hasn't yet diagnosed why the
  // index is unusable (e.g. between ensureIndex and the first scan).
  std::string _trainingError{StaticStrings::VectorIndexDefaultTrainingError};
};

}  // namespace arangodb
