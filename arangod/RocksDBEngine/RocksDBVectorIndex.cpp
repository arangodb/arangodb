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

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Assertions/Assert.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Basics/BoundedChannel.h"
#include "Inspection/VPack.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBIndex.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "Transaction/Helpers.h"
#include <sys/types.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>
#include <omp.h>
#include "Indexes/Index.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "faiss/IndexFlat.h"
#include "faiss/IndexIVFFlat.h"
#include "faiss/MetricType.h"
#include "faiss/index_factory.h"
#include "faiss/utils/distances.h"
#include "faiss/index_io.h"
#include "faiss/impl/io.h"
#include "faiss/IndexIVFPQ.h"

namespace arangodb {

// This assertion must hold for faiss::idx_t to be used
static_assert(sizeof(faiss::idx_t) == sizeof(LocalDocumentId::BaseType),
              "Faiss id and LocalDocumentId must be of same size");

// This assertion is that faiss::idx_t is the same type as std::int64_t
static_assert(std::is_same_v<faiss::idx_t, std::int64_t>,
              "Faiss idx_t base type is no longer int64_t");

faiss::MetricType metricToFaissMetric(SimilarityMetric const metric) {
  switch (metric) {
    case SimilarityMetric::kL2:
      return faiss::MetricType::METRIC_L2;
    case SimilarityMetric::kCosine:
      return faiss::MetricType::METRIC_INNER_PRODUCT;
  }
}

struct RocksDBInvertedListsIterator : faiss::InvertedListsIterator {
  RocksDBInvertedListsIterator(RocksDBVectorIndex* index,
                               LogicalCollection* collection,
                               transaction::Methods* trx,
                               std::size_t listNumber, std::size_t codeSize)
      : InvertedListsIterator(),
        _index(index),
        _listNumber(listNumber),
        _codeSize(codeSize) {
    RocksDBTransactionMethods* mthds =
        RocksDBTransactionState::toMethods(trx, collection->id());
    TRI_ASSERT(index->columnFamily() ==
               RocksDBColumnFamilyManager::get(
                   RocksDBColumnFamilyManager::Family::VectorIndex));

    _it = mthds->NewIterator(index->columnFamily(), [&](auto& opts) {
      TRI_ASSERT(opts.prefix_same_as_start);
    });

    _rocksdbKey.constructVectorIndexValue(_index->objectId(), _listNumber);
    _it->Seek(_rocksdbKey.string());
  }

  [[nodiscard]] bool is_available() const override {
    return _it->Valid() && _it->key().starts_with(_rocksdbKey.string());
  }

  void next() override { _it->Next(); }

  std::pair<faiss::idx_t, uint8_t const*> get_id_and_codes() override {
    auto const docId = RocksDBKey::indexDocumentId(_it->key());
    TRI_ASSERT(_codeSize == _it->value().size());
    auto const* value = reinterpret_cast<uint8_t const*>(_it->value().data());
    return {static_cast<faiss::idx_t>(docId.id()), value};
  }

 private:
  RocksDBKey _rocksdbKey;
  arangodb::RocksDBVectorIndex* _index = nullptr;

  std::unique_ptr<rocksdb::Iterator> _it;
  std::size_t _listNumber;
  std::size_t _codeSize;
};

struct RocksDBInvertedLists : faiss::InvertedLists {
  RocksDBInvertedLists(RocksDBVectorIndex* index, LogicalCollection* collection,
                       std::size_t nlist, size_t codeSize)
      : InvertedLists(nlist, codeSize), _index(index), _collection(collection) {
    use_iterator = true;
    assert(status.ok());
  }

  std::size_t list_size(std::size_t /*listNumber*/) const override {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "faiss list_size not supported");
  }

  std::uint8_t const* get_codes(std::size_t /*listNumber*/) const override {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "faiss get_codes not supported");
  }

  faiss::idx_t const* get_ids(std::size_t /*listNumber*/) const override {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "faiss get_ids not supported");
  }

  size_t add_entries(std::size_t listNumber, std::size_t nEntry,
                     faiss::idx_t const* ids,
                     std::uint8_t const* code) override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  void update_entries(std::size_t /*listNumber*/, std::size_t /*offset*/,
                      std::size_t /*n_entry*/, const faiss::idx_t* /*ids*/,
                      const std::uint8_t* /*code*/) override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  void resize(std::size_t /*listNumber*/, std::size_t /*new_size*/) override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  void remove_id(size_t list_no, faiss::idx_t id) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  faiss::InvertedListsIterator* get_iterator(std::size_t listNumber,
                                             void* context) const override {
    auto trx = static_cast<transaction::Methods*>(context);
    return new RocksDBInvertedListsIterator(_index, _collection, trx,
                                            listNumber, this->code_size);
  }

 private:
  RocksDBVectorIndex* _index;
  LogicalCollection* _collection;
};

struct RocksDBIndexIVFFlat : faiss::IndexIVFFlat {
  RocksDBIndexIVFFlat(Index* quantizer,
                      UserVectorIndexDefinition const& definition)
      : IndexIVFFlat(quantizer, definition.dimension, definition.nLists,
                     metricToFaissMetric(definition.metric)) {
    cp.check_input_data_for_NaNs = false;
    cp.niter = definition.trainingIterations;
  }

  void replace_invlists(RocksDBInvertedLists* invertedList) {
    IndexIVFFlat::replace_invlists(invertedList, false);
    rocksdbInvertedLists = invertedList;
  }

  void remove_id(std::vector<float>& vector, faiss::idx_t const docId) {
    faiss::idx_t listId{0};
    quantizer->assign(1, vector.data(), &listId);
    rocksdbInvertedLists->remove_id(listId, docId);
    ntotal -= 1;
  }

  RocksDBInvertedLists* rocksdbInvertedLists = nullptr;
};

RocksDBIndexIVFFlat createFaissIndex(auto& quantitizer,
                                     auto& vectorDefinition) {
  return std::visit(
      [&vectorDefinition](auto& quant) {
        return RocksDBIndexIVFFlat(&quant, vectorDefinition);
      },
      quantitizer);
}

#define LOG_VECTOR_INDEX(lid, level, topic) \
  LOG_TOPIC((lid), level, topic)            \
      << "[shard=" << _collection.name() << ", index=" << _iid.id() << "] "

RocksDBVectorIndex::RocksDBVectorIndex(IndexId iid, LogicalCollection& coll,
                                       arangodb::velocypack::Slice info)
    : RocksDBIndex(iid, coll, info,
                   RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::VectorIndex),
                   /*useCache*/ false,
                   /*cacheManager*/ nullptr,
                   /*engine*/
                   coll.vocbase().engine<RocksDBEngine>()) {
  TRI_ASSERT(type() == Index::TRI_IDX_TYPE_VECTOR_INDEX);
  velocypack::deserialize(info.get("params"), _definition);
  if (auto data = info.get("trainedData"); !data.isNone()) {
    velocypack::deserialize(data, _trainedData.emplace());
  }

  if (_trainedData) {
    faiss::VectorIOReader reader;
    // TODO prevent this copy, but instead implement own IOReader, reading
    // directly from the
    //  training data.
    reader.data = _trainedData->codeData;
    _faissIndex = std::unique_ptr<faiss::IndexIVF>{
        dynamic_cast<faiss::IndexIVF*>(faiss::read_index(&reader))};
    ADB_PROD_ASSERT(_faissIndex != nullptr);

    _faissIndex->replace_invlists(
        new RocksDBInvertedLists(this, &coll, _definition.nLists,
                                 _faissIndex->code_size),
        true /* faiss owns the inverted list */);
  } else {
    if (_definition.factory) {
      std::shared_ptr<faiss::Index> index;
      index.reset(faiss::index_factory(
          _definition.dimension, _definition.factory->c_str(),
          metricToFaissMetric(_definition.metric)));

      _faissIndex = std::dynamic_pointer_cast<faiss::IndexIVF>(index);
      if (_faissIndex == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "Index definition not supported. Expected IVF index.");
      }

      if (std::size_t(_definition.nLists) != _faissIndex->nlist) {
        THROW_ARANGO_EXCEPTION_FORMAT(
            TRI_ERROR_BAD_PARAMETER,
            "The nLists parameter has to agree with the actual nlists implied "
            "by the factory string (which is %zu)",
            _faissIndex->nlist);
      }

      _definition.nLists = _faissIndex->nlist;
    } else {
      auto quantizer = std::invoke([this]() -> std::unique_ptr<faiss::Index> {
        switch (_definition.metric) {
          case arangodb::SimilarityMetric::kL2:
            return std::make_unique<faiss::IndexFlatL2>(_definition.dimension);
          case arangodb::SimilarityMetric::kCosine:
            return std::make_unique<faiss::IndexFlatIP>(_definition.dimension);
        }
      });

      _faissIndex = std::make_unique<faiss::IndexIVFFlat>(
          quantizer.get(), _definition.dimension, _definition.nLists,
          metricToFaissMetric(_definition.metric));
      _faissIndex->own_fields = nullptr != quantizer.release();
    }
  }
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

  return true;
}

void RocksDBVectorIndex::toVelocyPack(
    arangodb::velocypack::Builder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  VPackObjectBuilder objectBuilder(&builder);
  RocksDBIndex::toVelocyPack(builder, flags);
  builder.add(VPackValue("params"));
  velocypack::serialize(builder, _definition);

  if (_trainedData && Index::hasFlag(flags, Index::Serialize::Internals) &&
      !Index::hasFlag(flags, Index::Serialize::Maintenance)) {
    builder.add(VPackValue("trainedData"));
    velocypack::serialize(builder, *_trainedData);
  }
}

std::pair<std::vector<VectorIndexLabelId>, std::vector<float>>
RocksDBVectorIndex::readBatch(std::vector<float>& inputs,
                              SearchParameters const& searchParameters,
                              RocksDBMethods* rocksDBMethods,
                              transaction::Methods* trx,
                              std::shared_ptr<LogicalCollection> collection,
                              std::size_t count, std::size_t topK) {
  TRI_ASSERT(topK * count == (inputs.size() / _definition.dimension) * topK)
      << "Number of components does not match vectors dimensions, topK: "
      << topK << ", count: " << count
      << ", dimension: " << _definition.dimension
      << ", inputs size: " << inputs.size();

  std::vector<float> distances(topK * count);
  std::vector<faiss::idx_t> labels(topK * count);

  if (_definition.metric == SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(_definition.dimension, count, inputs.data());
  }

  faiss::SearchParametersIVF searchParametersIvf;
  searchParametersIvf.nprobe =
      searchParameters.nProbe.value_or(_definition.defaultNProbe);
  searchParametersIvf.inverted_list_context = trx;
  _faissIndex->search(count, inputs.data(), topK, distances.data(),
                      labels.data(), &searchParametersIvf);
  // faiss returns squared distances for L2, square them so they are returned in
  // normal form
  if (_definition.metric == SimilarityMetric::kL2) {
    std::ranges::transform(distances, distances.begin(),
                           [](auto const& elem) { return std::sqrt(elem); });
  }

  return {std::move(labels), std::move(distances)};
}

Result RocksDBVectorIndex::readDocumentVectorData(velocypack::Slice doc,
                                                  std::vector<float>& input) {
  TRI_ASSERT(_fields.size() == 1);
  VPackSlice value = rocksutils::accessDocumentPath(doc, _fields[0]);
  input.clear();
  input.reserve(_definition.dimension);
  if (auto res = velocypack::deserializeWithStatus(value, input); !res.ok()) {
    return {TRI_ERROR_BAD_PARAMETER, res.error()};
  }

  if (input.size() != static_cast<std::size_t>(_definition.dimension)) {
    return {TRI_ERROR_BAD_PARAMETER,
            fmt::format("input vector of {} dimension does not have the "
                        "correct dimension of {}",
                        input.size(), _definition.dimension)};
  }

  if (_definition.metric == SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(_definition.dimension, 1, input.data());
  }

  return {};
}

/// @brief inserts a document into the index
Result RocksDBVectorIndex::insert(transaction::Methods& /*trx*/,
                                  RocksDBMethods* methods,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& /*options*/,
                                  bool /*performChecks*/) {
  std::vector<float> input;
  if (auto res = readDocumentVectorData(doc, input); res.fail()) {
    return res;
  }

  faiss::idx_t listId{0};
  TRI_ASSERT(_faissIndex->quantizer != nullptr);
  _faissIndex->quantizer->assign(1, input.data(), &listId);

  RocksDBKey rocksdbKey;
  rocksdbKey.constructVectorIndexValue(objectId(), listId, documentId);
  std::unique_ptr<uint8_t[]> flat_codes(new uint8_t[_faissIndex->code_size]);
  _faissIndex->encode_vectors(1, input.data(), &listId, flat_codes.get());

  auto const value = RocksDBValue::VectorIndexValue(
      reinterpret_cast<const char*>(flat_codes.get()), _faissIndex->code_size);
  auto const status = methods->Put(_cf, rocksdbKey, value.string(), false);

  return rocksutils::convertStatus(status);
}

void RocksDBVectorIndex::prepareIndex(std::unique_ptr<rocksdb::Iterator> it,
                                      rocksdb::Slice upper,
                                      RocksDBMethods* methods) {
  // In normal replication code this can be called multiple times
  // so to stop retraining of vector index we ignore this part
  if (_faissIndex->is_trained) {
    return;
  }
  std::int64_t counter{0};
  std::int64_t trainingDataSize =
      _faissIndex->cp.max_points_per_centroid * _definition.nLists;
  std::vector<float> trainingData;
  std::vector<float> input;
  input.reserve(_definition.dimension);

  LOG_VECTOR_INDEX("b161b", INFO, Logger::FIXME)
      << "Loading " << trainingDataSize << " vectors of dimension "
      << _definition.dimension << " for training.";

  while (counter < trainingDataSize && it->Valid()) {
    TRI_ASSERT(it->key().compare(upper) < 0);
    auto doc = VPackSlice(reinterpret_cast<uint8_t const*>(it->value().data()));
    if (auto res = readDocumentVectorData(doc, input); res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    trainingData.insert(trainingData.end(), input.begin(), input.end());
    input.clear();

    it->Next();
    ++counter;
  }

  LOG_VECTOR_INDEX("a162b", INFO, Logger::FIXME)
      << "Loaded " << counter << " vectors. Start training process on "
      << _definition.nLists << " centroids.";

  if (_definition.metric == SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(_definition.dimension, counter, trainingData.data());
  }
  _faissIndex->train(counter, trainingData.data());
  LOG_VECTOR_INDEX("a160b", INFO, Logger::FIXME) << "Finished training.";

  // Update vector definition data with quantizier data
  faiss::VectorIOWriter writer;
  faiss::write_index(_faissIndex.get(), &writer);
  _trainedData.emplace().codeData = std::move(writer.data);

  _faissIndex->replace_invlists(
      new RocksDBInvertedLists(this, &collection(), _definition.nLists,
                               _faissIndex->code_size),
      true /* faiss owns the inverted list */);
}

/// @brief removes a document from the index
Result RocksDBVectorIndex::remove(transaction::Methods& /*trx*/,
                                  RocksDBMethods* methods,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& /*options*/) {
  std::vector<float> input;
  if (auto res = readDocumentVectorData(doc, input); res.fail()) {
    return res;
  }

  faiss::idx_t listId{0};
  TRI_ASSERT(_faissIndex->quantizer != nullptr);
  _faissIndex->quantizer->assign(1, input.data(), &listId);
  RocksDBKey rocksdbKey;
  rocksdbKey.constructVectorIndexValue(objectId(), listId, documentId);
  auto status = methods->Delete(_cf, rocksdbKey);

  if (!status.ok()) {
    // Here we need to throw since there is no way to return the status
    auto const res = rocksutils::convertStatus(status);
    THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
  }

  return {};
}

UserVectorIndexDefinition const&
RocksDBVectorIndex::getVectorIndexDefinition() {
  return getDefinition();
}

#define LOG_INGESTION LOG_DEVEL_IF(false)

Result RocksDBVectorIndex::ingestVectors(
    rocksdb::DB* rootDB, std::unique_ptr<rocksdb::Iterator> documentIterator) {
  // Ingestion Strategy
  // We have three thread groups:
  // 1. Reader - read documents and extract the vector data from them
  // 2. Encoder - use the faiss index to encode the vectors
  // 3. Writer - collect encoded vectors into write batches and write them to
  // disk The number of threads in each group can be configured. Also, each
  // stage communicates with the next stage via a bounded queue. This limits the
  // amount of excess work and makes sure that the bottleneck is never starving
  // of work.
  LOG_INGESTION << __func__
                << " BEGIN iter->valid = " << documentIterator->Valid();

  struct DocumentVectors {
    std::vector<LocalDocumentId> docIds;
    // dim * docIds.size() vectors
    std::vector<float> vectors;
  };

  struct EncodedVectors {
    std::vector<LocalDocumentId> docIds;
    std::unique_ptr<faiss::idx_t[]> lists;
    std::unique_ptr<uint8_t[]> codes;
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

  auto encodeVectors = [&]() {
    BoundedChannelProducerGuard guard(encodedChannel);
    // This trivially parallelizes
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

        LOG_INGESTION << "ENCODE encoded " << encoded->docIds.size()
                      << " vectors";
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

  auto extractDocumentVector =
      [&](velocypack::Slice doc, std::vector<basics::AttributeName> const& path,
          std::vector<float>& output) {
        try {
          auto v = rocksutils::accessDocumentPath(doc, path);
          if (!v.isArray()) {
            THROW_ARANGO_EXCEPTION_FORMAT(
                TRI_ERROR_TYPE_ERROR,
                "array expected for vector attribute for document %s",
                transaction::helpers::extractKeyFromDocument(doc)
                    .copyString()
                    .c_str());
          }
          if (v.length() != _definition.dimension) {
            THROW_ARANGO_EXCEPTION_FORMAT(
                TRI_ERROR_TYPE_ERROR,
                "provided vector is not of matching dimension for document %s",
                transaction::helpers::extractKeyFromDocument(doc)
                    .copyString()
                    .c_str());
          }
          for (auto d : VPackArrayIterator(v)) {
            if (not d.isNumber<double>()) {
              THROW_ARANGO_EXCEPTION_FORMAT(
                  TRI_ERROR_TYPE_ERROR,
                  "vector contains data not representable as double for "
                  "document %s",
                  transaction::helpers::extractKeyFromDocument(doc)
                      .copyString()
                      .c_str());
            }
            output.push_back(d.getNumericValue<double>());
          }
        } catch (velocypack::Exception const& e) {
          LOG_DEVEL << doc.toJson();
          THROW_ARANGO_EXCEPTION_FORMAT(
              TRI_ERROR_TYPE_ERROR,
              "deserialization error when accessing a document: %s", e.what());
        }
      };

  auto readDocuments = [&] {
    // This is a simple implementation, using a single thread.
    // If reading becomes the bottleneck, we can always adapt the parallel index
    // reader code
    static_assert(numReaders == 1,
                  "this code is not prepared for multiple reads");

    errorExceptionHandler([&] {
      BoundedChannelProducerGuard guard(documentChannel);

      auto prepareBatch = [&] {
        auto batch = std::make_unique<DocumentVectors>();
        batch->docIds.reserve(documentPerBatch);
        batch->vectors.reserve(documentPerBatch * _definition.dimension);
        return batch;
      };

      std::unique_ptr<DocumentVectors> batch = prepareBatch();
      while (documentIterator->Valid() && not hasError.load()) {
        LocalDocumentId docId = RocksDBKey::documentId(documentIterator->key());
        VPackSlice doc = RocksDBValue::data(documentIterator->value());
        extractDocumentVector(doc, _fields[0], batch->vectors);
        batch->docIds.push_back(docId);
        documentIterator->Next();

        if (batch->docIds.size() == documentPerBatch) {
          LOG_INGESTION << "READ done with batch " << documentPerBatch;
          auto [shouldStop, blocked] = documentChannel.push(std::move(batch));
          counters.readProduceBlocked += blocked;

          if (shouldStop) {
            return;
          }
          batch = prepareBatch();
        }
      }

      if (batch) {
        LOG_INGESTION << "READ producing final batch size="
                      << batch->docIds.size();
        std::ignore = documentChannel.push(std::move(batch));
      }
    });
  };

  auto writeDocuments = [&] {
    // This parallelized trivially already
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
          auto const value = RocksDBValue::VectorIndexValue(
              reinterpret_cast<const char*>(item->codes.get() +
                                            k * _faissIndex->code_size),
              _faissIndex->code_size);
          status = batch.Put(_cf, key.string(), value.string());
          if (not status.ok()) {
            THROW_ARANGO_EXCEPTION(rocksutils::convertStatus(status));
          }
        }

        LOG_INGESTION << "[WRITE] writing " << item->docIds.size()
                      << " encoded vectors, batch size = " << batch.Count();

        rocksdb::WriteOptions ro;
        status = rootDB->Write(ro, &batch);
        if (not status.ok()) {
          THROW_ARANGO_EXCEPTION(rocksutils::convertStatus(status));
        }
      });
    }
  };

  std::vector<std::jthread> _threads;

  auto startNThreads = [&](auto& func, size_t n) {
    for (size_t k = 0; k < n; k++) {
      _threads.emplace_back(func);
    }
  };

  LOG_VECTOR_INDEX("71c45", INFO, Logger::FIXME)
      << "Ingesting vectors into index. Threads: num-readers=" << numReaders
      << " num-encoders=" << numEncoders << " numWriters=" << numWriters;

  startNThreads(readDocuments, numReaders);
  startNThreads(encodeVectors, numEncoders);
  startNThreads(writeDocuments, numWriters);

  LOG_INGESTION << "ALL THREADS STARTED!";

  _threads.clear();

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
