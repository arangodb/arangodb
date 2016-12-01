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
#include "Indexes/IndexLookupContext.h"
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

/// @brief determines if two elements are equal
static bool IsEqualElementElementUnique(void*,
                                        HashIndexElement const* left,
                                        HashIndexElement const* right) {
  // this is quite simple
  return left->revisionId() == right->revisionId();
}

/// @brief determines if two elements are equal
static bool IsEqualElementElementMulti(void* userData,
                                       HashIndexElement const* left,
                                       HashIndexElement const* right) {
  TRI_ASSERT(left != nullptr);
  TRI_ASSERT(right != nullptr);
 
  if (left->revisionId() != right->revisionId()) {
    return false;
  }
  if (left->hash() != right->hash()) {
    return false;
  }

  IndexLookupContext* context = static_cast<IndexLookupContext*>(userData);
  TRI_ASSERT(context != nullptr);
      
  for (size_t i = 0; i < context->numFields(); ++i) {
    VPackSlice leftData = left->slice(context, i);
    VPackSlice rightData = right->slice(context, i);

    int res = arangodb::basics::VelocyPackHelper::compare(leftData, rightData, false);

    if (res != 0) {
      return false;
    }
  }

  return true;
}

/// @brief given a key generates a hash integer
static uint64_t HashKey(void*, VPackSlice const* key) {
  return HashIndexElement::hash(*key);
}

/// @brief determines if a key corresponds to an element
static bool IsEqualKeyElementMulti(void* userData,
                                   VPackSlice const* left,
                                   HashIndexElement const* right) {
  TRI_ASSERT(left->isArray());
  TRI_ASSERT(right->revisionId() != 0);
  IndexLookupContext* context = static_cast<IndexLookupContext*>(userData);
  TRI_ASSERT(context != nullptr);

  // TODO: is it a performance improvement to compare the hash values first?
  size_t const n = left->length();
  
  for (size_t i = 0; i < n; ++i) {
    VPackSlice const leftVPack = left->at(i);
    VPackSlice const rightVPack = right->slice(context, i);
  
    int res = arangodb::basics::VelocyPackHelper::compare(leftVPack, rightVPack, false);

    if (res != 0) {
      return false;
    }
  }

  return true;
}

/// @brief determines if a key corresponds to an element
static bool IsEqualKeyElementUnique(void* userData, VPackSlice const* left,
                                    uint64_t, HashIndexElement const* right) {
  return IsEqualKeyElementMulti(userData, left, right);
}

HashIndexIterator::HashIndexIterator(LogicalCollection* collection,
                                     arangodb::Transaction* trx,
                                     ManagedDocumentResult* mmdr,
                                     HashIndex const* index,
                                     arangodb::aql::AstNode const* node,
                                     arangodb::aql::Variable const* reference)
    : IndexIterator(collection, trx, mmdr, index),
      _index(index),
      _lookups(trx, node, reference, index->fields()),
      _buffer(),
      _posInBuffer(0) {
    _index->lookup(_trx, _lookups.lookup(), _buffer);
}

IndexLookupResult HashIndexIterator::next() {
  while (true) {
    if (_posInBuffer >= _buffer.size()) {
      if (!_lookups.hasAndGetNext()) {
        // we're at the end of the lookup values
        return IndexLookupResult();
      }

      // We have to refill the buffer
      _buffer.clear();
      _posInBuffer = 0;

      _index->lookup(_trx, _lookups.lookup(), _buffer);
    }

    if (!_buffer.empty()) {
      // found something
      return IndexLookupResult(_buffer[_posInBuffer++]->revisionId());
    }
  }
}

void HashIndexIterator::nextBabies(std::vector<IndexLookupResult>& result, size_t atMost) {
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
        result.emplace_back(_buffer[i]->revisionId());
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
  
HashIndexIteratorVPack::HashIndexIteratorVPack(LogicalCollection* collection,
                                               arangodb::Transaction* trx, 
                                               ManagedDocumentResult* mmdr,
                                               HashIndex const* index,
                                               std::unique_ptr<arangodb::velocypack::Builder>& searchValues)
    : IndexIterator(collection, trx, mmdr, index),
      _index(index),
      _searchValues(searchValues.get()),
      _iterator(_searchValues->slice()),
      _buffer(),
      _posInBuffer(0) {
  searchValues.release(); // now we have ownership for searchValues
}

HashIndexIteratorVPack::~HashIndexIteratorVPack() {
  if (_searchValues != nullptr) {
    // return the VPackBuilder to the transaction context 
    _trx->transactionContextPtr()->returnBuilder(_searchValues.release());
  }
}

IndexLookupResult HashIndexIteratorVPack::next() {
  while (true) {
    if (_posInBuffer >= _buffer.size()) {
      if (!_iterator.valid()) {
        // we're at the end of the lookup values
        return IndexLookupResult();
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
      return IndexLookupResult(_buffer[_posInBuffer++]->revisionId());
    }
  }
}

void HashIndexIteratorVPack::reset() {
  _buffer.clear();
  _posInBuffer = 0;
  _iterator.reset();
}

/// @brief create the unique array
HashIndex::UniqueArray::UniqueArray(
    size_t numPaths,
    TRI_HashArray_t* hashArray, HashElementFunc* hashElement,
    IsEqualElementElementByKey* isEqualElElByKey)
    : _hashArray(hashArray),
      _hashElement(hashElement),
      _isEqualElElByKey(isEqualElElByKey),
      _numPaths(numPaths) {
  TRI_ASSERT(_hashArray != nullptr);
  TRI_ASSERT(_hashElement != nullptr);
  TRI_ASSERT(_isEqualElElByKey != nullptr);
}

/// @brief destroy the unique array
HashIndex::UniqueArray::~UniqueArray() {
  if (_hashArray != nullptr) {
    auto cb = [this](HashIndexElement* element) -> bool { 
      element->free(); return true; 
    };
    _hashArray->invokeOnAllElements(cb);
  }

  delete _hashArray;
  delete _hashElement;
  delete _isEqualElElByKey;
}

/// @brief create the multi array
HashIndex::MultiArray::MultiArray(size_t numPaths,
                                  TRI_HashArrayMulti_t* hashArray,
                                  HashElementFunc* hashElement,
                                  IsEqualElementElementByKey* isEqualElElByKey)
    : _hashArray(hashArray),
      _hashElement(hashElement),
      _isEqualElElByKey(isEqualElElByKey),
      _numPaths(numPaths) {
  TRI_ASSERT(_hashArray != nullptr);
  TRI_ASSERT(_hashElement != nullptr);
  TRI_ASSERT(_isEqualElElByKey != nullptr);
}

/// @brief destroy the multi array
HashIndex::MultiArray::~MultiArray() {
  if (_hashArray != nullptr) {
    auto cb = [this](HashIndexElement* element) -> bool { 
      element->free(); return true; 
    };
    _hashArray->invokeOnAllElements(cb);
  }

  delete _hashArray;
  delete _hashElement;
  delete _isEqualElElByKey;
}

HashIndex::HashIndex(TRI_idx_iid_t iid, LogicalCollection* collection,
                     VPackSlice const& info)
    : PathBasedIndex(iid, collection, info, false), _uniqueArray(nullptr) {
  uint32_t indexBuckets = 1;

  if (collection != nullptr) {
    indexBuckets = collection->indexBuckets();
  }

  auto func = std::make_unique<HashElementFunc>();
  auto compare = std::make_unique<IsEqualElementElementByKey>(_paths.size(), _useExpansion);

  if (_unique) {
    auto array = std::make_unique<TRI_HashArray_t>(
        HashKey, *(func.get()), IsEqualKeyElementUnique, IsEqualElementElementUnique,
        *(compare.get()), indexBuckets,
        [this]() -> std::string { return this->context(); });

    _uniqueArray =
        new HashIndex::UniqueArray(numPaths(), array.get(), func.get(), compare.get());
    array.release();
  } else {
    _multiArray = nullptr;

    auto array = std::make_unique<TRI_HashArrayMulti_t>(
        HashKey, *(func.get()), IsEqualKeyElementMulti, IsEqualElementElementMulti,
        *(compare.get()), indexBuckets, 64,
        [this]() -> std::string { return this->context(); });

    _multiArray =
        new HashIndex::MultiArray(numPaths(), array.get(), func.get(), compare.get());

    array.release();
  }
  compare.release();

  func.release();
}

/// @brief destroys the index
HashIndex::~HashIndex() {
  if (_unique) {
    delete _uniqueArray;
  } else {
    delete _multiArray;
  }
}

/// @brief returns a selectivity estimate for the index
double HashIndex::selectivityEstimate(StringRef const*) const {
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

/// @brief returns the index memory usage
size_t HashIndex::memory() const {
  size_t elementSize = HashIndexElement::baseMemoryUsage(_paths.size());

  if (_unique) {
    return static_cast<size_t>(elementSize *
                                   _uniqueArray->_hashArray->size() +
                               _uniqueArray->_hashArray->memoryUsage());
  }
  return static_cast<size_t>(elementSize * _multiArray->_hashArray->size() +
                             _multiArray->_hashArray->memoryUsage());
}

/// @brief return a velocypack representation of the index
void HashIndex::toVelocyPack(VPackBuilder& builder, bool withFigures) const {
  Index::toVelocyPack(builder, withFigures);
  builder.add("unique", VPackValue(_unique));
  builder.add("sparse", VPackValue(_sparse));
}

/// @brief return a velocypack representation of the index figures
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

int HashIndex::insert(arangodb::Transaction* trx, TRI_voc_rid_t revisionId,
                      VPackSlice const& doc, bool isRollback) {
  if (_unique) {
    return insertUnique(trx, revisionId, doc, isRollback);
  }

  return insertMulti(trx, revisionId, doc, isRollback);
}

/// @brief removes an entry from the hash array part of the hash index
int HashIndex::remove(arangodb::Transaction* trx, TRI_voc_rid_t revisionId,
                      VPackSlice const& doc, bool isRollback) {
  std::vector<HashIndexElement*> elements;
  int res = fillElement<HashIndexElement>(elements, revisionId, doc);

  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& hashElement : elements) {
      hashElement->free();
    }
    return res;
  }

  for (auto& hashElement : elements) {
    int result;
    if (_unique) {
      result = removeUniqueElement(trx, hashElement, isRollback);
    } else {
      result = removeMultiElement(trx, hashElement, isRollback);
    }

    // we may be looping through this multiple times, and if an error
    // occurs, we want to keep it
    if (result != TRI_ERROR_NO_ERROR) {
      res = result;
    }
    hashElement->free();
  }

  return res;
}

int HashIndex::batchInsert(arangodb::Transaction* trx,
                           std::vector<std::pair<TRI_voc_rid_t, VPackSlice>> const& documents,
                           size_t numThreads) {
  if (_unique) {
    return batchInsertUnique(trx, documents, numThreads);
  }

  return batchInsertMulti(trx, documents, numThreads);
}
  
int HashIndex::unload() {
  if (_unique) {
    _uniqueArray->_hashArray->truncate([](HashIndexElement* element) -> bool { element->free(); return true; });
  } else {
    _multiArray->_hashArray->truncate([](HashIndexElement* element) -> bool { element->free(); return true; });
  }
  return TRI_ERROR_NO_ERROR;
}

/// @brief provides a size hint for the hash index
int HashIndex::sizeHint(arangodb::Transaction* trx, size_t size) {
  if (_sparse) {
    // for sparse indexes, we assume that we will have less index entries
    // than if the index would be fully populated
    size /= 5;
  }
 
  ManagedDocumentResult result(trx); 
  IndexLookupContext context(trx, _collection, &result, numPaths());

  if (_unique) {
    return _uniqueArray->_hashArray->resize(&context, size);
  }

  return _multiArray->_hashArray->resize(&context, size);
}

/// @brief locates entries in the hash index given VelocyPack slices
int HashIndex::lookup(arangodb::Transaction* trx,
                      VPackSlice key,
                      std::vector<HashIndexElement*>& documents) const {
  if (key.isNone()) {
    return TRI_ERROR_NO_ERROR;
  }

  ManagedDocumentResult result(trx); 
  IndexLookupContext context(trx, _collection, &result, numPaths()); 

  if (_unique) {
    HashIndexElement* found =
        _uniqueArray->_hashArray->findByKey(&context, &key);

    if (found != nullptr) {
      // unique hash index: maximum number is 1
      documents.emplace_back(found);
    }

    return TRI_ERROR_NO_ERROR;
  }

  documents.clear();
  try {
    _multiArray->_hashArray->lookupByKey(&context, &key, documents);
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  return TRI_ERROR_NO_ERROR;
}

int HashIndex::insertUnique(arangodb::Transaction* trx, TRI_voc_rid_t revisionId, 
                            VPackSlice const& doc, bool isRollback) {
  std::vector<HashIndexElement*> elements;
  int res = fillElement<HashIndexElement>(elements, revisionId, doc);

  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& it : elements) {
      // free all elements to prevent leak
      it->free();
    }

    return res;
  }
      
  ManagedDocumentResult result(trx); 
  IndexLookupContext context(trx, _collection, &result, numPaths()); 

  auto work =
      [this, &context](HashIndexElement* element, bool) -> int {
        TRI_IF_FAILURE("InsertHashIndex") { return TRI_ERROR_DEBUG; }
        return _uniqueArray->_hashArray->insert(&context, element);
      };

  size_t const n = elements.size();

  for (size_t i = 0; i < n; ++i) {
    auto hashElement = elements[i];
    res = work(hashElement, isRollback);

    if (res != TRI_ERROR_NO_ERROR) {
      for (size_t j = i; j < n; ++j) {
        // Free all elements that are not yet in the index
        elements[j]->free();
      }
      // Already indexed elements will be removed by the rollback
      break;
    }
  }

  return res;
}

int HashIndex::batchInsertUnique(arangodb::Transaction* trx, 
     std::vector<std::pair<TRI_voc_rid_t, VPackSlice>> const& documents, size_t numThreads) {
  std::vector<HashIndexElement*> elements;
  elements.reserve(documents.size());

  for (auto& doc : documents) {
    int res = fillElement<HashIndexElement>(elements, doc.first, doc.second);

    if (res != TRI_ERROR_NO_ERROR) {
      for (auto& it : elements) {
        // free all elements to prevent leak
        it->free();
      }
      return res;
    }
  }
  
  if (elements.empty()) {
    // no elements left to insert
    return TRI_ERROR_NO_ERROR;
  }
  
  // functions that will be called for each thread
  auto creator = [&trx, this]() -> void* {
    ManagedDocumentResult* result = new ManagedDocumentResult(trx);
    return new IndexLookupContext(trx, _collection, result, numPaths());
  };
  auto destroyer = [](void* userData) {
    IndexLookupContext* context = static_cast<IndexLookupContext*>(userData);
    delete context->result();
    delete context;
  };
  
  int res = _uniqueArray->_hashArray->batchInsert(creator, destroyer, &elements, numThreads);

  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& it : elements) {
      // free all elements to prevent leak
      it->free();
    }
  } 

  return res;
}

int HashIndex::insertMulti(arangodb::Transaction* trx, TRI_voc_rid_t revisionId,
                           VPackSlice const& doc, bool isRollback) {
  std::vector<HashIndexElement*> elements;
  int res = fillElement<HashIndexElement>(elements, revisionId, doc);

  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& hashElement : elements) {
      hashElement->free();
    }
    return res;
  }
  
  ManagedDocumentResult result(trx); 
  IndexLookupContext context(trx, _collection, &result, numPaths()); 

  auto work = [this, &context](HashIndexElement*& element, bool) {
    TRI_IF_FAILURE("InsertHashIndex") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    HashIndexElement* found =
        _multiArray->_hashArray->insert(&context, element, false, true);

    if (found != nullptr) {
      // already got the exact same index entry. now free our local element...
      element->free();
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
        elements[j]->free();
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

int HashIndex::batchInsertMulti(arangodb::Transaction* trx, 
        std::vector<std::pair<TRI_voc_rid_t, VPackSlice>> const& documents, size_t numThreads) {
  std::vector<HashIndexElement*> elements;
  elements.reserve(documents.size());

  for (auto& doc : documents) {
    int res = fillElement<HashIndexElement>(elements, doc.first, doc.second);

    if (res != TRI_ERROR_NO_ERROR) {
      // Filling the elements failed for some reason. Assume loading as failed
      for (auto& el : elements) {
        // Free all elements that are not yet in the index
        el->free();
      }
      return res;
    }
  }

  if (elements.empty()) {
    // no elements left to insert
    return TRI_ERROR_NO_ERROR;
  }
  
  // functions that will be called for each thread
  auto creator = [&trx, this]() -> void* {
    ManagedDocumentResult* result = new ManagedDocumentResult(trx);
    return new IndexLookupContext(trx, _collection, result, numPaths());
  };
  auto destroyer = [](void* userData) {
    IndexLookupContext* context = static_cast<IndexLookupContext*>(userData);
    delete context->result();
    delete context;
  };

  return _multiArray->_hashArray->batchInsert(creator, destroyer, &elements, numThreads);
}

int HashIndex::removeUniqueElement(arangodb::Transaction* trx,
                                   HashIndexElement* element,
                                   bool isRollback) {
  TRI_IF_FAILURE("RemoveHashIndex") { return TRI_ERROR_DEBUG; }
  ManagedDocumentResult result(trx); 
  IndexLookupContext context(trx, _collection, &result, numPaths()); 
  HashIndexElement* old = _uniqueArray->_hashArray->remove(&context, element);

  if (old == nullptr) {
    // not found
    if (isRollback) {  // ignore in this case, because it can happen
      return TRI_ERROR_NO_ERROR;
    }
    return TRI_ERROR_INTERNAL;
  }
  old->free();

  return TRI_ERROR_NO_ERROR;
}

int HashIndex::removeMultiElement(arangodb::Transaction* trx,
                                  HashIndexElement* element,
                                  bool isRollback) {
  TRI_IF_FAILURE("RemoveHashIndex") { return TRI_ERROR_DEBUG; }
  ManagedDocumentResult result(trx); 
  IndexLookupContext context(trx, _collection, &result, numPaths()); 
  HashIndexElement* old = _multiArray->_hashArray->remove(&context, element);

  if (old == nullptr) {
    // not found
    if (isRollback) {  // ignore in this case, because it can happen
      return TRI_ERROR_NO_ERROR;
    }
    return TRI_ERROR_INTERNAL;
  }
  old->free();

  return TRI_ERROR_NO_ERROR;
}

/// @brief checks whether the index supports the condition
bool HashIndex::supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {

  SimpleAttributeEqualityMatcher matcher(_fields);
  return matcher.matchAll(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* HashIndex::iteratorForCondition(
    arangodb::Transaction* trx,
    ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool) const {
  TRI_IF_FAILURE("HashIndex::noIterator")  {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  return new HashIndexIterator(_collection, trx, mmdr, this, node, reference);
}

/// @brief creates an IndexIterator for the given VelocyPackSlices
IndexIterator* HashIndex::iteratorForSlice(arangodb::Transaction* trx,
                                           ManagedDocumentResult* mmdr,
                                           VPackSlice const searchValues,
                                           bool) const {
  if (!searchValues.isArray()) {
    // Invalid searchValue
    return nullptr;
  }
  
  TransactionBuilderLeaser builder(trx);
  std::unique_ptr<VPackBuilder> keys(builder.steal());
  keys->add(searchValues);
  return new HashIndexIteratorVPack(_collection, trx, mmdr, this, keys);
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* HashIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {

  SimpleAttributeEqualityMatcher matcher(_fields);
  return matcher.specializeAll(this, node, reference);
}
