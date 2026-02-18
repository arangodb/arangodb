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
#include "RocksDBEngine/RocksDBBuilderIndex.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <thread>

#include "Assertions/Assert.h"
#include "Basics/BoundedChannel.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "RocksDBEngine/RocksDBVectorIndexList.h"
#include "Transaction/Helpers.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
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
                          transaction::helpers::extractKeyFromDocument(doc)
                              .copyString()
                              .c_str())};
    }

    if (!value.isArray()) {
      return {TRI_ERROR_TYPE_ERROR,
              std::format("array expected for vector attribute for document {}",
                          transaction::helpers::extractKeyFromDocument(doc)
                              .copyString()
                              .c_str())};
    }

    if (value.length() != dimension) {
      return {TRI_ERROR_TYPE_ERROR,
              std::format(
                  "provided vector is not of matching dimension for document "
                  "{}, index dimension: {}, document dimension: {}",
                  transaction::helpers::extractKeyFromDocument(doc)
                      .copyString()
                      .c_str(),
                  dimension, value.length())};
    }

    // We don't make assumptions here if output contains one or more vectors
    for (auto const d : VPackArrayIterator(value)) {
      if (not d.isNumber<double>()) {
        return {
            TRI_ERROR_TYPE_ERROR,
            std::format("vector contains data not representable as double for "
                        "document {}",
                        transaction::helpers::extractKeyFromDocument(doc)
                            .copyString()
                            .c_str())};
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

TrainingResult VectorIndexTrainer::restoreFromTrainedData(
    TrainedData const& data) {
  faiss::VectorIOReader reader;
  // TODO prevent this copy, but instead implement own IOReader, reading
  // directly from the training data.
  reader.data = data.codeData;
  auto faissIndex = std::unique_ptr<faiss::IndexIVF>{
      dynamic_cast<faiss::IndexIVF*>(faiss::read_index(&reader))};
  ADB_PROD_ASSERT(faissIndex != nullptr);

  TrainingResult result;
  result.faissIndex = std::move(faissIndex);
  // trainedData is not set — caller already has it.
  return result;
}

VectorIndexTrainer::VectorIndexTrainer(
    UserVectorIndexDefinition const& definition, bool isSparse,
    std::vector<std::vector<basics::AttributeName>> const& fields,
    std::string_view shardName, std::uint64_t indexId)
    : _definition(definition),
      _isSparse(isSparse),
      _fields(fields),
      _shardName(shardName),
      _indexId(indexId) {}

std::shared_ptr<faiss::IndexIVF> VectorIndexTrainer::createFaissIndex() const {
  if (_definition.factory) {
    std::shared_ptr<faiss::Index> index(
        faiss::index_factory(_definition.dimension, _definition.factory->c_str(),
                             metricToFaissMetric(_definition.metric)));

    auto ivfIndex = std::dynamic_pointer_cast<faiss::IndexIVF>(index);
    if (ivfIndex == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "Index definition not supported. Expected IVF index.");
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
            quantizer.get(), _definition.dimension, _definition.nLists,
            metricToFaissMetric(_definition.metric));
    ivfIndex->own_fields = nullptr != quantizer.release();
    return ivfIndex;
  }
}

std::vector<float> VectorIndexTrainer::collectTrainingDataset(
    rocksdb::Iterator& it, rocksdb::Slice upper,
    std::int64_t maxVectors) const {
  std::vector<float> trainingData;
  std::vector<float> input;
  input.reserve(_definition.dimension);
  std::int64_t counter{0};

  LOG_TOPIC("b161b", INFO, Logger::STATISTICS)
      << "[shard=" << _shardName << ", index=" << _indexId << "] "
      << "Loading " << maxVectors << " vectors of dimension "
      << _definition.dimension << " for training.";

  while (counter < maxVectors && it.Valid()) {
    TRI_ASSERT(it.key().compare(upper) < 0);
    auto doc = VPackSlice(reinterpret_cast<uint8_t const*>(it.value().data()));
    if (auto const res =
            readDocumentVectorData(doc, _fields, _definition.dimension, input);
        res.fail()) {
      if (res.is(TRI_ERROR_BAD_PARAMETER) && _isSparse) {
        it.Next();
        continue;
      }
      THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(),
                                     "invalid index type definition");
    }

    trainingData.insert(trainingData.end(), input.begin(), input.end());
    input.clear();

    it.Next();
    ++counter;
  }

  if (_definition.metric == SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(_definition.dimension, counter, trainingData.data());
  }

  if (trainingData.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_NOT_IMPLEMENTED,
        "For the vector index to be created documents "
        "must be present in the respective collection for the training "
        "process.");
  }

  return trainingData;
}

TrainingResult VectorIndexTrainer::train(rocksdb::Iterator& it,
                                         rocksdb::Slice upper) const {
  // Create the FAISS index with the resolved nLists.
  auto faissIndex = createFaissIndex();

  // Collect training vectors and train the FAISS index.
  std::int64_t trainingDataSize =
      faissIndex->cp.max_points_per_centroid * _definition.nLists;
  auto const trainingData = collectTrainingDataset(it, upper, trainingDataSize);
  auto numVectors =
      static_cast<std::int64_t>(trainingData.size() / _definition.dimension);

  LOG_TOPIC("a162b", INFO, Logger::STATISTICS)
      << "[shard=" << _shardName << ", index=" << _indexId << "] "
      << "Loaded " << numVectors << " vectors. Start training process on "
      << _definition.nLists << " centroids.";

  faissIndex->train(numVectors, trainingData.data());

  LOG_TOPIC("a160b", INFO, Logger::STATISTICS)
      << "[shard=" << _shardName << ", index=" << _indexId << "] "
      << "Finished training.";

  // Persist the resolved defaultNProbe into IndexIVF::nprobe so that it
  // survives serialization and can be read back on restore without needing
  // to recompute from the (no-longer-available) original document count.
  faissIndex->nprobe = _definition.defaultNProbe;

  // Serialize the trained FAISS index
  faiss::VectorIOWriter writer;
  faiss::write_index(faissIndex.get(), &writer);

  TrainingResult result;
  result.faissIndex = std::move(faissIndex);
  result.trainedData.codeData = std::move(writer.data);
  return result;
}

Result ingestVectors(RocksDBVectorIndex& index,
                     std::shared_ptr<faiss::IndexIVF> const& faissIndex,
                     rocksdb::DB* rootDB,
                     std::unique_ptr<rocksdb::Iterator> documentIterator) {
  auto const& definition = index.getDefinition();
  auto const dim = definition.dimension;
  bool const isSparse = index.sparse();
  auto const& fields = index.fields();
  auto const hasStored = index.hasStoredValues();
  auto const& stored = index.storedValues();
  auto* cf = index.columnFamily();
  auto const objId = index.objectId();

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
      } else {
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
          if (res.is(TRI_ERROR_BAD_PARAMETER) && isSparse) {
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

      if (definition.metric == SimilarityMetric::kCosine) {
        faiss::fvec_renorm_L2(dim, batch->docIds.size(),
                              batch->vectors.data());
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
        faissIndex->quantizer->assign(n, x, coarse_idx.get());
        auto code_size = faissIndex->code_size;
        std::unique_ptr<uint8_t[]> flat_codes(new uint8_t[n * code_size]);

        faissIndex->encode_vectors(n, x, coarse_idx.get(), flat_codes.get());

        auto encoded = std::make_unique<EncodedVectors>();
        encoded->docIds = std::move(item->docIds);
        encoded->lists = std::move(coarse_idx);
        encoded->codes = std::move(flat_codes);
        encoded->storedValues = std::move(item->storedValues);

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
          key.constructVectorIndexValue(objId, item->lists[k],
                                        item->docIds[k]);

          auto const value = std::invoke([&]() {
            auto* ptr = item->codes.get() + k * faissIndex->code_size;
            if (hasStored) {
              RocksDBVectorIndexEntryValue rocksdbEntryValue;
              rocksdbEntryValue.encodedValue =
                  std::vector<uint8_t>(ptr, ptr + faissIndex->code_size);
              rocksdbEntryValue.storedValues = std::move(item->storedValues[k]);

              return RocksDBValue::VectorIndexValue(rocksdbEntryValue);
            } else {
              return RocksDBValue::VectorIndexValue(ptr,
                                                    faissIndex->code_size);
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

  LOG_TOPIC("71c45", INFO, Logger::FIXME)
      << "[shard=" << index.collection().name()
      << ", index=" << index.id().id() << "] "
      << "Ingesting vectors into index. Threads: num-readers=" << numReaders
      << " num-encoders=" << numEncoders << " numWriters=" << numWriters;

  startNThreads(readDocuments, numReaders);
  startNThreads(encodeVectors, numEncoders);
  startNThreads(writeDocuments, numWriters);

  threads.clear();

  if (firstError.ok()) {
    LOG_TOPIC("41658", INFO, Logger::FIXME)
        << "[shard=" << index.collection().name()
        << ", index=" << index.id().id() << "] "
        << "Ingestion done. Encoded " << countDocuments << " vectors in "
        << countBatches
        << " batches. Pipeline skew: " << counters.readProduceBlocked << " "
        << counters.encodeConsumeBlocked << " " << counters.encodeProduceBlocked
        << " " << counters.writeConsumeBlocked;
  } else {
    LOG_TOPIC("96a80", ERR, Logger::FIXME)
        << "[shard=" << index.collection().name()
        << ", index=" << index.id().id() << "] "
        << "Ingestion failed: " << firstError;
  }

  return firstError;
}

Result buildVectorIndex(RocksDBVectorIndex& index) {
  auto& coll = index.collection();
  auto& engine = coll.vocbase().engine<RocksDBEngine>();
  rocksdb::DB* rootDB = engine.db()->GetRootDB();

  auto const* rcoll =
      static_cast<RocksDBCollection const*>(coll.getPhysical());
  auto const bounds =
      RocksDBKeyBounds::CollectionDocuments(rcoll->objectId());

  auto* docCF = RocksDBColumnFamilyManager::get(
      RocksDBColumnFamilyManager::Family::Documents);

  VectorIndexTrainer trainer(index.getDefinition(), index.sparse(),
                             index.fields(), coll.name(), index.id().id());

  // The insert transaction that triggered training may not have committed
  // yet, so the snapshot might not see the documents. Retry with backoff.
  constexpr int kMaxRetries = 10;
  for (int attempt = 0; attempt < kMaxRetries; ++attempt) {
    rocksdb::Slice upper(bounds.end());
    rocksdb::ReadOptions ro(false, false);
    ro.prefix_same_as_start = true;
    ro.iterate_upper_bound = &upper;

    std::unique_ptr<rocksdb::Iterator> trainIt(
        rootDB->NewIterator(ro, docCF));
    trainIt->Seek(bounds.start());

    try {
      auto result = trainer.train(*trainIt, upper);
      index.applyTrainingResult(std::move(result));
      break;
    } catch (basics::Exception const& e) {
      if (attempt + 1 >= kMaxRetries) {
        return {e.code(), e.message()};
      }
      LOG_TOPIC("e162b", INFO, Logger::ENGINES)
          << "[shard=" << coll.name() << ", index=" << index.id().id() << "] "
          << "Training attempt " << (attempt + 1) << " failed ("
          << e.message() << "), retrying...";
      std::this_thread::sleep_for(
          std::chrono::milliseconds(200 * (1 << attempt)));
    }
  }

  LOG_TOPIC("e163b", INFO, Logger::ENGINES)
      << "[shard=" << coll.name() << ", index=" << index.id().id() << "] "
      << "Training complete. Ingesting vectors via RocksDBBuilderIndex.";

  auto self = coll.lookupIndex(index.id());
  auto selfRocksDB = std::static_pointer_cast<RocksDBIndex>(self);
  auto builder = std::make_shared<RocksDBBuilderIndex>(
      selfRocksDB, rcoll->meta().numberDocuments(), /*parallelism*/ 1);

  auto res = builder->fillIndexForeground();
  if (res.fail()) {
    LOG_TOPIC("e164b", ERR, Logger::ENGINES)
        << "[shard=" << coll.name() << ", index=" << index.id().id() << "] "
        << "Vector ingestion failed: " << res.errorMessage();
  } else {
    LOG_TOPIC("e165b", INFO, Logger::ENGINES)
        << "[shard=" << coll.name() << ", index=" << index.id().id() << "] "
        << "Deferred training and ingestion completed.";
  }

  return res;
}

}  // namespace arangodb::vector
