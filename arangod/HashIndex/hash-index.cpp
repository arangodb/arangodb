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
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "hash-index.h"

#include "Basics/logging.h"
#include "ShapedJson/shape-accessor.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/document-collection.h"
#include "VocBase/voc-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of paths of the index
////////////////////////////////////////////////////////////////////////////////

static inline size_t NumPaths (TRI_hash_index_t const* idx) {
  return idx->_paths._length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the memory needed for an index key entry
////////////////////////////////////////////////////////////////////////////////

static inline size_t KeyEntrySize (TRI_hash_index_t const* idx) {
  return NumPaths(idx) * sizeof(TRI_shaped_json_t);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the index search from hash index element
////////////////////////////////////////////////////////////////////////////////

template<typename T>
static int FillIndexSearchValueByHashIndexElement (TRI_hash_index_t* hashIndex,
                                                   TRI_index_search_value_t* key,
                                                   T* element) {
  key->_values = static_cast<TRI_shaped_json_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, KeyEntrySize(hashIndex), false));

  if (key->_values == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  char const* ptr = element->_document->getShapedJsonPtr();  // ONLY IN INDEX
  size_t const n = NumPaths(hashIndex);

  for (size_t i = 0;  i < n;  ++i) {
    key->_values[i]._sid         = element->_subObjects[i]._sid;
    key->_values[i]._data.length = element->_subObjects[i]._length;
    key->_values[i]._data.data   = const_cast<char*>(ptr + element->_subObjects[i]._offset);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates space for sub-objects in the hash index element
////////////////////////////////////////////////////////////////////////////////

template<typename T>
static int AllocateSubObjectsHashIndexElement (TRI_hash_index_t const* idx,
                                               T* element) {

  element->_subObjects = static_cast<TRI_shaped_sub_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, KeyEntrySize(idx), false));

  if (element->_subObjects == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
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
static int HashIndexHelper (TRI_hash_index_t const* hashIndex,
                            T* hashElement,
                            TRI_doc_mptr_t const* document) {
  TRI_shaper_t* shaper;                 // underlying shaper
  TRI_shaped_json_t shapedObject;       // the sub-object
  TRI_shaped_json_t shapedJson;         // the object behind document
  TRI_shaped_sub_t shapedSub;           // the relative sub-object

  shaper = hashIndex->base._collection->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME

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

  size_t const n = NumPaths(hashIndex);
  for (size_t j = 0;  j < n;  ++j) {
    TRI_shape_pid_t path = *((TRI_shape_pid_t*)(TRI_AtVector(&hashIndex->_paths, j)));

    // determine if document has that particular shape
    TRI_shape_access_t const* acc = TRI_FindAccessorVocShaper(shaper, shapedJson._sid, path);

    // field not part of the object
    if (acc == nullptr || acc->_resultSid == TRI_SHAPE_ILLEGAL) {
      shapedSub._sid    = TRI_LookupBasicSidShaper(TRI_SHAPE_NULL);
      shapedSub._length = 0;
      shapedSub._offset = 0;

      res = TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING;
    }

    // extract the field
    else {
      if (! TRI_ExecuteShapeAccessor(acc, &shapedJson, &shapedObject)) {
        // hashElement->fields: memory deallocated in the calling procedure
        return TRI_ERROR_INTERNAL;
      }

      if (shapedObject._sid == TRI_LookupBasicSidShaper(TRI_SHAPE_NULL)) {
        res = TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING;
      }

      shapedSub._sid    = shapedObject._sid;
      shapedSub._length = shapedObject._data.length;
      shapedSub._offset = static_cast<uint32_t>(((char const*) shapedObject._data.data) - ptr);
    }

    // store the json shaped sub-object -- this is what will be hashed
    hashElement->_subObjects[j] = shapedSub;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief index helper for hashing with allocation
////////////////////////////////////////////////////////////////////////////////

template<typename T>
static int HashIndexHelperAllocate (TRI_hash_index_t const* hashIndex,
                                    T* hashElement,
                                    TRI_doc_mptr_t const* document) {
  // .............................................................................
  // Allocate storage to shaped json objects stored as a simple list.  These
  // will be used for hashing.  Fill the json field list from the document.
  // .............................................................................

  int res = AllocateSubObjectsHashIndexElement<T>(hashIndex, hashElement);

  if (res != TRI_ERROR_NO_ERROR) {
    // out of memory
    return res;
  }

  res = HashIndexHelper<T>(hashIndex, hashElement, document);

  // .............................................................................
  // It may happen that the document does not have the necessary attributes to
  // have particpated within the hash index. If the index is unique, we do not
  // report an error to the calling procedure, but return a warning instead. If
  // the index is not unique, we ignore this error.
  // .............................................................................

  if (res == TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING && ! hashIndex->base._unique) {
    res = TRI_ERROR_NO_ERROR;
  }
  else if (res != TRI_ERROR_NO_ERROR) {
    FreeSubObjectsHashIndexElement<T>(hashElement);
  }

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                              HASH INDEX MANAGMENT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                             hash array management
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a data element into the hash array
///
/// Since we do not allow duplicates - we must compare using keys, rather than
/// documents.
////////////////////////////////////////////////////////////////////////////////

static int HashIndex_insert (TRI_hash_index_t* hashIndex,
                             TRI_hash_index_element_t* element,
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

  res = TRI_InsertKeyHashArray(&hashIndex->_hashArray, &key, element, false, isRollback);

  if (key._values != nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, key._values);
  }

  if (res == TRI_RESULT_KEY_EXISTS) {
    return TRI_set_errno(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the hash array part of the hash index
////////////////////////////////////////////////////////////////////////////////

static int HashIndex_remove (TRI_hash_index_t* hashIndex,
                             TRI_hash_index_element_t* element) {
  TRI_IF_FAILURE("RemoveHashIndex") {
    return TRI_ERROR_DEBUG;
  }

  int res = TRI_RemoveElementHashArray(&hashIndex->_hashArray, element);

  // this might happen when rolling back
  if (res == TRI_RESULT_ELEMENT_NOT_FOUND) {
    return TRI_ERROR_NO_ERROR;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates a key within the hash array part
////////////////////////////////////////////////////////////////////////////////

static TRI_index_result_t HashIndex_find (TRI_hash_index_t* hashIndex,
                                          TRI_index_search_value_t* key) {
  TRI_hash_index_element_t* result;
  TRI_index_result_t results;

  // .............................................................................
  // A find request means that a set of values for the "key" was sent. We need
  // to locate the hash array entry by key.
  // .............................................................................

  result = TRI_FindByKeyHashArray(&hashIndex->_hashArray, key);

  if (result != nullptr) {

    // unique hash index: maximum number is 1
    results._length    = 1;
    results._documents = static_cast<TRI_doc_mptr_t**>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, 1 * sizeof(TRI_doc_mptr_t*), false));

    if (results._documents == nullptr) {
      // no memory. prevent worst case by re-setting results length to 0
      results._length = 0;
      return results;
    }

    results._documents[0] = result->_document;
  }
  else {
    results._length    = 0;
    results._documents = nullptr;
  }

  return results;
}

// -----------------------------------------------------------------------------
// --SECTION--                                       multi hash array management
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a data element into the hash array
///
/// Since we do allow duplicates - we must compare using documents, rather than
/// keys.
////////////////////////////////////////////////////////////////////////////////

static int MultiHashIndex_insert (TRI_hash_index_t* hashIndex,
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

  res = TRI_InsertElementHashArrayMulti(&hashIndex->_hashArrayMulti, &key, element, isRollback);
  
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

int MultiHashIndex_remove (TRI_hash_index_t* hashIndex,
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

  res = TRI_RemoveElementHashArrayMulti(&hashIndex->_hashArrayMulti, &key, element);

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
////////////////////////////////////////////////////////////////////////////////

static TRI_index_result_t MultiHashIndex_find (TRI_hash_index_t* hashIndex,
                                               TRI_index_search_value_t* key) {
  TRI_index_result_t results;

  // .............................................................................
  // We can only use the LookupByKey method for non-unique hash indexes, since
  // we want more than one result returned!
  // .............................................................................

  TRI_vector_pointer_t result = TRI_LookupByKeyHashArrayMulti(&hashIndex->_hashArrayMulti, key);

  if (result._length == 0) {
    results._length    = 0;
    results._documents = nullptr;
  }
  else {
    results._length    = result._length;
    results._documents = static_cast<TRI_doc_mptr_t**>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, result._length * sizeof(TRI_doc_mptr_t*), false));

    if (results._documents == nullptr) {
      // no memory. prevent worst case by re-setting results length to 0
      TRI_DestroyVectorPointer(&result);
      results._length = 0;

      return results;
    }

    for (size_t j = 0;  j < result._length;  ++j) {
      results._documents[j] = ((TRI_doc_mptr_t*) result._buffer[j]);
    }
  }

  TRI_DestroyVectorPointer(&result);
  return results;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        HASH INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the memory used by the index
////////////////////////////////////////////////////////////////////////////////

size_t MemoryHashIndex (TRI_index_t const* idx) {
  TRI_hash_index_t const* hashIndex = (TRI_hash_index_t const*) idx;

  if (hashIndex->base._unique) {
    return static_cast<size_t>(KeyEntrySize(hashIndex) * hashIndex->_hashArray._nrUsed + 
                               TRI_MemoryUsageHashArray(&hashIndex->_hashArray));
  }
  else {
    return static_cast<size_t>(KeyEntrySize(hashIndex) * hashIndex->_hashArrayMulti._nrUsed + 
                               TRI_MemoryUsageHashArrayMulti(&hashIndex->_hashArrayMulti));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief describes a hash index as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonHashIndex (TRI_index_t const* idx) {
  // .............................................................................
  // Recast as a hash index
  // .............................................................................

  TRI_hash_index_t const* hashIndex = (TRI_hash_index_t const*) idx;
  TRI_document_collection_t* document = idx->_collection;

  // .............................................................................
  // Allocate sufficent memory for the field list
  // .............................................................................

  char const** fieldList = TRI_FieldListByPathList(document->getShaper(), &hashIndex->_paths);  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (fieldList == nullptr) {
    return nullptr;
  }

  // ..........................................................................
  // create json object and fill it
  // ..........................................................................

  TRI_json_t* json = TRI_JsonIndex(TRI_CORE_MEM_ZONE, idx);

  if (json == nullptr) {
    return nullptr;
  }

  TRI_json_t* fields = TRI_CreateListJson(TRI_CORE_MEM_ZONE);

  for (size_t j = 0; j < hashIndex->_paths._length; ++j) {
    TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, fieldList[j]));
  }

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "fields", fields);
  TRI_Free(TRI_CORE_MEM_ZONE, (void*) fieldList);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a a document to a hash index
////////////////////////////////////////////////////////////////////////////////

static int InsertHashIndex (TRI_index_t* idx,
                            TRI_doc_mptr_t const* document,
                            bool isRollback) {
  TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;

  int res;
  if (hashIndex->base._unique) {
    TRI_hash_index_element_t hashElement;
    res = HashIndexHelperAllocate<TRI_hash_index_element_t>(hashIndex, &hashElement, document);

    if (res == TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING) {
      return TRI_ERROR_NO_ERROR;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    res = HashIndex_insert(hashIndex, &hashElement, isRollback);
  }
  else {
    TRI_hash_index_element_multi_t hashElement;
    res = HashIndexHelperAllocate<TRI_hash_index_element_multi_t>(hashIndex, &hashElement, document);

    if (res == TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING) {
      return TRI_ERROR_NO_ERROR;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    res = MultiHashIndex_insert(hashIndex, &hashElement, isRollback);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a hash index
////////////////////////////////////////////////////////////////////////////////

static int RemoveHashIndex (TRI_index_t* idx,
                            TRI_doc_mptr_t const* document,
                            bool isRollback) {
  TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;
  int res;

  if (hashIndex->base._unique) {
    TRI_hash_index_element_t hashElement;
    res = HashIndexHelperAllocate<TRI_hash_index_element_t>(hashIndex, &hashElement, document);

    if (res == TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING) {
      return TRI_ERROR_NO_ERROR;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
    res = HashIndex_remove(hashIndex, &hashElement);
    FreeSubObjectsHashIndexElement<TRI_hash_index_element_t>(&hashElement);
  }
  else {
    TRI_hash_index_element_multi_t hashElement;
    res = HashIndexHelperAllocate<TRI_hash_index_element_multi_t>(hashIndex, &hashElement, document);

    if (res == TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING) {
      return TRI_ERROR_NO_ERROR;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
    res = MultiHashIndex_remove(hashIndex, &hashElement);
    FreeSubObjectsHashIndexElement<TRI_hash_index_element_multi_t>(&hashElement);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief provides a size hint for the hash index
////////////////////////////////////////////////////////////////////////////////

static int SizeHintHashIndex (TRI_index_t* idx,
                              size_t size) {
  TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;

  if (hashIndex->base._unique) {
    TRI_ResizeHashArray(&hashIndex->_hashArray, size);
  }
  else {
    TRI_ResizeHashArrayMulti(&hashIndex->_hashArrayMulti, size);
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a hash index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateHashIndex (TRI_document_collection_t* document,
                                  TRI_idx_iid_t iid,
                                  TRI_vector_pointer_t* fields,
                                  TRI_vector_t* paths,
                                  bool unique) {
  // ...........................................................................
  // Initialize the index and the callback functions
  // ...........................................................................

  TRI_hash_index_t* hashIndex = static_cast<TRI_hash_index_t*>(TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_hash_index_t), false));
  TRI_index_t* idx = &hashIndex->base;

  TRI_InitIndex(idx, iid, TRI_IDX_TYPE_HASH_INDEX, document, unique, false);

  idx->memory   = MemoryHashIndex;
  idx->json     = JsonHashIndex;
  idx->insert   = InsertHashIndex;
  idx->remove   = RemoveHashIndex;
  idx->sizeHint = SizeHintHashIndex;

  // ...........................................................................
  // Copy the contents of the path list vector into a new vector and store this
  // ...........................................................................

  TRI_CopyPathVector(&hashIndex->_paths, paths);

  TRI_InitVectorString(&idx->_fields, TRI_CORE_MEM_ZONE);
  TRI_CopyDataFromVectorPointerVectorString(TRI_CORE_MEM_ZONE, &idx->_fields, fields);

  int res;
  if (unique) {
    res = TRI_InitHashArray(&hashIndex->_hashArray,
                            hashIndex->_paths._length);
  }
  else {
    res = TRI_InitHashArrayMulti(&hashIndex->_hashArrayMulti,
                                 hashIndex->_paths._length);
  }

  // oops, out of memory?
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyVector(&hashIndex->_paths);
    TRI_DestroyVectorString(&idx->_fields);
    TRI_Free(TRI_CORE_MEM_ZONE, hashIndex);
    return nullptr;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyHashIndex (TRI_index_t* idx) {
  TRI_hash_index_t* hashIndex;

  hashIndex = (TRI_hash_index_t*) idx;
  if (hashIndex->base._unique) {
    TRI_DestroyHashArray(&hashIndex->_hashArray);
  }
  else {
    TRI_DestroyHashArrayMulti(&hashIndex->_hashArrayMulti);
  }

  TRI_DestroyVectorString(&idx->_fields);
  TRI_DestroyVector(&hashIndex->_paths);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeHashIndex (TRI_index_t* idx) {
  TRI_DestroyHashIndex(idx);
  TRI_Free(TRI_CORE_MEM_ZONE, idx);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given shaped json objects
////////////////////////////////////////////////////////////////////////////////

TRI_index_result_t TRI_LookupHashIndex (TRI_index_t* idx,
                                        TRI_index_search_value_t* searchValue) {
  TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;

  if (hashIndex->base._unique) {
    return HashIndex_find(hashIndex, searchValue);
  }
  else {
    return MultiHashIndex_find(hashIndex, searchValue);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
