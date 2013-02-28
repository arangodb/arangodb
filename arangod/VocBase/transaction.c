////////////////////////////////////////////////////////////////////////////////
/// @brief transaction subsystem
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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

#include "transaction.h" 

#include "BasicsC/logging.h" 
#include "BasicsC/strings.h"
 
#include "VocBase/primary-collection.h"
#include "VocBase/vocbase.h"

// -----------------------------------------------------------------------------
// --SECTION--                                               TRANSACTION CONTEXT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hash a collection
////////////////////////////////////////////////////////////////////////////////

#if 0
static uint64_t HashCollection (TRI_associative_pointer_t* array, 
                                void const* element) {
  TRI_transaction_collection_global_t* collection = (TRI_transaction_collection_global_t*) element;

  return TRI_FnvHashPointer((void const*) &(collection->_cid), sizeof(TRI_transaction_cid_t));
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief comparison function used to determine collection equality
////////////////////////////////////////////////////////////////////////////////

#if 0
static bool IsEqualCollectionId (TRI_associative_pointer_t* array, 
                                 void const* key, 
                                 void const* element) {
  TRI_transaction_collection_global_t* collection = (TRI_transaction_collection_global_t*) element;

  return *((TRI_transaction_cid_t*) key) == collection->_cid;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the transaction as a string
////////////////////////////////////////////////////////////////////////////////

#if 0
static const char* TypeString (const TRI_transaction_type_e type) {
  switch (type) {
    case TRI_TRANSACTION_READ:
      return "read";
    case TRI_TRANSACTION_WRITE:
      return "write";
  }

  assert(false);
  return "unknown";
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief return the status of the transaction as a string
////////////////////////////////////////////////////////////////////////////////

#if 0
static const char* StatusString (const TRI_transaction_status_e status) {
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
    case TRI_TRANSACTION_FINISHED:
      return "finished";
    case TRI_TRANSACTION_FAILED:
      return "failed";
  }

  assert(false);
  return "unknown";
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a transaction id
/// The context lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

#if 0
static TRI_transaction_local_id_t NextLocalTransactionId (TRI_transaction_context_t* const context) {
  TRI_transaction_local_id_t id;

  id = ++context->_id._localId;
  return (TRI_transaction_local_id_t) id;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief register a transaction in the global transactions list
/// The context lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

#if 0
static int InsertTransactionList (TRI_transaction_list_t* const list, 
                                  TRI_transaction_t* const trx) {
  const TRI_transaction_local_id_t id = trx->_id._localId; 
  int res;
  TRI_transaction_list_entry_t entry;

  assert(trx->_status == TRI_TRANSACTION_RUNNING);

  entry._id = id;
  entry._status = TRI_TRANSACTION_RUNNING;

  res = TRI_PushBackVector(&list->_vector, &entry);
  if (res == TRI_ERROR_NO_ERROR) {
    ++list->_numRunning;
  }

  return res;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief locate a transaction in the global transactions list using a
/// binary search
/// The context lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

#if 0
static TRI_transaction_list_entry_t* FindTransactionList (const TRI_transaction_list_t* const list,
                                                          const TRI_transaction_local_id_t id,
                                                          size_t* position) {

  TRI_transaction_list_entry_t* entry;
  size_t l, r, n;
    
  LOG_TRACE("looking up transaction %lu", (unsigned long) id);

  n = list->_vector._length;
  if (n == 0) {
    return NULL;
  }

  l = 0;
  r = n - 1;
  while (true) {
    const size_t i = l + ((r - l) / 2);

    if (r < l) {
      // transaction not found
      return NULL;
    }

    entry = (TRI_transaction_list_entry_t*) TRI_AtVector(&list->_vector, i);
    assert(entry);

    if (entry->_id == id) {
      // found the transaction
      *position = i;

      return entry;
    }

    if (entry->_id > id) {
      r = i - 1;
    }
    else {
      l = i + 1;
    }
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a transaction from the global transactions list
/// The context lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

#if 0
static int RemoveTransactionList (TRI_transaction_list_t* const list, 
                                  const TRI_transaction_local_id_t id) {
  TRI_transaction_list_entry_t* entry;
  size_t position;

  entry = FindTransactionList(list, id, &position);
  if (entry == NULL) {
    // transaction not found, should not happen
    LOG_ERROR("logical error in transaction list");
    return TRI_ERROR_INTERNAL;
  }

  assert(entry);

  // update counters
  if (entry->_status == TRI_TRANSACTION_RUNNING) {
    --list->_numRunning;
  }
  else {
    LOG_ERROR("logical error in transaction list");
    assert(false);
  }

  // remove it from the vector
  TRI_RemoveVector(&list->_vector, position);

  return TRI_ERROR_NO_ERROR;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a transaction from the global transactions list
/// The context lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

#if 0
static int UpdateTransactionList (TRI_transaction_list_t* const list, 
                                  const TRI_transaction_local_id_t id,
                                  const TRI_transaction_status_e status) {
  TRI_transaction_list_entry_t* entry;
  size_t position;

  assert(status == TRI_TRANSACTION_ABORTED);

  entry = FindTransactionList(list, id, &position);
  if (entry == NULL) {
    // transaction not found, should not happen
    LOG_ERROR("logical error in transaction list");
    return TRI_ERROR_INTERNAL;
  }

  // update counters
  if (entry->_status == TRI_TRANSACTION_RUNNING) {
    --list->_numRunning;
  }
  else {
    LOG_ERROR("logical error in transaction list");
  }

  if (status == TRI_TRANSACTION_ABORTED) {
    ++list->_numAborted;
  }

  entry->_status = status;
  
  return TRI_ERROR_NO_ERROR;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a transactions list
////////////////////////////////////////////////////////////////////////////////

#if 0
static void InitTransactionList (TRI_transaction_list_t* const list) {
  TRI_InitVector(&list->_vector, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_list_entry_t));
  list->_numRunning = 0;
  list->_numAborted = 0;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a transactions list
////////////////////////////////////////////////////////////////////////////////

#if 0
static void DestroyTransactionList (TRI_transaction_list_t* const list) {
  TRI_DestroyVector(&list->_vector);
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief dump the contents of a transaction list
////////////////////////////////////////////////////////////////////////////////

#if 0
static void DumpTransactionList (const TRI_transaction_list_t* const list) {
  size_t i, n;

  n = list->_vector._length;
  for (i = 0; i < n; ++i) {
    TRI_transaction_list_entry_t* entry = TRI_AtVector(&list->_vector, i);

    LOG_INFO("- trx: #%lu, status: %s", 
             (unsigned long) entry->_id,
             StatusString(entry->_status));
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection for the global context
////////////////////////////////////////////////////////////////////////////////

#if 0
TRI_transaction_collection_global_t* CreateCollectionGlobalInstance (const TRI_transaction_collection_t* const collection) {
  TRI_transaction_collection_global_t* globalInstance;

  globalInstance = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_collection_global_t), false);
  if (globalInstance == NULL) {
    // OOM
    return NULL;
  }

  globalInstance->_cid = collection->_cid;

  TRI_InitMutex(&globalInstance->_writeLock);
  InitTransactionList(&globalInstance->_writeTransactions);

  return globalInstance;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief free a collection from the global context
////////////////////////////////////////////////////////////////////////////////

#if 0
void FreeCollectionGlobalInstance (TRI_transaction_collection_global_t* const globalInstance) {
  DestroyTransactionList(&globalInstance->_writeTransactions);
  TRI_DestroyMutex(&globalInstance->_writeLock);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, globalInstance);
}
#endif

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

TRI_transaction_context_t* TRI_CreateTransactionContext (TRI_vocbase_t* const vocbase,
                                                         TRI_transaction_server_id_t serverId) {
  TRI_transaction_context_t* context;

  context = (TRI_transaction_context_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_context_t), false);
  if (context == NULL) {
    return context;
  }

#if 0  
  TRI_InitMutex(&context->_lock);
  TRI_InitMutex(&context->_collectionLock);

  // create hashes
  TRI_InitAssociativePointer(&context->_collections,
                             TRI_UNKNOWN_MEM_ZONE,
                             TRI_HashStringKeyAssociativePointer, 
                             HashCollection,
                             IsEqualCollectionId, 
                             NULL);
 
  // setup global transaction lists
  InitTransactionList(&context->_readTransactions);
  InitTransactionList(&context->_writeTransactions);
#endif

  context->_vocbase = vocbase;
  context->_id._localId  = 0;
  context->_id._serverId = serverId;

  return context;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the global transaction context
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeTransactionContext (TRI_transaction_context_t* const context) {
#if 0
  uint32_t i, n;

  // destroy global transaction lists
  DestroyTransactionList(&context->_writeTransactions);
  DestroyTransactionList(&context->_readTransactions);

  // destroy global instances of collections
  n = context->_collections._nrAlloc;
  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_global_t* globalInstance;

    globalInstance = (TRI_transaction_collection_global_t*) context->_collections._table[i];
    if (globalInstance != NULL) {
      FreeCollectionGlobalInstance(globalInstance);
    }
  }
  TRI_DestroyAssociativePointer(&context->_collections);

  // destroy mutexes
  TRI_DestroyMutex(&context->_lock);  
  TRI_DestroyMutex(&context->_collectionLock);
#endif

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

void TRI_RemoveCollectionTransactionContext (TRI_transaction_context_t* const context, 
                                             const TRI_transaction_cid_t cid) {
#if 0  
  TRI_transaction_collection_global_t* collection;

  // start critical section -----------------------------------------
  TRI_LockMutex(&context->_collectionLock); 
    
  // TODO: must not remove collections with transactions going on
  collection = (TRI_transaction_collection_global_t*) TRI_RemoveKeyAssociativePointer(&context->_collections, &cid);

  TRI_UnlockMutex(&context->_collectionLock); 
  // end critical section -----------------------------------------

  if (collection != NULL) {
    FreeCollectionGlobalInstance(collection);
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump transaction context data
////////////////////////////////////////////////////////////////////////////////

void TRI_DumpTransactionContext (TRI_transaction_context_t* const context) {
#if 0  
  TRI_LockMutex(&context->_lock);

  LOG_INFO("transaction context, last-id: %lu:%lu", 
           (unsigned long) context->_id._serverId, 
           (unsigned long) context->_id._localId);
  
  LOG_INFO("read transactions: numRunning: %lu, length: %lu, aborted: %lu", 
            (unsigned long) context->_readTransactions._vector._length,
            (unsigned long) context->_readTransactions._numRunning,
            (unsigned long) context->_readTransactions._numAborted);
 
  DumpTransactionList(&context->_readTransactions);
  
  LOG_INFO("write transactions: numRunning: %lu, length: %lu, aborted: %lu", 
            (unsigned long) context->_writeTransactions._vector._length,
            (unsigned long) context->_writeTransactions._numRunning,
            (unsigned long) context->_writeTransactions._numAborted);

  DumpTransactionList(&context->_writeTransactions);

  TRI_UnlockMutex(&context->_lock);
#endif
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
/// @brief create a transaction collection container
////////////////////////////////////////////////////////////////////////////////

static TRI_transaction_collection_t* CreateCollection (const TRI_transaction_cid_t cid, 
                                                       const TRI_transaction_type_e type) {
  TRI_transaction_collection_t* collection;

  collection = (TRI_transaction_collection_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_collection_t), false);
  if (collection == NULL) {
    // OOM
    return NULL;
  }

  // initialise collection properties
  collection->_cid            = cid;
  collection->_type           = type;
  collection->_collection     = NULL;
#if 0
  collection->_globalInstance = NULL;
  collection->_globalLock     = false;
#endif
  collection->_locked         = false;

#if 0
  // initialise private copy of write transactions list
  InitTransactionList(&collection->_writeTransactions);
#endif
  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a transaction collection container
////////////////////////////////////////////////////////////////////////////////

static void FreeCollection (TRI_transaction_collection_t* collection) {
  assert(collection);

#if 0
  DestroyTransactionList(&collection->_writeTransactions);
#endif

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lock a collection
////////////////////////////////////////////////////////////////////////////////

static int LockCollection (TRI_transaction_collection_t* collection,
                          const TRI_transaction_type_e type) {
  TRI_primary_collection_t* primary;

  assert(collection != NULL);
  assert(collection->_collection != NULL);
  assert(collection->_collection->_collection != NULL);
  assert(collection->_locked == false);

  primary = collection->_collection->_collection;

  if (type == TRI_TRANSACTION_READ) {
    primary->beginRead(primary);
  }
  else {
    primary->beginWrite(primary);
  }

  collection->_locked = true;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlock a collection
////////////////////////////////////////////////////////////////////////////////

static int UnlockCollection (TRI_transaction_collection_t* collection,
                          const TRI_transaction_type_e type) {
  TRI_primary_collection_t* primary;

  assert(collection != NULL);
  assert(collection->_collection != NULL);
  assert(collection->_collection->_collection != NULL);
  assert(collection->_locked == true);

  primary = collection->_collection->_collection;

  if (type == TRI_TRANSACTION_READ) {
    primary->endRead(primary);
  }
  else {
    primary->endWrite(primary);
  }

  collection->_locked = false;
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a global instance for the collection
/// this function will create a global instance if it does not yet exist
////////////////////////////////////////////////////////////////////////////////

#if 0
static TRI_transaction_collection_global_t* GetGlobalCollection (TRI_transaction_context_t* const context,
                                                                 const TRI_transaction_collection_t* const collection) {
  TRI_transaction_collection_global_t* globalInstance;

  // start critical section -----------------------------------------
  TRI_LockMutex(&context->_collectionLock); 

  globalInstance = (TRI_transaction_collection_global_t*) TRI_LookupByKeyAssociativePointer(&context->_collections, &(collection->_cid));
  if (globalInstance == NULL) {
    globalInstance = CreateCollectionGlobalInstance(collection);
    if (globalInstance != NULL) {
      TRI_InsertKeyAssociativePointer(&context->_collections, &(collection->_cid), globalInstance, false); 
    }
  }

  TRI_UnlockMutex(&context->_collectionLock); 
  // end critical section -----------------------------------------

  return globalInstance;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief use all participating collections of a transaction
////////////////////////////////////////////////////////////////////////////////

static int UseCollections (TRI_transaction_t* const trx) {
#if 0  
  TRI_transaction_context_t* context;
#endif  
  size_t i, n;
  
  LOG_DEBUG("acquiring collection locks");

#if 0  
  context = trx->_context;
#endif

  n = trx->_collections._length;

  // process collections in forward order
  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* collection;

    collection = (TRI_transaction_collection_t*) TRI_AtVectorPointer(&trx->_collections, i);

    LOG_DEBUG("using collection %llu", (unsigned long long) collection->_cid);
    collection->_collection = TRI_UseCollectionByIdVocBase(trx->_context->_vocbase, collection->_cid);

    if (collection->_collection == NULL) {
      return TRI_errno();
    }

#if 0
    collection->_globalInstance = GetGlobalCollection(context, collection);
    if (collection->_globalInstance == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
#endif

    if (collection->_type == TRI_TRANSACTION_WRITE) {
      LOG_DEBUG("acquiring write-lock on collection %llu", (unsigned long long) collection->_cid);

#if 0
      // acquire write-lock on collection
      TRI_LockMutex(&collection->_globalInstance->_writeLock);
      collection->_globalLock = true;
#endif
    }
 
    if ((trx->_hints & (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_IMPLICIT_LOCK) != 0) {
      int res = LockCollection(collection, collection->_type);

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

static int ReleaseCollections (TRI_transaction_t* const trx) {
  size_t i;
  int res;

  LOG_DEBUG("releasing collections");
  
  res = TRI_ERROR_NO_ERROR;

  i = trx->_collections._length;

  // process collections in reverse order
  while (i-- > 0) {
    TRI_transaction_collection_t* collection;

    collection = TRI_AtVectorPointer(&trx->_collections, i);
    if (collection->_collection == NULL) {
      continue;
    }
    
    if (collection->_locked) { 
      UnlockCollection(collection, collection->_type);
    }

#if 0
    if (collection->_type == TRI_TRANSACTION_WRITE && collection->_globalLock) {
      LOG_DEBUG("releasing trx exclusive lock on collection %llu", (unsigned long long) collection->_cid);

      // release write-lock on collection
      TRI_UnlockMutex(&collection->_globalInstance->_writeLock);

      collection->_globalLock = false;
    }
#endif

    // unuse collection
    LOG_DEBUG("unusing collection %llu", (unsigned long long) collection->_cid);
    TRI_ReleaseCollectionVocBase(trx->_context->_vocbase, collection->_collection);
    
    collection->_collection = NULL;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a private copy of all write transactions from a global
/// collection transaction list
////////////////////////////////////////////////////////////////////////////////

#if 0
static int CloneTransactionList (TRI_transaction_list_t* const dest,
                                 const TRI_transaction_list_t* const source) {
  dest->_numRunning = source->_numRunning;
  dest->_numAborted = source->_numAborted;

  return TRI_CopyDataVector(&dest->_vector, &source->_vector);
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief make private copies of other write-transactions for each collection
/// participating in the current transaction
/// The context lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

#if 0
static int CopyWriteTransactions (TRI_transaction_t* const trx) {
  size_t i, n;

  n = trx->_collections._length;

  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* collection;
    int res;
    
    collection = (TRI_transaction_collection_t*) TRI_AtVectorPointer(&trx->_collections, i);
    // make a private copy of other write transactions for the collection
    res = CloneTransactionList(&collection->_writeTransactions, &collection->_globalInstance->_writeTransactions);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    // check if this is a write transaction
    if (collection->_type == TRI_TRANSACTION_WRITE) {
      // insert the current transaction into the global collection-specific list of write transactions
      res = InsertTransactionList(&collection->_globalInstance->_writeTransactions, trx);
      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a transaction from all collections' write transaction lists
/// The context lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

#if 0
static int RemoveCollectionsWriteTransaction (TRI_transaction_t* const trx, 
                                              const TRI_transaction_local_id_t id) {
  size_t i, n;

  n = trx->_collections._length;

  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* collection;
    int res;
    
    collection = (TRI_transaction_collection_t*) TRI_AtVectorPointer(&trx->_collections, i);

    if (collection->_type == TRI_TRANSACTION_READ) {
      continue;
    }

    if (collection->_globalInstance == NULL) {
      // should not happen
      assert(false);
    }

    res = RemoveTransactionList(&collection->_globalInstance->_writeTransactions, id); 
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief update the status of a transaction in all participating collections' 
/// write transaction lists
/// The context lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

#if 0
static int UpdateCollectionsWriteTransaction (TRI_transaction_t* const trx, 
                                              const TRI_transaction_local_id_t id,
                                              const TRI_transaction_status_e status) {
  size_t i, n;

  n = trx->_collections._length;

  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* collection;
    int res;
    
    collection = (TRI_transaction_collection_t*) TRI_AtVectorPointer(&trx->_collections, i);

    if (collection->_type == TRI_TRANSACTION_READ) {
      continue;
    }

    if (collection->_globalInstance == NULL) {
      // should not happen
      assert(false);
    }

    res = UpdateTransactionList(&collection->_globalInstance->_writeTransactions, id, status); 
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief update the status of a transaction 
////////////////////////////////////////////////////////////////////////////////

static int UpdateTransactionStatus (TRI_transaction_t* const trx, 
                                    const TRI_transaction_status_e status) {
#if 0  
  const TRI_transaction_local_id_t id = trx->_id._localId;
  TRI_transaction_context_t* context;
#endif
  int res;

  assert(trx->_status == TRI_TRANSACTION_RUNNING);
  assert(status == TRI_TRANSACTION_COMMITTED || 
         status == TRI_TRANSACTION_ABORTED || 
         status == TRI_TRANSACTION_FINISHED);
#if 0
  context = trx->_context;

  // start critical section -----------------------------------------
  TRI_LockMutex(&context->_lock); 

  if (trx->_type == TRI_TRANSACTION_READ) {
    // read-only transactions cannot commit or roll-back
    assert(status == TRI_TRANSACTION_ABORTED ||
           status == TRI_TRANSACTION_FINISHED);

    LOG_TRACE("removing read transaction %lu", (unsigned long) id);
    res = RemoveTransactionList(&context->_readTransactions, id);
  }
  else {
    // write transactions
    
    if (status == TRI_TRANSACTION_COMMITTED) {
      LOG_TRACE("removing write transaction %lu", (unsigned long) id);
      RemoveCollectionsWriteTransaction(trx, id);
      res = RemoveTransactionList(&context->_writeTransactions, id);
    }
    else if (status == TRI_TRANSACTION_ABORTED) {
      LOG_TRACE("updating write transaction %lu status %s", (unsigned long) id, StatusString(status));
      UpdateCollectionsWriteTransaction(trx, id, status);
      res = UpdateTransactionList(&context->_writeTransactions, id, status);
    }
    else {
      res = TRI_ERROR_INTERNAL;
    }
  }

  TRI_UnlockMutex(&context->_lock); 
  // end critical section -----------------------------------------
#else
  res = TRI_ERROR_NO_ERROR;
#endif

  if (res == TRI_ERROR_NO_ERROR) {
    trx->_status = status;
  }
  
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a transaction
////////////////////////////////////////////////////////////////////////////////

static int RegisterTransaction (TRI_transaction_t* const trx) {
#if 0  
  TRI_transaction_context_t* context;
  TRI_transaction_list_t* list;
#endif
  int res;

  assert(trx->_status == TRI_TRANSACTION_CREATED);
#if 0
  context = trx->_context;

  trx->_status = TRI_TRANSACTION_RUNNING;
  
  if (trx->_type == TRI_TRANSACTION_READ) {
    // read-only transaction
    list = &context->_readTransactions;
  }
  else {
    // write transaction
    list = &context->_writeTransactions;
  }

  // start critical section -----------------------------------------
  TRI_LockMutex(&context->_lock); 

  // create new trx id
  trx->_id._localId = NextLocalTransactionId(context);
  // insert transaction into global list of transactions
  res = InsertTransactionList(list, trx);

  // create private copies of other write-transactions
  // and insert the current transaction into the collection-specific lists of write transactions
  CopyWriteTransactions(trx);

  TRI_UnlockMutex(&context->_lock);
  // end critical section -----------------------------------------
#else
  res = TRI_ERROR_NO_ERROR;
#endif
  return res;
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

TRI_transaction_t* TRI_CreateTransaction (TRI_transaction_context_t* const context,
                                          const TRI_transaction_isolation_level_e isolationLevel) {
  TRI_transaction_t* trx;
  
  trx = (TRI_transaction_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_t), false);
  if (trx == NULL) {
    // out of memory
    return NULL;
  } 

  trx->_context           = context;
  trx->_id._serverId      = context->_id._serverId;
  trx->_id._localId       = 0;
  trx->_status            = TRI_TRANSACTION_CREATED;  
  trx->_type              = TRI_TRANSACTION_READ;
  trx->_isolationLevel    = isolationLevel;
  trx->_hints             = 0;
  
  TRI_InitVectorPointer2(&trx->_collections, TRI_UNKNOWN_MEM_ZONE, 2);

  return trx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a transaction container
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeTransaction (TRI_transaction_t* const trx) {
  size_t i;

  assert(trx);

  if (trx->_status == TRI_TRANSACTION_RUNNING) {
    TRI_AbortTransaction(trx);
  }

  ReleaseCollections(trx);

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
                                   const TRI_transaction_type_e type) {
  size_t i, n;

  n = trx->_collections._length;

  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* collection = TRI_AtVectorPointer(&trx->_collections, i);

    if (collection->_collection == NULL) {
      continue;
    }

    if (collection->_cid == cid) {
      return LockCollection(collection, type);
    }
  }

  return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief request an unlock for a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_UnlockCollectionTransaction (TRI_transaction_t* const trx,
                                     const TRI_transaction_cid_t cid,
                                     const TRI_transaction_type_e type) {
  size_t i, n;

  n = trx->_collections._length;

  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* collection = TRI_AtVectorPointer(&trx->_collections, i);

    if (collection->_collection == NULL) {
      continue;
    }

    if (collection->_cid == cid) {
      return UnlockCollection(collection, type);
    }
  }

  return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the transaction consists only of a single operation
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsSingleOperationTransaction (const TRI_transaction_t* const trx) {
  return (trx->_hints & (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_SINGLE_OPERATION) != 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the transaction spans multiple write collections
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsMultiCollectionWriteTransaction (const TRI_transaction_t* const trx) {
  size_t i, n;
  size_t writes = 0;

  if (trx->_type == TRI_TRANSACTION_READ) {
    // a read-only transaction
    return false;
  }
  
  n = trx->_collections._length;
  if (n < 2) {
    // just 1 collection
    return false;
  }

  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* collection = TRI_AtVectorPointer(&trx->_collections, i);

    if (collection->_type == TRI_TRANSACTION_WRITE) {
      if (++writes >= 2) {
        return true;
      }
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the local id of a transaction
////////////////////////////////////////////////////////////////////////////////

#if 0
TRI_transaction_local_id_t TRI_LocalIdTransaction (const TRI_transaction_t* const trx) {
  return trx->_id._localId;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief dump information about a transaction
////////////////////////////////////////////////////////////////////////////////

#if 0
void TRI_DumpTransaction (TRI_transaction_t* const trx) {
  size_t i, n;

  LOG_INFO("transaction id: %lu:%lu, type: %s, status: %s", 
           (unsigned long) trx->_id._serverId, 
           (unsigned long) trx->_id._localId, 
           TypeString(trx->_type), 
           StatusString(trx->_status));

  n = trx->_collections._length;
  for (i = 0; i < n; ++i) {
    size_t j;

    TRI_transaction_collection_t* collection = TRI_AtVectorPointer(&trx->_collections, i);

    LOG_INFO("- collection: %llu, type: %s", (unsigned long long) collection->_cid, TypeString(collection->_type));
    for (j = 0; j < collection->_writeTransactions._vector._length; ++j) {
      TRI_transaction_list_entry_t* entry;
      
      entry = (TRI_transaction_list_entry_t*) TRI_AtVector(&collection->_writeTransactions._vector, j);
      LOG_INFO("  - other write transaction: id: %lu, status: %s", 
               (unsigned long) entry->_id, 
               StatusString(entry->_status));
    }
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a collection is contained in a transaction and return it
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* TRI_CheckCollectionTransaction (TRI_transaction_t* const trx,
                                                   const TRI_transaction_cid_t cid,
                                                   const TRI_transaction_type_e type) {
  TRI_transaction_collection_t* collection;
  size_t i, n;

  assert(trx->_status == TRI_TRANSACTION_CREATED || 
         trx->_status == TRI_TRANSACTION_RUNNING);

  // check if we already have got this collection in the _collections vector
  // the vector is sorted by collection names
  n = trx->_collections._length;
  for (i = 0; i < n; ++i) {
    collection = TRI_AtVectorPointer(&trx->_collections, i);

    if (cid < collection->_cid) {
      // collection is not contained in vector
      break;
    }
    else if (cid == collection->_cid) {
      // collection is already contained in vector

      // check if access type matches
      if (type == TRI_TRANSACTION_WRITE && collection->_type == TRI_TRANSACTION_READ) {
        break;
      }

      return collection->_collection;
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_AddCollectionTransaction (TRI_transaction_t* const trx,
                                  const TRI_transaction_cid_t cid,
                                  const TRI_transaction_type_e type) {
  TRI_transaction_collection_t* collection;
  size_t i, n;

  assert(trx->_status == TRI_TRANSACTION_CREATED);

  // upgrade transaction type if required
  if (type == TRI_TRANSACTION_WRITE && trx->_type == TRI_TRANSACTION_READ) {
    // if one collection is written to, the whole transaction is a write-transaction
    trx->_type = TRI_TRANSACTION_WRITE;
  }

  // check if we already have got this collection in the _collections vector
  // the vector is sorted by collection names
  n = trx->_collections._length;
  for (i = 0; i < n; ++i) {
    collection = TRI_AtVectorPointer(&trx->_collections, i);

    if (cid < collection->_cid) {
      // collection is not contained in vector
      collection = CreateCollection(cid, type);
      if (collection == NULL) {
        // out of memory
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      TRI_InsertVectorPointer(&trx->_collections, collection, i);

      return TRI_ERROR_NO_ERROR;
    }
    else if (cid == collection->_cid) {
      // collection is already contained in vector

      // upgrade collection type if required
      if (type == TRI_TRANSACTION_WRITE && collection->_type == TRI_TRANSACTION_READ) {
        collection->_type = TRI_TRANSACTION_WRITE;
      }

      return TRI_ERROR_NO_ERROR;
    }
  }
  
  // collection was not contained. now insert it
  collection = CreateCollection(cid, type);
  if (collection == NULL) {
    // out of memory
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  TRI_PushBackVectorPointer(&trx->_collections, collection);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to an already running transaction
/// opens it and locks it in read-only mode
////////////////////////////////////////////////////////////////////////////////

int TRI_AddDelayedReadCollectionTransaction (TRI_transaction_t* const trx,
                                             const TRI_transaction_cid_t cid) {
  TRI_transaction_collection_t* collection;
  size_t i, n;
  int res;

  n = trx->_collections._length;
  for (i = 0; i < n; ++i) {
    collection = TRI_AtVectorPointer(&trx->_collections, i);

    if (cid < collection->_cid) {
      // collection is not contained in vector
      collection = CreateCollection(cid, TRI_TRANSACTION_READ);
      if (collection == NULL) {
        // out of memory
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      TRI_InsertVectorPointer(&trx->_collections, collection, i);

      collection->_collection = TRI_UseCollectionByIdVocBase(trx->_context->_vocbase, collection->_cid);
      if (collection->_collection == NULL) {
        return TRI_errno();
      }
  
      res = LockCollection(collection, TRI_TRANSACTION_READ);

      return res;
    }
  }

  // we should not get here. Otherwise this is a logic error
  LOG_WARNING("logic error. should have inserted collection");

  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_StartTransaction (TRI_transaction_t* const trx, TRI_transaction_hint_t hints) {
  int res;
  
  assert(trx->_status == TRI_TRANSACTION_CREATED);

  trx->_hints = hints;

  res = UseCollections(trx);
  if (res != TRI_ERROR_NO_ERROR) {
    // free what we have got so far
    trx->_status = TRI_TRANSACTION_FAILED;
    ReleaseCollections(trx);

    return res;
  }

  res = RegisterTransaction(trx);
  if (res != TRI_ERROR_NO_ERROR) {
    // free what we have got so far
    trx->_status = TRI_TRANSACTION_FAILED;
    ReleaseCollections(trx);

    return res;
  }

  trx->_status = TRI_TRANSACTION_RUNNING;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_CommitTransaction (TRI_transaction_t* const trx) {
  int res;

  assert(trx->_status == TRI_TRANSACTION_RUNNING);

  res = UpdateTransactionStatus(trx, TRI_TRANSACTION_COMMITTED);
  ReleaseCollections(trx);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief abort a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_AbortTransaction (TRI_transaction_t* const trx) {
  int res;

  assert(trx->_status == TRI_TRANSACTION_RUNNING);
  
  res = UpdateTransactionStatus(trx, TRI_TRANSACTION_ABORTED);
  ReleaseCollections(trx);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finish a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_FinishTransaction (TRI_transaction_t* const trx) {
  int res;
  TRI_transaction_status_e status;

  assert(trx->_status == TRI_TRANSACTION_RUNNING);

  if (trx->_type == TRI_TRANSACTION_READ) {
    // read transactions
    status = TRI_TRANSACTION_FINISHED;
  }
  else {
    // write transactions
    status = TRI_TRANSACTION_COMMITTED;
  }
  
  res = UpdateTransactionStatus(trx, status);

  ReleaseCollections(trx);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
