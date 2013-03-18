////////////////////////////////////////////////////////////////////////////////
/// @brief hash index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "hash-index.h"

#include "BasicsC/logging.h"
#include "ShapedJson/shape-accessor.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/document-collection.h"
#include "VocBase/voc-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the index search from hash index element
////////////////////////////////////////////////////////////////////////////////

static int FillIndexSearchValueByHashIndexElement (TRI_hash_index_t* idx,
                                                   TRI_index_search_value_t* key,
                                                   TRI_hash_index_element_t* element) {
  char const* ptr;
  size_t n;
  size_t i;

  n = idx->_paths._length;
  key->_values = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, n * sizeof(TRI_shaped_json_t), false);

  if (key->_values == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  ptr = (char const*) element->_document->_data;

  for (i = 0;  i < n;  ++i) {
    key->_values[i]._sid = element->_subObjects[i]._sid;
    key->_values[i]._data.length = element->_subObjects[i]._length;
    key->_values[i]._data.data = CONST_CAST(ptr + element->_subObjects[i]._offset);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates space for sub-objects in the hash index element
////////////////////////////////////////////////////////////////////////////////

static int AllocateSubObjectsHashIndexElement (TRI_hash_index_t const* idx,
                                               TRI_hash_index_element_t* element) {
  element->_subObjects = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE,
                                      idx->_paths._length * sizeof(TRI_shaped_sub_t),
                                      false);

  if (element->_subObjects == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees space for sub-objects in the hash index element
////////////////////////////////////////////////////////////////////////////////

static void FreeSubObjectsHashIndexElement (TRI_hash_index_element_t* element) {
  if (element->_subObjects != NULL) {
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

static int HashIndexHelper (TRI_hash_index_t const* hashIndex,
                            TRI_hash_index_element_t* hashElement,
                            TRI_doc_mptr_t const* document) {
  int res;
  size_t j;

  TRI_shaper_t* shaper;                 // underlying shaper
  TRI_shape_access_t const* acc;        // shape accessor
  TRI_shaped_json_t shapedObject;	// the sub-object
  TRI_shaped_json_t shapedJson;         // the object behind document
  TRI_shaped_sub_t shapedSub;           // the relative sub-object

  shaper = hashIndex->base._collection->_shaper;

  // .............................................................................
  // Assign the document to the TRI_hash_index_element_t structure - so that it
  // can later be retreived.
  // .............................................................................

  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, document->_data);

  hashElement->_document = CONST_CAST(document);

  // .............................................................................
  // Extract the attribute values
  // .............................................................................

  res = TRI_ERROR_NO_ERROR;

  for (j = 0;  j < hashIndex->_paths._length;  ++j) {
    TRI_shape_pid_t path = *((TRI_shape_pid_t*)(TRI_AtVector(&hashIndex->_paths, j)));

    // determine if document has that particular shape
    acc = TRI_FindAccessorVocShaper(shaper, shapedJson._sid, path);

    // field not part of the object
    if (acc == NULL || acc->_shape == NULL) {
      shapedSub._sid = shaper->_sidNull;
      shapedSub._length = 0;
      shapedSub._offset = 0;

      res = TRI_WARNING_ARANGO_INDEX_HASH_DOCUMENT_ATTRIBUTE_MISSING;
    }

    // extract the field
    else {
      if (! TRI_ExecuteShapeAccessor(acc, &shapedJson, &shapedObject)) {
        // hashElement->fields: memory deallocated in the calling procedure
        return TRI_ERROR_INTERNAL;
      }

      if (shapedObject._sid == shaper->_sidNull) {
        res = TRI_WARNING_ARANGO_INDEX_HASH_DOCUMENT_ATTRIBUTE_MISSING;
      }

      shapedSub._sid = shapedObject._sid;
      shapedSub._length = shapedObject._data.length;
      shapedSub._offset = ((char const*) shapedObject._data.data) - ((char const*) document->_data);
    }

    // store the json shaped sub-object -- this is what will be hashed
    hashElement->_subObjects[j] = shapedSub;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief index helper for hashing with allocation
////////////////////////////////////////////////////////////////////////////////

static int HashIndexHelperAllocate (TRI_hash_index_t const* hashIndex,
                                    TRI_hash_index_element_t* hashElement,
                                    TRI_doc_mptr_t const* document,
                                    bool allocate) {
  int res;

  // .............................................................................
  // Allocate storage to shaped json objects stored as a simple list.  These
  // will be used for hashing.  Fill the json field list from the document.
  // .............................................................................

  if (allocate) {
    res = AllocateSubObjectsHashIndexElement(hashIndex, hashElement);

    if (res != TRI_ERROR_NO_ERROR) {
      // out of memory
      return res;
    }
  }

  res = HashIndexHelper(hashIndex, hashElement, document);

  // .............................................................................
  // It may happen that the document does not have the necessary attributes to
  // have particpated within the hash index. If the index is unique, we do not
  // report an error to the calling procedure, but return a warning instead. If
  // the index is not unique, we ignore this error.
  // .............................................................................

  if (res == TRI_WARNING_ARANGO_INDEX_HASH_DOCUMENT_ATTRIBUTE_MISSING && ! hashIndex->base._unique) {
    res = TRI_ERROR_NO_ERROR;
  }
  else if (res != TRI_ERROR_NO_ERROR) {
    FreeSubObjectsHashIndexElement(hashElement);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              HASH INDEX MANAGMENT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                             hash array management
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a data element into the hash array
///
/// Since we do not allow duplicates - we must compare using keys, rather than
/// documents.
////////////////////////////////////////////////////////////////////////////////

static int HashIndex_insert (TRI_hash_index_t* hashIndex,
                             TRI_hash_index_element_t* element) {
  TRI_index_search_value_t key;
  int res;

  res = FillIndexSearchValueByHashIndexElement(hashIndex, &key, element);
  
  if (res != TRI_ERROR_NO_ERROR) {
    // out of memory
    return res;
  }

  res = TRI_InsertKeyHashArray(&hashIndex->_hashArray, &key, element, false);
  if (key._values != NULL) {
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
  int res;

  res = TRI_RemoveElementHashArray(&hashIndex->_hashArray, element);

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

  if (result != NULL) {

    // unique hash index: maximum number is 1
    results._length    = 1;
    results._documents = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, 1 * sizeof(TRI_doc_mptr_t*), false);

    if (results._documents == NULL) {
      return results;
    }

    results._documents[0] = result->_document;
  }
  else {
    results._length    = 0;
    results._documents = NULL;
  }

  return results;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                       multi hash array management
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a data element into the hash array
///
/// Since we do allow duplicates - we must compare using documents, rather than
/// keys.
////////////////////////////////////////////////////////////////////////////////

static int MultiHashIndex_insert (TRI_hash_index_t* hashIndex,
                                  TRI_hash_index_element_t* element) {
  int res;

  res = TRI_InsertElementHashArrayMulti(&hashIndex->_hashArray, element, false);

  if (res == TRI_RESULT_ELEMENT_EXISTS) {
    return TRI_ERROR_INTERNAL;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the associative array
////////////////////////////////////////////////////////////////////////////////

int MultiHashIndex_remove (TRI_hash_index_t* hashIndex,
                           TRI_hash_index_element_t* element) {
  int res;

  res = TRI_RemoveElementHashArrayMulti(&hashIndex->_hashArray, element);

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
  TRI_vector_pointer_t result;
  TRI_index_result_t results;

  // .............................................................................
  // We can only use the LookupByKey method for non-unique hash indexes, since
  // we want more than one result returned!
  // .............................................................................

  result = TRI_LookupByKeyHashArrayMulti(&hashIndex->_hashArray, key);

  if (result._length == 0) {
    results._length    = 0;
    results._documents = NULL;
  }
  else {
    size_t j;

    results._length    = result._length;
    results._documents = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE,
                                      result._length* sizeof(TRI_doc_mptr_t*),
                                      false);

    if (results._documents == NULL) {
      TRI_DestroyVectorPointer(&result);
      return results;
    }

    for (j = 0;  j < result._length;  ++j) {
      results._documents[j] = ((TRI_hash_index_element_t*)(result._buffer[j]))->_document;
    }
  }

  TRI_DestroyVectorPointer(&result);
  return results;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        HASH INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief describes a hash index as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonHashIndex (TRI_index_t* idx,
                                  TRI_primary_collection_t const* collection) {
  TRI_json_t* json;
  TRI_json_t* fields;
  TRI_hash_index_t* hashIndex;
  char const** fieldList;
  size_t j;

  // .............................................................................
  // Recast as a hash index
  // .............................................................................

  hashIndex = (TRI_hash_index_t*) idx;

  // .............................................................................
  // Allocate sufficent memory for the field list
  // .............................................................................

  fieldList = TRI_FieldListByPathList(hashIndex->base._collection->_shaper, &hashIndex->_paths);

  if (fieldList == NULL) {
    return NULL;
  }

  // ..........................................................................
  // create json object and fill it
  // ..........................................................................

  json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);
  fields = TRI_CreateListJson(TRI_CORE_MEM_ZONE);

  for (j = 0; j < hashIndex->_paths._length; ++j) {
    TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, fieldList[j]));
  }

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "id", TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, idx->_iid));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "unique", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, hashIndex->base._unique));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "type", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, "hash"));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "fields", fields);

  TRI_Free(TRI_CORE_MEM_ZONE, fieldList);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a hash index from a collection
////////////////////////////////////////////////////////////////////////////////

static void RemoveIndexHashIndex (TRI_index_t* idx,
                                  TRI_primary_collection_t* collection) {
  // the index will later be destroyed, so do nothing here
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a a document to a hash index
////////////////////////////////////////////////////////////////////////////////

static int InsertHashIndex (TRI_index_t* idx, TRI_doc_mptr_t const* document) {
  TRI_hash_index_element_t hashElement;
  TRI_hash_index_t* hashIndex;
  int res;

  hashIndex = (TRI_hash_index_t*) idx;

  res = HashIndexHelperAllocate(hashIndex, &hashElement, document, true);

  if (res == TRI_WARNING_ARANGO_INDEX_HASH_DOCUMENT_ATTRIBUTE_MISSING) {
    return TRI_ERROR_NO_ERROR;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (hashIndex->base._unique) {
    res = HashIndex_insert(hashIndex, &hashElement);
  }
  else {
    res = MultiHashIndex_insert(hashIndex, &hashElement);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a hash index
////////////////////////////////////////////////////////////////////////////////

static int RemoveHashIndex (TRI_index_t* idx, TRI_doc_mptr_t const* document) {
  TRI_hash_index_element_t hashElement;
  TRI_hash_index_t* hashIndex;
  int res;

  hashIndex = (TRI_hash_index_t*) idx;

  res = HashIndexHelperAllocate(hashIndex, &hashElement, document, true);

  if (res == TRI_WARNING_ARANGO_INDEX_HASH_DOCUMENT_ATTRIBUTE_MISSING) {
    return TRI_ERROR_NO_ERROR;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (hashIndex->base._unique) {
    res = HashIndex_remove(hashIndex, &hashElement);
  }
  else {
    res = MultiHashIndex_remove(hashIndex, &hashElement);
  }

  FreeSubObjectsHashIndexElement(&hashElement);
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a hash index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateHashIndex (struct TRI_primary_collection_s* collection,
                                  TRI_vector_pointer_t* fields,
                                  TRI_vector_t* paths,
                                  bool unique,
                                  size_t initialDocumentCount) {
  TRI_hash_index_t* hashIndex;
  int res;

  // ...........................................................................
  // Initialize the index and the callback functions
  // ...........................................................................

  hashIndex = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_hash_index_t), false);

  TRI_InitIndex(&hashIndex->base, TRI_IDX_TYPE_HASH_INDEX, collection, unique);

  hashIndex->base.json = JsonHashIndex;
  hashIndex->base.removeIndex = RemoveIndexHashIndex;

  hashIndex->base.insert = InsertHashIndex;
  hashIndex->base.remove = RemoveHashIndex;

  // ...........................................................................
  // Copy the contents of the path list vector into a new vector and store this
  // ...........................................................................

  TRI_CopyPathVector(&hashIndex->_paths, paths);

  TRI_InitVectorString(&hashIndex->base._fields, TRI_CORE_MEM_ZONE);
  TRI_CopyDataVectorStringFromVectorPointer(TRI_CORE_MEM_ZONE, &hashIndex->base._fields, fields);

  // create a index preallocated for the current number of documents
  res = TRI_InitHashArray(&hashIndex->_hashArray,
                          initialDocumentCount,
                          hashIndex->_paths._length);

  // oops, out of memory?
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyVector(&hashIndex->_paths);
    TRI_DestroyVectorString(&hashIndex->base._fields);
    TRI_Free(TRI_CORE_MEM_ZONE, hashIndex);
    return NULL;
  }

  // ...........................................................................
  // Assign the function calls used by the query engine
  // ...........................................................................

  hashIndex->base.indexQuery = NULL;
  hashIndex->base.indexQueryFree = NULL;
  hashIndex->base.indexQueryResult = NULL;

  return &hashIndex->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyHashIndex (TRI_index_t* idx) {
  TRI_hash_index_t* hashIndex;

  TRI_DestroyVectorString(&idx->_fields);

  hashIndex = (TRI_hash_index_t*) idx;

  TRI_DestroyVector(&hashIndex->_paths);
  TRI_DestroyHashArray(&hashIndex->_hashArray);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeHashIndex (TRI_index_t* idx) {
  TRI_DestroyHashIndex(idx);
  TRI_Free(TRI_CORE_MEM_ZONE, idx);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given shaped json objects
////////////////////////////////////////////////////////////////////////////////

TRI_index_result_t TRI_LookupHashIndex (TRI_index_t* idx,
                                        TRI_index_search_value_t* searchValue) {
  TRI_hash_index_t* hashIndex;

  hashIndex = (TRI_hash_index_t*) idx;

  if (hashIndex->base._unique) {
    return HashIndex_find(hashIndex, searchValue);
  }
  else {
    return MultiHashIndex_find(hashIndex, searchValue);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
