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
#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/AqlValue.h"
#include "Aql/DocumentExpressionContext.h"
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
#include "RocksDBEngine/RocksDBMetaCollection.h"
#include "VectorIndex/VectorIndexDefinition.h"
#include "VectorIndex/Metadata.h"

#include "faiss/MetricType.h"
#include "faiss/utils/distances.h"

namespace arangodb {

using vector::CaptureShape;
using vector::FilterMode;
using vector::ProjectionMode;

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
    vector::VectorSearchContext const& ctx,
    containers::NodeHashMap<LocalDocumentId, velocypack::SharedSlice>*
        captureSink) {
  if (config.strategy.filter == FilterMode::kNone) {
    vector::IteratorContext simpleCtx;
    simpleCtx.trx = ctx.trx;
    simpleCtx.capturedDocuments = captureSink;
    return simpleCtx;
  }
  TRI_ASSERT(ctx.queryContext != nullptr);
  TRI_ASSERT(config.filterExpression != nullptr);

  vector::IteratorFilterContext searchCtx;
  searchCtx.trx = ctx.trx;
  searchCtx.filterExpression = config.filterExpression;
  if (ctx.inputRow != nullptr) {
    searchCtx.inputRow = *ctx.inputRow;
  }
  searchCtx.queryContext = ctx.queryContext;
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

  loadStoredMetadata(info);
  _liveNProbe.store(_trainedData.tunedNProbe.value_or(0),
                    std::memory_order_relaxed);

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

void RocksDBVectorIndex::loadStoredMetadata(velocypack::Slice info) {
  // Try the V2 slot first, then fall back to the V1 slot. The slots are
  // distinct so an old binary that only knows the V1 slot will simply miss
  // V2 metadata after a downgrade and treat the index as unusable
  auto readSlot = [&](vector::VectorIndexFormatVersion version,
                      std::string& raw) -> bool {
    RocksDBKey key;
    key.constructVectorIndexTrainedData(objectId(), version);
    rocksdb::ReadOptions ro;
    return _engine.db()->GetRootDB()->Get(ro, _cf, key.string(), &raw).ok();
  };

  // Read leniently: a record may carry fields this binary no longer knows
  // (e.g. the legacy tunedNProbe once it is removed) and must still load.
  inspection::ParseOptions const opts{.ignoreUnknownFields = true};

  vector::OwnedMetadata result;
  std::string raw;
  if (readSlot(vector::VectorIndexFormatVersion::kV2, raw) ||
      readSlot(vector::VectorIndexFormatVersion::kV1, raw)) {
    auto slice =
        velocypack::Slice(reinterpret_cast<uint8_t const*>(raw.data()));
    velocypack::deserialize(slice, result, opts);
  } else if (auto data = info.get("trainedData"); !data.isNone()) {
    // Backwards compatibility: load from definitions CF for pre-migration
    // indexes. Such records contain only TrainedData fields (codeData +
    // tunedNProbe), so formatVersion stays at the default kV1.
    velocypack::deserialize(data, result, opts);
  } else {
    // Brand-new index: stamp it with the current on-disk format. Has no
    // effect for indexes without storedValues since their entry layout is
    // already format-agnostic.
    result.formatVersion = vector::kCurrentVectorIndexFormatVersion;
  }

  _trainedData.codeData = std::move(result.codeData);
  _trainedData.tunedNProbe = result.tunedNProbe;
  _trainedData.tunedTables = std::move(result.tunedTables);
  _formatVersion = result.formatVersion;
}

Result RocksDBVectorIndex::persistMetadata() const {
  // View alias: codeData is borrowed from live state, not copied.
  vector::MetadataView view{_trainedData.codeData, _trainedData.tunedNProbe,
                            _trainedData.tunedTables, _formatVersion};
  velocypack::Builder builder;
  velocypack::serialize(builder, view);

  // V1 metadata stays at the legacy slot so older binaries can still read it
  // after a downgrade. V2 metadata is parked in a separate slot; an old binary
  // looking at the V1 slot won't find it and treats the index as unusable.
  RocksDBKey key;
  key.constructVectorIndexTrainedData(objectId(), _formatVersion);
  auto value = RocksDBValue::VectorIndexValue(builder.slice());

  auto* vectorCF = RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::VectorIndex);
  rocksdb::WriteOptions wo;
  auto status = _engine.db()->GetRootDB()->Put(wo, vectorCF, key.string(),
                                               value.string());
  if (!status.ok()) {
    return Result{TRI_ERROR_INTERNAL,
                  std::string{"Failed to persist vector index metadata: "} +
                      status.ToString()};
  }
  return {};
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

bool RocksDBVectorIndex::getNormalizedVectorFromDocument(
    const velocypack::Slice& docSlice, vector::Vector& vec) {
  if (readDocumentVectorData(docSlice, vec).fail()) {
    return false;
  }
  auto dim = vec.size();
  if (_definition.metric == vector::SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(dim, 1, vec.data());
  }
  return true;
}

float RocksDBVectorIndex::computeDistance(const vector::Vector& vec1,
                                          const vector::Vector& vec2,
                                          bool isDescending) {
  TRI_ASSERT(vec1.size() == vec2.size())
      << "Vector dimensions don't match, "
      << "[" << vec1 << "] != [" << vec2 << "]";

  auto dim = vec1.size();

  float distance;
  if (isDescending)
    distance = faiss::fvec_inner_product(vec1.data(), vec2.data(), dim);
  else
    distance = faiss::fvec_L2sqr(vec1.data(), vec2.data(), dim);

  return distance;
}

bool RocksDBVectorIndex::filterDocuments(
    vector::VectorSearchConfig const& config,
    vector::VectorSearchContext const& ctx, velocypack::Slice docSlice) {
  TRI_ASSERT(ctx.queryContext != nullptr);
  TRI_ASSERT(config.filterExpression != nullptr);
  TRI_ASSERT(config.documentVariable != nullptr);
  TRI_ASSERT(ctx.inputRow != nullptr);

  auto* trx = ctx.trx;
  aql::AqlFunctionsInternalCache functionsCache;

  aql::GenericDocumentExpressionContext exprCtx(
      *trx, *ctx.queryContext, functionsCache, config.filterVarsToRegs,
      *ctx.inputRow, config.documentVariable);
  exprCtx.setCurrentDocument(docSlice);
  bool mustDestroy = false;
  aql::AqlValue result =
      config.filterExpression->execute(&exprCtx, mustDestroy);
  aql::AqlValueGuard guard(result, mustDestroy);
  return result.toBoolean();
}

void RocksDBVectorIndex::captureDocument(
    vector::VectorSearchConfig const& config,
    vector::VectorSearchContext const& ctx,
    containers::NodeHashMap<LocalDocumentId, velocypack::SharedSlice>*
        captureSink,
    LocalDocumentId docId, velocypack::Slice docSlice) {
  if (captureSink == nullptr) {
    return;
  }

  auto* trx = ctx.trx;
  auto const& projectionMode = config.strategy.projection;

  if (projectionMode == vector::ProjectionMode::kDocument) {
    captureSink->insert_or_assign(docId, vector::toOwnedSharedSlice(docSlice));
  } else if (projectionMode == vector::ProjectionMode::kCovered &&
             hasStoredValues()) {
    auto extracted =
        transaction::extractAttributeValues(*trx, _storedValues, docSlice, true)
            ->get();
    captureSink->insert_or_assign(docId, extracted->sharedSlice());
  }
}

std::pair<vector::Labels, vector::Distances>
RocksDBVectorIndex::bruteForceSearch(
    vector::Vector& searchVector, vector::VectorSearchConfig const& config,
    vector::VectorSearchContext const& ctx,
    containers::NodeHashMap<LocalDocumentId, velocypack::SharedSlice>*
        captureSink) {
  auto const dim = _definition.dimension;
  auto const topK = config.topK;
  auto* trx = ctx.trx;
  // auto const& projectionMode = config.strategy.projection;

  bool const isDescending =
      _definition.metric == vector::SimilarityMetric::kCosine ||
      _definition.metric == vector::SimilarityMetric::kInnerProduct;

  vector::Labels labels(topK, -1);
  vector::Distances distances(topK);
  std::size_t n = 0;

  auto iter = _collection.getPhysical()->getAllIterator(trx, ReadOwnWrites::no);

  vector::Vector currentDocVector;
  currentDocVector.reserve(dim);

  bool const hasFilter = config.strategy.filter != FilterMode::kNone;
  aql::AqlFunctionsInternalCache functionsCache;

  auto eraseCaptured = [&](vector::VectorIndexLabelId id) {
    if (captureSink != nullptr && id >= 0) {
      captureSink->erase(LocalDocumentId{static_cast<std::uint64_t>(id)});
    }
  };

  //  Iterate over all documents in the shard and build the heap.
  //  TODO: Redesign and refactor the min/max implementations
  //  https://arangodb.atlassian.net/browse/COR-615
  iter->allDocuments([&](LocalDocumentId docId, aql::DocumentData&&,
                         velocypack::Slice docSlice) -> bool {
    currentDocVector.clear();
    auto ret = getNormalizedVectorFromDocument(docSlice, currentDocVector);
    if (!ret) {
      return true;
    }

    if (hasFilter && !filterDocuments(config, ctx, docSlice)) {
      return true;
    }

    auto dist = computeDistance(currentDocVector, searchVector, isDescending);
    auto id = static_cast<vector::VectorIndexLabelId>(docId.id());

    if (n < topK) {
      if (isDescending) {
        faiss::minheap_push(n + 1, distances.data(), labels.data(), dist, id);
      } else {
        faiss::maxheap_push(n + 1, distances.data(), labels.data(), dist, id);
      }
      captureDocument(config, ctx, captureSink, docId, docSlice);
      ++n;
    } else {
      if (isDescending) {
        if (dist > distances[0]) {
          eraseCaptured(labels[0]);
          faiss::minheap_replace_top(topK, distances.data(), labels.data(),
                                     dist, id);
          captureDocument(config, ctx, captureSink, docId, docSlice);
        }
      } else {
        if (dist < distances[0]) {
          eraseCaptured(labels[0]);
          faiss::maxheap_replace_top(topK, distances.data(), labels.data(),
                                     dist, id);
          captureDocument(config, ctx, captureSink, docId, docSlice);
        }
      }
    }

    return true;
  });

  // Truncate labels and distances to min(topK, total_no_of_documents)
  labels.resize(n);
  distances.resize(n);

  // Reorder heap so results are sorted
  if (isDescending) {
    faiss::minheap_reorder(topK, distances.data(), labels.data());
  } else {
    faiss::maxheap_reorder(topK, distances.data(), labels.data());
  }

  // L2: fvec_L2sqr returns squared distances, take sqrt
  if (_definition.metric == vector::SimilarityMetric::kL2) {
    std::ranges::transform(distances, distances.begin(),
                           [](float d) { return std::sqrt(d); });
  }

  return {std::move(labels), std::move(distances)};
}

vector::SearchResult RocksDBVectorIndex::readBatch(
    vector::VectorSearchConfig const& config,
    vector::VectorSearchContext const& ctx) {
  TRI_ASSERT(ctx.inputs != nullptr);
  TRI_ASSERT(ctx.trx != nullptr);
  // The on_heap_changed are not thread safe unless this is true

  auto& inputs = *ctx.inputs;
  TRI_ASSERT(inputs.size() == _definition.dimension)
      << "Number of components does not match vector dimension, topK: "
      << config.topK << ", dimension: " << _definition.dimension
      << ", inputs size: " << inputs.size();

  if (_definition.metric == vector::SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(_definition.dimension, 1, inputs.data());
  }

  vector::SearchResult result;
  bool const captureNeeded =
      config.strategy.projection == ProjectionMode::kCovered ||
      (config.strategy.projection == ProjectionMode::kDocument &&
       config.strategy.filter == FilterMode::kDocument);
  auto* captureSink = captureNeeded ? &result.capturedDocuments : nullptr;

  if (auto const state = _trainingState.load();
      state != VectorIndexTrainingState::kReady) {
    if (isLinearScanEnabled()) {
      // Not enough training data: fall back to linear scan on this shard so
      // cluster vector search stays usable.

      auto [labels, distances] =
          bruteForceSearch(inputs, config, ctx, captureSink);
      result.labels = std::move(labels);
      result.distances = std::move(distances);
      return result;
    }

    // This should never happen — the optimizer should not use
    // EnumerateNearVectorNode when the vector index is not ready.
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_VECTOR_SEARCH_NOT_APPLIED,
        std::format("vector index is in state '{}', expected 'ready'",
                    trainingStateToString(state)));
  }
  TRI_ASSERT(_faissIndex != nullptr);

  // Trained: normal FAISS IVF search
  result.distances.resize(config.topK);
  result.labels.resize(config.topK);

  auto faissSearchContext = makeFaissIteratorContext(config, ctx, captureSink);

  faiss::SearchParametersIVF searchParametersIvf;
  searchParametersIvf.nprobe =
      config.searchParameters.nProbe.value_or(effectiveNProbe());
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

bool RocksDBVectorIndex::isLinearScanEnabled() const noexcept { return true; }

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
  _liveNProbe.store(_trainedData.tunedNProbe.value_or(0),
                    std::memory_order_release);

  _faissIndex->replace_invlists(
      new vector::RocksDBInvertedLists(this, &collection(), _faissIndex->nlist,
                                       _faissIndex->code_size),
      true /* faiss owns the inverted list */);
}

std::shared_ptr<faiss::IndexIVF> RocksDBVectorIndex::cloneFaissIndex() {
  auto clone = vector::VectorIndexTrainer::restoreFromTrainedData(_trainedData);
  clone->replace_invlists(
      new vector::RocksDBInvertedLists(this, &collection(), clone->nlist,
                                       clone->code_size),
      true /* faiss owns the inverted list */);
  return clone;
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

void RocksDBVectorIndex::setTrainingError(std::string error) noexcept {
  std::lock_guard lock(_trainingErrorMutex);
  _trainingError = std::move(error);
}

std::string RocksDBVectorIndex::trainingError() const {
  std::lock_guard lock(_trainingErrorMutex);
  return _trainingError;
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
  setTrainingError(std::string{StaticStrings::VectorIndexDefaultTrainingError});
  _faissIndex.reset();
  _trainedData = {};
  _liveNProbe.store(0, std::memory_order_release);
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
      auto const extractedAttributeValues =
          transaction::extractAttributeValues(trx, _storedValues, doc, true)
              ->get();
      auto storedValues = extractedAttributeValues->sharedSlice();

      if (_formatVersion == vector::VectorIndexFormatVersion::kV2) {
        return RocksDBValue::VectorIndexValueV2(
            flat_codes.get(), _faissIndex->code_size, storedValues);
      }
      RocksDBVectorIndexEntryValue rocksdbEntryValue;
      rocksdbEntryValue.encodedValue = std::vector<uint8_t>(
          flat_codes.get(), flat_codes.get() + _faissIndex->code_size);
      rocksdbEntryValue.storedValues = std::move(storedValues);
      return RocksDBValue::VectorIndexValueV1(rocksdbEntryValue);
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
