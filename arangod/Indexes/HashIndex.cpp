////////////////////////////////////////////////////////////////////////////////
/// @brief hash index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HashIndex.h"
#include "VocBase/transaction.h"
#include "VocBase/VocShaper.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Frees an index element
////////////////////////////////////////////////////////////////////////////////

static void FreeElement(TRI_index_element_t* element) {
  TRI_index_element_t::free(element);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if two elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementElement (TRI_index_element_t const* left,
                                   TRI_index_element_t const* right) {
  return left->document() == right->document();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief given a key generates a hash integer
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKey (TRI_index_search_value_t const* key) {
  uint64_t hash = 0x0123456789abcdef;

  for (size_t j = 0;  j < key->_length;  ++j) {
    // ignore the sid for hashing
    hash = fasthash64(key->_values[j]._data.data, key->_values[j]._data.length, hash);
  }

  return hash;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if a key corresponds to an element
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyElement (TRI_index_search_value_t const* left,
                               TRI_index_element_t const* right) {
  TRI_ASSERT_EXPENSIVE(right->document() != nullptr);

  for (size_t j = 0;  j < left->_length; ++j) {
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

static bool IsEqualKeyElementHash (TRI_index_search_value_t const* left,
                                   uint64_t const hash, // Has been computed but is not used here
                                   TRI_index_element_t const* right) {
  return IsEqualKeyElement(left, right);
}

// -----------------------------------------------------------------------------
// --SECTION--                                      class HashIndex::UniqueArray
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the unique array
////////////////////////////////////////////////////////////////////////////////
          
HashIndex::UniqueArray::UniqueArray (TRI_HashArray_t* hashArray,
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
/// @brief destroy the unique array
////////////////////////////////////////////////////////////////////////////////
           
HashIndex::UniqueArray::~UniqueArray () {
  if (_hashArray != nullptr) {
    _hashArray->invokeOnAllElements(FreeElement);
  }

  delete _hashArray;
  delete _hashElement;
}

// -----------------------------------------------------------------------------
// --SECTION--                                       class HashIndex::MultiArray
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the multi array
////////////////////////////////////////////////////////////////////////////////

HashIndex::MultiArray::MultiArray (TRI_HashArrayMulti_t* hashArray, 
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
           
HashIndex::MultiArray::~MultiArray () {
  if (_hashArray != nullptr) {
    _hashArray->invokeOnAllElements(FreeElement);
  }

  delete _hashArray;
  delete _hashElement;
  delete _isEqualElElByKey;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   class HashIndex
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the index
////////////////////////////////////////////////////////////////////////////////

HashIndex::HashIndex (TRI_idx_iid_t iid,
                      TRI_document_collection_t* collection,
                      std::vector<std::vector<triagens::basics::AttributeName>> const& fields,
                      bool unique,
                      bool sparse) 
  : PathBasedIndex(iid, collection, fields, unique, sparse),
    _uniqueArray(nullptr) {

  uint32_t indexBuckets = 1;

  if (collection != nullptr) {
    // document is a nullptr in the coordinator case
    indexBuckets = collection->_info._indexBuckets;
  }
    
  std::unique_ptr<HashElementFunc> func(new HashElementFunc(_paths.size()));
  std::unique_ptr<IsEqualElementElementByKey> compare(new IsEqualElementElementByKey(_paths.size()));

  if (unique) {
    std::unique_ptr<TRI_HashArray_t> array(new TRI_HashArray_t(HashKey,
                                                               *(func.get()),
                                                               IsEqualKeyElementHash,
                                                               IsEqualElementElement,
                                                               *(compare.get()),
                                                               indexBuckets,
                                                               [] () -> std::string { return "unique hash-array"; }));

    _uniqueArray = new HashIndex::UniqueArray(array.get(), func.get(), compare.get());
    array.release();
  }
  else {
    _multiArray = nullptr;
      
    std::unique_ptr<TRI_HashArrayMulti_t> array(new TRI_HashArrayMulti_t(HashKey, 
                                                                         *(func.get()),
                                                                         IsEqualKeyElement,
                                                                         IsEqualElementElement,
                                                                         *(compare.get()),
                                                                         indexBuckets,
                                                                         64,
                                                                         [] () -> std::string { return "multi hash-array"; }));
      
    _multiArray = new HashIndex::MultiArray(array.get(), func.get(), compare.get());

    array.release();
  }
  compare.release();

  func.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the index
////////////////////////////////////////////////////////////////////////////////

HashIndex::~HashIndex () {
  if (_unique) {
    delete _uniqueArray;
  }
  else {
    delete _multiArray;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a selectivity estimate for the index
////////////////////////////////////////////////////////////////////////////////
        
double HashIndex::selectivityEstimate () const {
  if (_unique) {
    return 1.0; 
  }

  double estimate = _multiArray->_hashArray->selectivity();
  TRI_ASSERT(estimate >= 0.0 && estimate <= 1.00001); // floating-point tolerance 
  return estimate;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the index memory usage
////////////////////////////////////////////////////////////////////////////////
        
size_t HashIndex::memory () const {
  if (_unique) {
    return static_cast<size_t>(elementSize() * _uniqueArray->_hashArray->size() + 
                               _uniqueArray->_hashArray->memoryUsage());
  }

  return static_cast<size_t>(elementSize() * _multiArray->_hashArray->size() +
                             _multiArray->_hashArray->memoryUsage());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the index
////////////////////////////////////////////////////////////////////////////////

triagens::basics::Json HashIndex::toJson (TRI_memory_zone_t* zone,
                                          bool withFigures) const {
  auto json = Index::toJson(zone, withFigures);

  json("unique", triagens::basics::Json(zone, _unique))
      ("sparse", triagens::basics::Json(zone, _sparse));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the index figures
////////////////////////////////////////////////////////////////////////////////

triagens::basics::Json HashIndex::toJsonFigures (TRI_memory_zone_t* zone) const {
  triagens::basics::Json json(zone, triagens::basics::Json::Object);
  json("memory", triagens::basics::Json(static_cast<double>(memory())));

  if (_unique) {
    _uniqueArray->_hashArray->appendToJson(zone, json);
  }
  else {
    _multiArray->_hashArray->appendToJson(zone, json);
  }
  return json;
}
  
int HashIndex::insert (TRI_doc_mptr_t const* doc, 
                       bool isRollback) {
  if (_unique) {
    return insertUnique(doc, isRollback);
  }
  return insertMulti(doc, isRollback);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the hash array part of the hash index
////////////////////////////////////////////////////////////////////////////////
         
int HashIndex::remove (TRI_doc_mptr_t const* doc, 
                       bool isRollback) {

  if (_unique) {
    return removeUnique(doc, isRollback);
  }
  return removeMulti(doc, isRollback);
}


int HashIndex::batchInsert (std::vector<TRI_doc_mptr_t const*> const* documents, 
                            size_t numThreads) {
  if (_unique) {
    return batchInsertUnique(documents, numThreads);
  }
  return batchInsertMulti(documents, numThreads);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief provides a size hint for the hash index
////////////////////////////////////////////////////////////////////////////////
  
int HashIndex::sizeHint (size_t size) {
  if (_sparse) {
    // for sparse indexes, we assume that we will have less index entries
    // than if the index would be fully populated
    size /= 5;
  }

  if (_unique) {
    return _uniqueArray->_hashArray->resize(size);
  }
  else {
    return _multiArray->_hashArray->resize(size);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given shaped json objects
////////////////////////////////////////////////////////////////////////////////

int HashIndex::lookup (TRI_index_search_value_t* searchValue,
                       std::vector<TRI_doc_mptr_copy_t>& documents) const {

  if (_unique) {
    TRI_index_element_t* found = _uniqueArray->_hashArray->findByKey(searchValue);

    if (found != nullptr) {
      // unique hash index: maximum number is 1
      documents.emplace_back(*(found->document()));
    }
    return TRI_ERROR_NO_ERROR;
  }

  std::vector<TRI_index_element_t*>* results = nullptr;
  try {
    results = _multiArray->_hashArray->lookupByKey(searchValue);
  }
  catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  if (results != nullptr) {
    try {
      for (size_t i = 0; i < results->size(); i++) {
        documents.emplace_back(*((*results)[i]->document()));
      }
      delete results;
    }
    catch (...) {
      delete results;
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given shaped json objects
////////////////////////////////////////////////////////////////////////////////

int HashIndex::lookup (TRI_index_search_value_t* searchValue,
                       std::vector<TRI_doc_mptr_copy_t>& documents,
                       TRI_index_element_t*& next,
                       size_t batchSize) const {

  if (_unique) {
    next = nullptr;
    TRI_index_element_t* found = _uniqueArray->_hashArray->findByKey(searchValue);

    if (found != nullptr) {
      // unique hash index: maximum number is 1
      documents.emplace_back(*(found->document()));
    }
    return TRI_ERROR_NO_ERROR;
  }

  std::vector<TRI_index_element_t*>* results = nullptr;

  if (next == nullptr) {
    try {
      results = _multiArray->_hashArray->lookupByKey(searchValue, batchSize);
    }
    catch (...) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  }
  else {
    try {
      results = _multiArray->_hashArray->lookupByKeyContinue(next, batchSize);
    }
    catch (...) {
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
      }
      catch (...) {
        delete results;
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }
    else {
      next = nullptr;
    }
    delete results;
  }
  else {
    next = nullptr;
  }
  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

int HashIndex::insertUnique (TRI_doc_mptr_t const* doc,
                             bool isRollback) {

  std::vector<TRI_index_element_t*> elements;
  int res = fillElement(elements, doc);
  
  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& it : elements) {
      // free all elements to prevent leak
      FreeElement(it);
    }

    return res;
  }
  
  auto work = [this] (TRI_index_element_t* element, bool isRollback) -> int {
    TRI_IF_FAILURE("InsertHashIndex") {
      return TRI_ERROR_DEBUG;
    }
    return _uniqueArray->_hashArray->insert(element);
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

int HashIndex::batchInsertUnique (std::vector<TRI_doc_mptr_t const*> const* documents, 
                                  size_t numThreads) {
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

  int res = _uniqueArray->_hashArray->batchInsert(&elements, numThreads);

  if (res != TRI_ERROR_NO_ERROR) {
    // TODO check leaks
    for (auto& it : elements) {
      // free all elements to prevent leak
      FreeElement(it);
    }
  }

  return res;
}

int HashIndex::insertMulti (TRI_doc_mptr_t const* doc,
                            bool isRollback) {

  std::vector<TRI_index_element_t*> elements;
  int res = fillElement(elements, doc);
    
  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& hashElement : elements) {
      FreeElement(hashElement);
    }
    return res;
  }

  auto work = [this] (TRI_index_element_t* element, bool isRollback) -> int {
    TRI_IF_FAILURE("InsertHashIndex") {
      return TRI_ERROR_DEBUG;
    }
  
    TRI_index_element_t* found = _multiArray->_hashArray->insert(element, false, true);

    if (found != nullptr) {   // bad, can only happen if we are in a rollback
      if (isRollback) {       // in which case we silently ignore it
        return TRI_ERROR_NO_ERROR;
      }
      // This is TRI_RESULT_ELEMENT_EXISTS, but this should not happen:
      return TRI_ERROR_INTERNAL;
    }
    
    return TRI_ERROR_NO_ERROR;
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
      for (size_t j = 0; j < i; ++j) {
        // Remove all allready indexed elements and free them
        removeMultiElement(elements[j], isRollback);
      }
      return res;
    }
  }
  return res;
}

int HashIndex::batchInsertMulti (std::vector<TRI_doc_mptr_t const*> const* documents, 
                                 size_t numThreads) {

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
  return _multiArray->_hashArray->batchInsert(&elements, numThreads);
}

int HashIndex::removeUniqueElement (TRI_index_element_t* element, bool isRollback) {
  TRI_IF_FAILURE("RemoveHashIndex") {
    return TRI_ERROR_DEBUG;
  }
  TRI_index_element_t* old = _uniqueArray->_hashArray->remove(element);

  // this might happen when rolling back
  if (old == nullptr) {
    if (isRollback) {
      return TRI_ERROR_NO_ERROR;
    }
    else {
      return TRI_ERROR_INTERNAL;
    }
  }

  FreeElement(old);

  return TRI_ERROR_NO_ERROR;
}

int HashIndex::removeUnique (TRI_doc_mptr_t const* doc, bool isRollback) {
  std::vector<TRI_index_element_t*> elements;
  int res = fillElement(elements, doc);

  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& hashElement : elements) {
      FreeElement(hashElement);
    }
    return res;
  }

  for (auto& hashElement : elements) {
    res = removeUniqueElement(hashElement, isRollback);
    FreeElement(hashElement);
  }
  return res;
}

int HashIndex::removeMultiElement (TRI_index_element_t* element, bool isRollback) {
  TRI_IF_FAILURE("RemoveHashIndex") {
    return TRI_ERROR_DEBUG;
  }

  TRI_index_element_t* old = _multiArray->_hashArray->remove(element);
      
  if (old == nullptr) {
    // not found
    if (isRollback) {   // ignore in this case, because it can happen
      return TRI_ERROR_NO_ERROR;
    }
    else {
      return TRI_ERROR_INTERNAL;
    }
  }
  FreeElement(old);

  return TRI_ERROR_NO_ERROR;
}

int HashIndex::removeMulti (TRI_doc_mptr_t const* doc, bool isRollback) {
  std::vector<TRI_index_element_t*> elements;
  int res = fillElement(elements, doc);

  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& hashElement : elements) {
      FreeElement(hashElement);
    }
  }

  for (auto& hashElement : elements) {
    res = removeMultiElement(hashElement, isRollback);
    FreeElement(hashElement);
  }
                 
  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
