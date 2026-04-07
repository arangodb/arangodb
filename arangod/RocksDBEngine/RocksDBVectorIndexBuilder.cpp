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

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <string>
#include <thread>

#include "Assertions/Assert.h"
#include "Basics/debugging.h"
#include "Basics/BoundedChannel.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Indexes/Index.h"
#include "Logger/LogMacros.h"
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

// Minimum vectors to collect unconditionally before applying the adaptive cap
// in the sparse+scaling path.
constexpr std::size_t kSparseScalingBootstrapVectors{1000};

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

VectorIndexTrainer::VectorIndexTrainer(
    UserVectorIndexDefinition const& definition, bool isSparse,
    std::vector<std::vector<basics::AttributeName>> const& fields,
    std::string_view shardName, std::uint64_t indexId,
    std::int64_t trainingThreshold, rocksdb::DB* db, RocksDBKeyBounds bounds)
    : _definition(definition),
      _isSparse(isSparse),
      _fields(fields),
      _shardName(shardName),
      _indexId(indexId),
      _trainingThreshold(trainingThreshold),
      _bounds(std::move(bounds)),
      _upper(_bounds.end()),
      _ro(false, false) {
  _ro.prefix_same_as_start = true;
  _ro.iterate_upper_bound = &_upper;
  auto* docCF = RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::Documents);
  _it.reset(db->NewIterator(_ro, docCF));
  _it->Seek(_bounds.start());
}

std::shared_ptr<faiss::IndexIVF> VectorIndexTrainer::createFaissIndex(
    std::int64_t resolvedNLists) const {
  if (_definition.factory) {
    auto const factoryString = _definition.factory->c_str();
    std::shared_ptr<faiss::Index> index(
        faiss::index_factory(_definition.dimension, factoryString,
                             metricToFaissMetric(_definition.metric)));

    auto ivfIndex = std::dynamic_pointer_cast<faiss::IndexIVF>(index);
    if (ivfIndex == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "Index definition not supported. Expected IVF index.");
    }
    if (std::size_t(resolvedNLists) != ivfIndex->nlist) {
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
      switch (_definition.metric) {
        case SimilarityMetric::kL2:
          return std::make_unique<faiss::IndexFlatL2>(_definition.dimension);
        case SimilarityMetric::kCosine:
          return std::make_unique<faiss::IndexFlatIP>(_definition.dimension);
        case SimilarityMetric::kInnerProduct:
          return std::make_unique<faiss::IndexFlatIP>(_definition.dimension);
      }
    });

    std::shared_ptr<faiss::IndexIVF> ivfIndex =
        std::make_unique<faiss::IndexIVFFlat>(
            quantizer.get(), _definition.dimension, resolvedNLists,
            metricToFaissMetric(_definition.metric));
    ivfIndex->own_fields = nullptr != quantizer.release();
    return ivfIndex;
  }
}

ResultT<VectorIndexTrainer::TrainingDataset>
VectorIndexTrainer::collectTrainingDataset(rocksdb::Iterator& it,
                                           rocksdb::Slice upper,
                                           std::uint64_t numDocsHint,
                                           std::stop_token stopToken) const {
  std::vector<float> trainingData;
  std::vector<float> input;
  input.reserve(_definition.dimension);
  std::optional<std::size_t> maxVectors;

  LOG_TOPIC("b161b", INFO, Logger::ENGINES) << std::format(
      "[shard={}, index={}] Collecting training data "
      "dimension {} for training for {} index with the numDocsHint: {}.",
      _shardName, _indexId, _definition.dimension,
      _isSparse ? "sparse" : "non-sparse", numDocsHint);

  // We need to do a full iteration to properly resolve the number of nLists
  bool const shouldDoFullIteration =
      _isSparse && isNListsScaling(_definition.nLists);
  if (!shouldDoFullIteration) {
    maxVectors = resolveNLists(numDocsHint) * kMaxTrainingSizePerNLists;
  }
  // only one of these can be true
  TRI_ASSERT(shouldDoFullIteration ^ maxVectors.has_value());

  std::size_t trainingDatasetCounter{0};
  std::size_t iterationCounter{0};
  while (it.Valid() &&
         (shouldDoFullIteration || trainingDatasetCounter < *maxVectors)) {
    if (iterationCounter % 1000 == 0 && stopToken.stop_requested()) {
      return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                    "vector training aborted"};
    }
    TRI_ASSERT(it.key().compare(upper) < 0);
    auto doc = VPackSlice(reinterpret_cast<uint8_t const*>(it.value().data()));
    if (auto const res =
            readDocumentVectorData(doc, _fields, _definition.dimension, input);
        res.fail()) {
      if (res.is(TRI_ERROR_BAD_PARAMETER) && _isSparse) {
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

    if (!shouldDoFullIteration) {
      trainingData.insert(trainingData.end(), input.begin(), input.end());
      ++trainingDatasetCounter;
    } else {
      // We collect only in case when we are below minimum 1000 docs or when we
      // have do not have enough docs for the currently projected nLists value
      bool const shouldCollect =
          trainingDatasetCounter < kSparseScalingBootstrapVectors ||
          trainingDatasetCounter <
              static_cast<std::size_t>(resolveNLists(trainingDatasetCounter) *
                                       kMaxTrainingSizePerNLists);
      if (shouldCollect) {
        trainingData.insert(trainingData.end(), input.begin(), input.end());
        ++trainingDatasetCounter;
      }
    }
    input.clear();

    it.Next();
    ++iterationCounter;
  }

  if (_definition.metric == SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(_definition.dimension, trainingDatasetCounter,
                          trainingData.data());
  }

  if (trainingData.empty()) {
    return Result{TRI_ERROR_NOT_IMPLEMENTED,
                  "For the vector index to be created documents "
                  "must be present in the respective collection for the "
                  "training process."};
  }

  return ResultT<TrainingDataset>({std::move(trainingData), iterationCounter});
}

std::int64_t VectorIndexTrainer::resolveNLists(
    std::uint64_t numDocsHint) const {
  if (isNListsScaling(_definition.nLists) && numDocsHint == 0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_NOT_IMPLEMENTED,
        "For the vector index to be created documents "
        "must be present in the respective collection for the training "
        "process.");
  }
  return resolveNListsParameter(_definition.nLists, numDocsHint);
}

ResultT<std::shared_ptr<faiss::IndexIVF>> VectorIndexTrainer::train(
    std::size_t numDocsHint, std::stop_token stopToken) {
  auto res = collectTrainingDataset(*_it, _upper, numDocsHint, stopToken);
  if (res.fail()) {
    return std::move(res).result();
  }
  auto const& trainingData = res.get();
  // When sparse we take how many document there are, for that we did full
  // iteration otherwise we take the hint provided by the collection
  auto const numberOfVectors = std::invoke([&]() {
    if (_isSparse && isNListsScaling(_definition.nLists)) {
      return trainingData.totalValidVectorCount;
    }

    return numDocsHint;
  });
  auto const resolvedNLists = resolveNLists(numberOfVectors);

  auto faissIndex = createFaissIndex(resolvedNLists);
  faissIndex->nprobe = _definition.defaultNProbe;
  auto const numOfTrainingVectors = static_cast<std::int64_t>(
      trainingData.data.size() / _definition.dimension);

  if (numOfTrainingVectors < _trainingThreshold) {
    return Result{TRI_ERROR_NOT_IMPLEMENTED,
                  std::format("Vector index requires at least {} "
                              "vectors for training, "
                              "but only {} were found.",
                              _trainingThreshold, numOfTrainingVectors)};
  }

  LOG_TOPIC("a162b", INFO, Logger::ENGINES) << std::format(
      "[shard={}, index={}] Loaded {} vectors. Start training "
      "process on {} centroids.",
      _shardName, _indexId, numOfTrainingVectors, resolvedNLists);
  faissIndex->train(numOfTrainingVectors, trainingData.data.data());
  LOG_TOPIC("a160b", INFO, Logger::ENGINES) << std::format(
      "[shard={}, index={}] Finished training.", _shardName, _indexId);

  return faissIndex;
}

Result ingestVectors(RocksDBVectorIndex& index, rocksdb::DB* rootDB,
                     std::unique_ptr<rocksdb::Iterator> documentIterator,
                     std::stop_token stopToken) {
  auto const& definition = index.getDefinition();
  auto const dim = definition.dimension;
  auto const& fields = index.fields();
  auto const hasStored = index.hasStoredValues();
  auto const& stored = index.storedValues();
  auto const faissIndex = index.faissIndex();
  auto* cf = index.columnFamily();
  auto const oid = index.objectId();

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
          counters.readProduceBlocked += blocked;

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
        counters.encodeConsumeBlocked += blocked;
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
          key.constructVectorIndexValue(oid, item->lists[k], item->docIds[k]);

          auto const value = std::invoke([&]() {
            auto* ptr = item->codes.get() + k * faissIndex->code_size;
            if (hasStored) {
              RocksDBVectorIndexEntryValue rocksdbEntryValue;
              rocksdbEntryValue.encodedValue =
                  std::vector<uint8_t>(ptr, ptr + faissIndex->code_size);
              rocksdbEntryValue.storedValues = std::move(item->storedValues[k]);

              return RocksDBValue::VectorIndexValue(rocksdbEntryValue);
            } else {
              return RocksDBValue::VectorIndexValue(ptr, faissIndex->code_size);
            }
          });

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

VectorIndexBuilder::VectorIndexBuilder(RocksDBVectorIndex& index)
    : _index(index),
      _engine(index.collection().vocbase().engine<RocksDBEngine>()),
      _rootDB(_engine.db()->GetRootDB()),
      _rcoll(static_cast<RocksDBCollection*>(index.collection().getPhysical())),
      _bounds(_rcoll->bounds()) {}

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

  auto numDocsHint = _rcoll->meta().numberDocuments();
  VectorIndexTrainer trainer(_index.getDefinition(), _index.sparse(),
                             _index.fields(), _index.collection().name(),
                             _index.id().id(), _index.trainingThreshold(),
                             _rootDB, _bounds);

  auto const trainStart = std::chrono::steady_clock::now();
  if (shouldAbort()) {
    _index.resetTrainingState();
    return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }
  auto faissIndexResult = trainer.train(numDocsHint, stopToken);
  auto const trainEnd = std::chrono::steady_clock::now();
  if (faissIndexResult.fail()) {
    _index.resetTrainingState();
    return std::move(faissIndexResult).result();
  }

  auto faissIndex = std::move(faissIndexResult).get();
  trainingDuration.count(
      std::chrono::duration<double>(trainEnd - trainStart).count());

  if (shouldAbort()) {
    _index.resetTrainingState();
    return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }

  LOG_TOPIC("e163b", INFO, Logger::ENGINES)
      << "[shard=" << _index.collection().name()
      << ", index=" << _index.id().id() << "] "
      << "Training complete. Ingesting vectors.";

  auto trainedData = serializeIndex(*faissIndex);

  // Persist trained data to the vector CF so it survives restarts.
  {
    velocypack::Builder builder;
    velocypack::serialize(builder, trainedData);

    RocksDBKey key;
    key.constructVectorIndexTrainedData(_index.objectId());
    auto value = RocksDBValue::VectorIndexValue(builder.slice());

    auto* vectorCF = RocksDBColumnFamilyManager::get(
        RocksDBColumnFamilyManager::Family::VectorIndex);
    rocksdb::WriteOptions wo;
    auto status = _rootDB->Put(wo, vectorCF, key.string(), value.string());
    if (!status.ok()) {
      _index.resetTrainingState();
      return Result{
          TRI_ERROR_INTERNAL,
          std::string{"Failed to persist trained data: "} + status.ToString()};
    }
  }

  _index.applyTrainingResult(std::move(faissIndex), std::move(trainedData));
  _index.setTrainingState(VectorIndexTrainingState::kTraining,
                          VectorIndexTrainingState::kIngesting);

  auto const ingestStart = std::chrono::steady_clock::now();

  rocksdb::Slice upper(_bounds.end());
  rocksdb::ReadOptions ro(false, false);
  ro.prefix_same_as_start = true;
  ro.iterate_upper_bound = &upper;
  auto* docCF = RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::Documents);
  std::unique_ptr<rocksdb::Iterator> ingestIt(_rootDB->NewIterator(ro, docCF));
  ingestIt->Seek(_bounds.start());

  TRI_IF_FAILURE("RocksDBVectorIndex::buildWrongDimension") {
    _index.resetTrainingState();
    return Result{TRI_ERROR_DEBUG};
  }

  Result res = ingestVectors(_index, _rootDB, std::move(ingestIt), stopToken);
  auto const ingestEnd = std::chrono::steady_clock::now();

  if (res.fail()) {
    LOG_TOPIC("e166b", ERR, Logger::ENGINES)
        << "[shard=" << _index.collection().name()
        << ", index=" << _index.id().id() << "] "
        << "Vector ingestion failed: " << res.errorMessage();
    _index.resetTrainingState();
  } else {
    ingestionDuration.count(
        std::chrono::duration<double>(ingestEnd - ingestStart).count());
    LOG_TOPIC("e165b", INFO, Logger::ENGINES)
        << "[shard=" << _index.collection().name()
        << ", index=" << _index.id().id() << "] "
        << "Ingestion completed.";
    _index.setTrainingState(VectorIndexTrainingState::kIngesting,
                            VectorIndexTrainingState::kReady);
  }

  return res;
}

}  // namespace arangodb::vector
