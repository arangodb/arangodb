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

#include "BasicsC/conversions.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"

#include "VocBase/collection.h"
#include "VocBase/document-collection.h"
#include "VocBase/replication-logger.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"
#include "Wal/DocumentOperation.h"
#include "Wal/LogfileManager.h"

#define LOG_TRX(trx, level, format, ...) \
  LOG_TRACE("trx #%llu.%d (%s): " format, (unsigned long long) trx->_id, (int) level, StatusTransaction(trx->_status), __VA_ARGS__)

// -----------------------------------------------------------------------------
// --SECTION--                                                       TRANSACTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a transaction consists of a single operation
////////////////////////////////////////////////////////////////////////////////

static triagens::wal::LogfileManager* GetLogfileManager () {
  return triagens::wal::LogfileManager::instance();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a transaction is read-only
////////////////////////////////////////////////////////////////////////////////

static inline bool IsReadOnlyTransaction (TRI_transaction_t const* trx) {
  return (trx->_type == TRI_TRANSACTION_READ);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a transaction consists of a single operation
////////////////////////////////////////////////////////////////////////////////

static inline bool IsSingleOperationTransaction (TRI_transaction_t const* trx) {
  return ((trx->_hints & (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_SINGLE_OPERATION) != 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a marker needs to be written
////////////////////////////////////////////////////////////////////////////////
  
static inline bool NeedWriteMarker (TRI_transaction_t const* trx) {
  return (trx->_nestingLevel == 0 && ! IsReadOnlyTransaction(trx) && ! IsSingleOperationTransaction(trx));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares two transaction ids
////////////////////////////////////////////////////////////////////////////////

static int TidCompare (void const* l, void const* r) {
  TRI_voc_tid_t const* left  = static_cast<TRI_voc_tid_t const*>(l);
  TRI_voc_tid_t const* right = static_cast<TRI_voc_tid_t const*>(r);

  if (*left != *right) {
    return (*left < *right) ? -1 : 1;
  }
  return 0;
}

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
/// @brief free all operations for a transaction
////////////////////////////////////////////////////////////////////////////////

static void FreeOperations (TRI_transaction_t* trx) {
  size_t const n = trx->_collections._length;
  
  for (size_t i = 0; i < n; ++i) {
    TRI_transaction_collection_t* trxCollection = static_cast<TRI_transaction_collection_t*>(TRI_AtVectorPointer(&trx->_collections, i));
    TRI_document_collection_t* document = trxCollection->_collection->_collection;

    if (trxCollection->_operations == nullptr) {
      continue;
    }
 
    if (trx->_status == TRI_TRANSACTION_ABORTED) {
      for (auto it = trxCollection->_operations->rbegin(); it != trxCollection->_operations->rend(); ++it) {
        triagens::wal::DocumentOperation* op = (*it);

        op->revert();
      }
    }

    for (auto it = trxCollection->_operations->rbegin(); it != trxCollection->_operations->rend(); ++it) {
      triagens::wal::DocumentOperation* op = (*it);
  
      delete op;
    }
    
    TRI_SetRevisionDocumentCollection(document, trxCollection->_originalRevision, true);

    delete trxCollection->_operations;
    trxCollection->_operations = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find a collection in the transaction's list of collections
////////////////////////////////////////////////////////////////////////////////

static TRI_transaction_collection_t* FindCollection (const TRI_transaction_t* const trx,
                                                     const TRI_voc_cid_t cid,
                                                     size_t* position) {
  size_t const n = trx->_collections._length;
  size_t i;
   
  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* trxCollection = static_cast<TRI_transaction_collection_t*>(TRI_AtVectorPointer(&trx->_collections, i));
    
    if (cid < trxCollection->_cid) {
      // collection not found
      break;
    }

    if (cid == trxCollection->_cid) {
      // found
      return trxCollection;
    }

    // next
  }
    
  if (position != nullptr) {
    // update the insert position if required
    *position = i;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a transaction collection container
////////////////////////////////////////////////////////////////////////////////

static TRI_transaction_collection_t* CreateCollection (TRI_transaction_t* trx,
                                                       const TRI_voc_cid_t cid,
                                                       const TRI_transaction_type_e accessType,
                                                       const int nestingLevel) {
  TRI_transaction_collection_t* trxCollection = static_cast<TRI_transaction_collection_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_collection_t), false));

  if (trxCollection == nullptr) {
    // OOM
    return nullptr;
  }

  // initialise collection properties
  trxCollection->_transaction      = trx;
  trxCollection->_cid              = cid;
  trxCollection->_accessType       = accessType;
  trxCollection->_nestingLevel     = nestingLevel;
  trxCollection->_collection       = nullptr;
  trxCollection->_operations       = nullptr;
  trxCollection->_originalRevision = 0;
  trxCollection->_locked           = false;
  trxCollection->_compactionLocked = false;
  trxCollection->_waitForSync      = false;

  return trxCollection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a transaction collection container
////////////////////////////////////////////////////////////////////////////////

static void FreeCollection (TRI_transaction_collection_t* trxCollection) {
  assert(trxCollection != nullptr);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, trxCollection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lock a collection
////////////////////////////////////////////////////////////////////////////////

static int LockCollection (TRI_transaction_collection_t* trxCollection,
                           const TRI_transaction_type_e type,
                           const int nestingLevel) {
  int res;

  assert(trxCollection != nullptr);

  TRI_transaction_t* trx = trxCollection->_transaction;
  
  if ((trx->_hints & (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_LOCK_NEVER) != 0) {
    // never lock
    return TRI_ERROR_NO_ERROR;
  }

  assert(trxCollection->_collection != nullptr);
  assert(trxCollection->_collection->_collection != nullptr);
  assert(trxCollection->_locked == false);

  TRI_document_collection_t* document = trxCollection->_collection->_collection;

  if (type == TRI_TRANSACTION_READ) {
    LOG_TRX(trx,
            nestingLevel, 
            "read-locking collection %llu", 
            (unsigned long long) trxCollection->_cid);
    if (trx->_timeout == 0) {
      res = document->beginRead(document); 
    }
    else {
      res = document->beginReadTimed(document, trx->_timeout, TRI_TRANSACTION_DEFAULT_SLEEP_DURATION);
    }
  }
  else {
    LOG_TRX(trx,
            nestingLevel, 
            "write-locking collection %llu", 
            (unsigned long long) trxCollection->_cid);
    if (trx->_timeout == 0) {
      res = document->beginWrite(document); 
    }
    else {
      res = document->beginWriteTimed(document, trx->_timeout, TRI_TRANSACTION_DEFAULT_SLEEP_DURATION);
    }
  }

  if (res == TRI_ERROR_NO_ERROR) {
    trxCollection->_locked = true;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlock a collection
////////////////////////////////////////////////////////////////////////////////

static int UnlockCollection (TRI_transaction_collection_t* trxCollection,
                             const TRI_transaction_type_e type,
                             const int nestingLevel) {

  assert(trxCollection != nullptr);
  
  if ((trxCollection->_transaction->_hints & (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_LOCK_NEVER) != 0) {
    // never unlock
    return TRI_ERROR_NO_ERROR;
  }

  assert(trxCollection->_collection != nullptr);
  assert(trxCollection->_collection->_collection != nullptr);
  assert(trxCollection->_locked == true);

  TRI_document_collection_t* document = trxCollection->_collection->_collection;
    
  if (trxCollection->_nestingLevel < nestingLevel) {
    // only process our own collections
    return TRI_ERROR_NO_ERROR;
  }

  if (type == TRI_TRANSACTION_READ) {
    LOG_TRX(trxCollection->_transaction,
            nestingLevel, 
            "read-unlocking collection %llu", 
            (unsigned long long) trxCollection->_cid);
    document->endRead(document);
  }
  else {
    LOG_TRX(trxCollection->_transaction, 
            nestingLevel, 
            "write-unlocking collection %llu", 
            (unsigned long long) trxCollection->_cid);
    document->endWrite(document);
  }

  trxCollection->_locked = false;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief use all participating collections of a transaction
////////////////////////////////////////////////////////////////////////////////

static int UseCollections (TRI_transaction_t* const trx,
                           const int nestingLevel) {
  size_t const n = trx->_collections._length;

  // process collections in forward order
  for (size_t i = 0; i < n; ++i) {
    TRI_transaction_collection_t* trxCollection;
    bool shouldLock;

    trxCollection = (TRI_transaction_collection_t*) TRI_AtVectorPointer(&trx->_collections, i);

    if (trxCollection->_nestingLevel != nestingLevel) {
      // only process our own collections
      continue;
    }
      
    if (trxCollection->_collection == nullptr) {
      // open the collection
      if ((trx->_hints & (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_LOCK_NEVER) == 0) {
        // use and usage-lock
        LOG_TRX(trx, nestingLevel, "using collection %llu", (unsigned long long) trxCollection->_cid);
        trxCollection->_collection = TRI_UseCollectionByIdVocBase(trx->_vocbase, trxCollection->_cid);
      }
      else {
        // use without usage-lock (lock already set externally)
        trxCollection->_collection = TRI_LookupCollectionByIdVocBase(trx->_vocbase, trxCollection->_cid);
      }

      if (trxCollection->_collection == nullptr ||
          trxCollection->_collection->_collection == nullptr) {
        // something went wrong
        return TRI_errno();
      }

      // store the waitForSync property
      trxCollection->_waitForSync = trxCollection->_collection->_collection->base._info._waitForSync;
    }

    assert(trxCollection->_collection != nullptr);
    assert(trxCollection->_collection->_collection != nullptr);
    
    if (nestingLevel == 0 && trxCollection->_accessType == TRI_TRANSACTION_WRITE) {
      // read-lock the compaction lock
      if (! trxCollection->_compactionLocked) {
        TRI_ReadLockReadWriteLock(&trxCollection->_collection->_collection->_compactionLock);
        trxCollection->_compactionLocked = true;
      }
    }
  
    if (trxCollection->_accessType == TRI_TRANSACTION_WRITE && trxCollection->_originalRevision == 0) {
      // store original revision at transaction start
      trxCollection->_originalRevision = trxCollection->_collection->_collection->base._info._revision;
    }
        
    shouldLock = ((trx->_hints & (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_LOCK_ENTIRELY) != 0);

    if (! shouldLock) {
      shouldLock = (trxCollection->_accessType == TRI_TRANSACTION_WRITE) && 
                   ((trx->_hints & (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_SINGLE_OPERATION) == 0);
    }
    
    if (shouldLock && ! trxCollection->_locked) {
      // r/w lock the collection
      int res = LockCollection(trxCollection, trxCollection->_accessType, nestingLevel);

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
  int res = TRI_ERROR_NO_ERROR;

  size_t i = trx->_collections._length;

  // process collections in reverse order
  while (i-- > 0) {
    TRI_transaction_collection_t* trxCollection = static_cast<TRI_transaction_collection_t*>(TRI_AtVectorPointer(&trx->_collections, i));

    if (trxCollection->_locked && 
        (nestingLevel == 0 || trxCollection->_nestingLevel == nestingLevel)) {
      // unlock our own r/w locks
      UnlockCollection(trxCollection, trxCollection->_accessType, nestingLevel);
    }
    
    // the top level transaction releases all collections
    if (nestingLevel == 0 && trxCollection->_collection != nullptr) {

      if (trxCollection->_accessType == TRI_TRANSACTION_WRITE && 
          trxCollection->_compactionLocked) {
        // read-unlock the compaction lock
        TRI_ReadUnlockReadWriteLock(&trxCollection->_collection->_collection->_compactionLock);
        trxCollection->_compactionLocked = false;
      }

      if ((trx->_hints & (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_LOCK_NEVER) == 0) {
        // unuse collection, remove usage-lock
        LOG_TRX(trx, nestingLevel, "unusing collection %llu", (unsigned long long) trxCollection->_cid);
  
        TRI_ReleaseCollectionVocBase(trx->_vocbase, trxCollection->_collection);
      }

      trxCollection->_locked = false;
      trxCollection->_collection = nullptr;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write WAL begin marker
////////////////////////////////////////////////////////////////////////////////

static int WriteBeginMarker (TRI_transaction_t* trx) {
  if (! NeedWriteMarker(trx)) {
    return TRI_ERROR_NO_ERROR;
  }
  
  triagens::wal::BeginTransactionMarker marker(trx->_vocbase->_id, trx->_id);

  return GetLogfileManager()->writeMarker(marker, false).errorCode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write WAL abort marker
////////////////////////////////////////////////////////////////////////////////

static int WriteAbortMarker (TRI_transaction_t* trx) {
  if (! NeedWriteMarker(trx)) {
    return TRI_ERROR_NO_ERROR;
  }

  triagens::wal::AbortTransactionMarker marker(trx->_vocbase->_id, trx->_id);

  return GetLogfileManager()->writeMarker(marker, false).errorCode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write WAL commit marker
////////////////////////////////////////////////////////////////////////////////

static int WriteCommitMarker (TRI_transaction_t* trx) {
  if (! NeedWriteMarker(trx)) {
    return TRI_ERROR_NO_ERROR;
  }

  triagens::wal::CommitTransactionMarker marker(trx->_vocbase->_id, trx->_id);

  return GetLogfileManager()->writeMarker(marker, trx->_waitForSync).errorCode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update the status of a transaction
////////////////////////////////////////////////////////////////////////////////

static void UpdateTransactionStatus (TRI_transaction_t* const trx,
                                     TRI_transaction_status_e status) {
  assert(trx->_status == TRI_TRANSACTION_CREATED || trx->_status == TRI_TRANSACTION_RUNNING);

  if (trx->_status == TRI_TRANSACTION_CREATED) {
    assert(status == TRI_TRANSACTION_RUNNING || status == TRI_TRANSACTION_FAILED);
  }
  else if (trx->_status == TRI_TRANSACTION_RUNNING) {
    assert(status == TRI_TRANSACTION_COMMITTED || status == TRI_TRANSACTION_ABORTED);
  }

  trx->_status = status;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a marker of the specified type
////////////////////////////////////////////////////////////////////////////////

template<typename T> static T* CreateMarker () {
  return static_cast<T*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(T), false));
}

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new transaction container
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_t* TRI_CreateTransaction (TRI_vocbase_t* vocbase,
                                          TRI_server_id_t generatingServer,
                                          bool replicate,
                                          double timeout,
                                          bool waitForSync) {
  TRI_transaction_t* trx;

  trx = (TRI_transaction_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_t), false);

  if (trx == nullptr) {
    // out of memory
    return nullptr;
  }

  trx->_vocbase           = vocbase;
  trx->_generatingServer  = generatingServer;

  // note: the real transaction id will be acquired on transaction start
  trx->_id                = 0; 
  
  trx->_status            = TRI_TRANSACTION_CREATED;
  trx->_type              = TRI_TRANSACTION_READ;
  trx->_hints             = 0;
  trx->_nestingLevel      = 0;
  trx->_timeout           = TRI_TRANSACTION_DEFAULT_LOCK_TIMEOUT;
  trx->_hasOperations     = false;
  trx->_replicate         = replicate;
  trx->_waitForSync       = waitForSync;

  if (timeout > 0.0) {
    trx->_timeout         = (uint64_t) (timeout * 1000000.0);
  }
  else if (timeout == 0.0) {
    trx->_timeout         = (uint64_t) 0;
  }

  TRI_InitVectorPointer2(&trx->_collections, TRI_UNKNOWN_MEM_ZONE, 2);

  // register a marker protector
  trx->_protectorId       = triagens::wal::LogfileManager::instance()->registerMarkerProtector();  

  return trx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a transaction container
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeTransaction (TRI_transaction_t* const trx) {
  assert(trx != nullptr);

  if (trx->_status == TRI_TRANSACTION_RUNNING) {
    TRI_AbortTransaction(trx, 0);
  }
  
  // release the marker protector
  triagens::wal::LogfileManager::instance()->unregisterMarkerProtector(trx->_protectorId);  

  // free all collections
  size_t i = trx->_collections._length;
  while (i-- > 0) {
    TRI_transaction_collection_t* trxCollection = static_cast<TRI_transaction_collection_t*>(TRI_AtVectorPointer(&trx->_collections, i));

    FreeCollection(trxCollection);
  }

  TRI_DestroyVectorPointer(&trx->_collections);
  
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, trx);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection from a transaction
////////////////////////////////////////////////////////////////////////////////

bool TRI_WasSynchronousCollectionTransaction (TRI_transaction_t const* trx,
                                              TRI_voc_cid_t cid) {
  
  assert(trx->_status == TRI_TRANSACTION_RUNNING ||
         trx->_status == TRI_TRANSACTION_ABORTED ||
         trx->_status == TRI_TRANSACTION_COMMITTED);
   
  return trx->_waitForSync;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection from a transaction
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_collection_t* TRI_GetCollectionTransaction (TRI_transaction_t const* trx,
                                                            TRI_voc_cid_t cid,
                                                            TRI_transaction_type_e accessType) {
  
  TRI_transaction_collection_t* trxCollection;
  
  assert(trx->_status == TRI_TRANSACTION_CREATED ||
         trx->_status == TRI_TRANSACTION_RUNNING);
   
  trxCollection = FindCollection(trx, cid, nullptr);
  
  if (trxCollection == nullptr) {
    // not found
    return nullptr;
  }
  
  if (trxCollection->_collection == nullptr) {
    if ((trxCollection->_transaction->_hints & (TRI_transaction_hint_t) TRI_TRANSACTION_HINT_LOCK_NEVER) == 0) {
      // not opened. probably a mistake made by the caller
      return nullptr;
    }
    else {
      // ok
    }
  }
  
  // check if access type matches
  if (accessType == TRI_TRANSACTION_WRITE && trxCollection->_accessType == TRI_TRANSACTION_READ) {
    // type doesn't match. probably also a mistake by the caller
    return nullptr;
  }

  return trxCollection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_AddCollectionTransaction (TRI_transaction_t* const trx,
                                  const TRI_voc_cid_t cid,
                                  const TRI_transaction_type_e accessType,
                                  const int nestingLevel) {

  TRI_transaction_collection_t* trxCollection;
  size_t position;
    
  LOG_TRX(trx, nestingLevel, "adding collection %llu", (unsigned long long) cid);
          
  // upgrade transaction type if required
  if (nestingLevel == 0) {
    assert(trx->_status == TRI_TRANSACTION_CREATED);

    if (accessType == TRI_TRANSACTION_WRITE && 
        trx->_type == TRI_TRANSACTION_READ) {
      // if one collection is written to, the whole transaction becomes a write-transaction
      trx->_type = TRI_TRANSACTION_WRITE;
    }
  }

  // check if we already have got this collection in the _collections vector
  trxCollection = FindCollection(trx, cid, &position);

  if (trxCollection != nullptr) {
    // collection is already contained in vector
   
    if (accessType == TRI_TRANSACTION_WRITE && trxCollection->_accessType != accessType) {
      if (nestingLevel > 0) {
        // trying to write access a collection that is only marked with read-access 
        return TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION;
      }

      assert(nestingLevel == 0);

      // upgrade collection type to write-access
      trxCollection->_accessType = TRI_TRANSACTION_WRITE;
    } 

    if (nestingLevel < trxCollection->_nestingLevel) {
      trxCollection->_nestingLevel = nestingLevel;
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
  trxCollection = CreateCollection(trx, cid, accessType, nestingLevel);

  if (trxCollection == nullptr) {
    // out of memory
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // insert collection at the correct position
  if (TRI_InsertVectorPointer(&trx->_collections, trxCollection, position) != TRI_ERROR_NO_ERROR) {
    FreeCollection(trxCollection);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief request a lock for a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_LockCollectionTransaction (TRI_transaction_collection_t* trxCollection,
                                   const TRI_transaction_type_e accessType,
                                   const int nestingLevel) {

  if (accessType == TRI_TRANSACTION_WRITE && trxCollection->_accessType != TRI_TRANSACTION_WRITE) {
    // wrong lock type
    return TRI_ERROR_INTERNAL;
  }

  if (trxCollection->_locked) {
    // already locked
    return TRI_ERROR_NO_ERROR;
  }

  return LockCollection(trxCollection, accessType, nestingLevel);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief request an unlock for a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_UnlockCollectionTransaction (TRI_transaction_collection_t* trxCollection,
                                     const TRI_transaction_type_e accessType,
                                     const int nestingLevel) {
  
  if (accessType == TRI_TRANSACTION_WRITE && trxCollection->_accessType != TRI_TRANSACTION_WRITE) {
    // wrong lock type
    return TRI_ERROR_INTERNAL;
  }

  if (! trxCollection->_locked) {
    // already unlocked
    return TRI_ERROR_NO_ERROR;
  }

  return UnlockCollection(trxCollection, accessType, nestingLevel);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a collection is locked in a transaction
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsLockedCollectionTransaction (TRI_transaction_collection_t* trxCollection,
                                        const TRI_transaction_type_e accessType,
                                        const int nestingLevel) {

  if (accessType == TRI_TRANSACTION_WRITE && trxCollection->_accessType != TRI_TRANSACTION_WRITE) {
    // wrong lock type
    LOG_WARNING("logic error. checking wrong lock type");
    return false;
  }

  return trxCollection->_locked;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief as the id of a failed transaction to a vector
////////////////////////////////////////////////////////////////////////////////

int TRI_AddIdFailedTransaction (TRI_vector_t* vector,
                                TRI_voc_tid_t tid) {
  if (tid == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  bool mustSort = false;
  size_t n = vector->_length;

  if (n > 0) {
    TRI_voc_tid_t* lastTid = static_cast<TRI_voc_tid_t*>(TRI_AtVector(vector, n - 1));

    if (tid == *lastTid) {
      // no need to insert same id
      return TRI_ERROR_NO_ERROR;
    }

    if (tid < *lastTid) {
      // for some reason, the id of the transaction just inserted is lower than
      // the last id in the vector
      // this means we need to re-sort the list of transaction ids. 
      // this case should almost never occur, but we need to handle it if it occurs
      mustSort = true;
    }
  }

  int res = TRI_PushBackVector(vector, &tid);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (mustSort) {
    qsort(TRI_BeginVector(vector), n + 1, sizeof(TRI_voc_tid_t), TidCompare);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a WAL operation for a transaction collection
////////////////////////////////////////////////////////////////////////////////

int TRI_AddOperationTransaction (triagens::wal::DocumentOperation& operation,
                                 bool syncRequested) {
  TRI_transaction_collection_t* trxCollection = operation.trxCollection;
  TRI_transaction_t* trx = trxCollection->_transaction;

  assert(operation.header != nullptr);
  
  bool const isSingleOperationTransaction = IsSingleOperationTransaction(trx);

  // default is false
  bool waitForSync = false;
  if (isSingleOperationTransaction) {
    waitForSync = syncRequested || trxCollection->_waitForSync;
  }
  else {
    // upgrade the info for the transaction
    if (syncRequested || trxCollection->_waitForSync) {
      trx->_waitForSync = true;
    }
  }
  
  triagens::wal::SlotInfo slotInfo = triagens::wal::LogfileManager::instance()->allocateAndWrite(operation.marker->mem(), operation.marker->size(), waitForSync);
  
  if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
    // some error occurred
 //   operation.revert();

    return slotInfo.errorCode;
  }

    
  if (operation.type == TRI_VOC_DOCUMENT_OPERATION_INSERT ||
      operation.type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
    // adjust the data position in the header
    operation.header->_data = slotInfo.mem;
  } 
  
  if (isSingleOperationTransaction) {
    operation.handle();
  }
  else {
    // buffer the operation
    if (trxCollection->_operations == nullptr) {
      trxCollection->_operations = new std::vector<triagens::wal::DocumentOperation*>;
      trx->_hasOperations = true;
    }

    triagens::wal::DocumentOperation* copy = operation.swap();
    trxCollection->_operations->push_back(copy);
    copy->handle();
  }

  if (operation.rid > 0) {
    TRI_document_collection_t* document = trxCollection->_collection->_collection;
    TRI_SetRevisionDocumentCollection(document, operation.rid, false);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a transaction's id
////////////////////////////////////////////////////////////////////////////////

TRI_voc_tid_t TRI_GetIdTransaction (TRI_transaction_t const* trx) {
  return trx->_id;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_BeginTransaction (TRI_transaction_t* trx, 
                          TRI_transaction_hint_t hints,
                          int nestingLevel) {
  LOG_TRX(trx, nestingLevel, "%s transaction", "beginning");

  if (nestingLevel == 0) {
    assert(trx->_status == TRI_TRANSACTION_CREATED);

    // get a new id
    trx->_id = TRI_NewTickServer();

    // set hints
    trx->_hints = hints;
  }
  else {
    assert(trx->_status == TRI_TRANSACTION_RUNNING);
  }

  int res = UseCollections(trx, nestingLevel);

  if (res == TRI_ERROR_NO_ERROR) {
    // all valid
    if (nestingLevel == 0) {
      UpdateTransactionStatus(trx, TRI_TRANSACTION_RUNNING);
      
      res = WriteBeginMarker(trx);
    }
  }
  

  if (res != TRI_ERROR_NO_ERROR) {
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

int TRI_CommitTransaction (TRI_transaction_t* trx,
                           int nestingLevel) {
  LOG_TRX(trx, nestingLevel, "%s transaction", "committing");

  assert(trx->_status == TRI_TRANSACTION_RUNNING);

  int res = TRI_ERROR_NO_ERROR;

  if (nestingLevel == 0) {
    res = WriteCommitMarker(trx);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_AbortTransaction(trx, nestingLevel);

      // return original error
      return res;
    }
      
    UpdateTransactionStatus(trx, TRI_TRANSACTION_COMMITTED);

    FreeOperations(trx);
  }

  ReleaseCollections(trx, nestingLevel);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief abort and rollback a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_AbortTransaction (TRI_transaction_t* trx,
                          int nestingLevel) {
  LOG_TRX(trx, nestingLevel, "%s transaction", "aborting");

  assert(trx->_status == TRI_TRANSACTION_RUNNING);

  int res = TRI_ERROR_NO_ERROR;

  if (nestingLevel == 0) {
    res = WriteAbortMarker(trx);

    UpdateTransactionStatus(trx, TRI_TRANSACTION_ABORTED);

    FreeOperations(trx);
  }

  ReleaseCollections(trx, nestingLevel);

  return res;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
