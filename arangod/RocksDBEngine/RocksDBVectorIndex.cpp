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
#include <functional>
#include <cstring>
#include <thread>
#include <omp.h>

#include "Aql/AstNode.h"
#include "Basics/BoundedChannel.h"
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
#include "Indexes/IndexIterator.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"
#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/ExecutorExpressionContext.h"
#include "Aql/DocumentExpressionContext.h"

#include <rocksdb/db.h>
#include <rocksdb/write_batch.h>

#include "faiss/MetricType.h"
#include "faiss/utils/distances.h"
#include "faiss/utils/Heap.h"

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

std::string_view buildStateToString(
    VectorIndexBuildState const state) noexcept {
  switch (state) {
    case VectorIndexBuildState::kUninitialized:
      return "uninitialized";
    case VectorIndexBuildState::kTraining:
      return "training";
    case VectorIndexBuildState::kBuilding:
      return "building";
    case VectorIndexBuildState::kReady:
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
    velocypack::deserialize(data, _trainedData.emplace());
  }

  if (_trainedData) {
    auto result =
        vector::VectorIndexTrainer::restoreFromTrainedData(*_trainedData);
    _faissIndex = std::move(result.faissIndex);

    _faissIndex->replace_invlists(
        new vector::RocksDBInvertedLists(this, &coll, _definition.nLists,
                                         _faissIndex->code_size),
        true /* faiss owns the inverted list */);

    setBuildState(VectorIndexBuildState::kReady);
  }
  // Below 1000 documents training is not worth the effort nor having a index
  // 39 is the minumum number of documents to train the vector index, but that
  // does not mean it cannot be achieved with less documents.
  _trainingThreshold = std::max<std::int64_t>(_definition.nLists * 39, 1000);
}

void RocksDBVectorIndex::joinBuildThread() noexcept {
  if (!_buildThread.joinable()) {
    return;
  }
  // Avoid self-join: if the current thread is the build thread, joining would
  // deadlock and the runtime can call std::terminate().
  if (std::this_thread::get_id() == _buildThread.get_id()) {
    return;
  }
  _buildThread.request_stop();
  _buildThread.join();
}

RocksDBVectorIndex::~RocksDBVectorIndex() {
  // Join from this thread only if we are not the build thread (otherwise
  // ~jthread() would run in the build thread and effectively self-join).
  joinBuildThread();
}

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

  builder.add("buildState", VPackValue(buildStateToString(_buildState)));

  if (_trainedData && Index::hasFlag(flags, Index::Serialize::Internals) &&
      !Index::hasFlag(flags, Index::Serialize::Maintenance)) {
    builder.add(VPackValue("trainedData"));
    velocypack::serialize(builder, *_trainedData);
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

  if (_buildState != VectorIndexBuildState::kReady) {
    return bruteForceSearch(inputs, topK, trx, filterExpression, inputRow,
                            &queryContext, &filterVarsToRegs, documentVariable);
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

Result RocksDBVectorIndex::readDocumentVectorData(velocypack::Slice const doc,
                                                  std::vector<float>& output) {
  return vector::readDocumentVectorData(doc, _fields, _definition.dimension,
                                        output);
}

void RocksDBVectorIndex::applyTrainingResult(vector::TrainingResult result) {
  _faissIndex = std::move(result.faissIndex);
  _trainedData.emplace(std::move(result.trainedData));

  _faissIndex->replace_invlists(
      new vector::RocksDBInvertedLists(this, &collection(), _definition.nLists,
                                       _faissIndex->code_size),
      true /* faiss owns the inverted list */);
}

void RocksDBVectorIndex::tryBuilding() {
  if (_documentCount < _trainingThreshold) {
    return;
  }
  VectorIndexBuildState expected{VectorIndexBuildState::kUninitialized};
  if (!_buildState.compare_exchange_strong(
          expected, VectorIndexBuildState::kTraining, std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    return;  // another thread already started training, or state changed
  }

  LOG_VECTOR_INDEX("e161b", INFO, Logger::ENGINES)
      << "Training threshold reached (" << _trainingThreshold
      << " documents). Starting deferred training.";

  _buildThread = std::jthread([this] {
    auto const indexId = id().id();
    try {
      vector::VectorIndexBuildManager builder(*this);
      if (auto const res = builder.build(); res.fail()) {
        LOG_TOPIC("e164b", ERR, Logger::ENGINES)
            << "[index=" << indexId
            << "] Vector build failed: " << res.errorMessage();
        this->setBuildState(VectorIndexBuildState::kUninitialized);
        return;
      }
      this->setBuildState(VectorIndexBuildState::kReady);
    } catch (std::exception const& ex) {
      LOG_TOPIC("e164c", ERR, Logger::ENGINES)
          << "[index=" << indexId
          << "] Vector build thread exception: " << ex.what();
      this->setBuildState(VectorIndexBuildState::kUninitialized);
    } catch (...) {
      LOG_TOPIC("e164d", ERR, Logger::ENGINES)
          << "[index=" << indexId << "] Vector build thread unknown exception";
      this->setBuildState(VectorIndexBuildState::kUninitialized);
    }
  });
#ifdef TRI_HAVE_SYS_PRCTL_H
  pthread_setname_np(_buildThread.native_handle(), "VectorIndexBuilder");
#endif
}

void RocksDBVectorIndex::setBuildState(VectorIndexBuildState state) noexcept {
  _buildState.store(state);
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
    // We ignore the documents without the embedding field if the index is
    // sparse
    if (_sparse && res.is(TRI_ERROR_BAD_PARAMETER)) {
      return {};
    }
    return res;
  }
  // During initial fill or deferred training trigger, only count and maybe
  // start building; do not write. During kBuilding we're in fillIndexBackground
  // and must perform the actual insert.
  if (const auto buildState = _buildState.load();
      buildState == VectorIndexBuildState::kUninitialized ||
      buildState == VectorIndexBuildState::kTraining) {
    // For non-sparse index, validate that the document has the vector field
    // even when we are not writing yet; otherwise fail early.
    _documentCount.fetch_add(1);
    tryBuilding();
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
  if (auto const buildState = _buildState.load();
      buildState == VectorIndexBuildState::kUninitialized ||
      buildState == VectorIndexBuildState::kTraining) {
    // Nothing stored in the vector index yet, nothing to remove
    _documentCount.fetch_sub(1, std::memory_order_relaxed);
    return {};
  }

  std::vector<float> input;
  input.reserve(_definition.dimension);
  if (auto const res = readDocumentVectorData(doc, input); res.fail()) {
    return res;
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

std::pair<std::vector<VectorIndexLabelId>, std::vector<float>>
RocksDBVectorIndex::bruteForceSearch(
    std::vector<float>& inputs, std::size_t topK, transaction::Methods* trx,
    aql::Expression* filterExpression, aql::InputAqlItemRow const* inputRow,
    aql::QueryContext* queryContext,
    std::vector<std::pair<aql::VariableId, aql::RegisterId>> const*
        filterVarsToRegs,
    aql::Variable const* documentVariable) {
  auto const dim = _definition.dimension;
  bool const isDescending =
      _definition.metric == SimilarityMetric::kCosine ||
      _definition.metric == SimilarityMetric::kInnerProduct;

  std::vector<faiss::idx_t> labels(topK, -1);
  // Initialize heap: max-heap for L2 (keep smallest distances),
  // min-heap for IP/cosine (keep largest inner products)
  auto const minValue = std::invoke([&]() {
    if (isDescending) {
      return -std::numeric_limits<float>::max();
    } else {
      return std::numeric_limits<float>::max();
    }
  });
  std::vector<float> distances(topK, minValue);

  auto iter = _collection.getPhysical()->getAllIterator(trx, ReadOwnWrites::no);
  aql::AqlFunctionsInternalCache aqlFunctionsInternalCache;
  iter->allDocuments([&](LocalDocumentId docId, aql::DocumentData&&,
                         velocypack::Slice docSlice) -> bool {
    std::vector<float> vec;
    vec.reserve(dim);
    if (readDocumentVectorData(docSlice, vec).fail()) {
      return true;
    }

    if (filterExpression != nullptr) {
      TRI_ASSERT(queryContext != nullptr);
      TRI_ASSERT(filterVarsToRegs != nullptr);
      TRI_ASSERT(inputRow != nullptr);

      aql::GenericDocumentExpressionContext ctx(
          *trx, *queryContext, aqlFunctionsInternalCache, *filterVarsToRegs,
          *inputRow, documentVariable);
      ctx.setCurrentDocument(docSlice);

      bool mustDestroy{false};
      aql::AqlValue expressionEvaluation =
          filterExpression->execute(&ctx, mustDestroy);
      aql::AqlValueGuard const guard(expressionEvaluation, mustDestroy);
      if (!expressionEvaluation.toBoolean()) {
        return true;
      }
    }

    if (_definition.metric == SimilarityMetric::kCosine) {
      faiss::fvec_renorm_L2(dim, 1, vec.data());
    }

    auto const id = static_cast<faiss::idx_t>(docId.id());

    if (isDescending) {
      float dist = faiss::fvec_inner_product(inputs.data(), vec.data(), dim);
      if (dist > distances[0]) {
        faiss::minheap_replace_top(topK, distances.data(), labels.data(), dist,
                                   id);
      }
    } else {
      float dist = faiss::fvec_L2sqr(inputs.data(), vec.data(), dim);
      if (dist < distances[0]) {
        faiss::maxheap_replace_top(topK, distances.data(), labels.data(), dist,
                                   id);
      }
    }
    return true;
  });

  // Reorder heap so results are sorted
  if (isDescending) {
    faiss::minheap_reorder(topK, distances.data(), labels.data());
  } else {
    faiss::maxheap_reorder(topK, distances.data(), labels.data());
  }

  // L2: fvec_L2sqr returns squared distances, take sqrt
  if (_definition.metric == SimilarityMetric::kL2) {
    std::ranges::transform(distances, distances.begin(),
                           [](float d) { return std::sqrt(d); });
  }

  return {std::move(labels), std::move(distances)};
}

Result RocksDBVectorIndex::ingestVectors(
    rocksdb::DB* rootDB, std::unique_ptr<rocksdb::Iterator> documentIterator) {
  auto const dim = _definition.dimension;
  auto const& fields = _fields;
  auto const hasStored = hasStoredValues();
  auto const& stored = _storedValues;

  struct DocumentVectors {
    std::vector<LocalDocumentId> docIds;
    std::vector<float> vectors;
    std::vector<velocypack::SharedSlice> storedValues;
  };

  struct EncodedVectors {
    std::vector<LocalDocumentId> docIds;
    std::unique_ptr<faiss::idx_t[]> lists;
    std::unique_ptr<uint8_t[]> codes;
    std::vector<velocypack::SharedSlice> storedValues;
  };

  struct BlockCounters {
    uint64_t readProduceBlocked{0};
    uint64_t encodeProduceBlocked{0};
    uint64_t encodeConsumeBlocked{0};
    uint64_t writeConsumeBlocked{0};
  } counters;

  BoundedChannel<DocumentVectors> documentChannel{5};
  BoundedChannel<EncodedVectors> encodedChannel{5};

  constexpr auto numReaders = 1;
  constexpr auto numEncoders = 8;
  constexpr auto numWriters = 2;

  constexpr auto documentPerBatch = 8000;

  std::atomic<std::size_t> countBatches{0};
  std::atomic<std::size_t> countDocuments{0};

  std::atomic<bool> hasError;
  Result firstError;

  auto setResult = [&](Result result) {
    if (result.fail()) {
      if (hasError.exchange(true) == false) {
        firstError = std::move(result);
      }
    }
  };

  auto errorExceptionHandler = [&](auto&& fn) noexcept {
    try {
      auto constexpr returnsResult = requires {
        { fn() } -> std::convertible_to<Result>;
      };

      if constexpr (returnsResult) {
        setResult(fn());
      }
      else {
        fn();
      }
    } catch (basics::Exception const& e) {
      setResult({e.code(), e.message()});
    } catch (std::exception const& e) {
      setResult({TRI_ERROR_INTERNAL, e.what()});
    }
  };

  auto readDocuments = [&] {
    static_assert(numReaders == 1,
                  "this code is not prepared for multiple reads");

    errorExceptionHandler([&] {
      BoundedChannelProducerGuard guard(documentChannel);

      auto const prepareBatch = [&] {
        auto batch = std::make_unique<DocumentVectors>();
        batch->docIds.reserve(documentPerBatch);
        batch->vectors.reserve(documentPerBatch * dim);
        if (hasStored) {
          batch->storedValues.reserve(documentPerBatch);
        }
        return batch;
      };

      std::unique_ptr<DocumentVectors> batch = prepareBatch();
      while (documentIterator->Valid() && not hasError.load()) {
        LocalDocumentId docId = RocksDBKey::documentId(documentIterator->key());
        VPackSlice doc = RocksDBValue::data(documentIterator->value());
        if (auto const res = vector::readDocumentVectorData(doc, fields, dim,
                                                            batch->vectors);
            res.fail()) {
          if (res.is(TRI_ERROR_BAD_PARAMETER) && sparse()) {
            documentIterator->Next();
            continue;
          }
          THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
        }
        batch->docIds.push_back(docId);
        if (hasStored) {
          auto const extractedAttributeValues =
              transaction::extractAttributeValues(stored, doc, true);
          batch->storedValues.push_back(
              extractedAttributeValues->sharedSlice());
        }

        documentIterator->Next();
        if (batch->docIds.size() == documentPerBatch) {
          auto [shouldStop, blocked] = documentChannel.push(std::move(batch));
          counters.readProduceBlocked += blocked;

          if (shouldStop) {
            return;
          }
          batch = prepareBatch();
        }
      }

      if (_definition.metric == SimilarityMetric::kCosine) {
        faiss::fvec_renorm_L2(dim, batch->docIds.size(), batch->vectors.data());
      }

      if (batch) {
        std::ignore = documentChannel.push(std::move(batch));
      }
    });
  };

  auto encodeVectors = [&]() {
    BoundedChannelProducerGuard guard(encodedChannel);
    while (true) {
      auto [item, blocked] = documentChannel.pop();
      if (item == nullptr) {
        return;
      }

      bool shouldStop = false;
      errorExceptionHandler([&] {
        counters.encodeConsumeBlocked += blocked;
        auto n = item->docIds.size();
        countBatches += 1;
        countDocuments += n;

        float* x = item->vectors.data();
        std::unique_ptr<faiss::idx_t[]> coarse_idx(new faiss::idx_t[n]);
        _faissIndex->quantizer->assign(n, x, coarse_idx.get());
        auto code_size = _faissIndex->code_size;
        std::unique_ptr<uint8_t[]> flat_codes(new uint8_t[n * code_size]);

        // TODO: since we only use IVTFlat this is just copying the data.
        //  Probably we want to use some PQ encoding later on.
        _faissIndex->encode_vectors(n, x, coarse_idx.get(), flat_codes.get());

        auto encoded = std::make_unique<EncodedVectors>();
        encoded->docIds = std::move(item->docIds);
        encoded->lists = std::move(coarse_idx);
        encoded->codes = std::move(flat_codes);
        encoded->storedValues = std::move(item->storedValues);

        LOG_VECTOR_INDEX("e167b", INFO, Logger::ENGINES)
            << "ENCODE encoded " << encoded->docIds.size()
            << " vectors, code size: " << code_size;
        bool pushBlocked = false;
        std::tie(shouldStop, pushBlocked) =
            encodedChannel.push(std::move(encoded));
        counters.encodeProduceBlocked += pushBlocked;
      });
      if (shouldStop) {
        break;
      }
    }
  };

  auto writeDocuments = [&] {
    rocksdb::WriteBatch batch;
    while (true) {
      auto [item, blocked] = encodedChannel.pop();
      if (item == nullptr) {
        break;
      }

      errorExceptionHandler([&] {
        counters.writeConsumeBlocked += blocked;
        batch.Clear();

        RocksDBKey key;
        rocksdb::Status status;

        for (size_t k = 0; k < item->docIds.size(); k++) {
          key.constructVectorIndexValue(objectId(), item->lists[k],
                                        item->docIds[k]);

          auto const value = std::invoke([&]() {
            auto* ptr = item->codes.get() + k * _faissIndex->code_size;
            if (hasStored) {
              RocksDBVectorIndexEntryValue rocksdbEntryValue;
              rocksdbEntryValue.encodedValue =
                  std::vector<uint8_t>(ptr, ptr + _faissIndex->code_size);
              rocksdbEntryValue.storedValues = std::move(item->storedValues[k]);

              return RocksDBValue::VectorIndexValue(rocksdbEntryValue);
            } else {
              return RocksDBValue::VectorIndexValue(ptr,
                                                    _faissIndex->code_size);
            }
          });

          status = batch.Put(_cf, key.string(), value.string());
          if (not status.ok()) {
            THROW_ARANGO_EXCEPTION(rocksutils::convertStatus(status));
          }
        }

        rocksdb::WriteOptions wo;
        status = rootDB->Write(wo, &batch);
        if (not status.ok()) {
          THROW_ARANGO_EXCEPTION(rocksutils::convertStatus(status));
        }
      });
    }
  };

  std::vector<std::jthread> threads;

  auto startNThreads = [&](auto& func, size_t n) {
    for (size_t k = 0; k < n; k++) {
      threads.emplace_back(func);
    }
  };

  LOG_VECTOR_INDEX("71c45", INFO, Logger::FIXME)
      << "Ingesting vectors into index. Threads: num-readers=" << numReaders
      << " num-encoders=" << numEncoders << " numWriters=" << numWriters;

  startNThreads(readDocuments, numReaders);
  startNThreads(encodeVectors, numEncoders);
  startNThreads(writeDocuments, numWriters);

  threads.clear();

  if (firstError.ok()) {
    LOG_VECTOR_INDEX("41658", INFO, Logger::FIXME)
        << "Ingestion done. Encoded " << countDocuments << " vectors in "
        << countBatches
        << " batches. Pipeline skew: " << counters.readProduceBlocked << " "
        << counters.encodeConsumeBlocked << " " << counters.encodeProduceBlocked
        << " " << counters.writeConsumeBlocked;
  } else {
    LOG_VECTOR_INDEX("96a80", ERR, Logger::FIXME)
        << "Ingestion failed: " << firstError;
  }

  return firstError;
}

}  // namespace arangodb
