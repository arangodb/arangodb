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
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "VocBase/transaction.h"
#include "VocBase/VocShaper.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief Frees an index element
////////////////////////////////////////////////////////////////////////////////

static void FreeElement(TRI_index_element_t* element) {
  TRI_index_element_t::freeElement(element);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if two elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementElement(void* userData,
                                  TRI_index_element_t const* left,
                                  TRI_index_element_t const* right) {
  return left->document() == right->document();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief given a key generates a hash integer
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKey(void* userData,
                        TRI_hash_index_search_value_t const* key) {
  uint64_t hash = 0x0123456789abcdef;

  for (size_t j = 0; j < key->_length; ++j) {
    // ignore the sid for hashing
    hash = fasthash64(key->_values[j]._data.data, key->_values[j]._data.length,
                      hash);
  }

  return hash;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if a key corresponds to an element
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyElement(void* userData,
                              TRI_hash_index_search_value_t const* left,
                              TRI_index_element_t const* right) {
  TRI_ASSERT_EXPENSIVE(right->document() != nullptr);

  for (size_t j = 0; j < left->_length; ++j) {
    TRI_shaped_json_t* leftJson = &left->_values[j];
    TRI_shaped_sub_t* rightSub = &right->subObjects()[j];

    if (leftJson->_sid != rightSub->_sid) {
      return false;
    }

    auto length = leftJson->_data.length;

    char const* rightData;
    size_t rightLength;
    TRI_InspectShapedSub(rightSub, right->document(), rightData, rightLength);

    if (length != rightLength) {
      return false;
    }

    if (length > 0 && memcmp(leftJson->_data.data, rightData, length) != 0) {
      return false;
    }
  }

  return true;
}

static bool IsEqualKeyElementHash(
    void* userData, TRI_hash_index_search_value_t const* left,
    uint64_t const hash,  // Has been computed but is not used here
    TRI_index_element_t const* right) {
  return IsEqualKeyElement(userData, left, right);
}

TRI_doc_mptr_t* HashIndexIterator::next() {
  while (true) {
    if (_posInBuffer >= _buffer.size()) {
      if (_position >= _keys.size()) {
        // we're at the end of the lookup values
        return nullptr;
      }

      // We have to refill the buffer
      _buffer.clear();
      _posInBuffer = 0;

      int res = _index->lookup(_trx, _keys[_position++], _buffer);

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

void HashIndexIterator::reset() {
  _buffer.clear();
  _position = 0;
  _posInBuffer = 0;
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
/// @brief create an index search value
////////////////////////////////////////////////////////////////////////////////

TRI_hash_index_search_value_t::TRI_hash_index_search_value_t()
    : _length(0), _values(nullptr) {}

TRI_hash_index_search_value_t::~TRI_hash_index_search_value_t() { destroy(); }

void TRI_hash_index_search_value_t::reserve(size_t n) {
  TRI_ASSERT(_values == nullptr);
  _values = static_cast<TRI_shaped_json_t*>(
      TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, n * sizeof(TRI_shaped_json_t), true));

  if (_values == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  _length = n;
}

void TRI_hash_index_search_value_t::destroy() {
  if (_values != nullptr) {
    for (size_t i = 0; i < _length; ++i) {
      TRI_DestroyShapedJson(TRI_UNKNOWN_MEM_ZONE, &_values[i]);
    }

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, _values);
    _values = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the index
////////////////////////////////////////////////////////////////////////////////

HashIndex::HashIndex(
    TRI_idx_iid_t iid, TRI_document_collection_t* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    bool unique, bool sparse)
    : PathBasedIndex(iid, collection, fields, unique, sparse, false),
      _uniqueArray(nullptr) {
  uint32_t indexBuckets = 1;

  if (collection != nullptr) {
    // document is a nullptr in the coordinator case
    indexBuckets = collection->_info.indexBuckets();
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

  if (_multiArray == nullptr) {
    // use hard-coded selectivity estimate in case of cluster coordinator
    return _selectivityEstimate;
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
/// @brief locates entries in the hash index given shaped json objects
////////////////////////////////////////////////////////////////////////////////

int HashIndex::lookup(arangodb::Transaction* trx,
                      TRI_hash_index_search_value_t* searchValue,
                      std::vector<TRI_doc_mptr_t*>& documents) const {
  if (_unique) {
    TRI_index_element_t* found =
        _uniqueArray->_hashArray->findByKey(trx, searchValue);

    if (found != nullptr) {
      // unique hash index: maximum number is 1
      documents.emplace_back(found->document());
    }

    return TRI_ERROR_NO_ERROR;
  }

  std::vector<TRI_index_element_t*>* results = nullptr;
  try {
    results = _multiArray->_hashArray->lookupByKey(trx, searchValue);
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  if (results != nullptr) {
    try {
      for (size_t i = 0; i < results->size(); i++) {
        documents.emplace_back((*results)[i]->document());
      }
      delete results;
    } catch (...) {
      delete results;
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given shaped json objects
////////////////////////////////////////////////////////////////////////////////

int HashIndex::lookup(arangodb::Transaction* trx,
                      TRI_hash_index_search_value_t* searchValue,
                      std::vector<TRI_doc_mptr_copy_t>& documents,
                      TRI_index_element_t*& next, size_t batchSize) const {
  if (_unique) {
    next = nullptr;
    TRI_index_element_t* found =
        _uniqueArray->_hashArray->findByKey(trx, searchValue);

    if (found != nullptr) {
      // unique hash index: maximum number is 1
      documents.emplace_back(*(found->document()));
    }
    return TRI_ERROR_NO_ERROR;
  }

  std::vector<TRI_index_element_t*>* results = nullptr;

  if (next == nullptr) {
    try {
      results =
          _multiArray->_hashArray->lookupByKey(trx, searchValue, batchSize);
    } catch (...) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  } else {
    try {
      results =
          _multiArray->_hashArray->lookupByKeyContinue(trx, next, batchSize);
    } catch (...) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  }

  if (results != nullptr) {
    if (results->size() > 0) {
      next = results->back();  // for continuation the next time
      try {
        for (size_t i = 0; i < results->size(); i++) {
          documents.emplace_back(*((*results)[i]->document()));
        }
      } catch (...) {
        delete results;
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    } else {
      next = nullptr;
    }
    delete results;
  } else {
    next = nullptr;
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
      [this, trx](TRI_index_element_t* element, bool isRollback) -> int {
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

  auto work = [this, trx](TRI_index_element_t*& element, bool isRollback) {
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
        // Remove all allready indexed elements and free them
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
  SimpleAttributeEqualityMatcher matcher(fields());
  return matcher.matchAll(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an IndexIterator for the given Condition
////////////////////////////////////////////////////////////////////////////////

IndexIterator* HashIndex::iteratorForCondition(
    arangodb::Transaction* trx, IndexIteratorContext* context,
    arangodb::aql::Ast* ast, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) const {
  TRI_ASSERT(node->type == aql::NODE_TYPE_OPERATOR_NARY_AND);

  SimpleAttributeEqualityMatcher matcher(fields());
  size_t const n = _fields.size();
  TRI_ASSERT(node->numMembers() == n);

  // initialize permutations
  std::vector<PermutationState> permutationStates;
  permutationStates.reserve(n);
  size_t maxPermutations = 1;

  std::pair<arangodb::aql::Variable const*,
            std::vector<arangodb::basics::AttributeName>> paramPair;

  for (size_t i = 0; i < n; ++i) {
    auto comp = node->getMemberUnchecked(i);
    auto attrNode = comp->getMember(0);
    auto valNode = comp->getMember(1);

    paramPair.first = nullptr;
    paramPair.second.clear();

    if (!attrNode->isAttributeAccessForVariable(paramPair) ||
        paramPair.first != reference) {
      attrNode = comp->getMember(1);
      valNode = comp->getMember(0);

      if (!attrNode->isAttributeAccessForVariable(paramPair) ||
          paramPair.first != reference) {
        return nullptr;
      }
    }

    size_t attributePosition = SIZE_MAX;
    for (size_t j = 0; j < _fields.size(); ++j) {
      if (arangodb::basics::AttributeName::isIdentical(
              _fields[j], paramPair.second, true)) {
        attributePosition = j;
        break;
      }
    }

    if (attributePosition == SIZE_MAX) {
      // index attribute not found in condition. this is a severe error
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    arangodb::aql::AstNodeType type = comp->type;

    if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
      permutationStates.emplace_back(
          PermutationState(type, valNode, attributePosition, 1));
      TRI_IF_FAILURE("HashIndex::permutationEQ") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    } else if (comp->type == aql::NODE_TYPE_OPERATOR_BINARY_IN) {
      if (isAttributeExpanded(attributePosition)) {
        type = aql::NODE_TYPE_OPERATOR_BINARY_EQ;
        permutationStates.emplace_back(
            PermutationState(type, valNode, attributePosition, 1));
        TRI_IF_FAILURE("HashIndex::permutationArrayIN") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
      } else {
        if (valNode->numMembers() == 0) {
          return nullptr;
        }
        permutationStates.emplace_back(PermutationState(
            type, valNode, attributePosition, valNode->numMembers()));
        TRI_IF_FAILURE("HashIndex::permutationIN") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        maxPermutations *= valNode->numMembers();
      }
    }
  }

  if (permutationStates.empty()) {
    // can only be caused by empty IN lists
    return nullptr;
  }

  std::vector<TRI_hash_index_search_value_t*> searchValues;
  searchValues.reserve(maxPermutations);

  try {
    // create all permutations
    auto shaper = _collection->getShaper();
    size_t current = 0;
    bool done = false;
    while (!done) {
      auto searchValue = std::make_unique<TRI_hash_index_search_value_t>();
      searchValue->reserve(n);

      bool valid = true;
      for (size_t i = 0; i < n; ++i) {
        auto& state = permutationStates[i];
        std::shared_ptr<VPackBuilder> valBuilder =
            state.getValue()->toVelocyPackValue();

        if (valBuilder == nullptr) {
          valid = false;
          break;
        }

        auto shaped =
            TRI_ShapedJsonVelocyPack(shaper, valBuilder->slice(), false);

        if (shaped == nullptr) {
          // no such shape exists. this means we won't find this value and can
          // go on with the next permutation
          valid = false;
          break;
        }

        searchValue->_values[state.attributePosition] = *shaped;
        TRI_Free(shaper->memoryZone(), shaped);
      }

      if (valid) {
        searchValues.push_back(searchValue.get());
        searchValue.release();
      }

      // now permute
      while (true) {
        if (++permutationStates[current].current <
            permutationStates[current].n) {
          current = 0;
          // abort inner iteration
          break;
        }

        permutationStates[current].current = 0;

        if (++current >= n) {
          done = true;
          break;
        }
        // next inner iteration
      }
    }

    TRI_ASSERT(searchValues.size() <= maxPermutations);

    // Create the iterator
    TRI_IF_FAILURE("HashIndex::noIterator") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

  } catch (...) {
    // prevent a leak here
    for (auto& it : searchValues) {
      delete it;
    }
    throw;
  }

  return new HashIndexIterator(trx, this, searchValues);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief specializes the condition for use with the index
////////////////////////////////////////////////////////////////////////////////

arangodb::aql::AstNode* HashIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  SimpleAttributeEqualityMatcher matcher(fields());
  return matcher.specializeAll(this, node, reference);
}
