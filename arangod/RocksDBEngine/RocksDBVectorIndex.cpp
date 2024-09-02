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
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEngine/RocksDBVectorIndex.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"
#include "Assertions/Assert.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Containers/Enumerate.h"
#include "Inspection/VPack.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBMethods.h"
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

namespace arangodb {

// This assertion must hold for faiss idx_t to be used
static_assert(sizeof(faiss::idx_t) == sizeof(LocalDocumentId::BaseType),
              "Faiss id and LocalDocumentId must be of same size");

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

  virtual bool is_available() const override {
    return _it->Valid() && _it->key().starts_with(_rocksdbKey.string());
  }

  virtual void next() override { _it->Next(); }

  virtual std::pair<faiss::idx_t, const uint8_t*> get_id_and_codes() override {
    auto const id = RocksDBKey::indexDocumentId(_it->key());
    TRI_ASSERT(_codeSize == _it->value().size());
    auto value = reinterpret_cast<const uint8_t*>(_it->value().data());
    return {static_cast<faiss::idx_t>(id.id()), value};
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

  std::size_t list_size(std::size_t listNumber) const override {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_SHUTTING_DOWN,
                                   "faiss list_size not supported");
  }

  const std::uint8_t* get_codes(std::size_t listNumber) const override {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_SHUTTING_DOWN,
                                   "faiss get_codes not supported");
  }

  const faiss::idx_t* get_ids(std::size_t listNumber) const override {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_SHUTTING_DOWN,
                                   "faiss get_ids not supported");
  }

  size_t add_entries(std::size_t listNumber, std::size_t nEntry,
                     const faiss::idx_t* ids,
                     const std::uint8_t* code) override {
    for (std::size_t i = 0; i < nEntry; i++) {
      RocksDBKey rocksdbKey;
      // Can we get away with saving LocalDocumentID in key, maybe we need to
      // the use IdSelector in options during search
      auto const docId = LocalDocumentId(*(ids + i));
      rocksdbKey.constructVectorIndexValue(_index->objectId(), listNumber,
                                           docId);

      auto const value = RocksDBValue::VectorIndexValue(
          reinterpret_cast<const char*>(code + i * code_size), code_size);
      _rocksDBMethods->PutUntracked(_cf, rocksdbKey, value.string());

      assert(status.ok());
    }
    return 0;  // ignored
  }

  void update_entries(std::size_t listNumber, std::size_t offset,
                      std::size_t n_entry, const faiss::idx_t* ids,
                      const std::uint8_t* code) override {
    // TODO
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  void resize(std::size_t listNumber, std::size_t new_size) override {
    // TODO
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  faiss::InvertedListsIterator* get_iterator(
      std::size_t listNumber, void* inverted_list_context) const override {
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

class RocksDBVectorIndexIterator final : public IndexIterator {
 public:
  RocksDBVectorIndexIterator(LogicalCollection* collection,
                             transaction::Methods* trx,
                             std::vector<float>&& input,
                             ReadOwnWrites readOwnWrites,
                             faiss::IndexFlatL2& quantitizer,
                             FullVectorIndexDefinition& indexDefinition,
                             RocksDBVectorIndex* index,
                             rocksdb::ColumnFamilyHandle* cf, std::size_t topK)
      : IndexIterator(collection, trx, readOwnWrites),
        _ids(topK, -1),
        _flatIndex(&quantitizer, indexDefinition.dimensions,
                   indexDefinition.nLists),
        _ril(index, _collection, trx, nullptr, cf, indexDefinition.nLists,
             _flatIndex.code_size),
        _input(input),
        _topK(topK) {
    _flatIndex.replace_invlists(&_ril, false);
  }

  std::string_view typeName() const noexcept final {
    return "rocksdb-vector-index-iterator";
  }

 protected:
  bool nextImpl(LocalDocumentIdCallback const& callback,
                uint64_t limit) override {
    if (limit == 0) {
      // No limit no data, or we are actually done. The last call should have
      // returned false
      TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
      return false;
    }

    if (!_initialized) {
      std::vector<float> distances(_topK);
      _flatIndex.search(1, _input.data(), _topK, distances.data(), _ids.data(),
                        nullptr);
      TRI_ASSERT(std::ranges::any_of(_ids, [](auto const& elem) {
        return elem != -1;
      })) << "Elements not found";
      _initialized = true;
    }

    for (std::size_t i{0}; i < limit && _producedElements < _topK;
         ++i, ++_producedElements) {
      LocalDocumentId docId = LocalDocumentId(_ids[i]);
      if (docId.isSet()) {
        callback(docId);
      }
    }

    return false;
  }

  void resetImpl() override {
    // TODO
  }

  bool nextCoveringImpl(CoveringCallback const& callback,
                        uint64_t limit) override {
    return false;
  }

 private:
  RocksDBKey _rocksdbKey;
  rocksdb::Slice _upperBound;
  RocksDBKey _upperBoundKey;
  std::vector<std::uint8_t> _cur;
  std::vector<faiss::idx_t> _ids;
  bool _initialized{false};
  std::size_t _producedElements{0};

  faiss::IndexIVFFlat _flatIndex;
  RocksDBInvertedLists _ril;
  std::vector<float> _input;
  std::size_t _topK;
};

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

  _quantizer = faiss::IndexFlatL2(_definition.dimensions);
  if (_definition.trainedData) {
    _quantizer.code_size = _definition.trainedData->codeSize;
    _quantizer.ntotal = _definition.trainedData->numberOfCodes;
    _quantizer.codes = _definition.trainedData->codeData;
    _quantizer.is_trained = true;
  }
}

/// @brief Test if this index matches the definition
bool RocksDBVectorIndex::matchesDefinition(VPackSlice const& info) const {
  return false;
}

void RocksDBVectorIndex::toVelocyPack(
    arangodb::velocypack::Builder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  VPackObjectBuilder objectBuilder(&builder);
  RocksDBIndex::toVelocyPack(builder, flags);
  builder.add(VPackValue("params"));
  velocypack::serialize(builder, _definition);
}

// TODO Move away, also remove from MDIIndex.cpp
auto accessDocumentPath(VPackSlice doc,
                        std::vector<basics::AttributeName> const& path)
    -> VPackSlice {
  for (auto&& attrib : path) {
    TRI_ASSERT(attrib.shouldExpand == false);
    if (!doc.isObject()) {
      return VPackSlice::noneSlice();
    }

    doc = doc.get(attrib.name);
  }

  return doc;
}

/// @brief inserts a document into the index
Result RocksDBVectorIndex::insert(transaction::Methods& trx,
                                  RocksDBMethods* methods,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& options,
                                  bool performChecks) {
#ifdef USE_ENTERPRISE
  TRI_ASSERT(_fields.size() == 1);
  faiss::IndexIVFFlat flatIndex(&_quantizer, _definition.dimensions,
                                _definition.nLists);
  RocksDBInvertedLists ril(this, nullptr, nullptr, methods, _cf,
                           _definition.nLists, flatIndex.code_size);
  flatIndex.replace_invlists(&ril, false);

  VPackSlice value = accessDocumentPath(doc, _fields[0]);
  std::vector<float> input;
  input.reserve(_definition.dimensions);
  if (auto res = velocypack::deserializeWithStatus(value, input); !res.ok()) {
    return {TRI_ERROR_BAD_PARAMETER, res.error()};
  }
  TRI_ASSERT(input.size() == static_cast<std::size_t>(_definition.dimensions));

  if (input.size() != static_cast<std::size_t>(_definition.dimensions)) {
    // TODO Find better error code
    return {TRI_ERROR_BAD_PARAMETER,
            "input vector does not have correct dimensions"};
  }

  auto const docId = static_cast<faiss::idx_t>(documentId.id());
  flatIndex.add_with_ids(1, input.data(), &docId);

  return Result{};
#else
  return Result(TRI_ERROR_ONLY_ENTERPRISE);
#endif
}

void RocksDBVectorIndex::prepareIndex(std::unique_ptr<rocksdb::Iterator> it,
                                      rocksdb::Slice upper,
                                      RocksDBMethods* methods) {
  faiss::IndexIVFFlat flatIndex(&_quantizer, _definition.dimensions,
                                _definition.nLists);
  RocksDBInvertedLists ril(this, &_collection, nullptr, methods, _cf,
                           _definition.nLists, flatIndex.code_size);
  flatIndex.replace_invlists(&ril, false);

  std::vector<float> trainingData;
  std::size_t counter{0};
  while (counter < 50000 && it->Valid()) {
    TRI_ASSERT(it->key().compare(upper) < 0);
    std::vector<float> input;
    input.reserve(_definition.dimensions);

    auto doc = VPackSlice(reinterpret_cast<uint8_t const*>(it->value().data()));
    VPackSlice value = accessDocumentPath(doc, _fields[0]);
    if (auto res = velocypack::deserializeWithStatus(value, input); !res.ok()) {
      // TODO Handle this
      // return {TRI_ERROR_BAD_PARAMETER, res.error()};
    }
    TRI_ASSERT(input.size() ==
               static_cast<std::size_t>(_definition.dimensions));
    trainingData.insert(trainingData.end(), input.begin(), input.end());

    it->Next();
    ++counter;
  }

  flatIndex.train(counter, trainingData.data());
  // Update vector definition data with quantitizer data
  _definition.trainedData =
      TrainedData{.codeData = _quantizer.codes,
                  .numberOfCodes = static_cast<size_t>(_quantizer.ntotal),
                  .codeSize = _quantizer.code_size};
}

/// @brief removes a document from the index
Result RocksDBVectorIndex::remove(transaction::Methods& trx,
                                  RocksDBMethods* methods,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& options) {
#ifdef USE_ENTERPRISE
  // TODO
  /*return processDocument(doc, documentId, [this, &methods](auto const& key)
   * {*/
  /*return methods->Delete(this->_cf, key);*/
  /*});*/
  return Result(TRI_ERROR_ONLY_ENTERPRISE);
#else
  return Result(TRI_ERROR_ONLY_ENTERPRISE);
#endif
}

std::unique_ptr<IndexIterator> RocksDBVectorIndex::iteratorForCondition(
    ResourceMonitor& monitor, transaction::Methods* trx,
    aql::AstNode const* node, aql::Variable const* reference,
    IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites, int) {
  TRI_ASSERT(node->numMembers() == 1);

  auto const* functionCallNode = node->getMember(0);
  TRI_ASSERT(functionCallNode->type == aql::NODE_TYPE_FCALL);

  auto const* functionCallParams = functionCallNode->getMember(0);
  TRI_ASSERT(functionCallParams->numMembers() == 2);
  TRI_ASSERT(functionCallParams->type == aql::NODE_TYPE_ARRAY);

  auto const* queryPoint = functionCallParams->getMember(1);

  if (queryPoint->numMembers() !=
      static_cast<std::size_t>(_definition.dimensions)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE,
        fmt::format(
            "vector length must be of size {}, same as index dimensions!",
            _definition.dimensions));
  }
  std::vector<float> input;
  input.reserve(queryPoint->numMembers());
  for (size_t i = 0; i < queryPoint->numMembers(); ++i) {
    auto const vectorField = queryPoint->getMember(i);
    if (!vectorField->isNumericValue()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE,
                                     "vector field must be numeric value");
    }
    auto dv = vectorField->getDoubleValue();
    if (std::isnan(dv)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE,
                                     "vector field cannot be NaN");
    }

    input.push_back(dv);
  }

  // TODO should IndexIteratorOptions be used
  auto const topK = opts.limit;

  return std::make_unique<RocksDBVectorIndexIterator>(
      &_collection, trx, std::move(input), readOwnWrites, _quantizer,
      _definition, this, _cf, topK);
}

// Remove conditions covered by this index
aql::AstNode* RocksDBVectorIndex::specializeCondition(
    transaction::Methods& trx, aql::AstNode* condition,
    aql::Variable const* reference) const {
  TEMPORARILY_UNLOCK_NODE(condition);

  // TODO Fix this
  return condition;
}

}  // namespace arangodb
