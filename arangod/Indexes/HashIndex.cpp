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
/// @brief determines if two elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool isEqualElementElement (TRI_index_element_t const* left,
                                   TRI_index_element_t const* right) {
  return left->document() == right->document();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief given a key generates a hash integer
////////////////////////////////////////////////////////////////////////////////

static uint64_t hashKey (TRI_index_search_value_t const* key) {
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

static bool isEqualKeyElement (TRI_index_search_value_t const* left,
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

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the index search from hash index element
////////////////////////////////////////////////////////////////////////////////

static int FillIndexSearchValueByHashIndexElement (HashIndex const* hashIndex,
                                                   TRI_index_search_value_t* key,
                                                   TRI_index_element_t const* element) {
  key->_values = static_cast<TRI_shaped_json_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, hashIndex->keyEntrySize(), false));

  if (key->_values == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  char const* ptr = element->document()->getShapedJsonPtr();  // ONLY IN INDEX
  size_t const n = hashIndex->paths().size();

  for (size_t i = 0;  i < n;  ++i) {
    auto sid = element->subObjects()[i]._sid;
    key->_values[i]._sid = sid;

    TRI_InspectShapedSub(&element->subObjects()[i], ptr, key->_values[i]);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper for hashing
///
/// This function takes a document master pointer and creates a corresponding
/// hash index element. The index element contains the document master pointer
/// and a lists of offsets and sizes describing the parts of the documents to be
/// hashed and the shape identifier of each part.
////////////////////////////////////////////////////////////////////////////////

/*
<<<<<<< HEAD
    // TODO needs to be extracted as a helper function
    if ( triagens::basics::TRI_AttributeNamesHaveExpansion(hashIndex->fields()[j]) ) {
      TRI_shape_t const* shape = shaper->lookupShapeId(shapedObject._sid); 
      if (shape->_type >= TRI_SHAPE_LIST && shape->_type <= TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
        std::cout << "Should expand here" << std::endl;
        auto json = triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, TRI_JsonShapedJson(shaper, &shapedObject));
        std::cout << "Is Array " << json.isArray() << " :: " << json << std::endl;
=======
static int HashIndexHelper (HashIndex const* hashIndex,
                            TRI_hash_index_element_t* hashElement,
                            TRI_doc_mptr_t const* document) {
  TRI_shaped_json_t shapedJson;         // the object behind document

  auto shaper = hashIndex->collection()->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME
  bool const sparse = hashIndex->sparse();

  // .............................................................................
  // Assign the document to the TRI_hash_index_element_t structure - so that it
  // can later be retreived.
  // .............................................................................

  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, document->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

  hashElement->_document = const_cast<TRI_doc_mptr_t*>(document);
  char const* ptr = document->getShapedJsonPtr();  // ONLY IN INDEX

  // .............................................................................
  // Extract the attribute values
  // .............................................................................

  int res = TRI_ERROR_NO_ERROR;

  auto const& paths = hashIndex->paths();
  size_t const n = paths.size();

  for (size_t j = 0;  j < n;  ++j) {
    TRI_shape_pid_t path = paths[j];

    // determine if document has that particular shape
    TRI_shape_access_t const* acc = shaper->findAccessor(shapedJson._sid, path);

    // field not part of the object
    if (acc == nullptr || acc->_resultSid == TRI_SHAPE_ILLEGAL) {
      hashElement->_subObjects[j]._sid = BasicShapes::TRI_SHAPE_SID_NULL;

      res = TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING;

      if (sparse) {
        // no need to continue
        return res;
>>>>>>> origin/eimerung_hashindex
      }
    }
}
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief locates a key within the hash array part
/// it is the callers responsibility to destroy the result
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_pointer_t HashIndex_find (TRI_hash_array_t const* hashArray,
                                            TRI_index_search_value_t* key) {
  TRI_vector_pointer_t results;
  TRI_InitVectorPointer(&results, TRI_UNKNOWN_MEM_ZONE);

  // .............................................................................
  // A find request means that a set of values for the "key" was sent. We need
  // to locate the hash array entry by key.
  // .............................................................................

  TRI_index_element_t* result = TRI_FindByKeyHashArray(hashArray, key);

  if (result != nullptr) {
    // unique hash index: maximum number is 1
    TRI_PushBackVectorPointer(&results, result->document());
  }

  return results;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates a key within the hash array part
////////////////////////////////////////////////////////////////////////////////

static int HashIndex_find (TRI_hash_array_t const* hashArray,
                           TRI_index_search_value_t* key,
                           std::vector<TRI_doc_mptr_copy_t>& result) {

  // .............................................................................
  // A find request means that a set of values for the "key" was sent. We need
  // to locate the hash array entry by key.
  // .............................................................................

  TRI_index_element_t* found = TRI_FindByKeyHashArray(hashArray, key);

  if (found != nullptr) {
    // unique hash index: maximum number is 1
    result.emplace_back(*(found->document()));
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       class Index
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

HashIndex::HashIndex (TRI_idx_iid_t iid,
                      TRI_document_collection_t* collection,
                      std::vector<std::vector<triagens::basics::AttributeName>> const& fields,
                      bool unique,
                      bool sparse) 
  : Index(iid, collection, fields),
    _paths(fillPidPaths()),
    _unique(unique),
    _sparse(sparse) {

  TRI_ASSERT(! fields.empty());

  TRI_ASSERT(iid != 0);

  if (unique) {
    _hashArray._table = nullptr;

    if (TRI_InitHashArray(&_hashArray, _paths.size()) != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }
  else {
    uint32_t indexBuckets = 1;
    if (collection != nullptr) {
      // document is a nullptr in the coordinator case
      indexBuckets = collection->_info._indexBuckets;
    }
          
    _multi._hashArray = nullptr;
    _multi._isEqualElElByKey = nullptr;
    _multi._hashElement = nullptr;
    try {
      _multi._hashElement = new HashElementFunc(_paths.size());
      _multi._isEqualElElByKey = new IsEqualElementElementByKey(_paths.size());
      _multi._hashArray = new TRI_HashArrayMulti_t(hashKey, 
                                                 *_multi._hashElement,
                                                 isEqualKeyElement,
                                                 isEqualElementElement,
                                                 *_multi._isEqualElElByKey,
                                                 indexBuckets);
    }
    catch (...) {
      delete _multi._hashElement;
      _multi._hashElement = nullptr;
      delete _multi._isEqualElElByKey;
      _multi._isEqualElElByKey = nullptr;
      _multi._hashArray = nullptr;
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }
}

HashIndex::~HashIndex () {
  if (_unique) {
    TRI_DestroyHashArray(&_hashArray);
  }
  else {
    delete _multi._hashElement;
    delete _multi._isEqualElElByKey;
    delete _multi._hashArray;   // FIXME: should we free the pointers in there?
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

  double estimate = _multi._hashArray->selectivity();
  TRI_ASSERT(estimate >= 0.0 && estimate <= 1.00001); // floating-point tolerance 
  return estimate;
}
        
size_t HashIndex::memory () const {
  if (_unique) {
    return static_cast<size_t>(keyEntrySize() * _hashArray._nrUsed + 
                               TRI_MemoryUsageHashArray(&_hashArray));
  }

  return static_cast<size_t>(keyEntrySize() * _multi._hashArray->size() +
                             _multi._hashArray->memoryUsage());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the index
////////////////////////////////////////////////////////////////////////////////

triagens::basics::Json HashIndex::toJson (TRI_memory_zone_t* zone) const {
  auto json = Index::toJson(zone);

  json("unique", triagens::basics::Json(zone, _unique))
      ("sparse", triagens::basics::Json(zone, _sparse));

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
    return TRI_ResizeHashArray(this, &_hashArray, size);
  }
  else {
    return _multi._hashArray->resize(size);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given shaped json objects
/// it is the callers responsibility to destroy the result
////////////////////////////////////////////////////////////////////////////////

// FIXME: use std::vector here as well
TRI_vector_pointer_t HashIndex::lookup (TRI_index_search_value_t* searchValue) const {
  if (_unique) {
    return HashIndex_find(&_hashArray, searchValue);
  }

  std::vector<TRI_index_element_t*>* results 
      = _multi._hashArray->lookupByKey(searchValue);
  TRI_vector_pointer_t resultsvec;
  int res = TRI_InitVectorPointer(&resultsvec, TRI_UNKNOWN_MEM_ZONE, 
                                  results->size());
  if (res == TRI_ERROR_NO_ERROR) {
    for (size_t i = 0; i < results->size(); i++) {
      TRI_PushBackVectorPointer(&resultsvec, (*results)[i]->document());
    }
  }
  delete results;
  return resultsvec;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given shaped json objects
////////////////////////////////////////////////////////////////////////////////

int HashIndex::lookup (TRI_index_search_value_t* searchValue,
                       std::vector<TRI_doc_mptr_copy_t>& documents) const {

  if (_unique) {
    return HashIndex_find(&_hashArray, searchValue, documents);
  }

  std::vector<TRI_index_element_t*>* results = nullptr;
  try {
    results = _multi._hashArray->lookupByKey(searchValue);
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
    return HashIndex_find(&_hashArray, searchValue, documents);
  }

  std::vector<TRI_index_element_t*>* results = nullptr;

  if (next == nullptr) {
    try {
      results = _multi._hashArray->lookupByKey(searchValue, batchSize);
    }
    catch (...) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
  }
  else {
    try {
      results = _multi._hashArray->lookupByKeyContinue(next, batchSize);
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

  auto allocate = [this] () -> TRI_index_element_t* {
    return TRI_index_element_t::allocate(keyEntrySize(), false);
  };

  std::vector<TRI_index_element_t*> elements;
  int res = fillElement(allocate, elements, doc, paths(), sparse());
  
  auto work = [this] (TRI_index_element_t const* element, bool isRollback) -> int {
    TRI_IF_FAILURE("InsertHashIndex") {
      return TRI_ERROR_DEBUG;
    }

    TRI_index_search_value_t key;
    int res = FillIndexSearchValueByHashIndexElement(this, &key, element);

    if (res != TRI_ERROR_NO_ERROR) {
      // out of memory
      return res;
    }

    res = TRI_InsertKeyHashArray(this, &_hashArray, &key, element, isRollback);

    if (key._values != nullptr) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, key._values);
    }

    return res;
  };

  for (auto& hashElement : elements) {
    //TODO FIXME Multiple elements
    res = work(hashElement, isRollback);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_index_element_t::free(hashElement);
    }
  }
  return res;
}

int HashIndex::insertMulti (TRI_doc_mptr_t const* doc,
                            bool isRollback) {

  auto allocate = [this] () -> TRI_index_element_t* {
    return TRI_index_element_t::allocate(keyEntrySize(), false);
  };

  std::vector<TRI_index_element_t*> elements;
  int res = fillElement(allocate, elements, doc, paths(), sparse());

  auto work = [this] (TRI_index_element_t* element, bool isRollback) -> int {
    TRI_IF_FAILURE("InsertHashIndex") {
      return TRI_ERROR_DEBUG;
    }
  
    TRI_index_element_t* found = _multi._hashArray->insert(element,
                                                                false,
                                                                true);
    if (found != nullptr) {   // bad, can only happen if we are in a rollback
      if (isRollback) {       // in which case we silently ignore it
        return TRI_ERROR_NO_ERROR;
      }
      // This is TRI_RESULT_ELEMENT_EXISTS, but this should not happen:
      return TRI_ERROR_INTERNAL;
    }
    
    return TRI_ERROR_NO_ERROR;
  };
  
  for (auto& hashElement : elements) {
    //TODO FIXME Multiple elements
    res = work(hashElement, isRollback);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_index_element_t::free(hashElement);
    }
  }
  return res;
}

int HashIndex::removeUnique (TRI_doc_mptr_t const* doc, bool isRollback) {
  auto allocate = [this] () -> TRI_index_element_t* {
    return TRI_index_element_t::allocate(keyEntrySize(), false);
  };
  std::vector<TRI_index_element_t*> elements;
  int res = fillElement(allocate, elements, doc, paths(), sparse());

  if (res != TRI_ERROR_NO_ERROR) {
    for (auto& hashElement : elements) {
      TRI_index_element_t::free(hashElement);
    }
    return res;
  }

  auto work = [this] (TRI_index_element_t* element, bool isRollback) -> int {
    TRI_IF_FAILURE("RemoveHashIndex") {
      return TRI_ERROR_DEBUG;
    }

    int res = TRI_RemoveElementHashArray(this, &_hashArray, element);

    // this might happen when rolling back
    if (res == TRI_RESULT_ELEMENT_NOT_FOUND) {
      if (isRollback) {
        return TRI_ERROR_NO_ERROR;
      }
      else {
        return TRI_ERROR_INTERNAL;
      }
    }

    return res;
  };

  for (auto& hashElement : elements) {
    res = work(hashElement, isRollback);
    TRI_index_element_t::free(hashElement);
  }
  return res;
}

int HashIndex::removeMulti (TRI_doc_mptr_t const* doc, bool isRollback) {

  auto allocate = [this] () -> TRI_index_element_t* {
    return TRI_index_element_t::allocate(keyEntrySize(), false);
  };
  std::vector<TRI_index_element_t*> elements;
  int res = fillElement(allocate, elements, doc, paths(), sparse());

  auto work = [this] (TRI_index_element_t* element, bool isRollback) -> int {
    TRI_IF_FAILURE("RemoveHashIndex") {
      return TRI_ERROR_DEBUG;
    }

    TRI_index_element_t* old = _multi._hashArray->remove(element);
       
    if (old == nullptr) {
      // not found
      if (isRollback) {   // ignore in this case, because it can happen
        return TRI_ERROR_NO_ERROR;
      }
      else {
        return TRI_ERROR_INTERNAL;
      }
    }
    return TRI_ERROR_NO_ERROR;
  };

  for (auto& hashElement : elements) {
    res = work(hashElement, isRollback);
    TRI_index_element_t::free(hashElement);
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
