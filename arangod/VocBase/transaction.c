////////////////////////////////////////////////////////////////////////////////
/// @brief transaction subsystem
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
/// @author Jan Steemann
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "transaction.h"

#include "BasicsC/logging.h"
#include "BasicsC/strings.h"

#include "VocBase/primary-collection.h"
#include "VocBase/vocbase.h"

// -----------------------------------------------------------------------------
// --SECTION--                                               TRANSACTION CONTEXT
// -----------------------------------------------------------------------------

#define LOG_TRX(trx, level, format, ...) \
  LOG_TRACE("trx #%llu.%d (%s): " format, (unsigned long long) trx->_id, (int) level, StatusTransaction(trx->_status), __VA_ARGS__)

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the collection id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyCid (TRI_associative_pointer_t* array, void const* key) {
  TRI_transaction_cid_t const* k = key;

  return (uint64_t) *k;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the collection id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementCid (TRI_associative_pointer_t* array, void const* element) {
  TRI_transaction_collection_global_t const* e = element;

  return (uint64_t) e->_cid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a collection id and a collection
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyCid (TRI_associative_pointer_t* array, void const* key, void const* element) {
  TRI_transaction_cid_t const* k = key;
  TRI_transaction_collection_global_t const* e = element;

  return *k == e->_cid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a global instance of a collection
////////////////////////////////////////////////////////////////////////////////

static TRI_transaction_collection_global_t* CreateGlobalInstance (TRI_transaction_cid_t cid) {
  TRI_transaction_collection_global_t* globalInstance;

  globalInstance = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_collection_global_t), false);

  if (globalInstance != NULL) {
    globalInstance->_cid                       = cid;
    globalInstance->_stats._lastStartedReader  = 0;
    globalInstance->_stats._lastFinishedReader = 0;
    globalInstance->_stats._lastStartedWriter  = 0;
    globalInstance->_stats._lastAbortedWriter  = 0;
    globalInstance->_stats._lastFinishedWriter = 0;

    TRI_InitReadWriteLock(&globalInstance->_lock);
  }

  return globalInstance;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a global instance of a collection
////////////////////////////////////////////////////////////////////////////////

static void FreeGlobalInstance (TRI_transaction_collection_global_t* globalInstance) {
  TRI_DestroyReadWriteLock(&globalInstance->_lock);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, globalInstance);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the global instance of a collection
////////////////////////////////////////////////////////////////////////////////

static TRI_transaction_collection_global_t* GetGlobalInstance (TRI_transaction_context_t* context,
                                                               TRI_transaction_cid_t cid,
                                                               const bool create) {
  TRI_transaction_collection_global_t* globalInstance;
  TRI_transaction_collection_global_t* found;

  // acquire a read-lock on the global list of collections
  TRI_ReadLockReadWriteLock(&context->_collectionsLock);
  globalInstance = TRI_LookupByKeyAssociativePointer(&context->_collections, &cid);
  TRI_ReadUnlockReadWriteLock(&context->_collectionsLock);

  if (globalInstance != NULL) {
    // we already have a global instance
    return globalInstance;
  }

  // collection not found

  if (! create) {
    // not asked to create a new global instance
    return NULL;
  }

  // create a new global instance
  globalInstance = CreateGlobalInstance(cid);

  if (globalInstance == NULL) {
    return NULL;
  }

  // write lock and insert the new instance, but only if no one else has inserted it
  TRI_WriteLockReadWriteLock(&context->_collectionsLock);
  found = TRI_InsertKeyAssociativePointer(&context->_collections, &cid, globalInstance, false);
  TRI_WriteUnlockReadWriteLock(&context->_collectionsLock);

  if (found != NULL) {
    // someone else inserted another global instance. so we'll return it
    FreeGlobalInstance(globalInstance);
    return found;
  }

  // our own instance has been inserted
  return globalInstance;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update the global transaction statistics
////////////////////////////////////////////////////////////////////////////////

static int UpdateGlobalStats (const TRI_transaction_t const* trx,
                              const TRI_transaction_status_e status) {
  size_t i, n;

  n = trx->_collections._length;

  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* collection;
    TRI_transaction_collection_global_t* globalInstance;

    collection = (TRI_transaction_collection_t*) TRI_AtVectorPointer(&trx->_collections, i);

    globalInstance = collection->_globalInstance;
    TRI_ASSERT_DEBUG(globalInstance != NULL);

    TRI_WriteLockReadWriteLock(&globalInstance->_lock);

    if (collection->_accessType == TRI_TRANSACTION_READ) {
      if (status == TRI_TRANSACTION_RUNNING) {    
        globalInstance->_stats._lastStartedReader = trx->_id;
      }
      else if (status == TRI_TRANSACTION_ABORTED ||
               status == TRI_TRANSACTION_COMMITTED) {
        globalInstance->_stats._lastFinishedReader = trx->_id;
      }
    }
    else {
      if (status == TRI_TRANSACTION_RUNNING) {    
        globalInstance->_stats._lastStartedWriter = trx->_id;
      }
      else if (status == TRI_TRANSACTION_ABORTED) {
        globalInstance->_stats._lastAbortedWriter = trx->_id;
      }
      else if (status == TRI_TRANSACTION_COMMITTED) {
        globalInstance->_stats._lastFinishedWriter = trx->_id;
      }
    }

    TRI_WriteUnlockReadWriteLock(&globalInstance->_lock);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create the global transaction context
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_context_t* TRI_CreateTransactionContext (TRI_vocbase_t* const vocbase) {
  TRI_transaction_context_t* context;

  context = (TRI_transaction_context_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_context_t), false);

  if (context == NULL) {
    return context;
  }

  context->_vocbase = vocbase;
  
  TRI_InitAssociativePointer(&context->_collections,
                             TRI_UNKNOWN_MEM_ZONE,
                             HashKeyCid,
                             HashElementCid,
                             EqualKeyCid,
                             NULL);
  TRI_InitReadWriteLock(&context->_collectionsLock);

  return context;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the global transaction context
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeTransactionContext (TRI_transaction_context_t* const context) {
  TRI_DestroyAssociativePointer(&context->_collections);
  TRI_DestroyReadWriteLock(&context->_collectionsLock);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, context);
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
/// @brief free all data associated with a specific collection
/// this function gets called for all collections that are dropped
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveCollectionTransactionContext (TRI_transaction_context_t* context,
                                             const TRI_transaction_cid_t cid) {
  TRI_transaction_collection_global_t* globalInstance;

  globalInstance = GetGlobalInstance(context, cid, false);
  
  if (globalInstance != NULL) {
    // write lock and remove the instance
    TRI_WriteLockReadWriteLock(&context->_collectionsLock);
    TRI_RemoveKeyAssociativePointer(&context->_collections, &cid);
    TRI_WriteUnlockReadWriteLock(&context->_collectionsLock);

    FreeGlobalInstance(globalInstance);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief populates a struct with transaction statistics for a collections
////////////////////////////////////////////////////////////////////////////////

int TRI_StatsCollectionTransactionContext (TRI_transaction_context_t* context,
                                           const TRI_transaction_cid_t cid,
                                           TRI_transaction_collection_stats_t* stats) {
  TRI_transaction_collection_global_t* globalInstance;

  globalInstance = GetGlobalInstance(context, cid, false);

  if (globalInstance == NULL) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  TRI_ReadLockReadWriteLock(&globalInstance->_lock);
  memcpy(stats, &globalInstance->_stats, sizeof(TRI_transaction_collection_stats_t));
  TRI_ReadUnlockReadWriteLock(&globalInstance->_lock);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       TRANSACTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return the status of the transaction as a string
////////////////////////////////////////////////////////////////////////////////

static const char* StatusTransaction (const TRI_transaction_status_e status) {
  switch (status) {
    case TRI_TRANSACTION_UNDEFINED:
      return "undefined";
    case TRI_TRANSACTION_CREATED:
      return "created";
    case TRI_TRANSACTION_RUNNING:
      return "running";
    case TRI_TRANSACTION_COMMITTED:
      return "committed";
    case TRI_TRANSACTION_ABORTED:
      return "aborted";
    case TRI_TRANSACTION_FAILED:
      return "failed";
  }

  assert(false);
  return "unknown";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find a collection in the transaction's list of collections
////////////////////////////////////////////////////////////////////////////////

static TRI_transaction_collection_t* FindCollection (const TRI_transaction_t* const trx,
                                                     const TRI_transaction_cid_t cid,
                                                     size_t* position) {
  size_t i, n;

  n = trx->_collections._length;
  
  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* collection = TRI_AtVectorPointer(&trx->_collections, i);
    
    if (cid < collection->_cid) {
      // collection not found
      break;
    }

    if (cid == collection->_cid) {
      // found
      return collection;
    }

    // next
  }
    
  if (position != NULL) {
    // update the insert position if required
    *position = i;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a transaction collection container
////////////////////////////////////////////////////////////////////////////////

static TRI_transaction_collection_t* CreateCollection (TRI_transaction_context_t* context,
                                                       const TRI_transaction_cid_t cid,
                                                       const TRI_transaction_type_e accessType,
                                                       const int nestingLevel) {
  TRI_transaction_collection_t* collection;
  TRI_transaction_collection_global_t* globalInstance;
  
  globalInstance = GetGlobalInstance(context, cid, true);

  if (globalInstance == NULL) {
    return NULL;
  }

  collection = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_collection_t), false);

  if (collection == NULL) {
    // OOM
    return NULL;
  }

  // initialise collection properties
  collection->_cid            = cid;
  collection->_accessType     = accessType;
  collection->_nestingLevel   = nestingLevel;
  collection->_collection     = NULL;
  collection->_locked         = false;
  collection->_globalInstance = globalInstance;

  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a transaction collection container
////////////////////////////////////////////////////////////////////////////////

static void FreeCollection (TRI_transaction_collection_t* collection) {
  TRI_ASSERT_DEBUG(collection != NULL);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lock a collection
////////////////////////////////////////////////////////////////////////////////

static int LockCollection (TRI_transaction_t* trx,
                           TRI_transaction_collection_t* collection,
                           const TRI_transaction_type_e type,
                           const int nestingLevel) {
  TRI_primary_collection_t* primary;

  TRI_ASSERT_DEBUG(collection != NULL);
  TRI_ASSERT_DEBUG(collection->_collection != NULL);
  TRI_ASSERT_DEBUG(collection->_collection->_collection != NULL);
  TRI_ASSERT_DEBUG(collection->_locked == false);

  primary = collection->_collection->_collection;

  if (type == TRI_TRANSACTION_READ) {
    LOG_TRX(trx, nestingLevel, "read-locking collection %llu", (unsigned long long) collection->_cid);
    primary->beginRead(primary);
  }
  else {
    LOG_TRX(trx, nestingLevel, "write-locking collection %llu", (unsigned long long) collection->_cid);
    primary->beginWrite(primary);
  }

  collection->_locked = true;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlock a collection
////////////////////////////////////////////////////////////////////////////////

static int UnlockCollection (TRI_transaction_t* trx,
                             TRI_transaction_collection_t* collection,
                             const TRI_transaction_type_e type,
                             const int nestingLevel) {
  TRI_primary_collection_t* primary;

  TRI_ASSERT_DEBUG(collection != NULL);
  TRI_ASSERT_DEBUG(collection->_collection != NULL);
  TRI_ASSERT_DEBUG(collection->_collection->_collection != NULL);
  TRI_ASSERT_DEBUG(collection->_locked == true);

  primary = collection->_collection->_collection;

  if (type == TRI_TRANSACTION_READ) {
    LOG_TRX(trx, nestingLevel, "read-unlocking collection %llu", (unsigned long long) collection->_cid);
    primary->endRead(primary);
  }
  else {
    LOG_TRX(trx, nestingLevel, "write-unlocking collection %llu", (unsigned long long) collection->_cid);
    primary->endWrite(primary);
  }

  collection->_locked = false;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief notify all collections of a transaction status change
////////////////////////////////////////////////////////////////////////////////

static int NotifyCollections (TRI_transaction_t* const trx,
                              const TRI_transaction_status_e status) {
  size_t i, n;

  n = trx->_collections._length;

  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* collection;

    collection = (TRI_transaction_collection_t*) TRI_AtVectorPointer(&trx->_collections, i);

    if (collection->_collection != NULL &&
        collection->_collection->_collection != NULL) {
      TRI_primary_collection_t* primary;
      bool lock;
      
      primary = collection->_collection->_collection;
      lock = ! collection->_locked;

      if (lock) {
        primary->beginRead(primary);
      }

      // TODO: adjust the signature and pass the transaction in
      primary->notifyTransaction(primary, status);

      if (lock) {
        primary->endRead(primary);
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief use all participating collections of a transaction
////////////////////////////////////////////////////////////////////////////////

static int UseCollections (TRI_transaction_t* const trx,
                           const int nestingLevel) {
  size_t i, n;

  n = trx->_collections._length;

  // process collections in forward order
  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* collection;

    collection = (TRI_transaction_collection_t*) TRI_AtVectorPointer(&trx->_collections, i);

    if (collection->_nestingLevel != nestingLevel) {
      // only process our own collections
      continue;
    }
    
    if (collection->_collection == NULL) {
      LOG_TRX(trx, nestingLevel, "using collection %llu", (unsigned long long) collection->_cid);
      collection->_collection = TRI_UseCollectionByIdVocBase(trx->_context->_vocbase, collection->_cid);

      if (collection->_collection == NULL) {
        // something went wrong
        return TRI_errno();
      }
    }

    TRI_ASSERT_DEBUG(collection->_collection != NULL);
    
    if (! collection->_locked &&
        ((trx->_hints & (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_LOCK_ENTIRELY) != 0)) {
      int res = LockCollection(trx, collection, collection->_accessType, nestingLevel);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief release collection locks for a transaction
////////////////////////////////////////////////////////////////////////////////

static int ReleaseCollections (TRI_transaction_t* const trx,
                               const int nestingLevel) {
  size_t i;
  int res;

  res = TRI_ERROR_NO_ERROR;

  i = trx->_collections._length;

  // process collections in reverse order
  while (i-- > 0) {
    TRI_transaction_collection_t* collection;

    collection = TRI_AtVectorPointer(&trx->_collections, i);

    if (collection->_locked && 
        (nestingLevel == 0 || collection->_nestingLevel == nestingLevel)) {
      // unlock our own locks
      UnlockCollection(trx, collection, collection->_accessType, nestingLevel);
    }

    // the top level transaction releases all collections
    if (nestingLevel == 0 && collection->_collection != NULL) {
      // unuse collection
      LOG_TRX(trx, nestingLevel, "unusing collection %llu", (unsigned long long) collection->_cid);
      TRI_ReleaseCollectionVocBase(trx->_context->_vocbase, collection->_collection);

      collection->_locked = false;
      collection->_collection = NULL;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update the status of a transaction
////////////////////////////////////////////////////////////////////////////////

static int UpdateTransactionStatus (TRI_transaction_t* const trx,
                                    const TRI_transaction_status_e status) {

  assert(trx->_status == TRI_TRANSACTION_CREATED || trx->_status == TRI_TRANSACTION_RUNNING);

  if (trx->_status == TRI_TRANSACTION_CREATED) {
    TRI_ASSERT_DEBUG(status == TRI_TRANSACTION_RUNNING || status == TRI_TRANSACTION_FAILED);
  }
  else if (trx->_status == TRI_TRANSACTION_RUNNING) {
    TRI_ASSERT_DEBUG(status == TRI_TRANSACTION_COMMITTED || status == TRI_TRANSACTION_ABORTED);
  }

  if (status == TRI_TRANSACTION_RUNNING ||
      status == TRI_TRANSACTION_ABORTED ||
      status == TRI_TRANSACTION_COMMITTED) {
    // notify all participating collections about the status change
    UpdateGlobalStats(trx, status);

    // update the global statistics
    NotifyCollections(trx, status);
  }

  trx->_status = status;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new transaction container
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_t* TRI_CreateTransaction (TRI_transaction_context_t* const context) {
  TRI_transaction_t* trx;

  trx = (TRI_transaction_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_t), false);

  if (trx == NULL) {
    // out of memory
    return NULL;
  }

  trx->_context      = context;

  // note: the real transaction id will be acquired on transaction start
  trx->_id           = 0; 
  
  trx->_status       = TRI_TRANSACTION_CREATED;
  trx->_type         = TRI_TRANSACTION_READ;
  trx->_hints        = 0;
  trx->_nestingLevel = 0;

  TRI_InitVectorPointer2(&trx->_collections, TRI_UNKNOWN_MEM_ZONE, 2);

  return trx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a transaction container
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeTransaction (TRI_transaction_t* const trx) {
  size_t i;

  TRI_ASSERT_DEBUG(trx != NULL);

  if (trx->_status == TRI_TRANSACTION_RUNNING) {
    TRI_AbortTransaction(trx, 0);
  }

  // free all collections
  i = trx->_collections._length;
  while (i-- > 0) {
    TRI_transaction_collection_t* collection = TRI_AtVectorPointer(&trx->_collections, i);

    FreeCollection(collection);
  }

  TRI_DestroyVectorPointer(&trx->_collections);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, trx);
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
/// @brief request a lock for a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_LockCollectionTransaction (TRI_transaction_t* const trx,
                                   const TRI_transaction_cid_t cid,
                                   const TRI_transaction_type_e accessType,
                                   const int nestingLevel) {

  TRI_transaction_collection_t* collection = FindCollection(trx, cid, NULL);

  if (collection == NULL || collection->_collection == NULL) {
    return TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION;
  }

  if (accessType == TRI_TRANSACTION_WRITE && collection->_accessType != TRI_TRANSACTION_WRITE) {
    // wrong lock type
    return TRI_ERROR_INTERNAL;
  }

  if (collection->_locked) {
    // already locked
    return TRI_ERROR_NO_ERROR;
  }

  return LockCollection(trx, collection, accessType, nestingLevel);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief request an unlock for a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_UnlockCollectionTransaction (TRI_transaction_t* const trx,
                                     const TRI_transaction_cid_t cid,
                                     const TRI_transaction_type_e accessType,
                                     const int nestingLevel) {
  
  TRI_transaction_collection_t* collection = FindCollection(trx, cid, NULL);
  
  if (collection == NULL || collection->_collection == NULL) {
    return TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION;
  }
  
  if (accessType == TRI_TRANSACTION_WRITE && collection->_accessType != TRI_TRANSACTION_WRITE) {
    // wrong lock type
    return TRI_ERROR_INTERNAL;
  }

  if (! collection->_locked) {
    // already unlocked
    return TRI_ERROR_NO_ERROR;
  }

  return UnlockCollection(trx, collection, accessType, nestingLevel);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a collection is locked in a transaction
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsLockedCollectionTransaction (TRI_transaction_t* const trx,
                                        const TRI_transaction_cid_t cid,
                                        const TRI_transaction_type_e accessType,
                                        const int nestingLevel) {

  TRI_transaction_collection_t* collection = FindCollection(trx, cid, NULL);

  if (collection == NULL || collection->_collection == NULL) {
    // unknown collection
    LOG_WARNING("logic error. checking lock status for a non-registered collection");
    return false;
  }
  
  if (accessType == TRI_TRANSACTION_WRITE && collection->_accessType != TRI_TRANSACTION_WRITE) {
    // wrong lock type
    LOG_WARNING("logic error. checking wrong lock type");
    return false;
  }

  return collection->_locked;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a collection is contained in a transaction and return it
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_GetCollectionTransaction (TRI_transaction_t* const trx,
                                                 const TRI_transaction_cid_t cid,
                                                 const TRI_transaction_type_e accessType) {
  
  TRI_transaction_collection_t* collection;
  
  TRI_ASSERT_DEBUG(trx->_status == TRI_TRANSACTION_CREATED ||
                   trx->_status == TRI_TRANSACTION_RUNNING);
   
  collection = FindCollection(trx, cid, NULL);
  
  if (collection == NULL || collection->_collection == NULL) {
    return NULL;
  }

  // check if access type matches
  if (accessType == TRI_TRANSACTION_WRITE && collection->_accessType == TRI_TRANSACTION_READ) {
    // type doesn't match
    return NULL;
  }

  return collection->_collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_AddCollectionTransaction (TRI_transaction_t* const trx,
                                  const TRI_transaction_cid_t cid,
                                  const TRI_transaction_type_e accessType,
                                  const int nestingLevel) {

  TRI_transaction_collection_t* collection;
  size_t position;
    
  LOG_TRX(trx, nestingLevel, "adding collection %llu", (unsigned long long) cid);
          
  // upgrade transaction type if required
  if (nestingLevel == 0) {
    TRI_ASSERT_DEBUG(trx->_status == TRI_TRANSACTION_CREATED);

    if (accessType == TRI_TRANSACTION_WRITE && 
        trx->_type == TRI_TRANSACTION_READ) {
      // if one collection is written to, the whole transaction becomes a write-transaction
      trx->_type = TRI_TRANSACTION_WRITE;
    }
  }

  // check if we already have got this collection in the _collections vector
  collection = FindCollection(trx, cid, &position);

  if (collection != NULL) {
    // collection is already contained in vector
   
    if (accessType == TRI_TRANSACTION_WRITE && collection->_accessType != accessType) {
      if (nestingLevel > 0) {
        // trying to write access a collection that is only marked with read-access 
        return TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION;
      }

      TRI_ASSERT_DEBUG(nestingLevel == 0);

      // upgrade collection type to write-access
      collection->_accessType = TRI_TRANSACTION_WRITE;
    } 

    if (nestingLevel < collection->_nestingLevel) {
      collection->_nestingLevel = nestingLevel;
    }

    // all correct
    return TRI_ERROR_NO_ERROR;
  }


  // collection not found.

  if (nestingLevel > 0 && accessType == TRI_TRANSACTION_WRITE) {
    // trying to write access a collection in an embedded transaction
    return TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION;
  }

  // collection was not contained. now create and insert it
  collection = CreateCollection(trx->_context, cid, accessType, nestingLevel);

  if (collection == NULL) {
    // out of memory
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // insert collection at the correct position
  if (TRI_InsertVectorPointer(&trx->_collections, collection, position) != TRI_ERROR_NO_ERROR) {
    FreeCollection(collection);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_BeginTransaction (TRI_transaction_t* const trx, 
                          TRI_transaction_hint_t hints,
                          const int nestingLevel) {
  int res;

  LOG_TRX(trx, nestingLevel, "%s transaction", "beginning");

  if (nestingLevel == 0) {
    TRI_ASSERT_DEBUG(trx->_status == TRI_TRANSACTION_CREATED);

    trx->_id = TRI_NewTickVocBase();
    trx->_hints = hints;
  }
  else {
    TRI_ASSERT_DEBUG(trx->_status == TRI_TRANSACTION_RUNNING);
  }

  res = UseCollections(trx, nestingLevel);

  if (res == TRI_ERROR_NO_ERROR) {
    // all valid
    if (nestingLevel == 0) {
      UpdateTransactionStatus(trx, TRI_TRANSACTION_RUNNING);
    }
  }
  else {
    // something is wrong
    if (nestingLevel == 0) {
      UpdateTransactionStatus(trx, TRI_TRANSACTION_FAILED);
    }

    // free what we have got so far
    ReleaseCollections(trx, nestingLevel);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_CommitTransaction (TRI_transaction_t* const trx,
                           const int nestingLevel) {
  int res;

  LOG_TRX(trx, nestingLevel, "%s transaction", "committing");

  TRI_ASSERT_DEBUG(trx->_status == TRI_TRANSACTION_RUNNING);

  if (nestingLevel == 0) {
    res = UpdateTransactionStatus(trx, TRI_TRANSACTION_COMMITTED);
  }
  else {
    res = TRI_ERROR_NO_ERROR;
  }

  ReleaseCollections(trx, nestingLevel);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief abort a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_AbortTransaction (TRI_transaction_t* const trx,
                          const int nestingLevel) {
  int res;

  LOG_TRX(trx, nestingLevel, "%s transaction", "aborting");

  TRI_ASSERT_DEBUG(trx->_status == TRI_TRANSACTION_RUNNING);

  if (nestingLevel == 0) {
    res = UpdateTransactionStatus(trx, TRI_TRANSACTION_ABORTED);
  }
  else {
    res = TRI_ERROR_NO_ERROR;
  }

  ReleaseCollections(trx, nestingLevel);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
