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
                         
struct TRI_hash_index_element_multi_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of paths of the index
////////////////////////////////////////////////////////////////////////////////

static inline size_t NumPaths (TRI_hash_index_t const* idx) {
  return TRI_LengthVector(&idx->_paths);
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
                                                   T const* element) {
  key->_values = static_cast<TRI_shaped_json_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, KeyEntrySize(hashIndex), false));

  if (key->_values == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  char const* ptr = element->_document->getShapedJsonPtr();  // ONLY IN INDEX
  size_t const n = NumPaths(hashIndex);

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
static int HashIndexHelper (TRI_hash_index_t const* hashIndex,
                            T* hashElement,
                            TRI_doc_mptr_t const* document) {
  TRI_shaped_json_t shapedJson;         // the object behind document

  TRI_shaper_t* shaper = hashIndex->base._collection->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME
  bool const sparse = hashIndex->base._sparse;

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
static int HashIndexHelperAllocate (TRI_hash_index_t const* hashIndex,
                                    T* hashElement,
                                    TRI_doc_mptr_t const* document) {
  // .............................................................................
  // Allocate storage to shaped json objects stored as a simple list.  These
  // will be used for hashing.  Fill the json field list from the document.
  // .............................................................................

  hashElement->_subObjects = static_cast<TRI_shaped_sub_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, KeyEntrySize(hashIndex), false));

  if (hashElement->_subObjects == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  int res = HashIndexHelper<T>(hashIndex, hashElement, document);
  
  if (res == TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING) {
    // if the document does not have the indexed attribute or has it, and it is null,
    // then in case of a sparse index we don't index this document
    if (! hashIndex->base._sparse) {
      // in case of a non-sparse index, we index this document
      res = TRI_ERROR_NO_ERROR;
    }
    // and for a sparse index, we return TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING
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

  res = TRI_InsertKeyHashArray(&hashIndex->_hashArray, &key, element, isRollback);

  if (key._values != nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, key._values);
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
/// it is the callers responsibility to destroy the result
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_pointer_t HashIndex_find (TRI_hash_index_t* hashIndex,
                                            TRI_index_search_value_t* key) {
  TRI_vector_pointer_t results;
  TRI_InitVectorPointer(&results, TRI_UNKNOWN_MEM_ZONE);

  // .............................................................................
  // A find request means that a set of values for the "key" was sent. We need
  // to locate the hash array entry by key.
  // .............................................................................

  TRI_hash_index_element_t* result = TRI_FindByKeyHashArray(&hashIndex->_hashArray, key);

  if (result != nullptr) {
    // unique hash index: maximum number is 1
    TRI_PushBackVectorPointer(&results, result->_document);
  }

  return results;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates a key within the hash array part
////////////////////////////////////////////////////////////////////////////////

static int HashIndex_find (TRI_hash_index_t* hashIndex,
                           TRI_index_search_value_t* key,
                           std::vector<TRI_doc_mptr_copy_t>& result) {

  // .............................................................................
  // A find request means that a set of values for the "key" was sent. We need
  // to locate the hash array entry by key.
  // .............................................................................

  TRI_hash_index_element_t* found = TRI_FindByKeyHashArray(&hashIndex->_hashArray, key);

  if (found != nullptr) {
    // unique hash index: maximum number is 1
    result.emplace_back(*(found->_document));
  }

  return TRI_ERROR_NO_ERROR;
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

  return static_cast<size_t>(KeyEntrySize(hashIndex) * hashIndex->_hashArrayMulti._nrUsed + 
                             TRI_MemoryUsageHashArrayMulti(&hashIndex->_hashArrayMulti));
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

  // ..........................................................................
  // Allocate sufficent memory for the field list
  // ..........................................................................

  size_t const n = TRI_LengthVector(&hashIndex->_paths);
  char const** fieldList = static_cast<char const**>(TRI_Allocate(TRI_CORE_MEM_ZONE, (sizeof(char*) * n) , false));

  for (size_t j = 0; j < n; ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*) TRI_AtVector(&hashIndex->_paths, j));
    TRI_shape_path_t const* path = document->getShaper()->lookupAttributePathByPid(document->getShaper(), shape);  // ONLY IN INDEX, PROTECTED by RUNTIME

    if (path == nullptr) {
      TRI_Free(TRI_CORE_MEM_ZONE, (void*) fieldList);

      return nullptr;
    }
    fieldList[j] = ((const char*) path) + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t);
  }

  // ..........................................................................
  // create json object and fill it
  // ..........................................................................

  TRI_json_t* json = TRI_JsonIndex(TRI_CORE_MEM_ZONE, idx);

  if (json == nullptr) {
    return nullptr;
  }

  TRI_json_t* fields = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

  for (size_t j = 0; j < n; ++j) {
    TRI_PushBack3ArrayJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, fieldList[j], strlen(fieldList[j])));
  }

  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "fields", fields);
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
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_t>(&hashElement);
      return TRI_ERROR_NO_ERROR;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_t>(&hashElement);
      return res;
    }

    res = HashIndex_insert(hashIndex, &hashElement, isRollback);

    if (res != TRI_ERROR_NO_ERROR) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_t>(&hashElement);
    }
  }
  else {
    TRI_hash_index_element_multi_t hashElement;
    res = HashIndexHelperAllocate<TRI_hash_index_element_multi_t>(hashIndex, &hashElement, document);

    if (res == TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_multi_t>(&hashElement);
      return TRI_ERROR_NO_ERROR;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_multi_t>(&hashElement);
      return res;
    }

    res = MultiHashIndex_insert(hashIndex, &hashElement, isRollback);
    if (res != TRI_ERROR_NO_ERROR) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_multi_t>(&hashElement);
    }
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
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_t>(&hashElement);
      return TRI_ERROR_NO_ERROR;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_t>(&hashElement);
      return res;
    }
    res = HashIndex_remove(hashIndex, &hashElement);
    FreeSubObjectsHashIndexElement<TRI_hash_index_element_t>(&hashElement);
  }
  else {
    TRI_hash_index_element_multi_t hashElement;
    res = HashIndexHelperAllocate<TRI_hash_index_element_multi_t>(hashIndex, &hashElement, document);

    if (res == TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_multi_t>(&hashElement);
      return TRI_ERROR_NO_ERROR;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      FreeSubObjectsHashIndexElement<TRI_hash_index_element_multi_t>(&hashElement);
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

  if (idx->_sparse) {
    // for sparse indexes, we assume that we will have less index entries
    // than if the index would be fully populated
    size /= 5;
  }

  if (hashIndex->base._unique) {
    TRI_ResizeHashArray(&hashIndex->_hashArray, size);
  }
  else {
    TRI_ResizeHashArrayMulti(&hashIndex->_hashArrayMulti, size);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the hash index can provide a selectivity estimate
////////////////////////////////////////////////////////////////////////////////

static double SelectivityEstimateHashIndex (TRI_index_t const* idx) {
  TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;

  if (hashIndex->base._unique) {
    return 1.0;
  }

  return TRI_SelectivityHashArrayMulti(&hashIndex->_hashArrayMulti);
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
                                  bool sparse,
                                  bool unique) {
  // ...........................................................................
  // Initialize the index and the callback functions
  // ...........................................................................

  TRI_hash_index_t* hashIndex = static_cast<TRI_hash_index_t*>(TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_hash_index_t), false));
  TRI_index_t* idx = &hashIndex->base;

  TRI_InitIndex(idx, iid, TRI_IDX_TYPE_HASH_INDEX, document, sparse, unique);

  idx->_hasSelectivityEstimate = true;
  idx->selectivityEstimate     = SelectivityEstimateHashIndex;
  idx->memory                  = MemoryHashIndex;
  idx->json                    = JsonHashIndex;
  idx->insert                  = InsertHashIndex;
  idx->remove                  = RemoveHashIndex;
  idx->sizeHint                = SizeHintHashIndex;

  // ...........................................................................
  // Copy the contents of the path list vector into a new vector and store this
  // ...........................................................................

  TRI_CopyPathVector(&hashIndex->_paths, paths);

  TRI_InitVectorString(&idx->_fields, TRI_CORE_MEM_ZONE);
  TRI_CopyDataFromVectorPointerVectorString(TRI_CORE_MEM_ZONE, &idx->_fields, fields);

  int res;
  if (unique) {
    res = TRI_InitHashArray(&hashIndex->_hashArray, TRI_LengthVector(&hashIndex->_paths));
  }
  else {
    res = TRI_InitHashArrayMulti(&hashIndex->_hashArrayMulti, TRI_LengthVector(&hashIndex->_paths));
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
  TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;

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
/// it is the callers responsibility to destroy the result
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupHashIndex (TRI_index_t* idx,
                                          TRI_index_search_value_t* searchValue) {
  TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;

  if (hashIndex->base._unique) {
    return HashIndex_find(hashIndex, searchValue);
  }

  return TRI_LookupByKeyHashArrayMulti(&hashIndex->_hashArrayMulti, searchValue);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given shaped json objects
////////////////////////////////////////////////////////////////////////////////

int TRI_LookupHashIndex (TRI_index_t* idx,
                         TRI_index_search_value_t* searchValue,
                         std::vector<TRI_doc_mptr_copy_t>& documents) {
  TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;

  if (hashIndex->base._unique) {
    return HashIndex_find(hashIndex, searchValue, documents);
  }

  return TRI_LookupByKeyHashArrayMulti(&hashIndex->_hashArrayMulti, searchValue, documents);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given shaped json objects
////////////////////////////////////////////////////////////////////////////////

int TRI_LookupHashIndex (TRI_index_t* idx,
                         TRI_index_search_value_t* searchValue,
                         std::vector<TRI_doc_mptr_copy_t>& documents,
                         struct TRI_hash_index_element_multi_s*& next,
                         size_t batchSize) {
  TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;

  if (hashIndex->base._unique) {
    next = nullptr;
    return HashIndex_find(hashIndex, searchValue, documents);
  }

  return TRI_LookupByKeyHashArrayMulti(&hashIndex->_hashArrayMulti, searchValue, documents, next, batchSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a selectivity estimate for the index
////////////////////////////////////////////////////////////////////////////////

double TRI_SelectivityHashIndex (TRI_index_t* idx) {
  TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;

  if (hashIndex->base._unique) {
    return 1.0; 
  }
  return TRI_SelectivityHashArrayMulti(&hashIndex->_hashArrayMulti);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
