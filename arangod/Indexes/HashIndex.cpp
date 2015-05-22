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
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/voc-shaper.h"

struct TRI_hash_index_element_multi_s;

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the index search from hash index element
////////////////////////////////////////////////////////////////////////////////

template<typename T>
static int FillIndexSearchValueByHashIndexElement (HashIndex const* hashIndex,
                                                   TRI_index_search_value_t* key,
                                                   T const* element) {
  key->_values = static_cast<TRI_shaped_json_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, hashIndex->keyEntrySize(), false));

  if (key->_values == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  char const* ptr = element->_document->getShapedJsonPtr();  // ONLY IN INDEX
  size_t const n = hashIndex->paths().size();

  for (size_t i = 0;  i < n;  ++i) {
    auto sid = element->_subObjects[i]._sid;
    key->_values[i]._sid = sid;

    TRI_InspectShapedSub(&element->_subObjects[i], ptr, key->_values[i]);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees space for sub-objects in the hash index element
////////////////////////////////////////////////////////////////////////////////

template<typename T>
static void FreeSubObjectsHashIndexElement (T* element) {
  if (element->_subObjects != nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, element->_subObjects);
    element->_subObjects = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper for hashing
///
/// This function takes a document master pointer and creates a corresponding
/// hash index element. The index element contains the document master pointer
/// and a lists of offsets and sizes describing the parts of the documents to be
/// hashed and the shape identifier of each part.
////////////////////////////////////////////////////////////////////////////////

template<typename T>
static int HashIndexHelper (HashIndex const* hashIndex,
                            T* hashElement,
                            TRI_doc_mptr_t const* document) {
  TRI_shaped_json_t shapedJson;         // the object behind document

  TRI_shaper_t* shaper = hashIndex->collection()->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME
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
    TRI_shape_access_t const* acc = TRI_FindAccessorVocShaper(shaper, shapedJson._sid, path);

    // field not part of the object
    if (acc == nullptr || acc->_resultSid == TRI_SHAPE_ILLEGAL) {
      hashElement->_subObjects[j]._sid = BasicShapes::TRI_SHAPE_SID_NULL;

      res = TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING;

      if (sparse) {
        // no need to continue
        return res;
      }

      continue;
    }

    // extract the field
    TRI_shaped_json_t shapedObject; 
    if (! TRI_ExecuteShapeAccessor(acc, &shapedJson, &shapedObject)) {
      return TRI_ERROR_INTERNAL;
    }

    if (shapedObject._sid == BasicShapes::TRI_SHAPE_SID_NULL) {
      res = TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING;

      if (sparse) {
        // no need to continue
        return res;
      }
    }
 
    TRI_FillShapedSub(&hashElement->_subObjects[j], &shapedObject, ptr);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief index helper for hashing with allocation
////////////////////////////////////////////////////////////////////////////////

template<typename T>
static int HashIndexHelperAllocate (HashIndex const* hashIndex,
                                    T* hashElement,
                                    TRI_doc_mptr_t const* document) {
  // .............................................................................
  // Allocate storage to shaped json objects stored as a simple list.  These
  // will be used for hashing.  Fill the json field list from the document.
  // .............................................................................

  hashElement->_subObjects = static_cast<TRI_shaped_sub_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, hashIndex->keyEntrySize(), false));

  if (hashElement->_subObjects == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  int res = HashIndexHelper<T>(hashIndex, hashElement, document);
  
  if (res == TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING) {
    // if the document does not have the indexed attribute or has it, and it is null,
    // then in case of a sparse index we don't index this document
    if (! hashIndex->sparse()) {
      // in case of a non-sparse index, we index this document
      res = TRI_ERROR_NO_ERROR;
    }
    // and for a sparse index, we return TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a data element into the hash array
///
/// Since we do not allow duplicates - we must compare using keys, rather than
/// documents.
////////////////////////////////////////////////////////////////////////////////

static int HashIndex_insert (HashIndex* hashIndex,
                             TRI_hash_array_t* hashArray,
                             TRI_hash_index_element_t const* element,
                             bool isRollback) {
  TRI_IF_FAILURE("InsertHashIndex") {
    return TRI_ERROR_DEBUG;
  }

  TRI_index_search_value_t key;
  int res = FillIndexSearchValueByHashIndexElement<TRI_hash_index_element_t>(hashIndex, &key, element);

  if (res != TRI_ERROR_NO_ERROR) {
    // out of memory
    return res;
  }

  res = TRI_InsertKeyHashArray(hashArray, &key, element, isRollback);

  if (key._values != nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, key._values);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the hash array part of the hash index
////////////////////////////////////////////////////////////////////////////////

static int HashIndex_remove (TRI_hash_array_t* hashArray,
                             TRI_hash_index_element_t* element) {
  TRI_IF_FAILURE("RemoveHashIndex") {
    return TRI_ERROR_DEBUG;
  }

  int res = TRI_RemoveElementHashArray(hashArray, element);

  // this might happen when rolling back
  if (res == TRI_RESULT_ELEMENT_NOT_FOUND) {
    return TRI_ERROR_NO_ERROR;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a data element into the hash array
///
/// Since we do allow duplicates - we must compare using documents, rather than
/// keys.
////////////////////////////////////////////////////////////////////////////////

static int MultiHashIndex_insert (HashIndex* hashIndex,
                                  TRI_hash_array_multi_t* hashArrayMulti,
                                  TRI_hash_index_element_multi_t* element,
                                  bool isRollback) {
  TRI_IF_FAILURE("InsertHashIndex") {
    return TRI_ERROR_DEBUG;
  }

  TRI_index_search_value_t key;
  int res = FillIndexSearchValueByHashIndexElement<TRI_hash_index_element_multi_t>(hashIndex, &key, element);

  if (res != TRI_ERROR_NO_ERROR) {
    // out of memory
    return res;
  }

  res = TRI_InsertElementHashArrayMulti(hashArrayMulti, &key, element, isRollback);
  
  if (key._values != nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, key._values);
  }

  if (res == TRI_RESULT_ELEMENT_EXISTS) {
    return TRI_ERROR_INTERNAL;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the associative array
////////////////////////////////////////////////////////////////////////////////

static int MultiHashIndex_remove (HashIndex* hashIndex,
                                  TRI_hash_array_multi_t* hashArrayMulti,
                                  TRI_hash_index_element_multi_t* element) {
  TRI_IF_FAILURE("RemoveHashIndex") {
    return TRI_ERROR_DEBUG;
  }

  TRI_index_search_value_t key;
  int res = FillIndexSearchValueByHashIndexElement<TRI_hash_index_element_multi_t>(hashIndex, &key, element);

  if (res != TRI_ERROR_NO_ERROR) {
    // out of memory
    return res;
  }

  res = TRI_RemoveElementHashArrayMulti(hashArrayMulti, &key, element);

  if (key._values != nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, key._values);
  }

  if (res == TRI_RESULT_ELEMENT_NOT_FOUND) {
    return TRI_ERROR_INTERNAL;
  }

  return res;
}

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

  TRI_hash_index_element_t* result = TRI_FindByKeyHashArray(hashArray, key);

  if (result != nullptr) {
    // unique hash index: maximum number is 1
    TRI_PushBackVectorPointer(&results, result->_document);
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

  TRI_hash_index_element_t* found = TRI_FindByKeyHashArray(hashArray, key);

  if (found != nullptr) {
    // unique hash index: maximum number is 1
    result.emplace_back(*(found->_document));
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
                      std::vector<std::string> const& fields,
                      std::vector<TRI_shape_pid_t> const& paths,
                      bool unique,
                      bool sparse) 
  : Index(iid, fields),
    _collection(collection),
    _paths(paths),
    _unique(unique),
    _sparse(sparse) {

  if (unique) {
    _hashArray._table = nullptr;
    _hashArray._tablePtr = nullptr;

    if (TRI_InitHashArray(&_hashArray, paths.size()) != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }
  else {
    _hashArrayMulti._table = nullptr;
    _hashArrayMulti._tablePtr = nullptr;
    _hashArrayMulti._freelist = nullptr;

    if (TRI_InitHashArrayMulti(&_hashArrayMulti, paths.size()) != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }
}

HashIndex::~HashIndex () {
  if (_unique) {
    TRI_DestroyHashArray(&_hashArray);
  }
  else {
    TRI_DestroyHashArrayMulti(&_hashArrayMulti);
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

  return TRI_SelectivityHashArrayMulti(&_hashArrayMulti);
}
        
size_t HashIndex::memory () const {
  if (_unique) {
    return static_cast<size_t>(keyEntrySize() * _hashArray._nrUsed + 
                               TRI_MemoryUsageHashArray(&_hashArray));
  }

  return static_cast<size_t>(keyEntrySize() * _hashArrayMulti._nrUsed + 
                             TRI_MemoryUsageHashArrayMulti(&_hashArrayMulti));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the index
////////////////////////////////////////////////////////////////////////////////

triagens::basics::Json HashIndex::toJson (TRI_memory_zone_t* zone) const {
  auto json = Index::toJson(zone);

  triagens::basics::Json f(zone, triagens::basics::Json::Array, fields().size());

  for (auto const& field : fields()) {
    f.add(triagens::basics::Json(zone, field));
  }

  json("fields", f)
      ("unique", triagens::basics::Json(zone, _unique))
      ("sparse", triagens::basics::Json(zone, _sparse));

  return json;
}
  
int HashIndex::insert (TRI_doc_mptr_t const* doc, 
                       bool isRollback) {
  int res;

  if (_unique) {
    TRI_hash_index_element_t hashElement;
    res = HashIndexHelperAllocate<TRI_hash_index_element_t>(this, &hashElement, doc);

    if (res == TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_t>(&hashElement);
      return TRI_ERROR_NO_ERROR;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_t>(&hashElement);
      return res;
    }

    res = HashIndex_insert(this, &_hashArray, &hashElement, isRollback);

    if (res != TRI_ERROR_NO_ERROR) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_t>(&hashElement);
    }
  }
  else {
    TRI_hash_index_element_multi_t hashElement;
    res = HashIndexHelperAllocate<TRI_hash_index_element_multi_t>(this, &hashElement, doc);

    if (res == TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_multi_t>(&hashElement);
      return TRI_ERROR_NO_ERROR;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_multi_t>(&hashElement);
      return res;
    }

    res = MultiHashIndex_insert(this, &_hashArrayMulti, &hashElement, isRollback);
    if (res != TRI_ERROR_NO_ERROR) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_multi_t>(&hashElement);
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the hash array part of the hash index
////////////////////////////////////////////////////////////////////////////////
         
int HashIndex::remove (TRI_doc_mptr_t const* doc, 
                       bool) {
  int res;

  if (_unique) {
    TRI_hash_index_element_t hashElement;
    res = HashIndexHelperAllocate<TRI_hash_index_element_t>(this, &hashElement, doc);

    if (res == TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_t>(&hashElement);
      return TRI_ERROR_NO_ERROR;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_t>(&hashElement);
      return res;
    }
    res = HashIndex_remove(&_hashArray, &hashElement);
    FreeSubObjectsHashIndexElement<TRI_hash_index_element_t>(&hashElement);
  }
  else {
    TRI_hash_index_element_multi_t hashElement;
    res = HashIndexHelperAllocate<TRI_hash_index_element_multi_t>(this, &hashElement, doc);

    if (res == TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_multi_t>(&hashElement);
      return TRI_ERROR_NO_ERROR;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_multi_t>(&hashElement);
      return res;
    }
    res = MultiHashIndex_remove(this, &_hashArrayMulti, &hashElement);
    FreeSubObjectsHashIndexElement<TRI_hash_index_element_multi_t>(&hashElement);
  }

  return res;
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
    TRI_ResizeHashArray(&_hashArray, size);
  }
  else {
    TRI_ResizeHashArrayMulti(&_hashArrayMulti, size);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given shaped json objects
/// it is the callers responsibility to destroy the result
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t HashIndex::lookup (TRI_index_search_value_t* searchValue) const {
  if (_unique) {
    return HashIndex_find(&_hashArray, searchValue);
  }

  return TRI_LookupByKeyHashArrayMulti(&_hashArrayMulti, searchValue);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given shaped json objects
////////////////////////////////////////////////////////////////////////////////

int HashIndex::lookup (TRI_index_search_value_t* searchValue,
                       std::vector<TRI_doc_mptr_copy_t>& documents) const {

  if (_unique) {
    return HashIndex_find(&_hashArray, searchValue, documents);
  }

  return TRI_LookupByKeyHashArrayMulti(&_hashArrayMulti, searchValue, documents);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given shaped json objects
////////////////////////////////////////////////////////////////////////////////

int HashIndex::lookup (TRI_index_search_value_t* searchValue,
                       std::vector<TRI_doc_mptr_copy_t>& documents,
                       struct TRI_hash_index_element_multi_s*& next,
                       size_t batchSize) const {

  if (_unique) {
    next = nullptr;
    return HashIndex_find(&_hashArray, searchValue, documents);
  }

  return TRI_LookupByKeyHashArrayMulti(&_hashArrayMulti, searchValue, documents, next, batchSize);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
