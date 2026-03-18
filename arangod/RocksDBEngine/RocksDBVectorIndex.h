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

#include "Aql/Expression.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/QueryContext.h"
#include "Aql/RegisterId.h"
#include "Aql/Variable.h"
#include "Indexes/VectorIndexDefinition.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "Transaction/Methods.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/Identifiers/LocalDocumentId.h"

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
  UserVectorIndexDefinition const& getDefinition() const noexcept {
    return _definition;
  }

  std::pair<std::vector<VectorIndexLabelId>, std::vector<float>> readBatch(
      std::vector<float>& inputs, SearchParameters const& searchParameters,
      RocksDBMethods* rocksDBMethods, transaction::Methods* trx,
      std::shared_ptr<LogicalCollection> collection, std::size_t topK,
      aql::Expression* filterExpression, aql::InputAqlItemRow const* inputRow,
      aql::QueryContext& queryContext,
      std::vector<std::pair<aql::VariableId, aql::RegisterId>> const&
          filterVarsToRegs,
      aql::Variable const* documentVariable, bool isCovered);

  UserVectorIndexDefinition const& getVectorIndexDefinition() override;

  bool isVectorIndexReady() const noexcept override;

  Result readDocumentVectorData(velocypack::Slice doc,
                                std::vector<float>& vector);

  std::shared_ptr<faiss::IndexIVF> const& faissIndex() const noexcept {
    return _faissIndex;
  }

  std::int64_t trainingThreshold() const noexcept { return _trainingThreshold; }

  void applyTrainingResult(std::shared_ptr<faiss::IndexIVF> faissIndex,
                           TrainedData trainedData);

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
  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& options, bool performChecks) override;

  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& /*options*/) override;

 private:
  UserVectorIndexDefinition _definition;
  std::shared_ptr<faiss::IndexIVF> _faissIndex;
  TrainedData _trainedData;
  StoredValues const _storedValues;

  std::int64_t _trainingThreshold{0};
  std::atomic<VectorIndexTrainingState> _trainingState{
      VectorIndexTrainingState::kUnusable};
};

}  // namespace arangodb
