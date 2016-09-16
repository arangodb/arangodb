////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "HashIndex.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Utils/TransactionContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/transaction.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

LookupBuilder::LookupBuilder(
    arangodb::Transaction* trx, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields)
    : _builder(trx), _usesIn(false), _isEmpty(false), _inStorage(trx) {

  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);
  _coveredFields = fields.size();
  TRI_ASSERT(node->numMembers() == _coveredFields);

  std::pair<arangodb::aql::Variable const*,
            std::vector<arangodb::basics::AttributeName>> paramPair;
  std::vector<size_t> storageOrder;

  for (size_t i = 0; i < _coveredFields; ++i) {
    auto comp = node->getMemberUnchecked(i);
    auto attrNode = comp->getMember(0);
    auto valNode = comp->getMember(1);

    if (!attrNode->isAttributeAccessForVariable(paramPair) ||
        paramPair.first != reference) {
      attrNode = comp->getMember(1);
      valNode = comp->getMember(0);

      if (!attrNode->isAttributeAccessForVariable(paramPair) ||
          paramPair.first != reference) {
        _isEmpty = true;
        return;
      }
    }

    for (size_t j = 0; j < fields.size(); ++j) {
      if (arangodb::basics::AttributeName::isIdentical(
              fields[j], paramPair.second, true)) {
        if (TRI_AttributeNamesHaveExpansion(fields[j])) {
          TRI_IF_FAILURE("HashIndex::permutationArrayIN") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
          _mappingFieldCondition.emplace(j, valNode);
        } else {
          TRI_IF_FAILURE("HashIndex::permutationEQ") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }
          arangodb::aql::AstNodeType type = comp->type;
          if (type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
            if (!_usesIn) {
              _inStorage->openArray();
            }
            valNode->toVelocyPackValue(*(_inStorage.get()));
            _inPosition.emplace(j, std::make_pair(0, std::vector<arangodb::velocypack::Slice>()));
            _usesIn = true;
            storageOrder.emplace_back(j);
          } else {
            _mappingFieldCondition.emplace(j, valNode);
          }
        }
        break;
      }
    }
  }
  if (_usesIn) {
    _inStorage->close();
    arangodb::basics::VelocyPackHelper::VPackLess<true> sorter;
    std::unordered_set<VPackSlice,
                       arangodb::basics::VelocyPackHelper::VPackHash,
                       arangodb::basics::VelocyPackHelper::VPackEqual>
        tmp(16, arangodb::basics::VelocyPackHelper::VPackHash(),
            arangodb::basics::VelocyPackHelper::VPackEqual());
    VPackSlice storageSlice = _inStorage->slice();
    auto f = storageOrder.begin();
    for (auto const& values : VPackArrayIterator(storageSlice)) {
      tmp.clear();
      TRI_IF_FAILURE("Index::permutationIN")  {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      for (auto const& value : VPackArrayIterator(values)) {
        tmp.emplace(value);
      }
      if (tmp.empty()) {
        // IN [] short-circuit, cannot be fullfilled;
        _isEmpty = true;
        return;
      }
      // Now the elements are unique
      auto& vector = _inPosition.find(*f)->second.second;
      vector.insert(vector.end(), tmp.begin(), tmp.end());
      std::sort(vector.begin(), vector.end(), sorter);
      f++;
    }
  }
  buildNextSearchValue();
}

VPackSlice LookupBuilder::lookup() {
  return _builder->slice();
}

bool LookupBuilder::hasAndGetNext() {
  _builder->clear();
  if (!_usesIn || _isEmpty) {
    return false;
  }
  if (!incrementInPosition()) {
    return false;
  }
  buildNextSearchValue();
  return true;
}

void LookupBuilder::reset() {
  if (_isEmpty) {
    return;
  }
  if (_usesIn) {
    for (auto& it : _inPosition) {
      it.second.first = 0;
    }
  }
  buildNextSearchValue();
}

bool LookupBuilder::incrementInPosition() {
  size_t i = _coveredFields - 1;
  while (true) {
    auto it = _inPosition.find(i);
    if (it != _inPosition.end()) {
      it->second.first++;
      if (it->second.first == it->second.second.size()) {
        //  Reached end of this array. start form begining.
        //  Increment another array.
        it->second.first = 0;
      } else {
        return true;
      }
    }
    if (i == 0) {
      return false;
    }
    --i;
  }
}

void LookupBuilder::buildNextSearchValue() {
  if (_isEmpty) {
    return;
  }
  _builder->openArray();
  if (!_usesIn) {
    // Fast path, do no search and checks
    for (size_t i = 0; i < _coveredFields; ++i) {
      _mappingFieldCondition[i]->toVelocyPackValue(*(_builder.get()));
    }
  } else {
    for (size_t i = 0; i < _coveredFields; ++i) {
      auto in = _inPosition.find(i);
      if (in != _inPosition.end()) {
        _builder->add(in->second.second[in->second.first]);
      } else {
        _mappingFieldCondition[i]->toVelocyPackValue(*(_builder.get()));
      }
    }
  }
  _builder->close(); // End of search Array
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Frees an index element
////////////////////////////////////////////////////////////////////////////////

static inline bool FreeElement(TRI_index_element_t* element) {
  TRI_index_element_t::freeElement(element);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if two elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementElement(void*,
                                  TRI_index_element_t const* left,
                                  TRI_index_element_t const* right) {
  return left->document() == right->document();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief given a key generates a hash integer
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKey(void*,
                        VPackSlice const* key) {
  TRI_ASSERT(key->isArray());
  uint64_t hash = 0x0123456789abcdef;
  size_t const n = key->length();
 
  for (size_t j = 0; j < n; ++j) {
    // must use normalized hash here, to normalize different representations 
    // of arrays/objects/numbers
    hash = (*key)[j].normalizedHash(hash);
  }

  return hash;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if a key corresponds to an element
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyElement(void*,
                              VPackSlice const* left,
                              TRI_index_element_t const* right) {
  TRI_ASSERT(left->isArray());
  TRI_ASSERT(right->document() != nullptr);
  
  size_t const n = left->length();
  
  for (size_t j = 0; j < n; ++j) {
    VPackSlice const leftVPack = (*left)[j];
    TRI_vpack_sub_t* rightSub = right->subObjects() + j;
    VPackSlice const rightVPack = rightSub->slice(right->document());

    int res = arangodb::basics::VelocyPackHelper::compare(leftVPack, rightVPack, false);
    if (res != 0) {
      return false;
    }
  }

  return true;
}

static bool IsEqualKeyElementHash(
    void* userData, VPackSlice const* left,
    uint64_t const,  // Has been computed but is not used here
    TRI_index_element_t const* right) {
  return IsEqualKeyElement(userData, left, right);
}

HashIndexIterator::HashIndexIterator(arangodb::Transaction* trx,
                                     HashIndex const* index,
                                     arangodb::aql::AstNode const* node,
                                     arangodb::aql::Variable const* reference)
    : _trx(trx),
      _index(index),
      _lookups(trx, node, reference, index->fields()),
      _buffer(),
      _posInBuffer(0) {
    _index->lookup(_trx, _lookups.lookup(), _buffer);
}

TRI_doc_mptr_t* HashIndexIterator::next() {
  while (true) {
    if (_posInBuffer >= _buffer.size()) {
      if (!_lookups.hasAndGetNext()) {
        // we're at the end of the lookup values
        return nullptr;
      }

      // We have to refill the buffer
      _buffer.clear();
      _posInBuffer = 0;

      _index->lookup(_trx, _lookups.lookup(), _buffer);
    }

    if (!_buffer.empty()) {
      // found something
      return _buffer.at(_posInBuffer++);
    }
  }
}

void HashIndexIterator::nextBabies(std::vector<TRI_doc_mptr_t*>& result, size_t atMost) {
  result.clear();
  
  if (atMost == 0) {
    return;
  }

  while (true) {
    if (_posInBuffer >= _buffer.size()) {
      if (!_lookups.hasAndGetNext()) {
        // we're at the end of the lookup values
        return;
      }

      // TODO maybe we can hand in result directly and remove the buffer

      // We have to refill the buffer
      _buffer.clear();
      _posInBuffer = 0;

      _index->lookup(_trx, _lookups.lookup(), _buffer);
    }

    if (!_buffer.empty()) {
      // found something
      // TODO OPTIMIZE THIS
      if (_buffer.size() - _posInBuffer < atMost) {
        atMost = _buffer.size() - _posInBuffer;
      }

      for (size_t i = _posInBuffer; i < atMost + _posInBuffer; ++i) {
        result.emplace_back(_buffer.at(i));
      }
      _posInBuffer += atMost;
      return;
    }
  }
}

void HashIndexIterator::reset() {
  _buffer.clear();
  _posInBuffer = 0;
  _lookups.reset();
  _index->lookup(_trx, _lookups.lookup(), _buffer);
}

HashIndexIteratorVPack::~HashIndexIteratorVPack() {
  if (_searchValues != nullptr) {
    // return the VPackBuilder to the transaction context 
    _trx->transactionContextPtr()->returnBuilder(_searchValues.release());
  }
}

TRI_doc_mptr_t* HashIndexIteratorVPack::next() {
  while (true) {
    if (_posInBuffer >= _buffer.size()) {
      if (!_iterator.valid()) {
        // we're at the end of the lookup values
        return nullptr;
      }

      // We have to refill the buffer
      _buffer.clear();
      _posInBuffer = 0;

      int res = TRI_ERROR_NO_ERROR;
      _index->lookup(_trx, _iterator.value(), _buffer);
      _iterator.next();

      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }

    if (!_buffer.empty()) {
      // found something
      return _buffer.at(_posInBuffer++);
    }
  }
}

void HashIndexIteratorVPack::reset() {
  _buffer.clear();
  _posInBuffer = 0;
  _iterator.reset();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the unique array
////////////////////////////////////////////////////////////////////////////////

HashIndex::UniqueArray::UniqueArray(
    TRI_HashArray_t* hashArray, HashElementFunc* hashElement,
    IsEqualElementElementByKey* isEqualElElByKey)
    : _hashArray(hashArray),
      _hashElement(hashElement),
      _isEqualElElByKey(isEqualElElByKey) {
  TRI_ASSERT(_hashArray != nullptr);
  TRI_ASSERT(_hashElement != nullptr);
  TRI_ASSERT(_isEqualElElByKey != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the unique array
////////////////////////////////////////////////////////////////////////////////

HashIndex::UniqueArray::~UniqueArray() {
  if (_hashArray != nullptr) {
    _hashArray->invokeOnAllElements(FreeElement);
  }

  delete _hashArray;
  delete _hashElement;
  delete _isEqualElElByKey;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the multi array
////////////////////////////////////////////////////////////////////////////////

HashIndex::MultiArray::MultiArray(TRI_HashArrayMulti_t* hashArray,
                                  HashElementFunc* hashElement,
                                  IsEqualElementElementByKey* isEqualElElByKey)
    : _hashArray(hashArray),
      _hashElement(hashElement),
      _isEqualElElByKey(isEqualElElByKey) {
  TRI_ASSERT(_hashArray != nullptr);
  TRI_ASSERT(_hashElement != nullptr);
  TRI_ASSERT(_isEqualElElByKey != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the multi array
////////////////////////////////////////////////////////////////////////////////

HashIndex::MultiArray::~MultiArray() {
  if (_hashArray != nullptr) {
    _hashArray->invokeOnAllElements(FreeElement);
  }

  delete _hashArray;
  delete _hashElement;
  delete _isEqualElElByKey;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the index
////////////////////////////////////////////////////////////////////////////////

HashIndex::HashIndex(
    TRI_idx_iid_t iid, arangodb::LogicalCollection* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    bool unique, bool sparse)
    : PathBasedIndex(iid, collection, fields, unique, sparse, false),
      _uniqueArray(nullptr) {
  uint32_t indexBuckets = 1;

  if (collection != nullptr) {
    // document is a nullptr in the coordinator case
    indexBuckets = collection->indexBuckets();
  }

  auto func = std::make_unique<HashElementFunc>(_paths.size());
  auto compare = std::make_unique<IsEqualElementElementByKey>(_paths.size());

  if (unique) {
    auto array = std::make_unique<TRI_HashArray_t>(
        HashKey, *(func.get()), IsEqualKeyElementHash, IsEqualElementElement,
        *(compare.get()), indexBuckets,
        []() -> std::string { return "unique hash-array"; });

    _uniqueArray =
        new HashIndex::UniqueArray(array.get(), func.get(), compare.get());
    array.release();
  } else {
    _multiArray = nullptr;

    auto array = std::make_unique<TRI_HashArrayMulti_t>(
        HashKey, *(func.get()), IsEqualKeyElement, IsEqualElementElement,
        *(compare.get()), indexBuckets, 64,
        []() -> std::string { return "multi hash-array"; });

    _multiArray =
        new HashIndex::MultiArray(array.get(), func.get(), compare.get());

    array.release();
  }
  compare.release();

  func.release();
}

HashIndex::HashIndex(TRI_idx_iid_t iid, LogicalCollection* collection,
                     VPackSlice const& info)
    : PathBasedIndex(iid, collection, info, false), _uniqueArray(nullptr) {
  uint32_t indexBuckets = 1;

  if (collection != nullptr) {
    // document is a nullptr in the coordinator case
    // TODO: That ain't true any more
    indexBuckets = collection->indexBuckets();
  }

  auto func = std::make_unique<HashElementFunc>(_paths.size());
  auto compare = std::make_unique<IsEqualElementElementByKey>(_paths.size());

  if (_unique) {
    auto array = std::make_unique<TRI_HashArray_t>(
        HashKey, *(func.get()), IsEqualKeyElementHash, IsEqualElementElement,
        *(compare.get()), indexBuckets,
        []() -> std::string { return "unique hash-array"; });

    _uniqueArray =
        new HashIndex::UniqueArray(array.get(), func.get(), compare.get());
    array.release();
  } else {
    _multiArray = nullptr;

    auto array = std::make_unique<TRI_HashArrayMulti_t>(
        HashKey, *(func.get()), IsEqualKeyElement, IsEqualElementElement,
        *(compare.get()), indexBuckets, 64,
        []() -> std::string { return "multi hash-array"; });

    _multiArray =
        new HashIndex::MultiArray(array.get(), func.get(), compare.get());

    array.release();
  }
  compare.release();

  func.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an index stub with a hard-coded selectivity estimate
/// this is used in the cluster coordinator case
////////////////////////////////////////////////////////////////////////////////

HashIndex::HashIndex(VPackSlice const& slice)
    : PathBasedIndex(slice, false), _uniqueArray(nullptr) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the index
////////////////////////////////////////////////////////////////////////////////

HashIndex::~HashIndex() {
  if (_unique) {
    delete _uniqueArray;
  } else {
    delete _multiArray;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a selectivity estimate for the index
////////////////////////////////////////////////////////////////////////////////

double HashIndex::selectivityEstimate() const {
  if (_unique) {
    return 1.0;
  }

  if (_multiArray == nullptr || ServerState::instance()->isCoordinator()) {
    // use hard-coded selectivity estimate in case of cluster coordinator
    return 0.1;
  }

  double estimate = _multiArray->_hashArray->selectivity();
  TRI_ASSERT(estimate >= 0.0 &&
             estimate <= 1.00001);  // floating-point tolerance
  return estimate;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the index memory usage
////////////////////////////////////////////////////////////////////////////////

size_t HashIndex::memory() const {
  if (_unique) {
    return static_cast<size_t>(elementSize() *
                                   _uniqueArray->_hashArray->size() +
                               _uniqueArray->_hashArray->memoryUsage());
  }

  return static_cast<size_t>(elementSize() * _multiArray->_hashArray->size() +
                             _multiArray->_hashArray->memoryUsage());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a velocypack representation of the index
////////////////////////////////////////////////////////////////////////////////

void HashIndex::toVelocyPack(VPackBuilder& builder, bool withFigures) const {
  Index::toVelocyPack(builder, withFigures);
  builder.add("unique", VPackValue(_unique));
  builder.add("sparse", VPackValue(_sparse));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a velocypack representation of the index figures
////////////////////////////////////////////////////////////////////////////////

void HashIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add("memory", VPackValue(memory()));
  if (_unique) {
    _uniqueArray->_hashArray->appendToVelocyPack(builder);
  } else {
    _multiArray->_hashArray->appendToVelocyPack(builder);
  }
}

/// @brief Test if this index matches the definition
bool HashIndex::matchesDefinition(VPackSlice const& info) const {
  TRI_ASSERT(info.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  VPackSlice typeSlice = info.get("type");
  TRI_ASSERT(typeSlice.isString());
  StringRef typeStr(typeSlice);
  TRI_ASSERT(typeStr == typeName());
#endif
  auto value = info.get("id");
  if (!value.isNone()) {
    // We already have an id.
    if(!value.isString()) {
      // Invalid ID
      return false;
    }
    // Short circuit. If id is correct the index is identical.
    StringRef idRef(value);
    return idRef == std::to_string(_iid);
  }

  value = info.get("fields");
  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  if (n != _fields.size()) {
    return false;
  }
  if (_unique != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, "unique", false)) {
    return false;
  }
  if (_sparse != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, "sparse", false)) {
    return false;
  }

  // This check does not take ordering of attributes into account.
  std::vector<arangodb::basics::AttributeName> translate;
  for (auto const& f : VPackArrayIterator(value)) {
    bool found = false;
    if (!f.isString()) {
      // Invalid field definition!
      return false;
    }
    translate.clear();
    arangodb::StringRef in(f);
    TRI_ParseAttributeString(in, translate, true);

    for (size_t i = 0; i < n; ++i) {
      if (arangodb::basics::AttributeName::isIdentical(_fields[i], translate,
                                                        false)) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

int HashIndex::insert(arangodb::Transaction* trx, TRI_doc_mptr_t const* doc,
                      bool isRollback) {
  if (_unique) {
    return insertUnique(trx, doc, isRollback);
  }

  return insertMulti(trx, doc, isRollback);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the hash array part of the hash index
////////////////////////////////////////////////////////////////////////////////

int HashIndex::remove(arangodb::Transaction* trx, TRI_doc_mptr_t const* doc,
                      bool isRollback) {
  if (_unique) {
    return removeUnique(trx, doc, isRollback);
  }

  return removeMulti(trx, doc, isRollback);
}

int HashIndex::batchInsert(arangodb::Transaction* trx,
                           std::vector<TRI_doc_mptr_t const*> const* documents,
                           size_t numThreads) {
  if (_unique) {
    return batchInsertUnique(trx, documents, numThreads);
  }

  return batchInsertMulti(trx, documents, numThreads);
}
  
int HashIndex::unload() {
  if (_unique) {
    _uniqueArray->_hashArray->truncate(FreeElement);
  } else {
    _multiArray->_hashArray->truncate(FreeElement);
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief provides a size hint for the hash index
////////////////////////////////////////////////////////////////////////////////

int HashIndex::sizeHint(arangodb::Transaction* trx, size_t size) {
  if (_sparse) {
    // for sparse indexes, we assume that we will have less index entries
    // than if the index would be fully populated
    size /= 5;
  }

  if (_unique) {
    return _uniqueArray->_hashArray->resize(trx, size);
  }

  return _multiArray->_hashArray->resize(trx, size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given VelocyPack slices
////////////////////////////////////////////////////////////////////////////////

int HashIndex::lookup(arangodb::Transaction* trx,
                      VPackSlice key,
                      std::vector<TRI_doc_mptr_t*>& documents) const {
  if (key.isNone()) {
    return TRI_ERROR_NO_ERROR;
  }
  if (_unique) {
    TRI_index_element_t* found =
        _uniqueArray->_hashArray->findByKey(trx, &key);

    if (found != nullptr) {
      // unique hash index: maximum number is 1
      documents.emplace_back(found->document());
    }

    return TRI_ERROR_NO_ERROR;
  }

  // TODO: optimize this copying!
  std::vector<TRI_index_element_t*> results;
  try {
    _multiArray->_hashArray->lookupByKey(trx, &key, results);
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  try {
    for (size_t i = 0; i < results.size(); i++) {
      documents.emplace_back(results[i]->document());
    }
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  return TRI_ERROR_NO_ERROR;
}

int HashIndex::insertUnique(arangodb::Transaction* trx,
                            TRI_doc_mptr_t const* doc, bool isRollback) {
  std::vector<TRI_index_element_t*> elements;
  int res = fillElement(elements, doc);

  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& it : elements) {
      // free all elements to prevent leak
      FreeElement(it);
    }

    return res;
  }

  auto work =
      [this, trx](TRI_index_element_t* element, bool) -> int {
        TRI_IF_FAILURE("InsertHashIndex") { return TRI_ERROR_DEBUG; }
        return _uniqueArray->_hashArray->insert(trx, element);
      };

  size_t const n = elements.size();

  for (size_t i = 0; i < n; ++i) {
    auto hashElement = elements[i];
    res = work(hashElement, isRollback);

    if (res != TRI_ERROR_NO_ERROR) {
      for (size_t j = i; j < n; ++j) {
        // Free all elements that are not yet in the index
        FreeElement(elements[j]);
      }
      // Already indexed elements will be removed by the rollback
      break;
    }
  }
  return res;
}

int HashIndex::batchInsertUnique(
    arangodb::Transaction* trx,
    std::vector<TRI_doc_mptr_t const*> const* documents, size_t numThreads) {
  std::vector<TRI_index_element_t*> elements;
  elements.reserve(documents->size());

  for (auto& doc : *documents) {
    int res = fillElement(elements, doc);

    if (res != TRI_ERROR_NO_ERROR) {
      for (auto& it : elements) {
        // free all elements to prevent leak
        FreeElement(it);
      }
      return res;
    }
  }
  
  if (elements.empty()) {
    // no elements left to insert
    return TRI_ERROR_NO_ERROR;
  }

  int res = _uniqueArray->_hashArray->batchInsert(trx, &elements, numThreads);

  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& it : elements) {
      // free all elements to prevent leak
      FreeElement(it);
    }
  }

  return res;
}

int HashIndex::insertMulti(arangodb::Transaction* trx,
                           TRI_doc_mptr_t const* doc, bool isRollback) {
  std::vector<TRI_index_element_t*> elements;
  int res = fillElement(elements, doc);

  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& hashElement : elements) {
      FreeElement(hashElement);
    }
    return res;
  }

  auto work = [this, trx](TRI_index_element_t*& element, bool) {
    TRI_IF_FAILURE("InsertHashIndex") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_index_element_t* found =
        _multiArray->_hashArray->insert(trx, element, false, true);

    if (found != nullptr) {
      // already got the exact same index entry. now free our local element...
      FreeElement(element);
      // we're not responsible for this element anymore
      element = nullptr;
    }
  };

  size_t const n = elements.size();

  for (size_t i = 0; i < n; ++i) {
    auto hashElement = elements[i];

    try {
      work(hashElement, isRollback);
    } catch (arangodb::basics::Exception const& ex) {
      res = ex.code();
    } catch (...) {
      res = TRI_ERROR_OUT_OF_MEMORY;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      for (size_t j = i; j < n; ++j) {
        // Free all elements that are not yet in the index
        FreeElement(elements[j]);
      }
      for (size_t j = 0; j < i; ++j) {
        // Remove all already indexed elements and free them
        if (elements[j] != nullptr) {
          removeMultiElement(trx, elements[j], isRollback);
        }
      }

      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

int HashIndex::batchInsertMulti(
    arangodb::Transaction* trx,
    std::vector<TRI_doc_mptr_t const*> const* documents, size_t numThreads) {
  std::vector<TRI_index_element_t*> elements;
  elements.reserve(documents->size());

  for (auto& doc : *documents) {
    int res = fillElement(elements, doc);

    if (res != TRI_ERROR_NO_ERROR) {
      // Filling the elements failed for some reason. Assume loading as failed
      for (auto& el : elements) {
        // Free all elements that are not yet in the index
        FreeElement(el);
      }
      return res;
    }
  }

  if (elements.empty()) {
    // no elements left to insert
    return TRI_ERROR_NO_ERROR;
  }

  return _multiArray->_hashArray->batchInsert(trx, &elements, numThreads);
}

int HashIndex::removeUniqueElement(arangodb::Transaction* trx,
                                   TRI_index_element_t* element,
                                   bool isRollback) {
  TRI_IF_FAILURE("RemoveHashIndex") { return TRI_ERROR_DEBUG; }
  TRI_index_element_t* old = _uniqueArray->_hashArray->remove(trx, element);

  // this might happen when rolling back
  if (old == nullptr) {
    if (isRollback) {
      return TRI_ERROR_NO_ERROR;
    }
    return TRI_ERROR_INTERNAL;
  }

  FreeElement(old);

  return TRI_ERROR_NO_ERROR;
}

int HashIndex::removeUnique(arangodb::Transaction* trx,
                            TRI_doc_mptr_t const* doc, bool isRollback) {
  std::vector<TRI_index_element_t*> elements;
  int res = fillElement(elements, doc);

  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& hashElement : elements) {
      FreeElement(hashElement);
    }
    return res;
  }

  for (auto& hashElement : elements) {
    int result = removeUniqueElement(trx, hashElement, isRollback);

    // we may be looping through this multiple times, and if an error
    // occurs, we want to keep it
    if (result != TRI_ERROR_NO_ERROR) {
      res = result;
    }
    FreeElement(hashElement);
  }

  return res;
}

int HashIndex::removeMultiElement(arangodb::Transaction* trx,
                                  TRI_index_element_t* element,
                                  bool isRollback) {
  TRI_IF_FAILURE("RemoveHashIndex") { return TRI_ERROR_DEBUG; }

  TRI_index_element_t* old = _multiArray->_hashArray->remove(trx, element);

  if (old == nullptr) {
    // not found
    if (isRollback) {  // ignore in this case, because it can happen
      return TRI_ERROR_NO_ERROR;
    }
    return TRI_ERROR_INTERNAL;
  }
  FreeElement(old);

  return TRI_ERROR_NO_ERROR;
}

int HashIndex::removeMulti(arangodb::Transaction* trx,
                           TRI_doc_mptr_t const* doc, bool isRollback) {
  std::vector<TRI_index_element_t*> elements;
  int res = fillElement(elements, doc);

  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& hashElement : elements) {
      FreeElement(hashElement);
    }
  }

  for (auto& hashElement : elements) {
    int result = removeMultiElement(trx, hashElement, isRollback);

    // we may be looping through this multiple times, and if an error
    // occurs, we want to keep it
    if (result != TRI_ERROR_NO_ERROR) {
      res = result;
    }

    FreeElement(hashElement);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether the index supports the condition
////////////////////////////////////////////////////////////////////////////////

bool HashIndex::supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {

  SimpleAttributeEqualityMatcher matcher(_fields);
  return matcher.matchAll(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an IndexIterator for the given Condition
////////////////////////////////////////////////////////////////////////////////

IndexIterator* HashIndex::iteratorForCondition(
    arangodb::Transaction* trx, IndexIteratorContext*,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool) const {
  TRI_IF_FAILURE("HashIndex::noIterator")  {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  return new HashIndexIterator(trx, this, node, reference);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an IndexIterator for the given VelocyPackSlices
////////////////////////////////////////////////////////////////////////////////

IndexIterator* HashIndex::iteratorForSlice(arangodb::Transaction* trx,
                                           IndexIteratorContext*,
                                           VPackSlice const searchValues,
                                           bool) const {
  if (!searchValues.isArray()) {
    // Invalid searchValue
    return nullptr;
  }
  
  TransactionBuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());
  keys->add(searchValues);
  return new HashIndexIteratorVPack(trx, this, keys);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief specializes the condition for use with the index
////////////////////////////////////////////////////////////////////////////////

arangodb::aql::AstNode* HashIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {

  SimpleAttributeEqualityMatcher matcher(_fields);
  return matcher.specializeAll(this, node, reference);
}
