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
      return StaticStrings::IndexTrainingStateUnusable;
    case VectorIndexTrainingState::kTraining:
      return StaticStrings::IndexTrainingStateTraining;
    case VectorIndexTrainingState::kIngesting:
      return StaticStrings::IndexTrainingStateIngesting;
    case VectorIndexTrainingState::kReady:
      return StaticStrings::IndexTrainingStateReady;
  }
}

namespace {

// Translate the high-level SearchStrategy into the concrete iterator
// context. The capture sink is only wired when the iterator can supply
// per-survivor data of the shape the executor needs.
vector::RocksDBFaissIteratorContext makeFaissIteratorContext(
    vector::VectorSearchConfig const& config,
    containers::NodeHashMap<LocalDocumentId, velocypack::SharedSlice>*
        captureSink) {
  using vector::CaptureShape;
  using vector::FilterMode;
  using vector::ProjectionMode;

  if (config.strategy.filter == FilterMode::kNone) {
    vector::IteratorContext simpleCtx;
    simpleCtx.trx = config.trx;
    simpleCtx.capturedDocuments = captureSink;
    return simpleCtx;
  }
  TRI_ASSERT(config.queryContext != nullptr);
  TRI_ASSERT(config.filterExpression != nullptr);

  vector::IteratorFilterContext searchCtx;
  searchCtx.trx = config.trx;
  searchCtx.filterExpression = config.filterExpression;
  if (config.inputRow != nullptr) {
    searchCtx.inputRow = *config.inputRow;
  }
  searchCtx.queryContext = config.queryContext;
  searchCtx.filterVarsToRegs = &config.filterVarsToRegs;
  searchCtx.documentVariable = config.documentVariable;
  searchCtx.useStoredValuesIterator =
      (config.strategy.filter == FilterMode::kStoredValues);
  searchCtx.capturedDocuments = captureSink;
  // Scenario 6: filter loaded the doc, projections need the doc -> stash
  // the doc itself. Scenario 8: filter loaded the doc but projections are
  // covered -> stash only the storedValues array (cheaper).
  searchCtx.captureShape =
      (config.strategy.projection == ProjectionMode::kDocument)
          ? CaptureShape::kFullDocument
          : CaptureShape::kStoredValues;
  return searchCtx;
}

}  // namespace

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

  auto metadata = loadVectorIndexMetadata(info);
  _trainedData = std::move(metadata.trainedData);
  _formatVersion = metadata.formatVersion;

  if (!_trainedData.codeData.empty()) {
    _faissIndex =
        vector::VectorIndexTrainer::restoreFromTrainedData(_trainedData);

    _faissIndex->replace_invlists(
        new vector::RocksDBInvertedLists(this, &coll, _faissIndex->nlist,
                                         _faissIndex->code_size),
        true /* faiss owns the inverted list */);

    setTrainingState(VectorIndexTrainingState::kUnusable,
                     VectorIndexTrainingState::kReady);
  }

  _trainingThreshold = std::visit(
      overload{
          [](std::size_t fixed) { return fixed; },
          [](vector::NListsScalingSpec const& spec) { return spec.minNLists; }},
      _definition.nLists);
}

RocksDBVectorIndex::~RocksDBVectorIndex() = default;

vector::VectorIndexMetadata RocksDBVectorIndex::loadVectorIndexMetadata(
    velocypack::Slice info) const {
  RocksDBKey key;
  key.constructVectorIndexTrainedData(objectId());

  std::string raw;
  rocksdb::ReadOptions ro;
  auto status = _engine.db()->GetRootDB()->Get(ro, _cf, key.string(), &raw);

  vector::VectorIndexMetadata result;
  if (status.ok()) {
    auto slice =
        velocypack::Slice(reinterpret_cast<uint8_t const*>(raw.data()));
    velocypack::deserialize(slice, result);
  } else if (auto data = info.get("trainedData"); !data.isNone()) {
    // Backwards compatibility: load from definitions CF for pre-migration
    // indexes. Such records contain only TrainedData fields, so formatVersion
    // stays at the default kV1.
    velocypack::deserialize(data, result.trainedData);
  } else {
    // New index new format, does not make a diffrence if it does not ahve storedValues
    result.formatVersion = vector::kCurrentVectorIndexFormatVersion;
  }
  return result;
}

/// @brief Test if this index matches the definition
bool RocksDBVectorIndex::matchesDefinition(VPackSlice const& info) const {
  // check if we have the same parameter
  if (!RocksDBIndex::matchesDefinition(info)) {
    return false;
  }

  vector::UserVectorIndexDefinition definition;
  velocypack::deserialize(info.get("params"), definition);
  if (definition != _definition) {
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

  auto const trainingState = _trainingState.load();
  builder.add(StaticStrings::IndexTrainingState,
              VPackValue(trainingStateToString(trainingState)));
  if (trainingState == VectorIndexTrainingState::kUnusable) {
    builder.add(StaticStrings::ErrorMessage,
                VPackValue("not enough training data for vector index"));
  }

  if (auto const nLists = resolvedNLists(); nLists.has_value()) {
    builder.add(StaticStrings::IndexResolvedNLists, VPackValue(*nLists));
  }
}

vector::SearchResult RocksDBVectorIndex::readBatch(
    vector::VectorSearchConfig const& config) {
  TRI_ASSERT(config.inputs != nullptr);
  TRI_ASSERT(config.trx != nullptr);
  // The on_heap_changed are not thread safe unless this is true
  ADB_PROD_ASSERT(_faissIndex->parallel_mode == 0)
      << "FAISS parallel_mode must be 0; got " << _faissIndex->parallel_mode;

  auto& inputs = *config.inputs;
  TRI_ASSERT(inputs.size() == _definition.dimension)
      << "Number of components does not match vector dimension, topK: "
      << config.topK << ", dimension: " << _definition.dimension
      << ", inputs size: " << inputs.size();

  if (_definition.metric == vector::SimilarityMetric::kCosine) {
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
  vector::SearchResult result;
  result.distances.resize(config.topK);
  result.labels.resize(config.topK);

  using vector::FilterMode;
  using vector::ProjectionMode;
  bool const captureNeeded =
      config.strategy.projection == ProjectionMode::kCovered ||
      (config.strategy.projection == ProjectionMode::kDocument &&
       config.strategy.filter == FilterMode::kDocument);
  auto* captureSink = captureNeeded ? &result.capturedDocuments : nullptr;
  auto faissSearchContext = makeFaissIteratorContext(config, captureSink);

  faiss::SearchParametersIVF searchParametersIvf;
  searchParametersIvf.nprobe =
      config.searchParameters.nProbe.value_or(_definition.defaultNProbe);
  searchParametersIvf.inverted_list_context = &faissSearchContext;
  _faissIndex->search(1, inputs.data(), config.topK, result.distances.data(),
                      result.labels.data(), &searchParametersIvf);

  // faiss returns squared distances for L2, square them so they are returned in
  // normal form
  if (_definition.metric == vector::SimilarityMetric::kL2) {
    std::ranges::transform(result.distances, result.distances.begin(),
                           [](auto const& elem) { return std::sqrt(elem); });
  }

  return result;
}

bool RocksDBVectorIndex::isVectorIndexReady() const noexcept {
  return _trainingState.load(std::memory_order_acquire) ==
         VectorIndexTrainingState::kReady;
}

Result RocksDBVectorIndex::readDocumentVectorData(
    velocypack::Slice const doc, std::vector<float>& output) const {
  return vector::readDocumentVectorData(doc, _fields, _definition.dimension,
                                        output);
}

void RocksDBVectorIndex::applyTrainingResult(
    std::shared_ptr<faiss::IndexIVF> faissIndex,
    vector::TrainedData trainedData) {
  _faissIndex = std::move(faissIndex);
  _trainedData = std::move(trainedData);

  _faissIndex->replace_invlists(
      new vector::RocksDBInvertedLists(this, &collection(), _faissIndex->nlist,
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

ResultT<std::vector<float>> RocksDBVectorIndex::preModificationCheck(
    std::string_view operation, velocypack::Slice doc) const {
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

  if (auto const state = _trainingState.load(std::memory_order_acquire);
      state == VectorIndexTrainingState::kUnusable ||
      state == VectorIndexTrainingState::kTraining) {
    LOG_TOPIC("d1e0a", DEBUG, Logger::ENGINES) << std::format(
        "vector index {} not yet trained, skipping {}", _iid.id(), operation);

    // Not an error but the result is ignored
    return {};
  }

  if (_definition.metric == vector::SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(_definition.dimension, 1, input.data());
  }

  return {std::move(input)};
}

/// @brief inserts a document into the index
Result RocksDBVectorIndex::insert(transaction::Methods& trx,
                                  RocksDBMethods* methods,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& /*options*/,
                                  bool /*performChecks*/) {
  auto res = preModificationCheck("insert", doc);
  if (res.fail()) {
    return {res.errorNumber(), res.errorMessage()};
  }
  auto input = std::move(res.get());
  if (input.empty()) {
    // sparse or index or indexes in unusable/training state
    return {};
  }

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

      return _formatVersion == vector::VectorIndexFormatVersion::kV2
                 ? RocksDBValue::VectorIndexValueV2(rocksdbEntryValue)
                 : RocksDBValue::VectorIndexValueV1(rocksdbEntryValue);
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
  auto res = preModificationCheck("remove", doc);
  if (res.fail()) {
    return res.result();
  }
  auto input = std::move(res.get());
  if (input.empty()) {
    // sparse or index or indexes in unusable/training state
    return {};
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

vector::UserVectorIndexDefinition const&
RocksDBVectorIndex::getVectorIndexDefinition() {
  return getDefinition();
}

bool RocksDBVectorIndex::hasStoredValues() const noexcept {
  return !_storedValues.empty();
}

StoredValues const& RocksDBVectorIndex::storedValues() const {
  return _storedValues;
}

std::vector<std::vector<basics::AttributeName>> const&
RocksDBVectorIndex::coveredFields() const {
  return _storedValues;
}

}  // namespace arangodb
