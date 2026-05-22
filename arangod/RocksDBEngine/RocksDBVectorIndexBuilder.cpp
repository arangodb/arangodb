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

#include "RocksDBEngine/RocksDBVectorIndexBuilder.h"
#include "Basics/ResultT.h"
#include "Indexes/IndexFactory.h"
#include "Inspection/VPack.h"
#include "Metrics/Histogram.h"
#include "Metrics/LogScale.h"
#include "RocksDBEngine/RocksDBBuilderIndex.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "VectorIndex/VectorIndexAutoTuner.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <string>
#include <thread>

#include "Assertions/Assert.h"
#include "Basics/debugging.h"
#include "Basics/BoundedChannel.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
#include "Random/RandomGenerator.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBMetadata.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "RocksDBEngine/RocksDBVectorIndexList.h"
#include "Transaction/Helpers.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VectorIndex/VectorIndexTrainingSampler.h"
#include "VocBase/LogicalCollection.h"

#include <rocksdb/db.h>
#include <rocksdb/iterator.h>
#include <rocksdb/write_batch.h>
#include <velocypack/Iterator.h>
#include <velocypack/SharedSlice.h>
#include <velocypack/Slice.h>

#include "faiss/IndexFlat.h"
#include "faiss/IndexIVFFlat.h"
#include "faiss/impl/io.h"
#include "faiss/index_factory.h"
#include "faiss/index_io.h"
#include "faiss/utils/distances.h"

namespace arangodb::vector {

#define LOG_VECTOR_BUILD(lid, level, topic)                                 \
  LOG_TOPIC((lid), level, topic) << "[shard=" << _index.collection().name() \
                                 << ", index=" << _index.id().id() << "] "

// Shallow field-presence probe; ingestion already rejects malformed vectors.
bool hasVectorField(
    velocypack::Slice doc,
    std::vector<std::vector<basics::AttributeName>> const& fields) {
  TRI_ASSERT(fields.size() >= 1);
  try {
    return !rocksutils::accessDocumentPath(doc, fields[0]).isNone();
  } catch (velocypack::Exception const&) {
    return false;
  }
}

Result readDocumentVectorData(
    velocypack::Slice doc,
    std::vector<std::vector<basics::AttributeName>> const& fields,
    std::size_t dimension, std::vector<float>& output) {
  TRI_ASSERT(fields.size() >= 1);

  try {
    VPackSlice value = rocksutils::accessDocumentPath(doc, fields[0]);

    // this fails if index is not sparse
    if (value.isNone()) {
      return {TRI_ERROR_BAD_PARAMETER,
              std::format("vector field not present in document {}",
                          transaction::helpers::extractKeyFromDocument(doc))};
    }

    if (!value.isArray()) {
      return {TRI_ERROR_TYPE_ERROR,
              std::format("array expected for vector attribute for document {}",
                          transaction::helpers::extractKeyFromDocument(doc))};
    }

    if (value.length() != dimension) {
      return {TRI_ERROR_TYPE_ERROR,
              std::format("provided vector is not of matching dimension for "
                          "document {}, index dimension: {}, document "
                          "dimension: {}",
                          transaction::helpers::extractKeyFromDocument(doc),
                          dimension, value.length())};
    }

    // We don't make assumptions here if output contains one or more vectors
    for (auto const d : VPackArrayIterator(value)) {
      if (not d.isNumber<double>()) {
        return {
            TRI_ERROR_TYPE_ERROR,
            std::format("vector contains data not representable as double for "
                        "document {}",
                        transaction::helpers::extractKeyFromDocument(doc))};
      }
      output.push_back(d.getNumericValue<double>());
    }

    return {};
  } catch (velocypack::Exception const& e) {
    return {TRI_ERROR_TYPE_ERROR,
            std::format("deserialization error when accessing a document: {}",
                        e.what())};
  }
}

TrainedData serializeIndex(faiss::IndexIVF const& index) {
  faiss::VectorIOWriter writer;
  faiss::write_index(&index, &writer);
  TrainedData td;
  td.codeData = std::move(writer.data);
  return td;
}

std::shared_ptr<faiss::IndexIVF> VectorIndexTrainer::restoreFromTrainedData(
    TrainedData const& data) {
  faiss::VectorIOReader reader;
  // TODO prevent this copy, but instead implement own IOReader, reading
  // directly from the training data.
  reader.data = data.codeData;
  auto faissIndex = std::shared_ptr<faiss::IndexIVF>{
      dynamic_cast<faiss::IndexIVF*>(faiss::read_index(&reader))};
  ADB_PROD_ASSERT(faissIndex != nullptr);
  return faissIndex;
}

BoundedDocumentIterator::BoundedDocumentIterator(RocksDBKeyBounds bounds,
                                                 rocksdb::DB* db,
                                                 rocksdb::Snapshot const* snap)
    : bounds(std::move(bounds)), upper(this->bounds.end()), ro(false, false) {
  ro.prefix_same_as_start = true;
  ro.iterate_upper_bound = &upper;
  ro.snapshot = snap;
  auto* docCF = RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::Documents);
  it.reset(db->NewIterator(ro, docCF));
  it->Seek(this->bounds.start());
}

VectorIndexTrainer::VectorIndexTrainer(RocksDBVectorIndex const& index,
                                       ResourceMonitor& resourceMonitor,
                                       rocksdb::DB* db, RocksDBKeyBounds bounds)
    : _index(index),
      _resourceMonitor(resourceMonitor),
      _docIt(std::move(bounds), db) {}

std::shared_ptr<faiss::IndexIVF> VectorIndexTrainer::createFaissIndex(
    std::size_t resolvedNLists) const {
  auto const& def = _index.getDefinition();
  if (def.factory) {
    auto const factoryString = std::invoke([&]() -> std::string {
      if (isFactoryAStringScaling(*def.factory)) {
        return resolveFactoryString(*def.factory, resolvedNLists);
      }
      return *def.factory;
    });
    std::shared_ptr<faiss::Index> index(faiss::index_factory(
        def.dimension, factoryString.c_str(), metricToFaissMetric(def.metric)));

    auto ivfIndex = std::dynamic_pointer_cast<faiss::IndexIVF>(index);
    if (ivfIndex == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "Index definition not supported. Expected IVF index.");
    }

    if (resolvedNLists != ivfIndex->nlist) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          std::format(
              "The nLists parameter ({}) has to agree with the actual nlists "
              "implied by the factory string '{}' (which is {})",
              resolvedNLists, factoryString, ivfIndex->nlist));
    }

    return ivfIndex;
  } else {
    auto quantizer = std::invoke([&]() -> std::unique_ptr<faiss::Index> {
      switch (def.metric) {
        case SimilarityMetric::kL2:
          return std::make_unique<faiss::IndexFlatL2>(def.dimension);
        case SimilarityMetric::kCosine:
          return std::make_unique<faiss::IndexFlatIP>(def.dimension);
        case SimilarityMetric::kInnerProduct:
          return std::make_unique<faiss::IndexFlatIP>(def.dimension);
      }
    });

    std::shared_ptr<faiss::IndexIVF> ivfIndex =
        std::make_unique<faiss::IndexIVFFlat>(quantizer.get(), def.dimension,
                                              resolvedNLists,
                                              metricToFaissMetric(def.metric));
    ivfIndex->own_fields = nullptr != quantizer.release();
    return ivfIndex;
  }
}

ResultT<VectorIndexTrainer::TrainingDataset>
VectorIndexTrainer::collectTrainingDataset(rocksdb::Iterator& it,
                                           rocksdb::Slice upper,
                                           std::uint64_t numDocsHint,
                                           std::stop_token stopToken) const {
  auto const& def = _index.getDefinition();
  bool const sparseScaling = _index.sparse() && isNListsScaling(def.nLists);

  // Empty collection: resolveNLists would reject this in scaling mode, but the
  // situation is "no vectors to train on", not a parameter error.
  if (numDocsHint == 0) {
    return Result{TRI_ERROR_QUERY_VECTOR_INDEX_NOT_READY,
                  "For the vector index to be created documents "
                  "must be present in the respective collection for the "
                  "training process."};
  }

  // Size the reservoir from numDocsHint. For sparse+scaling this is an upper
  // bound on the valid vector count; the post-iteration resize below shrinks
  // it to the true value once the valid-vector count is known.
  auto estimatedResolvedNLists = resolveNLists(numDocsHint);
  if (estimatedResolvedNLists.fail()) {
    return std::move(estimatedResolvedNLists).result();
  }
  std::size_t const reservoirCapacity = std::invoke([&] {
    auto const capacity =
        estimatedResolvedNLists.get() * def.numberOfDocsPerCentroid;
    if (_index.sparse()) {
      return capacity;
    }

    return std::min(capacity, numDocsHint);
  });

  auto const seed = RandomDevice::seed64();

  // Guard against uint64_t overflow when computing the reservoir size.
  // Wrap here would under-account memScope and let training allocate unbounded.
  TRI_ASSERT(def.dimension > 0);
  TRI_ASSERT(def.dimension <=
             std::numeric_limits<std::uint64_t>::max() / sizeof(float));
  TRI_ASSERT(reservoirCapacity <= std::numeric_limits<std::uint64_t>::max() /
                                      (def.dimension * sizeof(float)));
  std::uint64_t const expectedReservoirBytes =
      static_cast<std::uint64_t>(reservoirCapacity) * def.dimension *
      sizeof(float);
  ResourceUsageScope memScope(_resourceMonitor, expectedReservoirBytes);

  LOG_TOPIC("b161b", INFO, Logger::ENGINES) << std::format(
      "[shard={}, index={}] Collecting training data dimension {} for {} "
      "index with numDocsHint: {}, reservoir capacity: {} (~{} MiB), sampler "
      "seed: {}.",
      _index.collection().name(), _index.id().id(), def.dimension,
      _index.sparse() ? "sparse" : "non-sparse", numDocsHint, reservoirCapacity,
      expectedReservoirBytes / (1024 * 1024), seed);

  VectorIndexTrainingSampler sampler{def.dimension, reservoirCapacity, seed};
  std::vector<float> inputBuffer;
  inputBuffer.reserve(def.dimension);

  while (it.Valid()) {
    if (stopToken.stop_requested()) {
      return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                    "vector training aborted"};
    }
    TRI_ASSERT(it.key().compare(upper) < 0);
    auto doc = VPackSlice(reinterpret_cast<uint8_t const*>(it.value().data()));

    if (!sampler.wantsItem()) {
      // Sparse mode: rows without the vector field must not count toward
      // itemsSeen, preserving the Algorithm L invariant over valid vectors.
      if (_index.sparse() && !hasVectorField(doc, _index.fields())) {
        it.Next();
        continue;
      }
      sampler.skip();
      it.Next();
      continue;
    }

    inputBuffer.clear();
    if (auto const res = readDocumentVectorData(doc, _index.fields(),
                                                def.dimension, inputBuffer);
        res.fail()) {
      if (res.is(TRI_ERROR_BAD_PARAMETER) && _index.sparse()) {
        it.Next();
        continue;
      }
      return Result{
          res.errorNumber(),
          std::format(
              "failed to read document vector data, "
              "embeddings are in a wrong format and index is not sparse: {}",
              res.errorMessage())};
    }
    sampler.consume(inputBuffer);
    it.Next();
  }

  std::size_t const validSeen = sampler.itemsSeen();

  if (sparseScaling && validSeen > 0) {
    if (auto res = shrinkReservoirForSparseScaling(validSeen, reservoirCapacity,
                                                   expectedReservoirBytes,
                                                   memScope, sampler);
        res.fail()) {
      return res;
    }
  }

  auto trainingData = std::move(sampler).release();
  std::size_t const finalCount = trainingData.size() / def.dimension;
  if (def.metric == SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(def.dimension, finalCount, trainingData.data());
  }

  if (trainingData.empty()) {
    // Reachable in sparse mode when no document carries the vector field.
    return Result{TRI_ERROR_QUERY_VECTOR_INDEX_NOT_READY,
                  "For the vector index to be created documents "
                  "must be present in the respective collection for the "
                  "training process."};
  }

  return ResultT<TrainingDataset>(
      {std::move(trainingData), validSeen, std::move(memScope)});
}

ResultT<std::size_t> VectorIndexTrainer::resolveNLists(
    std::uint64_t numDocsHint) const {
  auto const& nLists = _index.getDefinition().nLists;
  if (isNListsScaling(nLists) && numDocsHint == 0) {
    return Result{TRI_ERROR_BAD_PARAMETER,
                  "Cannot resolve nLists with scaling mode when "
                  "numDocsHint is 0"};
  }
  return resolveNListsParameter(nLists, numDocsHint);
}

// We resize after the full iteration because numDocsHint over-counts by the
// number of docs without the vector field. A uniform subsample of a uniform
// sample stays uniform, so this preserves the Algorithm L guarantee.
Result VectorIndexTrainer::shrinkReservoirForSparseScaling(
    std::size_t validSeen, std::size_t reservoirCapacity,
    std::uint64_t expectedReservoirBytes, ResourceUsageScope& memScope,
    VectorIndexTrainingSampler& sampler) const {
  auto resolvedNLists = resolveNLists(validSeen);
  if (resolvedNLists.fail()) {
    return std::move(resolvedNLists).result();
  }
  auto const& def = _index.getDefinition();
  auto const newCapacity = resolvedNLists.get() * def.numberOfDocsPerCentroid;
  sampler.resize(newCapacity);
  return {};
}

ResultT<VectorIndexTrainer::TrainingResult> VectorIndexTrainer::train(
    std::size_t numDocsHint, std::stop_token stopToken) {
  auto res =
      collectTrainingDataset(*_docIt.it, _docIt.upper, numDocsHint, stopToken);
  if (res.fail()) {
    return std::move(res).result();
  }
  auto const& trainingData = res.get();
  // When sparse we take how many document there are, for that we did full
  // iteration otherwise we take the hint provided by the collection
  auto const numberOfVectors = std::invoke([&]() {
    if (_index.sparse() && isNListsScaling(_index.getDefinition().nLists)) {
      return trainingData.totalValidVectorCount;
    }

    return numDocsHint;
  });
  if (numberOfVectors == 0) {
    return Result{TRI_ERROR_QUERY_VECTOR_INDEX_NOT_READY,
                  std::format("Vector index requires at least {} "
                              "vectors for training, "
                              "but none were found.",
                              _index.trainingThreshold())};
  }
  auto resolvedNLists = resolveNLists(numberOfVectors);
  if (resolvedNLists.fail()) {
    return std::move(resolvedNLists).result();
  }

  auto const& def = _index.getDefinition();
  auto faissIndex = createFaissIndex(resolvedNLists.get());
  faissIndex->nprobe = def.defaultNProbe;
  auto const numOfTrainingVectors = trainingData.data.size() / def.dimension;

  if (numOfTrainingVectors < _index.trainingThreshold()) {
    return Result{
        TRI_ERROR_QUERY_VECTOR_INDEX_NOT_READY,
        std::format("Vector index requires at least {} "
                    "vectors for training, "
                    "but only {} were found.",
                    _index.trainingThreshold(), numOfTrainingVectors)};
  }

  LOG_TOPIC("a162b", INFO, Logger::ENGINES) << std::format(
      "[shard={}, index={}] Loaded {} vectors. Start training "
      "process on {} centroids.",
      _index.collection().name(), _index.id().id(), numOfTrainingVectors,
      resolvedNLists.get());
  faissIndex->train(numOfTrainingVectors, trainingData.data.data());
  LOG_TOPIC("a160b", INFO, Logger::ENGINES)
      << std::format("[shard={}, index={}] Finished training.",
                     _index.collection().name(), _index.id().id());

  // Reservoir is uniform (Algorithm L), so a prefix is too.
  static constexpr std::size_t kAutoTuneSampleCap = 1024;
  auto const sampleCount =
      std::min<std::size_t>(kAutoTuneSampleCap, numOfTrainingVectors);
  std::vector<float> autoTuneSample(
      trainingData.data.begin(),
      trainingData.data.begin() + sampleCount * def.dimension);

  return TrainingResult{std::move(faissIndex), std::move(autoTuneSample)};
}

Result ingestVectors(RocksDBVectorIndex& index, rocksdb::DB* rootDB,
                     std::unique_ptr<rocksdb::Iterator> documentIterator,
                     std::stop_token stopToken) {
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  while (TRI_ShouldFailDebugging("RocksDBVectorIndex::pauseDuringIngestion")) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
#endif

  auto const& definition = index.getDefinition();
  auto const dim = definition.dimension;
  auto const& fields = index.fields();
  auto const hasStored = index.hasStoredValues();
  auto const& stored = index.storedValues();
  auto const faissIndex = index.faissIndex();
  auto* cf = index.columnFamily();
  auto const oid = index.objectId();
  auto const formatVersion = index.formatVersion();

  auto const shardName = index.collection().name();
  auto const indexId = index.id().id();

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
    std::atomic<uint64_t> readProduceBlocked{0};
    std::atomic<uint64_t> encodeProduceBlocked{0};
    std::atomic<uint64_t> encodeConsumeBlocked{0};
    std::atomic<uint64_t> writeConsumeBlocked{0};
  } counters;

  BoundedChannel<DocumentVectors> documentChannel{5};
  BoundedChannel<EncodedVectors> encodedChannel{5};

  constexpr auto numReaders = 1;
  constexpr auto numEncoders = 8;
  constexpr auto numWriters = 2;

  constexpr auto documentPerBatch = 8000;

  std::atomic<std::size_t> countBatches{0};
  std::atomic<std::size_t> countDocuments{0};

  std::atomic<bool> hasError{false};
  Result firstError;

  std::stop_callback stopCb(stopToken, [&] {
    if (hasError.exchange(true) == false) {
      firstError =
          Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, "ingestion aborted"};
    }
    documentChannel.stop();
    encodedChannel.stop();
  });

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
        if (auto const res =
                readDocumentVectorData(doc, fields, dim, batch->vectors);
            res.fail()) {
          if (res.is(TRI_ERROR_BAD_PARAMETER) && index.sparse()) {
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
          if (definition.metric == SimilarityMetric::kCosine) {
            faiss::fvec_renorm_L2(dim, batch->docIds.size(),
                                  batch->vectors.data());
          }
          auto [shouldStop, blocked] = documentChannel.push(std::move(batch));
          counters.readProduceBlocked.fetch_add(blocked,
                                                std::memory_order_relaxed);

          if (shouldStop) {
            return;
          }
          batch = prepareBatch();
        }
      }

      if (batch && !batch->docIds.empty()) {
        if (definition.metric == SimilarityMetric::kCosine) {
          faiss::fvec_renorm_L2(dim, batch->docIds.size(),
                                batch->vectors.data());
        }
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
        counters.encodeConsumeBlocked.fetch_add(blocked,
                                                std::memory_order_relaxed);
        auto n = item->docIds.size();
        countBatches += 1;
        countDocuments += n;

        float* x = item->vectors.data();
        std::unique_ptr<faiss::idx_t[]> coarse_idx(new faiss::idx_t[n]);
        faissIndex->quantizer->assign(n, x, coarse_idx.get());
        auto code_size = faissIndex->code_size;
        std::unique_ptr<uint8_t[]> flat_codes(new uint8_t[n * code_size]);

        // TODO: since we only use IVTFlat this is just copying the data.
        //  Probably we want to use some PQ encoding later on.
        faissIndex->encode_vectors(n, x, coarse_idx.get(), flat_codes.get());

        auto encoded = std::make_unique<EncodedVectors>();
        encoded->docIds = std::move(item->docIds);
        encoded->lists = std::move(coarse_idx);
        encoded->codes = std::move(flat_codes);
        encoded->storedValues = std::move(item->storedValues);

        LOG_TOPIC("e168b", INFO, Logger::ENGINES)
            << "[shard=" << shardName << ", index=" << indexId << "] "
            << "ENCODE encoded " << encoded->docIds.size()
            << " vectors, code size: " << code_size;
        bool pushBlocked = false;
        std::tie(shouldStop, pushBlocked) =
            encodedChannel.push(std::move(encoded));
        counters.encodeProduceBlocked.fetch_add(pushBlocked,
                                                std::memory_order_relaxed);
      });
      if (shouldStop) {
        break;
      }
    }
  };

  using MakeValueFn =
      RocksDBValue (*)(EncodedVectors&, size_t k, size_t codeSize);
  auto const makeValue = std::invoke([&]() -> MakeValueFn {
    if (!hasStored) {
      return [](EncodedVectors& item, size_t k, size_t codeSize) {
        auto* ptr = item.codes.get() + k * codeSize;
        return RocksDBValue::VectorIndexValue(ptr, codeSize);
      };
    } else if (formatVersion == VectorIndexFormatVersion::kV2) {
      return [](EncodedVectors& item, size_t k, size_t codeSize) {
        auto* ptr = item.codes.get() + k * codeSize;
        return RocksDBValue::VectorIndexValueV2(ptr, codeSize,
                                                item.storedValues[k]);
      };
    } else {
      return [](EncodedVectors& item, size_t k, size_t codeSize) {
        auto* ptr = item.codes.get() + k * codeSize;
        RocksDBVectorIndexEntryValue v;
        v.encodedValue = std::vector<uint8_t>(ptr, ptr + codeSize);
        v.storedValues = std::move(item.storedValues[k]);
        return RocksDBValue::VectorIndexValueV1(v);
      };
    }
  });

  auto writeDocuments = [&, codeSize = faissIndex->code_size] {
    rocksdb::WriteBatch batch;
    while (true) {
      auto [item, blocked] = encodedChannel.pop();
      if (item == nullptr) {
        break;
      }

      errorExceptionHandler([&] {
        counters.writeConsumeBlocked.fetch_add(blocked,
                                               std::memory_order_relaxed);
        batch.Clear();

        RocksDBKey key;
        rocksdb::Status status;

        for (size_t k = 0; k < item->docIds.size(); k++) {
          key.constructVectorIndexValue(oid, item->lists[k], item->docIds[k]);

          auto const value = makeValue(*item, k, codeSize);

          status = batch.Put(cf, key.string(), value.string());
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

  LOG_TOPIC("71c45", INFO, Logger::ENGINES)
      << "[shard=" << shardName << ", index=" << indexId << "] "
      << "Ingesting vectors into index on a fast path. Threads: num-readers="
      << numReaders << " num-encoders=" << numEncoders
      << " numWriters=" << numWriters;

  startNThreads(readDocuments, numReaders);
  startNThreads(encodeVectors, numEncoders);
  startNThreads(writeDocuments, numWriters);

  threads.clear();

  if (firstError.ok()) {
    LOG_TOPIC("41658", INFO, Logger::ENGINES)
        << "[shard=" << shardName << ", index=" << indexId << "] "
        << "Ingestion done. Encoded " << countDocuments << " vectors in "
        << countBatches
        << " batches. Pipeline skew: " << counters.readProduceBlocked << " "
        << counters.encodeConsumeBlocked << " " << counters.encodeProduceBlocked
        << " " << counters.writeConsumeBlocked;
  } else {
    LOG_TOPIC("96a80", ERR, Logger::ENGINES)
        << "[shard=" << shardName << ", index=" << indexId << "] "
        << "Ingestion failed: " << firstError;
  }

  return firstError;
}

VectorIndexBuilder::VectorIndexBuilder(RocksDBVectorIndex& index,
                                       ResourceMonitor& resourceMonitor)
    : _index(index),
      _resourceMonitor(resourceMonitor),
      _engine(index.collection().vocbase().engine<RocksDBEngine>()),
      _rootDB(_engine.db()->GetRootDB()),
      _rcoll(static_cast<RocksDBCollection*>(index.collection().getPhysical())),
      _bounds(_rcoll->bounds()) {}

Result VectorIndexBuilder::persistVectorIndexMetadata(
    VectorIndexMetadata const& metadata) {
  velocypack::Builder builder;
  velocypack::serialize(builder, metadata);

  // V1 metadata stays at the legacy slot so older binaries can still read it
  // after a downgrade. V2 metadata is parked in a separate slot; an old
  // binary looking at the V1 slot won't find it and will treat the index as
  // unusable
  RocksDBKey key;
  key.constructVectorIndexTrainedData(_index.objectId(),
                                      metadata.formatVersion);
  auto value = RocksDBValue::VectorIndexValue(builder.slice());

  auto* vectorCF = RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::VectorIndex);
  rocksdb::WriteOptions wo;
  auto status = _rootDB->Put(wo, vectorCF, key.string(), value.string());
  if (!status.ok()) {
    return Result{TRI_ERROR_INTERNAL,
                  std::string{"Failed to persist vector index metadata: "} +
                      status.ToString()};
  }
  return {};
}

void VectorIndexBuilder::runAutoTune(std::span<float const> autoTuneSample) {
  if (autoTuneSample.empty()) {
    return;
  }
  SingleCollectionTransaction trx(
      transaction::StandaloneContext::create(
          _index.collection().vocbase(),
          transaction::OperationOriginInternal{"vector index autotune"}),
      _index.collection(), AccessMode::Type::READ);
  if (auto trxRes = trx.begin(); trxRes.fail()) {
    LOG_VECTOR_BUILD("e16b0", WARN, Logger::ENGINES)
        << "Skipping autotune: " << trxRes.errorMessage();
    return;
  }
  RocksDBFaissIteratorContext faissCtx{
      IteratorContext{.trx = static_cast<transaction::Methods*>(&trx)}};
  auto tuned = autoTuneNProbe(*_index.faissIndex(), autoTuneSample, &faissCtx);
  if (tuned.fail()) {
    return;  // autoTuneNProbe already logged
  }
  auto const& td = _index.applyTunedNProbe(tuned.get());
  VectorIndexMetadata metadata{.trainedData = td,
                               .formatVersion = _index.formatVersion()};
  if (auto persistRes = persistVectorIndexMetadata(metadata);
      persistRes.fail()) {
    LOG_VECTOR_BUILD("e16ac", WARN, Logger::ENGINES)
        << "Failed to persist tuned nprobe: " << persistRes.errorMessage();
  }
}

Result VectorIndexBuilder::build(
    std::shared_ptr<RocksDBIndex> indexPtr,
    metrics::Histogram<metrics::LogScale<double>>& trainingDuration,
    metrics::Histogram<metrics::LogScale<double>>& ingestionDuration,
    std::stop_token stopToken) {
  auto const shouldAbort = [&]() -> bool {
    return stopToken.stop_requested() || _index.collection().deleted();
  };

  if (!_index.setTrainingState(VectorIndexTrainingState::kUnusable,
                               VectorIndexTrainingState::kTraining)) {
    return Result{TRI_ERROR_INTERNAL, "vector index is not in unusable state"};
  }

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  while (TRI_ShouldFailDebugging("RocksDBVectorIndex::pauseBeforeTraining")) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (shouldAbort()) {
      break;
    }
  }
#endif

  // TRAINING PHASE
  auto const numDocsHint = _rcoll->meta().numberDocuments();
  VectorIndexTrainer trainer(_index, _resourceMonitor, _rootDB, _bounds);

  auto const trainStart = std::chrono::steady_clock::now();
  if (shouldAbort()) {
    _index.resetTrainingState();
    return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }
  auto trainingResult = trainer.train(numDocsHint, stopToken);
  auto const trainEnd = std::chrono::steady_clock::now();
  if (trainingResult.fail()) {
    _index.resetTrainingState();
    return std::move(trainingResult).result();
  }

  auto faissIndex = std::move(trainingResult.get().index);
  auto autoTuneSample = std::move(trainingResult.get().autoTuneSample);
  trainingDuration.count(
      std::chrono::duration<double>(trainEnd - trainStart).count());

  if (shouldAbort()) {
    _index.resetTrainingState();
    return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }

  LOG_VECTOR_BUILD("e163b", INFO, Logger::ENGINES)
      << "Training complete. Ingesting vectors.";

  auto trainedData = serializeIndex(*faissIndex);

  VectorIndexMetadata metadata{.trainedData = trainedData,
                               .formatVersion = _index.formatVersion()};
  if (auto res = persistVectorIndexMetadata(metadata); res.fail()) {
    _index.resetTrainingState();
    return res;
  }

  _index.applyTrainingResult(std::move(faissIndex), std::move(trainedData));
  _index.setTrainingState(VectorIndexTrainingState::kTraining,
                          VectorIndexTrainingState::kIngesting);

  TRI_IF_FAILURE("RocksDBVectorIndex::buildWrongDimension") {
    _index.resetTrainingState();
    return Result{TRI_ERROR_DEBUG};
  }

  // INGESTION PHASE
  auto const ingestStart = std::chrono::steady_clock::now();

  // Create RocksDBBuilderIndex wrapper
  auto buildIdx = std::make_shared<RocksDBBuilderIndex>(
      indexPtr, numDocsHint, IndexFactory::kMaxParallelism);

  _rcoll->swapIndex(indexPtr, buildIdx);
  auto swapGuard = ScopeGuard([&]() noexcept {
    // On any exit, ensure the real index is back in _indexes.
    _rcoll->swapIndex(buildIdx, indexPtr);
  });

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  while (TRI_ShouldFailDebugging("RocksDBVectorIndex::pauseBeforeIngestion")) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (shouldAbort()) {
      break;
    }
  }
#endif

  RocksDBBuilderIndex::Locker locker(_rcoll);
  if (!locker.lock().waitAndGet()) {
    _index.resetTrainingState();
    return Result{TRI_ERROR_LOCK_TIMEOUT,
                  "failed to acquire lock for vector index build"};
  }

  // This solves all our problems with catching up with WAL entries
  auto res = buildIdx->fillIndexBackground(locker).waitAndGet();
  auto const ingestEnd = std::chrono::steady_clock::now();

  if (res.fail()) {
    LOG_VECTOR_BUILD("e166b", ERR, Logger::ENGINES)
        << "Vector index fill + WAL catch-up failed: " << res.errorMessage();
    _index.resetTrainingState();
    return res;
  }

  ingestionDuration.count(
      std::chrono::duration<double>(ingestEnd - ingestStart).count());

  // fillIndexBackground returns with the exclusive lock still held.
  // Swap the real index back and transition to kReady before releasing.
  swapGuard.fire();

  // TODO(jbajic) maybe not while we hold the lock!!!
  runAutoTune(autoTuneSample);

  _index.setTrainingState(VectorIndexTrainingState::kIngesting,
                          VectorIndexTrainingState::kReady);

  LOG_VECTOR_BUILD("e169c", INFO, Logger::ENGINES)
      << "Vector index build completed (fill + WAL catch-up).";

  return res;
}

}  // namespace arangodb::vector
