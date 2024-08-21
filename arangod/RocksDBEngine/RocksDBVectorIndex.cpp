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
#include "Assertions/Assert.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Indexes/VectorIndexDefinition.h"
#include "Inspection/VPack.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBIndex.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "Transaction/Helpers.h"
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

RocksDBInvertedListsIterator::RocksDBInvertedListsIterator(
    RocksDBVectorIndex* index, std::size_t listNumber, std::size_t codeSize)
    : InvertedListsIterator(),
      _index(index),
      _listNumber(codeSize),
      _codeSize(codeSize),
      _codes(codeSize) {
  _rocksdbKey.constructVectorIndexValue(_index->objectId(), _listNumber);
  _it->Seek(_rocksdbKey.string());
}

bool RocksDBInvertedListsIterator::is_available() const {
  return _it->Valid() && _it->key().starts_with(_rocksdbKey.string());
}

void RocksDBInvertedListsIterator::next() { _it->Next(); }

std::pair<std::int64_t, const uint8_t*>
RocksDBInvertedListsIterator::get_id_and_codes() {
  // TODO This is already handled somewhere
  std::int64_t id = *reinterpret_cast<const std::int64_t*>(
      &_it->key().data()[sizeof(size_t)]);
  assert(code_size == it->value().size());
  return {id, reinterpret_cast<const uint8_t*>(_it->value().data())};
}

RocksDBInvertedLists::RocksDBInvertedLists(RocksDBVectorIndex* index,
                                           RocksDBMethods* rocksDBMethods,
                                           rocksdb::ColumnFamilyHandle* cf,
                                           size_t nlist, size_t code_size)
    : InvertedLists(nlist, code_size),
      _index(index),
      _rocksDBMethods(rocksDBMethods),
      _cf(cf) {
  use_iterator = true;
  assert(status.ok());
}

size_t RocksDBInvertedLists::list_size(std::size_t /*list_no*/) const {
  // TODO
  THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
}

const uint8_t* RocksDBInvertedLists::get_codes(std::size_t /*list_no*/) const {
  // TODO
  THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
}

const faiss::idx_t* RocksDBInvertedLists::get_ids(size_t /*list_no*/) const {
  // TODO
  THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
}

size_t RocksDBInvertedLists::add_entries(std::size_t listNumber,
                                         std::size_t nEntry,
                                         const faiss::idx_t* ids,
                                         const std::uint8_t* code) {
  for (size_t i = 0; i < nEntry; i++) {
    RocksDBKey rocksdbKey;
    static_assert(sizeof(faiss::idx_t) == sizeof(LocalDocumentId::BaseType),
                  "Faiss id and LocalDocumentId must be of same size");
    // Can we get away with saving LocalDocumentID in key, maybe we need to the
    // use IdSelector in options during search
    rocksdbKey.constructVectorIndexValue(_index->objectId(), listNumber,
                                         LocalDocumentId(*(ids + i)));

    // Is this code neccessary... I think it is to represent vector
    auto value = RocksDBValue::VectorIndexValue(
        reinterpret_cast<const char*>(code + i * code_size), code_size);

    _rocksDBMethods->PutUntracked(_cf, rocksdbKey, value.string());

    assert(status.ok());
  }
  return 0;  // ignored
}

void RocksDBInvertedLists::update_entries(size_t /*list_no*/, size_t /*offset*/,
                                          size_t /*n_entry*/,
                                          const faiss::idx_t* /*ids*/,
                                          const uint8_t* /*code*/) {
  // TODO
  THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
}

void RocksDBInvertedLists::resize(size_t /*list_no*/, size_t /*new_size*/) {
  // TODO
  THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
}

faiss::InvertedListsIterator* RocksDBInvertedLists::get_iterator(
    size_t listNumber, void* inverted_list_context) const {
  return new RocksDBInvertedListsIterator(_index, listNumber, this->code_size);
}

class RocksDBVectorIndexIterator final : public IndexIterator {
 public:
  RocksDBVectorIndexIterator(ResourceMonitor& monitor,
                             LogicalCollection* collection,
                             RocksDBVectorIndex* index,
                             transaction::Methods* trx,
                             std::vector<double>&& input,
                             ReadOwnWrites readOwnWrites,
                             std::size_t numberOfLists, std::size_t codeSize)
      : IndexIterator(collection, trx, readOwnWrites),
        _bound(RocksDBKeyBounds::VectorVPackIndex(index->objectId())),
        _index(index) {
    _numberOfLists = numberOfLists;
    _codeSize = codeSize;

    _rocksdbKey.constructVectorIndexValue(_index->objectId(), _numberOfLists);
    _iter->Seek(_rocksdbKey.string());
  }

  std::string_view typeName() const noexcept final {
    return "rocksdb-vector-index-iterator";
  }

 protected:
  bool nextImpl(LocalDocumentIdCallback const& callback,
                uint64_t limit) override {
    // TODO
    // for (uint64_t i = 0; i < limit;) {
    /*   if (!_iter->Valid()) {*/
    /*return false;*/
    /*}*/
    /*auto key = _iter->key();*/
    /*auto indexValue = RocksDBKey::vectorVPackIndexValue(key);*/

    /*_flatIndex->search(nq, xb.data(), k, distances.data(), labels.data(),*/
    /*nullptr);*/
    //}

    return true;
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
  RocksDBKeyBounds _bound;

  std::unique_ptr<rocksdb::Iterator> _iter;
  RocksDBVectorIndex* _index{nullptr};
  std::size_t _numberOfLists;
  std::size_t _codeSize;
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
  LOG_DEVEL << "RocksDBVectorIndex ctor";
  TRI_ASSERT(type() == Index::TRI_IDX_TYPE_VECTOR_INDEX);
  velocypack::deserialize(info.get("params"), _definition);

  _quantizer = faiss::IndexFlatL2(_definition.dimensions);
  if (_definition.trainedData) {
    _quantizer.code_size = _definition.trainedData->codeSize;
    _quantizer.ntotal = _definition.trainedData->numberOfCodes;
    _quantizer.codes = _definition.trainedData->codeData;
    LOG_DEVEL << "Trained data is: " << _quantizer.ntotal << ", "
              << _quantizer.code_size;
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
  RocksDBInvertedLists ril(this, methods, _cf, _definition.nLists,
                           flatIndex.code_size);
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

  auto docId = static_cast<faiss::idx_t>(documentId.id());
  flatIndex.add_with_ids(1, input.data(), &docId);
  count++;

  return Result{};
#else
  return Result(TRI_ERROR_ONLY_ENTERPRISE);
#endif
}

Index::FilterCosts RocksDBVectorIndex::supportsFilterCondition(
    transaction::Methods& /*trx*/,
    std::vector<std::shared_ptr<Index>> const& allIndexes,
    aql::AstNode const* node, aql::Variable const* reference,
    size_t itemsInIndex) const {
  if (node->numMembers() != 1) {
    return {};
  }
  auto const* firstMemeber = node->getMember(0);
  if (firstMemeber->type == aql::NODE_TYPE_FCALL) {
    auto const* funcNode =
        static_cast<aql::Function const*>(firstMemeber->getData());

    if (funcNode->name == "APPROX_NEAR") {
      auto const* args = firstMemeber->getMember(0);
      auto const* indexName = args->getMember(0);
      if (indexName->isValueType(aql::AstNodeValueType::VALUE_TYPE_STRING)) {
        if (indexName->getStringView() == _name) {
          return {.supportsCondition = true};
        }
      }
    }
  }
  return {};
}

void RocksDBVectorIndex::prepareIndex(std::unique_ptr<rocksdb::Iterator> it,
                                      rocksdb::Slice upper,
                                      RocksDBMethods* methods) {
  LOG_DEVEL << __FUNCTION__;
  faiss::IndexIVFFlat flatIndex(&_quantizer, _definition.dimensions,
                                _definition.nLists);
  RocksDBInvertedLists ril(this, methods, _cf, _definition.nLists,
                           flatIndex.code_size);
  flatIndex.replace_invlists(&ril, false);

  std::vector<float> trainingData;

  std::size_t counter{0};
  LOG_DEVEL << "Is iterator valid " << it->Valid();
  while (counter < 50000 && it->Valid()) {
    TRI_ASSERT(it->key().compare(upper) < 0);
    std::vector<float> input;
    input.reserve(_definition.dimensions);

    auto doc = VPackSlice(reinterpret_cast<uint8_t const*>(it->value().data()));
    VPackSlice value = accessDocumentPath(doc, _fields[0]);
    if (auto res = velocypack::deserializeWithStatus(value, input); !res.ok()) {
      LOG_DEVEL << "HANDLE THIS";
      // return {TRI_ERROR_BAD_PARAMETER, res.error()};
    }
    TRI_ASSERT(input.size() ==
               static_cast<std::size_t>(_definition.dimensions));
    trainingData.insert(trainingData.end(), input.begin(), input.end());

    it->Next();
    ++counter;
  }

  LOG_DEVEL << "Prepared and now train " << counter;
  flatIndex.train(counter, trainingData.data());
  // Update vector definition data with quantitizer data
  _definition.trainedData =
      TrainedData{.codeData = _quantizer.codes,
                  .numberOfCodes = static_cast<size_t>(_quantizer.ntotal),
                  .codeSize = _quantizer.code_size};

  LOG_DEVEL << __FUNCTION__ << ": Trained " << flatIndex.is_trained;
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
  node = node->getMember(0);
  TRI_ASSERT(node->type == aql::NODE_TYPE_FCALL);
  auto const* funcNode = static_cast<aql::Function const*>(node->getData());

  TRI_ASSERT(funcNode->name == "APPROX_NEAR");

  auto const* functionCallParams = node->getMember(0);
  TRI_ASSERT(functionCallParams->numMembers() == 3);
  TRI_ASSERT(functionCallParams->type == aql::NODE_TYPE_ARRAY);

  auto const* rhs = functionCallParams->getMember(1);

  std::vector<double> input;
  if (rhs->numMembers() != static_cast<std::size_t>(_definition.dimensions)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE,
        fmt::format(
            "vector length must be of size {}, same as index dimensions!",
            _definition.dimensions));
  }
  input.reserve(rhs->numMembers());
  for (size_t i = 0; i < rhs->numMembers(); ++i) {
    auto const vectorField = rhs->getMember(i);
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

  // auto const topK = functionCallParams->getMember(2)->getIntValue();

  return std::make_unique<RocksDBVectorIndexIterator>(
      monitor, &_collection, this, trx, std::move(input), readOwnWrites,
      _definition.nLists, _definition.trainedData->codeSize);
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
