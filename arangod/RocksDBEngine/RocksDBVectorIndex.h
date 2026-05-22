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
#include <mutex>
#include <string>
#include <type_traits>

#include "Basics/StaticStrings.h"
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

  std::pair<std::vector<VectorIndexLabelId>, std::vector<float>> readBatch(
      std::vector<float>& inputs,
      vector::SearchParameters const& searchParameters,
      RocksDBMethods* rocksDBMethods, transaction::Methods* trx,
      std::shared_ptr<LogicalCollection> collection, std::size_t topK,
      aql::Expression* filterExpression, aql::InputAqlItemRow const* inputRow,
      aql::QueryContext& queryContext,
      std::vector<std::pair<aql::VariableId, aql::RegisterId>> const&
          filterVarsToRegs,
      aql::Variable const* documentVariable, bool isCovered);

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

  // Safe only while state is still kIngesting (no concurrent search reads).
  vector::TrainedData const& applyTunedNProbe(std::int64_t nprobe) noexcept {
    _trainedData.tunedNProbe = nprobe;
    return _trainedData;
  }

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
  vector::TrainedData loadTrainedData(velocypack::Slice info) const;

  vector::UserVectorIndexDefinition _definition;
  std::shared_ptr<faiss::IndexIVF> _faissIndex;
  vector::TrainedData _trainedData;
  StoredValues const _storedValues;

  std::size_t _trainingThreshold{0};
  std::atomic<VectorIndexTrainingState> _trainingState{
      VectorIndexTrainingState::kUnusable};

  mutable std::mutex _trainingErrorMutex;
  // Placeholder used while the build manager hasn't yet diagnosed why the
  // index is unusable (e.g. between ensureIndex and the first scan).
  std::string _trainingError{StaticStrings::VectorIndexDefaultTrainingError};
};

}  // namespace arangodb
