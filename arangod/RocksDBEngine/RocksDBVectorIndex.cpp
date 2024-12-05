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
#include "Indexes/Index.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "faiss/IndexIVFFlat.h"
#include "faiss/MetricType.h"
#include "faiss/utils/distances.h"

namespace arangodb {

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
                       transaction::Methods* trx,
                       RocksDBMethods* rocksDBMethods,
                       rocksdb::ColumnFamilyHandle* cf, std::size_t nlist,
                       size_t codeSize)
      : InvertedLists(nlist, codeSize),
        _index(index),
        _collection(collection),
        _trx(trx),
        _rocksDBMethods(rocksDBMethods),
        _cf(cf) {
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
    for (std::size_t i = 0; i < nEntry; i++) {
      RocksDBKey rocksdbKey;
      // Can we get away with saving LocalDocumentID in key, maybe we need to
      // the use IdSelector in options during search
      auto const docId = LocalDocumentId(*(ids + i));
      rocksdbKey.constructVectorIndexValue(_index->objectId(), listNumber,
                                           docId);

      auto const value = RocksDBValue::VectorIndexValue(
          reinterpret_cast<const char*>(code + i * code_size), code_size);
      auto const status =
          _rocksDBMethods->PutUntracked(_cf, rocksdbKey, value.string());

      if (!status.ok()) {
        // Here we need to throw since there is no way to return the status
        auto const res = rocksutils::convertStatus(status);
        THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
      }
    }
    return 0;
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
    RocksDBKey rocksdbKey;
    // Can we get away with saving LocalDocumentID in key, maybe we need to
    // the use IdSelector in options during search
    auto const docId = LocalDocumentId(id);
    rocksdbKey.constructVectorIndexValue(_index->objectId(), list_no, docId);
    auto status = _rocksDBMethods->Delete(_cf, rocksdbKey);

    if (!status.ok()) {
      // Here we need to throw since there is no way to return the status
      auto const res = rocksutils::convertStatus(status);
      THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
    }
  }

  faiss::InvertedListsIterator* get_iterator(
      std::size_t listNumber, void* /*inverted_list_context*/) const override {
    return new RocksDBInvertedListsIterator(_index, _collection, _trx,
                                            listNumber, this->code_size);
  }

 private:
  RocksDBVectorIndex* _index;
  LogicalCollection* _collection;
  transaction::Methods* _trx;
  RocksDBMethods* _rocksDBMethods;
  rocksdb::ColumnFamilyHandle* _cf;
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

  _quantizer =
      std::invoke([this]() -> std::variant<faiss::IndexFlat, faiss::IndexFlatL2,
                                           faiss::IndexFlatIP> {
        switch (_definition.metric) {
          case arangodb::SimilarityMetric::kL2:
            return {faiss::IndexFlatL2(_definition.dimension)};
          case arangodb::SimilarityMetric::kCosine:
            return {faiss::IndexFlatIP(_definition.dimension)};
        }
      });
  if (_trainedData) {
    std::visit(
        [this](auto&& quant) {
          quant.ntotal = _trainedData->numberOfCentroids;
          quant.codes = _trainedData->codeData;
          quant.code_size = _trainedData->codeSize;
          quant.d = _definition.dimension;
          quant.metric_type = metricToFaissMetric(_definition.metric);
          quant.is_trained = true;
        },
        _quantizer);
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

  if (_trainedData && Index::hasFlag(flags, Index::Serialize::Internals)) {
    builder.add(VPackValue("trainedData"));
    velocypack::serialize(builder, *_trainedData);
  }
}

std::pair<std::vector<VectorIndexLabelId>, std::vector<float>>
RocksDBVectorIndex::readBatch(std::vector<float>& inputs,
                              std::optional<std::size_t> nProbe,
                              RocksDBMethods* rocksDBMethods,
                              transaction::Methods* trx,
                              std::shared_ptr<LogicalCollection> collection,
                              std::size_t count, std::size_t topK) {
  TRI_ASSERT(topK * count == (inputs.size() / _definition.dimension) * topK)
      << "Number of components does not match vectors dimesnions, topK: "
      << topK << ", count: " << count
      << ", dimension: " << _definition.dimension
      << ", inputs size: " << inputs.size();

  auto flatIndex = createFaissIndex(_quantizer, _definition);
  RocksDBInvertedLists ril(this, collection.get(), trx, rocksDBMethods, _cf,
                           _definition.nLists, flatIndex.code_size);
  if (nProbe) {
    flatIndex.nprobe = *nProbe;
  }
  flatIndex.replace_invlists(&ril);

  std::vector<float> distances(topK * count);
  std::vector<faiss::idx_t> labels(topK * count);

  if (_definition.metric == SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(_definition.dimension, count, inputs.data());
  }
  flatIndex.search(count, inputs.data(), topK, distances.data(), labels.data(),
                   nullptr);
  // faiss returns squared distances for L2, square them so they are returned in
  // normal form
  if (_definition.metric == SimilarityMetric::kL2) {
    std::ranges::transform(distances, distances.begin(),
                           [](auto const& elem) { return std::sqrt(elem); });
  }

  return {std::move(labels), std::move(distances)};
}

/// @brief inserts a document into the index
Result RocksDBVectorIndex::insert(transaction::Methods& /*trx*/,
                                  RocksDBMethods* methods,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& /*options*/,
                                  bool /*performChecks*/) {
  TRI_ASSERT(_fields.size() == 1);
  auto flatIndex = createFaissIndex(_quantizer, _definition);
  RocksDBInvertedLists ril(this, nullptr, nullptr, methods, _cf,
                           _definition.nLists, flatIndex.code_size);
  flatIndex.replace_invlists(&ril);

  VPackSlice value = rocksutils::accessDocumentPath(doc, _fields[0]);
  std::vector<float> input;
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

  auto const docId = static_cast<faiss::idx_t>(documentId.id());
  if (_definition.metric == SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(_definition.dimension, 1, input.data());
  }
  try {
    flatIndex.add_with_ids(1, input.data(), &docId);
  } catch (basics::Exception& e) {
    return {e.code(), e.message()};
  }

  return {};
}

void RocksDBVectorIndex::prepareIndex(std::unique_ptr<rocksdb::Iterator> it,
                                      rocksdb::Slice upper,
                                      RocksDBMethods* methods) {
  auto flatIndex = createFaissIndex(_quantizer, _definition);
  RocksDBInvertedLists ril(this, &_collection, nullptr, methods, _cf,
                           _definition.nLists, flatIndex.code_size);
  flatIndex.replace_invlists(&ril);

  std::int64_t counter{0};
  std::int64_t trainingDataSize =
      flatIndex.cp.max_points_per_centroid * _definition.nLists;
  std::vector<float> trainingData;
  std::vector<float> input;
  input.reserve(_definition.dimension);

  LOG_TOPIC("b161b", INFO, Logger::FIXME)
      << "[shard=" << _collection.name() << ", index=" << _iid.id()
      << "] Loading " << trainingDataSize << " vectors of dimension "
      << _definition.dimension << " for training.";

  while (counter < trainingDataSize && it->Valid()) {
    TRI_ASSERT(it->key().compare(upper) < 0);

    auto doc = VPackSlice(reinterpret_cast<uint8_t const*>(it->value().data()));
    VPackSlice value = rocksutils::accessDocumentPath(doc, _fields[0]);
    if (auto res = velocypack::deserializeWithStatus(value, input); !res.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE,
                                     "vector must be array of numbers!");
    }

    if (input.size() != static_cast<std::size_t>(_definition.dimension)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE,
          fmt::format(
              "vector length must be of size {}, same as index dimension!",
              _definition.dimension));
    }

    trainingData.insert(trainingData.end(), input.begin(), input.end());
    input.clear();

    it->Next();
    ++counter;
  }

  LOG_TOPIC("a162b", INFO, Logger::FIXME)
      << "[shard=" << _collection.name() << ", index=" << _iid.id()
      << "] Loaded " << counter
      << " vectors of dimension. Start training process.";

  if (_definition.metric == SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(_definition.dimension, counter, trainingData.data());
  }
  flatIndex.train(counter, trainingData.data());
  LOG_TOPIC("a160b", INFO, Logger::FIXME)
      << "[shard=" << _collection.name() << ", index=" << _iid.id()
      << "] Finished training";

  // Update vector definition data with quantizier data
  _trainedData = std::visit(
      [](auto&& quant) {
        return TrainedData{.codeData = quant.codes,
                           .numberOfCentroids = quant.ntotal,
                           .codeSize = quant.code_size};
      },
      _quantizer);
}

/// @brief removes a document from the index
Result RocksDBVectorIndex::remove(transaction::Methods& /*trx*/,
                                  RocksDBMethods* methods,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& /*options*/) {
  TRI_ASSERT(_fields.size() == 1);
  auto flatIndex = createFaissIndex(_quantizer, _definition);
  RocksDBInvertedLists ril(this, nullptr, nullptr, methods, _cf,
                           _definition.nLists, flatIndex.code_size);
  flatIndex.replace_invlists(&ril);

  VPackSlice value = rocksutils::accessDocumentPath(doc, _fields[0]);
  std::vector<float> input;
  input.reserve(_definition.dimension);
  if (auto res = velocypack::deserializeWithStatus(value, input); !res.ok()) {
    return {TRI_ERROR_BAD_PARAMETER, res.error()};
  }
  TRI_ASSERT(input.size() == static_cast<std::size_t>(_definition.dimension));

  if (input.size() != static_cast<std::size_t>(_definition.dimension)) {
    return {TRI_ERROR_BAD_PARAMETER,
            "input vector does not have correct dimension"};
  }

  if (_definition.metric == SimilarityMetric::kCosine) {
    faiss::fvec_renorm_L2(_definition.dimension, 1, input.data());
  }
  auto const docId = static_cast<faiss::idx_t>(documentId.id());

  try {
    flatIndex.remove_id(input, docId);
  } catch (basics::Exception& e) {
    return {e.code(), e.message()};
  }

  return {};
}

UserVectorIndexDefinition const&
RocksDBVectorIndex::getVectorIndexDefinition() {
  return getDefinition();
}

}  // namespace arangodb
