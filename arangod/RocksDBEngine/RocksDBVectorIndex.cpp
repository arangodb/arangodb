////////////////////////////////////////////////////////////////////////////////
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

#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "RocksDBEngine/RocksDBVectorIndexBuilder.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <cstring>

#include "Aql/AstNode.h"
#include "Basics/StaticStrings.h"
#include "Aql/Function.h"
#include "Assertions/Assert.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Inspection/VPack.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "RocksDBEngine/RocksDBVectorIndexList.h"
#include "RocksDBIndex.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "Transaction/Helpers.h"
#include <velocypack/Builder.h>
#include <velocypack/SharedSlice.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>
#include "Indexes/Index.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include <rocksdb/db.h>

#include "faiss/MetricType.h"
#include "faiss/utils/distances.h"

namespace arangodb {

// This assertion must hold for faiss::idx_t to be used
static_assert(sizeof(faiss::idx_t) == sizeof(LocalDocumentId::BaseType),
              "Faiss id and LocalDocumentId must be of same size");

// This assertion is that faiss::idx_t is the same type as std::int64_t
static_assert(std::is_same_v<faiss::idx_t, std::int64_t>,
              "Faiss idx_t base type is no longer int64_t");

#define LOG_VECTOR_INDEX(lid, level, topic) \
  LOG_TOPIC((lid), level, topic)            \
      << "[shard=" << _collection.name() << ", index=" << _iid.id() << "] "

std::string_view trainingStateToString(
    VectorIndexTrainingState const state) noexcept {
  switch (state) {
    case VectorIndexTrainingState::kUnusable:
      return "unusable";
    case VectorIndexTrainingState::kTraining:
      return "training";
    case VectorIndexTrainingState::kIngesting:
      return "ingesting";
    case VectorIndexTrainingState::kReady:
      return "ready";
  }
}

RocksDBVectorIndex::RocksDBVectorIndex(IndexId iid, LogicalCollection& coll,
                                       arangodb::velocypack::Slice info)
    : RocksDBIndex(iid, coll, info,
                   RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::VectorIndex),
                   /*useCache*/ false,
                   /*cacheManager*/ nullptr,
                   /*engine*/
                   coll.vocbase().engine<RocksDBEngine>()),
      _storedValues(
          Index::parseFields(info.get(StaticStrings::IndexStoredValues),
                             /*allowEmpty*/ true,
                             /*allowExpansion*/ false)) {
  TRI_ASSERT(type() == Index::TRI_IDX_TYPE_VECTOR_INDEX);
  velocypack::deserialize(info.get("params"), _definition);
  if (auto data = info.get("trainedData"); !data.isNone()) {
    LOG_DEVEL << ADB_HERE
              << " Trained vector index found, restoring from trained data.";
    velocypack::deserialize(data, _trainedData);
    _faissIndex =
        vector::VectorIndexTrainer::restoreFromTrainedData(_trainedData);

    _faissIndex->replace_invlists(
        new vector::RocksDBInvertedLists(this, &coll, _definition.nLists,
                                         _faissIndex->code_size),
        true /* faiss owns the inverted list */);

    setTrainingState(VectorIndexTrainingState::kUnusable,
                     VectorIndexTrainingState::kReady);
  }

  // Below 1000 documents training is not worth the effort nor having a index
  // 39 is the minimum number of documents to train the vector index, but that
  // does not mean it cannot be achieved with less documents.
  _trainingThreshold = std::max<std::int64_t>(_definition.nLists * 39, 1000);
}

RocksDBVectorIndex::~RocksDBVectorIndex() = default;

/// @brief Test if this index matches the definition
bool RocksDBVectorIndex::matchesDefinition(VPackSlice const& info) const {
  // check if we have the same parameter
  if (!RocksDBIndex::matchesDefinition(info)) {
    return false;
  }

  UserVectorIndexDefinition definition;
  velocypack::deserialize(info.get("params"), definition);
  if (definition != _definition) {
    return false;
  }

  auto storedValues =
      Index::parseFields(info.get(StaticStrings::IndexStoredValues),
                         /*allowEmpty*/ true,
                         /*allowExpansion*/ false);
  if (storedValues != _storedValues) {
    return false;
  }

  return true;
}

void RocksDBVectorIndex::toVelocyPack(
    arangodb::velocypack::Builder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  VPackObjectBuilder objectBuilder(&builder);
  RocksDBIndex::toVelocyPack(builder, flags);
  builder.add(VPackValue("params"));
  velocypack::serialize(builder, _definition);

  if (!_storedValues.empty()) {
    builder.add(velocypack::Value(StaticStrings::IndexStoredValues));
    builder.openArray();

    for (auto const& field : _storedValues) {
      std::string fieldString;
      TRI_AttributeNamesToString(field, fieldString);
      builder.add(VPackValue(fieldString));
    }

    builder.close();
  }

  builder.add(StaticStrings::IndexTrainingState,
              VPackValue(trainingStateToString(_trainingState)));

  if (_trainingState.load() == VectorIndexTrainingState::kReady &&
      Index::hasFlag(flags, Index::Serialize::Internals) &&
      !Index::hasFlag(flags, Index::Serialize::Maintenance)) {
    builder.add(VPackValue("trainedData"));
    velocypack::serialize(builder, _trainedData);
  }
}

std::pair<std::vector<VectorIndexLabelId>, std::vector<float>>
RocksDBVectorIndex::readBatch(
    std::vector<float>& inputs, SearchParameters const& searchParameters,
    RocksDBMethods* rocksDBMethods, transaction::Methods* trx,
    std::shared_ptr<LogicalCollection> collection, std::size_t topK,
    aql::Expression* filterExpression, aql::InputAqlItemRow const* inputRow,
    aql::QueryContext& queryContext,
    std::vector<std::pair<aql::VariableId, aql::RegisterId>> const&
        filterVarsToRegs,
    aql::Variable const* documentVariable, bool isCoveredByStoredValues) {
  TRI_ASSERT(inputs.size() == _definition.dimension)
      << "Number of components does not match vector dimension, topK: " << topK
      << ", dimension: " << _definition.dimension
      << ", inputs size: " << inputs.size();

  if (_definition.metric == SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(_definition.dimension, 1, inputs.data());
  }

  if (auto const state = _trainingState.load();
      state != VectorIndexTrainingState::kReady) {
    // This should never happen — the optimizer should not use
    // EnumerateNearVectorNode when the vector index is not ready.
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED,
        std::format("vector index is in state '{}', expected 'ready'",
                    trainingStateToString(state)));
  }
  TRI_ASSERT(_faissIndex != nullptr);

  // Trained: normal FAISS IVF search
  std::vector<float> distances(topK);
  std::vector<faiss::idx_t> labels(topK);

  // Used by the faiss iterator
  auto faissSearchContext =
      std::invoke([&]() -> vector::RocksDBFaissSearchContext {
        if (filterExpression == nullptr) {
          return {trx};
        }

        vector::SearchParametersContext searchCtx;
        searchCtx.trx = trx;
        searchCtx.filterExpression = filterExpression;
        if (inputRow != nullptr) {
          searchCtx.inputRow = *inputRow;
        }
        searchCtx.queryContext = &queryContext;
        searchCtx.filterVarsToRegs = &filterVarsToRegs;
        searchCtx.documentVariable = documentVariable;
        searchCtx.isCoveredByStoredValues = isCoveredByStoredValues;
        return searchCtx;
      });

  faiss::SearchParametersIVF searchParametersIvf;
  searchParametersIvf.nprobe =
      searchParameters.nProbe.value_or(_definition.defaultNProbe);
  searchParametersIvf.inverted_list_context = &faissSearchContext;
  _faissIndex->search(1, inputs.data(), topK, distances.data(), labels.data(),
                      &searchParametersIvf);

  // faiss returns squared distances for L2, square them so they are returned in
  // normal form
  if (_definition.metric == SimilarityMetric::kL2) {
    std::ranges::transform(distances, distances.begin(),
                           [](auto const& elem) { return std::sqrt(elem); });
  }

  return {std::move(labels), std::move(distances)};
}

bool RocksDBVectorIndex::isVectorIndexReady() const noexcept {
  return _trainingState.load(std::memory_order_acquire) ==
         VectorIndexTrainingState::kReady;
}

Result RocksDBVectorIndex::readDocumentVectorData(velocypack::Slice const doc,
                                                  std::vector<float>& output) {
  return vector::readDocumentVectorData(doc, _fields, _definition.dimension,
                                        output);
}

void RocksDBVectorIndex::applyTrainingResult(
    std::shared_ptr<faiss::IndexIVF> faissIndex, TrainedData trainedData) {
  _faissIndex = std::move(faissIndex);
  _trainedData = std::move(trainedData);

  _faissIndex->replace_invlists(
      new vector::RocksDBInvertedLists(this, &collection(), _definition.nLists,
                                       _faissIndex->code_size),
      true /* faiss owns the inverted list */);
}

bool RocksDBVectorIndex::setTrainingState(
    VectorIndexTrainingState expected,
    VectorIndexTrainingState desired) noexcept {
  if (!_trainingState.compare_exchange_strong(expected, desired,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire)) {
    LOG_TOPIC("e167b", WARN, Logger::ENGINES)
        << "Training state CAS failed: desired "
        << trainingStateToString(desired) << ", actual "
        << trainingStateToString(expected);
    return false;
  }

  return true;
}

void RocksDBVectorIndex::resetTrainingState() noexcept {
  _trainingState.exchange(VectorIndexTrainingState::kUnusable,
                          std::memory_order_acq_rel);
}

Result RocksDBVectorIndex::prepareIndex(std::unique_ptr<rocksdb::Iterator> it,
                                        rocksdb::Slice upper,
                                        RocksDBMethods* /*methods*/) {
  std::vector<float> input;
  input.reserve(_definition.dimension);

  while (it->Valid() && it->key().compare(upper) < 0) {
    auto doc = VPackSlice(reinterpret_cast<uint8_t const*>(it->value().data()));
    if (auto const res = vector::readDocumentVectorData(
            doc, _fields, _definition.dimension, input);
        res.fail()) {
      if (_sparse && res.is(TRI_ERROR_BAD_PARAMETER)) {
        it->Next();
        continue;
      }
      return res;
    }
    input.clear();
    it->Next();
  }

  return {};
}

void RocksDBVectorIndex::truncateCommit(TruncateGuard&& guard,
                                        TRI_voc_tick_t tick,
                                        transaction::Methods* trx) {
  resetTrainingState();
  _faissIndex.reset();
  _trainedData = {};
  RocksDBIndex::truncateCommit(std::move(guard), tick, trx);
}

/// @brief inserts a document into the index
Result RocksDBVectorIndex::insert(transaction::Methods& trx,
                                  RocksDBMethods* methods,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& /*options*/,
                                  bool /*performChecks*/) {
  std::vector<float> input;
  input.reserve(_definition.dimension);
  if (auto const res = readDocumentVectorData(doc, input); res.fail()) {
    // ignore the documents without the embedding field if the index is
    // sparse
    if (_sparse && res.is(TRI_ERROR_BAD_PARAMETER)) {
      return {};
    }
    return res;
  }
  // During initial fill or deferred training, only count; do not write.
  // During kIngesting we're in fillIndexBackground and must perform the
  // actual insert.
  if (const auto state = _trainingState.load();
      state == VectorIndexTrainingState::kUnusable ||
      state == VectorIndexTrainingState::kTraining) {
    return {};
  }
  TRI_ASSERT(_faissIndex != nullptr);

  if (_definition.metric == SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(_definition.dimension, 1, input.data());
  }

  // Trained: normal FAISS path
  faiss::idx_t listId{0};
  TRI_ASSERT(_faissIndex->quantizer != nullptr);
  _faissIndex->quantizer->assign(1, input.data(), &listId);

  RocksDBKey rocksdbKey;
  rocksdbKey.constructVectorIndexValue(objectId(), listId, documentId);
  std::unique_ptr<uint8_t[]> flat_codes(new uint8_t[_faissIndex->code_size]);
  _faissIndex->encode_vectors(1, input.data(), &listId, flat_codes.get());

  auto value = std::invoke([&]() {
    if (hasStoredValues()) {
      RocksDBVectorIndexEntryValue rocksdbEntryValue;
      rocksdbEntryValue.encodedValue = std::vector<uint8_t>(
          flat_codes.get(), flat_codes.get() + _faissIndex->code_size);

      auto const extractedAttributeValues =
          transaction::extractAttributeValues(trx, _storedValues, doc, true)
              ->get();
      rocksdbEntryValue.storedValues = extractedAttributeValues->sharedSlice();

      return RocksDBValue::VectorIndexValue(rocksdbEntryValue);
    } else {
      // Store raw encoded values directly for better performance and
      // backwards compatibility
      return RocksDBValue::VectorIndexValue(flat_codes.get(),
                                            _faissIndex->code_size);
    }
  });

  auto const status = methods->Put(_cf, rocksdbKey, value.string(), false);

  return rocksutils::convertStatus(status);
}

/// @brief removes a document from the index
Result RocksDBVectorIndex::remove(transaction::Methods& /*trx*/,
                                  RocksDBMethods* methods,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& /*options*/) {
  std::vector<float> input;
  input.reserve(_definition.dimension);
  if (auto const res = readDocumentVectorData(doc, input); res.fail()) {
    if (_sparse && res.is(TRI_ERROR_BAD_PARAMETER)) {
      return {};
    }
    return res;
  }

  if (auto const state = _trainingState.load();
      state == VectorIndexTrainingState::kUnusable ||
      state == VectorIndexTrainingState::kTraining) {
    return {};
  }

  if (_definition.metric == SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(_definition.dimension, 1, input.data());
  }

  faiss::idx_t listId{0};
  TRI_ASSERT(_faissIndex->quantizer != nullptr);
  _faissIndex->quantizer->assign(1, input.data(), &listId);

  RocksDBKey rocksdbKey;
  rocksdbKey.constructVectorIndexValue(objectId(), listId, documentId);
  auto const status = methods->Delete(_cf, rocksdbKey);

  if (!status.ok()) {
    auto const res = rocksutils::convertStatus(status);
    THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
  }

  return {};
}

UserVectorIndexDefinition const&
RocksDBVectorIndex::getVectorIndexDefinition() {
  return getDefinition();
}

bool RocksDBVectorIndex::hasStoredValues() const noexcept {
  return !_storedValues.empty();
}

StoredValues const& RocksDBVectorIndex::storedValues() const {
  return _storedValues;
}

}  // namespace arangodb
