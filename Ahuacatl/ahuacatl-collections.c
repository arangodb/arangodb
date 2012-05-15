////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, collection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Ahuacatl/ahuacatl-collections.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief comparator function to sort collections by name
////////////////////////////////////////////////////////////////////////////////

static int CollectionNameComparator (const void* l, const void* r) {
  TRI_aql_collection_t* left = *((TRI_aql_collection_t**) l);
  TRI_aql_collection_t* right = *((TRI_aql_collection_t**) r);

  return strcmp(left->_name, right->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection container
////////////////////////////////////////////////////////////////////////////////
    
static TRI_aql_collection_t* CreateCollectionContainer (const char* const name) {
  TRI_aql_collection_t* collection;

  assert(name);

  collection = (TRI_aql_collection_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_collection_t), false);
  if (!collection) {
    return NULL;
  }

  collection->_name = (char*) name;
  collection->_collection = NULL;
  collection->_barrier = NULL; 
  collection->_readLocked = false;

  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set up the collection names vector and order it
////////////////////////////////////////////////////////////////////////////////

bool SetupCollections (TRI_aql_context_t* const context) {
  size_t i;
  size_t n;
  bool result = true;

  // each collection used is contained once in the assoc. array
  // so we do not have to care about duplicate names here
  n = context->_collectionNames._nrAlloc;
  for (i = 0; i < n; ++i) {
    char* name = context->_collectionNames._table[i];
    TRI_aql_collection_t* collection;

    if (!name) {
      continue;
    }

    collection = CreateCollectionContainer(name);
    if (!collection) {
      result = false;
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      break;
    }

    TRI_PushBackVectorPointer(&context->_collections, (void*) collection);
  }

  if (result && n > 0) {
    qsort(context->_collections._buffer, 
          context->_collections._length, 
          sizeof(void*), 
          &CollectionNameComparator);
  }

  // now collections contains the sorted list of collections

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief open all collections used
////////////////////////////////////////////////////////////////////////////////

bool OpenCollections (TRI_aql_context_t* const context) {
  size_t i;
  size_t n;

  n = context->_collections._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_collection_t* collection = context->_collections._buffer[i];
    char* name;

    assert(collection);
    assert(collection->_name);
    assert(!collection->_collection);
    assert(!collection->_barrier);
    assert(!collection->_readLocked);

    TRI_AQL_LOG("locking collection '%s'", collection->_name);

    name = collection->_name;
    collection->_collection = TRI_UseCollectionByNameVocBase(context->_vocbase, name);
    if (!collection->_collection) {
      TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_COLLECTION_NOT_FOUND, name);
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief lookip a collection in the internal vector
////////////////////////////////////////////////////////////////////////////////

TRI_aql_collection_t* TRI_GetCollectionAql (const TRI_aql_context_t* const context,
                                            const char* const collectionName) {
  size_t i, n;

  assert(context);

  n = context->_collections._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_collection_t* col = (TRI_aql_collection_t*) TRI_AtVectorPointer(&context->_collections, i);

    if (TRI_EqualString(col->_name, collectionName)) {
      return col;
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlock all collections used
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockCollectionsAql (TRI_aql_context_t* const context) {
  size_t i;
  
  // unlock in reverse order
  i = context->_collections._length;
  while (i--) {
    TRI_aql_collection_t* collection = context->_collections._buffer[i];

    assert(collection);
    assert(collection->_name);

    if (!collection->_collection) {
      // collection not yet opened
      continue;
    }
  
    TRI_AQL_LOG("unlocking collection '%s'", collection->_name);
    
    TRI_ReleaseCollectionVocBase(context->_vocbase, collection->_collection);

    collection->_collection = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lock all collections used
////////////////////////////////////////////////////////////////////////////////

bool TRI_LockCollectionsAql (TRI_aql_context_t* const context) {
  if (!SetupCollections(context)) {
    return false;
  }

  if (!OpenCollections(context)) {
    TRI_UnlockCollectionsAql(context);
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read-locks all collections used in a query
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReadLockCollectionsAql (TRI_aql_context_t* const context) {
  size_t i;
  size_t n;
  bool result = true;

  // lock in forward order
  n = context->_collections._length;
  for (i = 0; i < n; ++i) {
    int lockResult;

    TRI_aql_collection_t* collection = (TRI_aql_collection_t*) context->_collections._buffer[i];
    TRI_doc_collection_t* documentCollection;

    assert(collection);
    assert(collection->_name);
    assert(collection->_collection);
    assert(collection->_collection->_collection);
    assert(!collection->_readLocked);
    assert(!collection->_barrier);

    documentCollection = (TRI_doc_collection_t*) collection->_collection->_collection;

    TRI_AQL_LOG("read-locking collection '%s'", collection->_name);

    lockResult = documentCollection->beginRead(documentCollection);
    if (lockResult != TRI_ERROR_NO_ERROR) {
      // couldn't acquire the read lock
      result = false;
      TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED, collection->_name);
      break;
    }
    else {
      collection->_readLocked = true;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read-unlocks all collections used in a query
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadUnlockCollectionsAql (TRI_aql_context_t* const context) {
  size_t i;

  // unlock in reverse order
  i = context->_collections._length;
  while (i--) {
    TRI_aql_collection_t* collection = (TRI_aql_collection_t*) context->_collections._buffer[i];
    TRI_doc_collection_t* documentCollection;

    assert(collection);
    assert(collection->_name);

    if (!collection->_collection) {
      // don't unlock collections we weren't able to lock at all
      continue;
    }

    if (!collection->_readLocked) {
      // don't unlock non-read-locked collections
      continue;
    }
    
    assert(collection->_collection->_collection);
    assert(!collection->_barrier);

    documentCollection = (TRI_doc_collection_t*) collection->_collection->_collection;

    TRI_AQL_LOG("read-unlocking collection '%s'", collection->_name);

    documentCollection->endRead(documentCollection);
    collection->_readLocked = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a gc marker for all collections used in a query
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddBarrierCollectionsAql (TRI_aql_context_t* const context) {
  size_t i;
  size_t n;
  bool result = true;

  // iterate in forward order
  n = context->_collections._length;
  for (i = 0; i < n; ++i) {
    TRI_barrier_t* ce;

    TRI_aql_collection_t* collection = (TRI_aql_collection_t*) context->_collections._buffer[i];
    TRI_doc_collection_t* documentCollection;

    assert(collection);
    assert(collection->_name);
    assert(collection->_collection);
    assert(collection->_readLocked);
    assert(collection->_collection->_collection);
    assert(!collection->_barrier);

    documentCollection = (TRI_doc_collection_t*) collection->_collection->_collection;

    TRI_AQL_LOG("adding barrier for collection '%s'", collection->_name);

    ce = TRI_CreateBarrierElement(&documentCollection->_barrierList);
    if (!ce) {
      // couldn't create the barrier
      result = false;
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      break;
    }
    else {
      collection->_barrier = ce;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes the gc markers for all collections used in a query
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveBarrierCollectionsAql (TRI_aql_context_t* const context) {
  size_t i;

  // iterate in reverse order
  i = context->_collections._length;
  while (i--) {
    TRI_aql_collection_t* collection = (TRI_aql_collection_t*) context->_collections._buffer[i];

    assert(collection);
    assert(collection->_name);

    if (!collection->_collection || !collection->_readLocked || !collection->_barrier) {
      // don't process collections we weren't able to lock at all
      continue;
    }

    assert(collection->_readLocked);
    assert(collection->_barrier);
    assert(collection->_collection->_collection);

    TRI_AQL_LOG("removing barrier for collection '%s'", collection->_name);

    TRI_FreeBarrier(collection->_barrier);
    collection->_barrier = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
