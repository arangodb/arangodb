////////////////////////////////////////////////////////////////////////////////
/// @brief shadow data storage
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2011-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <BasicsC/logging.h>

#include "VocBase/shadow-data.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                       SHADOW DATA
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief set the timestamp of a shadow to the current date & time
////////////////////////////////////////////////////////////////////////////////

static inline void UpdateTimestampShadow (TRI_shadow_t* const shadow) {
  shadow->_timestamp = TRI_microtime();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief init a shadow data structure
////////////////////////////////////////////////////////////////////////////////
  
static TRI_shadow_t* CreateShadow (const void* const data) {
  TRI_shadow_t* shadow = (TRI_shadow_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shadow_t)); 

  if (!shadow) {
    return NULL;
  }

  shadow->_rc        = 1;
  shadow->_data      = (void*) data;
  shadow->_id        = TRI_NewTickVocBase();
  shadow->_deleted   = false;
  shadow->_type      = SHADOW_TRANSIENT;

  UpdateTimestampShadow(shadow);
  
  LOG_TRACE("created shadow %p with data ptr %p and id %lu", 
            shadow, 
            data, 
            (unsigned long) shadow->_id);

  return shadow;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decrease the refcount for a shadow
////////////////////////////////////////////////////////////////////////////////

static void DecreaseRefCount (TRI_shadow_store_t* const store, TRI_shadow_t* const shadow) {
  LOG_TRACE("decreasing refcount for shadow %p with data ptr %p and id %lu", 
            shadow, 
            shadow->_data, 
            (unsigned long) shadow->_id);

  if (--shadow->_rc <= 0 && shadow->_type == SHADOW_TRANSIENT) {
    LOG_TRACE("deleting shadow %p", shadow);

    TRI_RemoveKeyAssociativePointer(&store->_ids, &shadow->_id);
    TRI_RemoveKeyAssociativePointer(&store->_pointers, shadow->_data);
    store->destroyShadow(shadow->_data);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, shadow);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the refcount for a shadow
////////////////////////////////////////////////////////////////////////////////

static void IncreaseRefCount (TRI_shadow_store_t* const store, TRI_shadow_t* const shadow) {
  LOG_TRACE("increasing refcount for shadow %p with data ptr %p and id %lu", 
            shadow, 
            shadow->_data, 
            (unsigned long) shadow->_id);

  ++shadow->_rc;
  UpdateTimestampShadow(shadow);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the persistence flag for a shadow
////////////////////////////////////////////////////////////////////////////////

static void PersistShadow (TRI_shadow_t* const shadow) {
  LOG_TRACE("persisting shadow %p with data ptr %p and id %lu", 
            shadow, 
            shadow->_data, 
            (unsigned long) shadow->_id);

  shadow->_type = SHADOW_PERSISTENT;
  UpdateTimestampShadow(shadow);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the deleted flag for a shadow
////////////////////////////////////////////////////////////////////////////////

static void DeleteShadow (TRI_shadow_store_t* const store, TRI_shadow_t* const shadow) {
  LOG_TRACE("setting deleted flag for shadow %p with data ptr %p and id %lu", 
            shadow, 
            shadow->_data, 
            (unsigned long) shadow->_id);

  shadow->_deleted = true;
  DecreaseRefCount(store, shadow);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an element in the ids index
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyId (TRI_associative_pointer_t* array, void const* k) {
  TRI_shadow_id key = *((TRI_shadow_id*) k);

  return (uint64_t) key;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an element in the ids index
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementId (TRI_associative_pointer_t* array, void const* e) {
  TRI_shadow_t const* element = e;

  return (uint64_t) element->_id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if two elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyId (TRI_associative_pointer_t* array, void const* k, void const* e) {
  TRI_shadow_t const* element = e;
  TRI_shadow_id key = *((TRI_shadow_id*) k);

  return (key == element->_id);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an element in the pointers index
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyData (TRI_associative_pointer_t* array, void const* k) {
  uint64_t key = 0;
  
  key = (uint64_t) (uintptr_t) k;
  return key;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an element in the pointers index
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementData (TRI_associative_pointer_t* array, void const* e) {
  TRI_shadow_t const* element = e;

  return (uint64_t) (uintptr_t) element->_data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if two elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyData (TRI_associative_pointer_t* array, void const* k, void const* e) {
  TRI_shadow_t const* element = e;

  return ((uint64_t) (uintptr_t) k == (uint64_t) (uintptr_t) element->_data);
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
/// @brief creates a shadow data storage
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_store_t* TRI_CreateShadowStore (void (*destroy) (void*)) {
  TRI_shadow_store_t* store = 
    (TRI_shadow_store_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shadow_store_t));

  if (store) {
    TRI_InitAssociativePointer(&store->_ids,
                               TRI_UNKNOWN_MEM_ZONE, 
                               HashKeyId,
                               HashElementId,
                               EqualKeyId,
                               NULL); 
    
    TRI_InitAssociativePointer(&store->_pointers,
                               TRI_UNKNOWN_MEM_ZONE, 
                               HashKeyData,
                               HashElementData,
                               EqualKeyData,
                               NULL);

    store->destroyShadow = destroy;
    
    TRI_InitMutex(&store->_lock);
  }

  return store;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shadow data storage
///
/// Note: all remaining shadows will be destroyed
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShadowStore (TRI_shadow_store_t* const store) {
  assert(store);

  // force deletion of all remaining shadows
  TRI_CleanupShadowData(store, 0, true); 

  TRI_DestroyMutex(&store->_lock);
  TRI_DestroyAssociativePointer(&store->_ids);
  TRI_DestroyAssociativePointer(&store->_pointers);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, store);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a shadow in the index using its data pointer and return 
/// its id
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_id TRI_GetIdDataShadowData (TRI_shadow_store_t* const store,
                                       const void* const data) {
  TRI_shadow_t* shadow;
  TRI_shadow_id id = 0;

  assert(store);
  
  if (data) {
    TRI_LockMutex(&store->_lock);
    shadow = (TRI_shadow_t*) TRI_LookupByKeyAssociativePointer(&store->_pointers, data);

    if (shadow && !shadow->_deleted) {
      id = shadow->_id;
      UpdateTimestampShadow(shadow);
    } 

    TRI_UnlockMutex(&store->_lock);
  }

  return id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a shadow in the index using its data pointer
///
/// If the shadow is found, this will return the data pointer, NULL otherwise.
/// When the shadow is found, its refcount will also be increased by one
////////////////////////////////////////////////////////////////////////////////

void* TRI_BeginUsageDataShadowData (TRI_shadow_store_t* const store,
                                    const void* const data) {
  TRI_shadow_t* shadow;

  assert(store);
  
  if (!data) {
    return NULL;
  }
  
  TRI_LockMutex(&store->_lock);
  shadow = (TRI_shadow_t*) TRI_LookupByKeyAssociativePointer(&store->_pointers, data);

  if (shadow && !shadow->_deleted) {
    IncreaseRefCount(store, shadow);
    TRI_UnlockMutex(&store->_lock);
    return shadow->_data;
  } 

  TRI_UnlockMutex(&store->_lock);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a shadow in the index using its id
///
/// If the shadow is found, this will return the data pointer, NULL otherwise.
/// When the shadow is found, its refcount will also be increased by one
////////////////////////////////////////////////////////////////////////////////

void* TRI_BeginUsageIdShadowData (TRI_shadow_store_t* const store,
                                  const TRI_shadow_id id) {
  TRI_shadow_t* shadow;

  assert(store);

  TRI_LockMutex(&store->_lock);
  shadow = (TRI_shadow_t*) TRI_LookupByKeyAssociativePointer(&store->_ids, (void const*) &id);

  if (shadow && !shadow->_deleted) {
    IncreaseRefCount(store, shadow);
    TRI_UnlockMutex(&store->_lock);
    return shadow->_data;
  } 

  TRI_UnlockMutex(&store->_lock);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a shadow in the index using its data pointer
///
/// If the shadow is found, its refcount will be decreased by one.
/// If the refcount is 0 and the shadow is of type SHADOW_TRANSIENT, the shadow
/// object will be destroyed.
////////////////////////////////////////////////////////////////////////////////

void TRI_EndUsageDataShadowData (TRI_shadow_store_t* const store,
                                 const void* const data) {
  TRI_shadow_t* shadow;

  assert(store);
  
  TRI_LockMutex(&store->_lock);
  shadow = (TRI_shadow_t*) TRI_LookupByKeyAssociativePointer(&store->_pointers, data);

  if (shadow && !shadow->_deleted) {
    DecreaseRefCount(store, shadow); // this might delete the shadow
  } 

  TRI_UnlockMutex(&store->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief look up a shadow in the index using its id
///
/// If the shadow is found, its refcount will be decreased by one.
/// If the refcount is 0 and the shadow is of type SHADOW_TRANSIENT, the shadow
/// object will be destroyed.
////////////////////////////////////////////////////////////////////////////////

void TRI_EndUsageIdShadowData (TRI_shadow_store_t* const store,
                             const TRI_shadow_id id) {
  TRI_shadow_t* shadow;

  assert(store);
  
  TRI_LockMutex(&store->_lock);
  shadow = (TRI_shadow_t*) TRI_LookupByKeyAssociativePointer(&store->_ids, &id);

  if (shadow && !shadow->_deleted) {
    DecreaseRefCount(store, shadow); // this might delete the shadow
  } 

  TRI_UnlockMutex(&store->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the persistence flag for a shadow using its data pointer
////////////////////////////////////////////////////////////////////////////////

bool TRI_PersistDataShadowData (TRI_shadow_store_t* const store,
                                const void* const data) {
  TRI_shadow_t* shadow;
  bool result = false;

  assert(store);
  
  TRI_LockMutex(&store->_lock);
  shadow = (TRI_shadow_t*) TRI_LookupByKeyAssociativePointer(&store->_pointers, data);

  if (shadow && !shadow->_deleted) {
    PersistShadow(shadow);
    result = true;
  } 

  TRI_UnlockMutex(&store->_lock);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the persistence flag for a shadow using its id
////////////////////////////////////////////////////////////////////////////////

bool TRI_PersistIdShadowData (TRI_shadow_store_t* const store,
                              const TRI_shadow_id id) {
  TRI_shadow_t* shadow;
  bool result = false;

  assert(store);

  TRI_LockMutex(&store->_lock);
  shadow = (TRI_shadow_t*) TRI_LookupByKeyAssociativePointer(&store->_ids, &id);

  if (shadow && !shadow->_deleted) {
    PersistShadow(shadow);
    result = true;
  } 

  TRI_UnlockMutex(&store->_lock);
  
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the deleted flag for a shadow using its data pointer
////////////////////////////////////////////////////////////////////////////////

bool TRI_DeleteDataShadowData (TRI_shadow_store_t* const store,
                               const void* const data) {
  TRI_shadow_t* shadow;
  bool found = false;

  assert(store);

  if (data) {
    TRI_LockMutex(&store->_lock);
    shadow = (TRI_shadow_t*) TRI_LookupByKeyAssociativePointer(&store->_pointers, data);

    if (shadow && !shadow->_deleted) {
      DeleteShadow(store, shadow);
      found = true;
    } 

    TRI_UnlockMutex(&store->_lock);
  }

  return found;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the deleted flag for a shadow using its id
////////////////////////////////////////////////////////////////////////////////

bool TRI_DeleteIdShadowData (TRI_shadow_store_t* const store,
                             const TRI_shadow_id id) {
  TRI_shadow_t* shadow;
  bool found = false;

  assert(store);

  TRI_LockMutex(&store->_lock);
  shadow = (TRI_shadow_t*) TRI_LookupByKeyAssociativePointer(&store->_ids, &id);

  if (shadow && !shadow->_deleted) {
    DeleteShadow(store, shadow);
    found = true;
  } 

  TRI_UnlockMutex(&store->_lock);
  return found;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enumerate all shadows and remove them if 
/// - their refcount is 0 and they are transient
/// - their refcount is 0 and they are expired
/// - the force flag is set
/// 
/// The max age must be specified in seconds. The max age is ignored if the
/// force flag is set. In this case all remaining shadows will be deleted
////////////////////////////////////////////////////////////////////////////////
  
void TRI_CleanupShadowData (TRI_shadow_store_t* const store, 
                            const double maxAge, 
                            const bool force) {
  double compareStamp = TRI_microtime() - maxAge; // age must be specified in secs
  size_t deleteCount = 0;

  // we need an exclusive lock on the index
  TRI_LockMutex(&store->_lock);

  // loop until there's nothing to delete or 
  // we have deleted SHADOW_MAX_DELETE elements 
  while (deleteCount++ < SHADOW_MAX_DELETE || force) {
    bool deleted = false;
    size_t i;

    for (i = 0; i < store->_ids._nrAlloc; i++) {
      // enum all shadows
      TRI_shadow_t* shadow = (TRI_shadow_t*) store->_ids._table[i];
      if (!shadow) {
        continue;
      }

      // check if shadow is unused and expired
      if (shadow->_rc < 1 || force) {
        if (shadow->_type == SHADOW_TRANSIENT || 
            shadow->_timestamp < compareStamp || 
            force) {
          LOG_TRACE("cleaning expired shadow %p", shadow);

          TRI_RemoveKeyAssociativePointer(&store->_ids, &shadow->_id);
          TRI_RemoveKeyAssociativePointer(&store->_pointers, shadow->_data);
          store->destroyShadow(shadow->_data);
          TRI_Free(TRI_UNKNOWN_MEM_ZONE, shadow);

          deleted = true;
          // the remove might reposition elements in the container.
          // therefore break here and start iteration anew
          break;
        }
      }
    }

    if (!deleted) {
      // we did not find anything to delete, so give up
      break;
    }
  }

  // release lock
  TRI_UnlockMutex(&store->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief store a new shadow in the store
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_t* TRI_StoreShadowData (TRI_shadow_store_t* const store, 
                                   const void* const data) {
  TRI_shadow_t* shadow;
  
  assert(store);

  shadow = CreateShadow(data);
  if (shadow) {
    LOG_TRACE("storing shadow %p with data ptr %p and id %lu", 
            shadow, 
            shadow->_data, 
            (unsigned long) shadow->_id);

    TRI_LockMutex(&store->_lock);
    if (TRI_InsertKeyAssociativePointer(&store->_ids, &shadow->_id, shadow, false)) {
      // duplicate entry
      LOG_INFO("storing shadow failed");
      TRI_UnlockMutex(&store->_lock);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, shadow);
      return NULL;
    }
    TRI_InsertKeyAssociativePointer(&store->_pointers, data, shadow, false);

    TRI_UnlockMutex(&store->_lock);
  }

  // might be NULL in case of OOM
  return shadow; 
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

/*
// -----------------------------------------------------------------------------
// --SECTION--                                  UNUSED AND UNTESTED CODE FOLLOWS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  SHADOW DOCUMENTS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an element
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashShadowDocumentElement (TRI_associative_pointer_t* array, 
                                           void const* e) {
  TRI_shadow_document_t const* element = e;

  return element->_did;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tests if two elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool EqualShadowDocumentElement (TRI_associative_pointer_t* array, 
                                        void const* l, 
                                        void const* r) {
  TRI_shadow_document_t const* left = l;
  TRI_shadow_document_t const* right = r;

  return ((left->_base->_id == right->_base->_id) || 
          (left->_cid == right->_cid && left->_did == right->_did));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a shadow document data structure
////////////////////////////////////////////////////////////////////////////////

static TRI_shadow_document_t* CreateShadowDocument (void* const element, 
                                                    TRI_voc_cid_t cid, 
                                                    TRI_voc_did_t did,
                                                    TRI_voc_rid_t rid) {
  TRI_shadow_document_t* shadow;
  TRI_shadow_t* base = CreateShadow(element);

  if (!base) {
    return NULL;
  }

  shadow = TRI_Allocate(sizeof(TRI_shadow_document_t));
  if (!shadow) {
    TRI_Free(base);
    return NULL;
  }

  shadow->_base = base;
  shadow->_cid  = cid;
  shadow->_did  = did;
  shadow->_rid  = rid;

  return shadow;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a shadow document or creates it
///
/// Note: this function is called under an exclusive lock on the index
////////////////////////////////////////////////////////////////////////////////

static TRI_shadow_document_t* LookupShadowDocument (TRI_shadow_document_store_t* const store,
                                                    TRI_doc_collection_t* collection,
                                                    TRI_doc_mptr_t const* mptr,
                                                    TRI_voc_cid_t cid, 
                                                    TRI_voc_did_t did) {
  union { TRI_shadow_document_t* s; TRI_shadow_document_t const* c; } cnv;
  TRI_shadow_document_t* shadow;
  TRI_shadow_document_t search;
  void* element;

  // check if we already know a parsed version
  search._cid = cid;
  search._did = did;
  cnv.c = TRI_LookupByElementAssociativePointer(&store->_base->_index, &search);
  shadow = cnv.s;

  if (shadow) {
    bool ok = store->verifyShadow(store, collection, mptr, shadow->_base->_data);
    if (ok) {
      ++shadow->_base->_rc;
      UpdateTimestampShadow(shadow->_base);
      return shadow;
    }
    else {
     TRI_ReleaseShadowDocument(store, shadow);
    }
  }

  // parse the document
  element = store->createShadow(store, collection, mptr);
  if (!element) {
    return NULL;
  }

  shadow = CreateShadowDocument(element, cid, did, mptr->_rid);
  if (shadow) {
    // enter the element into the store
    TRI_InsertElementAssociativePointer(&store->_base->_index, shadow, true);
  }

  // might be NULL
  return shadow;
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
/// @brief initialises a shadow document storage
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_document_store_t* TRI_CreateShadowDocumentStore (
  void* (*create) (TRI_shadow_document_store_t*, TRI_doc_collection_t*, TRI_doc_mptr_t const*),
  bool (*verify) (TRI_shadow_document_store_t*, TRI_doc_collection_t*, TRI_doc_mptr_t const*, void*),
  void (*destroy) (TRI_shadow_document_store_t*, TRI_shadow_document_t*)) {

  TRI_shadow_document_store_t* store;
  TRI_shadow_store_t* base;
  
  base = (TRI_shadow_store_t*) TRI_Allocate(sizeof(TRI_shadow_store_t));
  if (!base) {
    return NULL;
  }

  TRI_InitAssociativePointer(&base->_index,
                             NULL,
                             HashShadowDocumentElement,
                             NULL,
                             EqualShadowDocumentElement);

  store = (TRI_shadow_document_store_t*) TRI_Allocate(sizeof(TRI_shadow_document_store_t));
  if (!store) {
    TRI_FreeShadowStore(base);
    return NULL;
  }
  store->_base = base;

  TRI_InitMutex(&store->_base->_lock);

  store->createShadow  = create;
  store->verifyShadow  = verify;
  store->destroyShadow = destroy;

  return store;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shadow document storage
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShadowDocumentStore (TRI_shadow_document_store_t* const store) {
  assert(store);

  TRI_FreeShadowStore(store->_base);
  TRI_Free(store);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up or creates a shadow document
////////////////////////////////////////////////////////////////////////////////

TRI_shadow_document_t* TRI_FindShadowDocument (TRI_shadow_document_store_t* const store, 
                                               TRI_vocbase_t* vocbase, 
                                               TRI_voc_cid_t cid, 
                                               TRI_voc_did_t did) {
  TRI_vocbase_col_t const* col;
  TRI_doc_collection_t* collection;
  TRI_doc_mptr_t const* mptr;
  TRI_shadow_document_t* shadow;

  assert(store);

  // extract the collection
  col = TRI_LookupCollectionByIdVocBase(vocbase, cid);
  if (!col) {
    return NULL;
  }

  collection = col->_collection;

  // lock the collection
  collection->beginRead(collection);

  // find the document
  mptr = collection->read(collection, did);

  shadow = NULL;
  if (mptr) {
    TRI_LockMutex(&store->_base->_lock);
    shadow = LookupShadowDocument(store, collection, mptr, cid, did);
    TRI_UnlockMutex(&store->_base->_lock);
  }

  // unlock the collection
  collection->endRead(collection);
 
  // might be null
  return shadow;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a shadow document
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReleaseShadowDocument (TRI_shadow_document_store_t* const store, 
                                TRI_shadow_document_t* shadow) {
  bool result;

  assert(store);

  TRI_LockMutex(&store->_base->_lock);

  // release the element
  --shadow->_base->_rc;

  // need to destroy the element
  if (shadow->_base->_rc < 1) {
    TRI_RemoveElementAssociativePointer(&store->_base->_index, shadow);
    store->destroyShadow(store, shadow);
    TRI_Free(shadow->_base);
    TRI_Free(shadow);
    result = true; // object was destroyed
  }
  else {
    result = false; // object was not destroyed
  }

  TRI_UnlockMutex(&store->_base->_lock);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enumerate all shadows and remove them if expired 
////////////////////////////////////////////////////////////////////////////////
  
void TRI_CleanupShadowDocuments (TRI_shadow_document_store_t* const store, const double maxAge) {
  double compareStamp = TRI_microtime() - maxAge; // age must be specified in secs
  size_t deleteCount = 0;

  // we need an exclusive lock on the index
  TRI_LockMutex(&store->_base->_lock);

  // loop until there's nothing to delete or 
  // we have deleted SHADOW_MAX_DELETE elements 
  while (deleteCount++ < SHADOW_MAX_DELETE) {
    bool deleted = false;
    size_t i;

    for (i = 0; i < store->_base->_index._nrAlloc; i++) {
      // enum all shadows
      TRI_shadow_t* shadow = (TRI_shadow_t*) store->_base->_index._table[i];
      if (!shadow) {
        continue;
      }

      // check if shadow is unused and expired
      if (shadow->_rc <= 1 && shadow->_timestamp < compareStamp) {
        LOG_DEBUG("cleaning expired shadow %p", shadow);
        TRI_RemoveElementAssociativePointer(&store->_base->_index, shadow);
//        store->destroyShadow(store, shadow);
//        TRI_Free(shadow);

        deleted = true;
        // the remove might reposition elements in the container.
        // therefore break here and start iteration anew
        break;
      }
    }

    if (!deleted) {
      // we did not find anything to delete, so give up
      break;
    }
  }

  // release lock
  TRI_UnlockMutex(&store->_base->_lock);
}
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////
*/

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
