////////////////////////////////////////////////////////////////////////////////
/// @brief transaction subsystem
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "transaction.h"

#include "Basics/conversions.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"

#include "Utils/Exception.h"
#include "VocBase/collection.h"
#include "VocBase/document-collection.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"
#include "Wal/DocumentOperation.h"
#include "Wal/LogfileManager.h"

#ifdef TRI_ENABLE_MAINTAINER_MODE

#define LOG_TRX(trx, level, format, ...) \
  LOG_TRACE("trx #%llu.%d (%s): " format, (unsigned long long) trx->_id, (int) level, StatusTransaction(trx->_status), __VA_ARGS__)

#else

#define LOG_TRX(...) while (0)

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       TRANSACTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether the collection is currently locked
////////////////////////////////////////////////////////////////////////////////

static inline bool IsLocked (TRI_transaction_collection_t const* trxCollection) {
  return (trxCollection->_lockType != TRI_TRANSACTION_NONE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the logfile manager
////////////////////////////////////////////////////////////////////////////////

static inline triagens::wal::LogfileManager* GetLogfileManager () {
  return triagens::wal::LogfileManager::instance();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a transaction is read-only
////////////////////////////////////////////////////////////////////////////////

static inline bool IsReadOnlyTransaction (TRI_transaction_t const* trx) {
  return (trx->_type == TRI_TRANSACTION_READ);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a specific hint is set for the transaction
////////////////////////////////////////////////////////////////////////////////

static inline bool HasHint (TRI_transaction_t const* trx,
                            TRI_transaction_hint_e hint) {
  return ((trx->_hints & (TRI_transaction_hint_t) hint) != 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a transaction consists of a single operation
////////////////////////////////////////////////////////////////////////////////

static inline bool IsSingleOperationTransaction (TRI_transaction_t const* trx) {
  return HasHint(trx, TRI_TRANSACTION_HINT_SINGLE_OPERATION);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a marker needs to be written
////////////////////////////////////////////////////////////////////////////////

static inline bool NeedWriteMarker (TRI_transaction_t const* trx,
                                    bool isBeginMarker) {
  if (isBeginMarker) {
    return (! IsReadOnlyTransaction(trx) && 
            ! IsSingleOperationTransaction(trx));
  }

  return (trx->_nestingLevel == 0 &&
          trx->_beginWritten &&  
          ! IsReadOnlyTransaction(trx) && 
          ! IsSingleOperationTransaction(trx));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the status of the transaction as a string
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
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
  }

  TRI_ASSERT(false);
  return "unknown";
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief free all operations for a transaction
////////////////////////////////////////////////////////////////////////////////

static void FreeOperations (TRI_transaction_t* trx) {
  size_t const n = trx->_collections._length;
  bool const mustRollback = (trx->_status == TRI_TRANSACTION_ABORTED);
  bool const isSingleOperation = IsSingleOperationTransaction(trx);

  for (size_t i = 0; i < n; ++i) {
    TRI_transaction_collection_t* trxCollection = static_cast<TRI_transaction_collection_t*>(TRI_AtVectorPointer(&trx->_collections, i));
    TRI_document_collection_t* document = trxCollection->_collection->_collection;

    if (trxCollection->_operations == nullptr) {
      continue;
    }

    if (mustRollback) {
      // revert all operations
      for (auto it = trxCollection->_operations->rbegin(); it != trxCollection->_operations->rend(); ++it) {
        triagens::wal::DocumentOperation* op = (*it);

        op->revert();
      }
    }
    else {
      // update datafile statistics for all operations
      // pair (number of dead markers, size of dead markers)
      std::unordered_map<TRI_voc_fid_t, std::pair<int64_t, int64_t>> stats;

      for (auto it = trxCollection->_operations->rbegin(); it != trxCollection->_operations->rend(); ++it) {
        triagens::wal::DocumentOperation* op = (*it);

        if (op->type == TRI_VOC_DOCUMENT_OPERATION_UPDATE ||
            op->type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
          TRI_voc_fid_t fid = op->oldHeader._fid;
          TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(op->oldHeader.getDataPtr());  // PROTECTED by trx from above

          auto it2 = stats.find(fid);

          if (it2 == stats.end()) {
            stats.insert(it2, std::make_pair(fid, std::make_pair(1, TRI_DF_ALIGN_BLOCK(marker->_size))));
          }
          else {
            (*it2).second.first++;
            (*it2).second.second += TRI_DF_ALIGN_BLOCK(marker->_size);
          }
        }
      }

      // now update the stats for all datafiles of the collection in one go
      TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
      
      for (auto it = stats.begin(); it != stats.end(); ++it) {
        TRI_voc_fid_t fid = (*it).first;

        TRI_doc_datafile_info_t* dfi = TRI_FindDatafileInfoDocumentCollection(document, fid, false);
        // the old header might point to the WAL. in this case, there'll be no stats update

        if (dfi != nullptr) {
          dfi->_numberDead += static_cast<TRI_voc_ssize_t>((*it).second.first);
          dfi->_sizeDead += (*it).second.second;
          dfi->_numberAlive -= static_cast<TRI_voc_ssize_t>((*it).second.first);
          dfi->_sizeAlive -= (*it).second.second;
        }
      }
      
      TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
    }

    for (auto it = trxCollection->_operations->rbegin(); it != trxCollection->_operations->rend(); ++it) {
      triagens::wal::DocumentOperation* op = (*it);

      delete op;
    }

    if (mustRollback) {
      document->_info._revision = trxCollection->_originalRevision;
    }
    else if (! document->_info._isVolatile && ! isSingleOperation) {
      // only count logfileEntries if the collection is durable
      document->_uncollectedLogfileEntries += trxCollection->_operations->size();
    }

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
  trxCollection->_barrier          = nullptr;
  trxCollection->_operations       = nullptr;
  trxCollection->_originalRevision = 0;
  trxCollection->_lockType         = TRI_TRANSACTION_NONE;
  trxCollection->_compactionLocked = false;
  trxCollection->_waitForSync      = false;

  return trxCollection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a transaction collection container
////////////////////////////////////////////////////////////////////////////////

static void FreeCollection (TRI_transaction_collection_t* trxCollection) {
  TRI_ASSERT(trxCollection != nullptr);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, trxCollection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees an unused barrier if it exists
////////////////////////////////////////////////////////////////////////////////

static void FreeBarrier (TRI_transaction_collection_t* trxCollection) {
  if (trxCollection->_barrier != nullptr) {
    // we're done with this barrier
    auto barrier = reinterpret_cast<TRI_barrier_blocker_t*>(trxCollection->_barrier);

    TRI_FreeBarrier(barrier, true /* fromTransaction */ );
    // If some external entity is still using the barrier, it is kept!

    trxCollection->_barrier = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lock a collection
////////////////////////////////////////////////////////////////////////////////

static int LockCollection (TRI_transaction_collection_t* trxCollection,
                           TRI_transaction_type_e type,
                           int nestingLevel) {
  int res;

  TRI_ASSERT(trxCollection != nullptr);

  TRI_transaction_t* trx = trxCollection->_transaction;

  if (HasHint(trx, TRI_TRANSACTION_HINT_LOCK_NEVER)) {
    // never lock
    return TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(trxCollection->_collection != nullptr);
  TRI_ASSERT(trxCollection->_collection->_collection != nullptr);
  TRI_ASSERT(! IsLocked(trxCollection));

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
    trxCollection->_lockType = type;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlock a collection
////////////////////////////////////////////////////////////////////////////////

static int UnlockCollection (TRI_transaction_collection_t* trxCollection,
                             TRI_transaction_type_e type,
                             int nestingLevel) {

  TRI_ASSERT(trxCollection != nullptr);

  if (HasHint(trxCollection->_transaction, TRI_TRANSACTION_HINT_LOCK_NEVER)) {
    // never unlock
    return TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(trxCollection->_collection != nullptr);
  TRI_ASSERT(trxCollection->_collection->_collection != nullptr);
  TRI_ASSERT(IsLocked(trxCollection));

  TRI_document_collection_t* document = trxCollection->_collection->_collection;

  if (trxCollection->_nestingLevel < nestingLevel) {
    // only process our own collections
    return TRI_ERROR_NO_ERROR;
  }

  if (type == TRI_TRANSACTION_READ && 
      trxCollection->_lockType == TRI_TRANSACTION_WRITE) {
    // do not remove a write-lock if a read-unlock was requested!
    return TRI_ERROR_NO_ERROR;
  }
  else if (type == TRI_TRANSACTION_WRITE && 
           trxCollection->_lockType == TRI_TRANSACTION_READ) {
    // we should never try to write-unlock a collection that we have only read-locked
    LOG_ERROR("logic error in UnlockCollection");
    TRI_ASSERT(false);
    return TRI_ERROR_INTERNAL;
  }

  if (trxCollection->_lockType == TRI_TRANSACTION_READ) {
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

  trxCollection->_lockType = TRI_TRANSACTION_NONE;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief use all participating collections of a transaction
////////////////////////////////////////////////////////////////////////////////

static int UseCollections (TRI_transaction_t* trx,
                           int nestingLevel) {
  size_t const n = trx->_collections._length;

  // process collections in forward order
  for (size_t i = 0; i < n; ++i) {
    TRI_transaction_collection_t* trxCollection = static_cast<TRI_transaction_collection_t*>(TRI_AtVectorPointer(&trx->_collections, i));

    if (trxCollection->_nestingLevel != nestingLevel) {
      // only process our own collections
      continue;
    }

    if (trxCollection->_collection == nullptr) {
      // open the collection
      if (! HasHint(trx, TRI_TRANSACTION_HINT_LOCK_NEVER)) {
        // use and usage-lock
        TRI_vocbase_col_status_e status;
        LOG_TRX(trx, nestingLevel, "using collection %llu", (unsigned long long) trxCollection->_cid);
        trxCollection->_collection = TRI_UseCollectionByIdVocBase(trx->_vocbase, trxCollection->_cid, status);
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

      if (trxCollection->_accessType == TRI_TRANSACTION_WRITE &&
          TRI_GetOperationModeServer() == TRI_VOCBASE_MODE_NO_CREATE &&
          ! TRI_IsSystemNameCollection(trxCollection->_collection->_collection->_info._name)) {
        return TRI_ERROR_ARANGO_READ_ONLY;
      }

      // store the waitForSync property
      trxCollection->_waitForSync = trxCollection->_collection->_collection->_info._waitForSync;
    }

    TRI_ASSERT(trxCollection->_collection != nullptr);
    TRI_ASSERT(trxCollection->_collection->_collection != nullptr);

    if (nestingLevel == 0 && trxCollection->_accessType == TRI_TRANSACTION_WRITE) {
      // read-lock the compaction lock
      if (! trxCollection->_compactionLocked) {
        TRI_ReadLockReadWriteLock(&trxCollection->_collection->_collection->_compactionLock);
        trxCollection->_compactionLocked = true;
      }
    }

    if (trxCollection->_accessType == TRI_TRANSACTION_WRITE && trxCollection->_originalRevision == 0) {
      // store original revision at transaction start
      trxCollection->_originalRevision = trxCollection->_collection->_collection->_info._revision;
    }

    bool shouldLock = HasHint(trx, TRI_TRANSACTION_HINT_LOCK_ENTIRELY);

    if (! shouldLock) {
      shouldLock = (trxCollection->_accessType == TRI_TRANSACTION_WRITE) && (! IsSingleOperationTransaction(trx));
    }

    if (shouldLock && ! IsLocked(trxCollection)) {
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

static int UnuseCollections (TRI_transaction_t* trx,
                             int nestingLevel) {
  int res = TRI_ERROR_NO_ERROR;

  size_t i = trx->_collections._length;

  // process collections in reverse order
  while (i-- > 0) {
    TRI_transaction_collection_t* trxCollection = static_cast<TRI_transaction_collection_t*>(TRI_AtVectorPointer(&trx->_collections, i));

    if (IsLocked(trxCollection) &&
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

      trxCollection->_lockType = TRI_TRANSACTION_NONE;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief release collection locks for a transaction
////////////////////////////////////////////////////////////////////////////////

static int ReleaseCollections (TRI_transaction_t* trx,
                               int nestingLevel) {
  TRI_ASSERT(nestingLevel == 0);
  if (HasHint(trx, TRI_TRANSACTION_HINT_LOCK_NEVER)) {
    return TRI_ERROR_NO_ERROR;
  }

  size_t i = trx->_collections._length;

  // process collections in reverse order
  while (i-- > 0) {
    TRI_transaction_collection_t* trxCollection = static_cast<TRI_transaction_collection_t*>(TRI_AtVectorPointer(&trx->_collections, i));

    // the top level transaction releases all collections
    if (trxCollection->_collection != nullptr) {
      // unuse collection, remove usage-lock
      LOG_TRX(trx, nestingLevel, "unusing collection %llu", (unsigned long long) trxCollection->_cid);

      TRI_ReleaseCollectionVocBase(trx->_vocbase, trxCollection->_collection);
      trxCollection->_collection = nullptr;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write WAL begin marker
////////////////////////////////////////////////////////////////////////////////

static int WriteBeginMarker (TRI_transaction_t* trx) {
  if (! NeedWriteMarker(trx, true)) {
    return TRI_ERROR_NO_ERROR;
  }

  if (HasHint(trx, TRI_TRANSACTION_HINT_NO_BEGIN_MARKER)) {
    return TRI_ERROR_NO_ERROR;
  }

  TRI_IF_FAILURE("TransactionWriteBeginMarker") {
    return TRI_ERROR_DEBUG;
  }

  TRI_ASSERT(! trx->_beginWritten);

  int res;

  try {
    if (trx->_externalId > 0) {
      // remotely started trx
      triagens::wal::BeginRemoteTransactionMarker marker(trx->_vocbase->_id, trx->_id, trx->_externalId);
      res = GetLogfileManager()->allocateAndWrite(marker, false).errorCode;
    }
    else {
      // local trx
      triagens::wal::BeginTransactionMarker marker(trx->_vocbase->_id, trx->_id);
      res = GetLogfileManager()->allocateAndWrite(marker, false).errorCode;
    }
  
    
    if (res == TRI_ERROR_NO_ERROR) {
      trx->_beginWritten = true;
    }
  }
  catch (triagens::arango::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_WARNING("could not save transaction begin marker in log: %s", TRI_errno_string(res));
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write WAL abort marker
////////////////////////////////////////////////////////////////////////////////

static int WriteAbortMarker (TRI_transaction_t* trx) {
  if (! NeedWriteMarker(trx, false)) {
    return TRI_ERROR_NO_ERROR;
  }

  if (HasHint(trx, TRI_TRANSACTION_HINT_NO_ABORT_MARKER)) {
    return TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(trx->_beginWritten);

  TRI_IF_FAILURE("TransactionWriteAbortMarker") {
    return TRI_ERROR_DEBUG;
  }

  int res;

  try {
    if (trx->_externalId > 0) {
      // remotely started trx
      triagens::wal::AbortRemoteTransactionMarker marker(trx->_vocbase->_id, trx->_id, trx->_externalId);
      res = GetLogfileManager()->allocateAndWrite(marker, false).errorCode;
    }
    else {
      // local trx
      triagens::wal::AbortTransactionMarker marker(trx->_vocbase->_id, trx->_id);
      res = GetLogfileManager()->allocateAndWrite(marker, false).errorCode;
    }
  }
  catch (triagens::arango::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_WARNING("could not save transaction abort marker in log: %s", TRI_errno_string(res));
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write WAL commit marker
////////////////////////////////////////////////////////////////////////////////

static int WriteCommitMarker (TRI_transaction_t* trx) {
  if (! NeedWriteMarker(trx, false)) {
    return TRI_ERROR_NO_ERROR;
  }
  
  TRI_IF_FAILURE("TransactionWriteCommitMarker") {
    return TRI_ERROR_DEBUG;
  }

  TRI_ASSERT(trx->_beginWritten);

  int res;

  try {
    if (trx->_externalId > 0) {
      // remotely started trx
      triagens::wal::CommitRemoteTransactionMarker marker(trx->_vocbase->_id, trx->_id, trx->_externalId);
      res = GetLogfileManager()->allocateAndWrite(marker, false).errorCode;
    }
    else {
      // local trx
      triagens::wal::CommitTransactionMarker marker(trx->_vocbase->_id, trx->_id);
      res = GetLogfileManager()->allocateAndWrite(marker, false).errorCode;
    }
  }
  catch (triagens::arango::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_WARNING("could not save transaction commit marker in log: %s", TRI_errno_string(res));
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update the status of a transaction
////////////////////////////////////////////////////////////////////////////////

static void UpdateTransactionStatus (TRI_transaction_t* const trx,
                                     TRI_transaction_status_e status) {
  TRI_ASSERT(trx->_status == TRI_TRANSACTION_CREATED || trx->_status == TRI_TRANSACTION_RUNNING);

  if (trx->_status == TRI_TRANSACTION_CREATED) {
    TRI_ASSERT(status == TRI_TRANSACTION_RUNNING || status == TRI_TRANSACTION_ABORTED);
  }
  else if (trx->_status == TRI_TRANSACTION_RUNNING) {
    TRI_ASSERT(status == TRI_TRANSACTION_COMMITTED || status == TRI_TRANSACTION_ABORTED);
  }

  trx->_status = status;
}

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new transaction container
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_t* TRI_CreateTransaction (TRI_vocbase_t* vocbase,
                                          TRI_voc_tid_t externalId,
                                          double timeout,
                                          bool waitForSync) {
  TRI_transaction_t* trx = static_cast<TRI_transaction_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_t), false));

  if (trx == nullptr) {
    // out of memory
    return nullptr;
  }

  trx->_vocbase           = vocbase;

  // note: the real transaction id will be acquired on transaction start
  trx->_id                = 0;           // local trx id
  trx->_externalId        = externalId;  // remote trx id (used in replication)
  trx->_status            = TRI_TRANSACTION_CREATED;
  trx->_type              = TRI_TRANSACTION_READ;
  trx->_hints             = 0;
  trx->_nestingLevel      = 0;
  trx->_timeout           = TRI_TRANSACTION_DEFAULT_LOCK_TIMEOUT;
  trx->_hasOperations     = false;
  trx->_waitForSync       = waitForSync;
  trx->_beginWritten      = false;

  if (timeout > 0.0) {
    trx->_timeout         = (uint64_t) (timeout * 1000000.0);
  }
  else if (timeout == 0.0) {
    trx->_timeout         = (uint64_t) 0;
  }

  if (trx->_externalId != 0) {
    // replication transaction is always a write transaction
    trx->_type = TRI_TRANSACTION_WRITE;
  }

  TRI_InitVectorPointer2(&trx->_collections, TRI_UNKNOWN_MEM_ZONE, 2);

  return trx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a transaction container
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeTransaction (TRI_transaction_t* trx) {
  TRI_ASSERT(trx != nullptr);

  if (trx->_status == TRI_TRANSACTION_RUNNING) {
    TRI_AbortTransaction(trx, 0);
  }

  // release the marker protector
  bool const hasFailedOperations = (trx->_hasOperations && trx->_status == TRI_TRANSACTION_ABORTED);
  triagens::wal::LogfileManager::instance()->unregisterTransaction(trx->_id, hasFailedOperations);

  ReleaseCollections(trx, 0);

  // free all collections
  size_t i = trx->_collections._length;
  while (i-- > 0) {
    TRI_transaction_collection_t* trxCollection = static_cast<TRI_transaction_collection_t*>(TRI_AtVectorPointer(&trx->_collections, i));

    FreeBarrier(trxCollection);
    FreeCollection(trxCollection);
  }

  TRI_DestroyVectorPointer(&trx->_collections);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, trx);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the transaction type from a string
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_type_e TRI_GetTransactionTypeFromStr (char const* s) {
  if (TRI_EqualString(s, "read")) {
    return TRI_TRANSACTION_READ;
  }
  else if (TRI_EqualString(s, "write")) {
    return TRI_TRANSACTION_WRITE;
  }
  else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid transaction type");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection from a transaction
////////////////////////////////////////////////////////////////////////////////

bool TRI_WasSynchronousCollectionTransaction (TRI_transaction_t const* trx,
                                              TRI_voc_cid_t cid) {

  TRI_ASSERT(trx->_status == TRI_TRANSACTION_RUNNING ||
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

  TRI_ASSERT(trx->_status == TRI_TRANSACTION_CREATED ||
             trx->_status == TRI_TRANSACTION_RUNNING);

  TRI_transaction_collection_t* trxCollection = FindCollection(trx, cid, nullptr);

  if (trxCollection == nullptr) {
    // not found
    return nullptr;
  }

  if (trxCollection->_collection == nullptr) {
    if (! HasHint(trx, TRI_TRANSACTION_HINT_LOCK_NEVER)) {
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

int TRI_AddCollectionTransaction (TRI_transaction_t* trx,
                                  TRI_voc_cid_t cid,
                                  TRI_transaction_type_e accessType,
                                  int nestingLevel,
                                  bool force) {

  LOG_TRX(trx, nestingLevel, "adding collection %llu", (unsigned long long) cid);

  // upgrade transaction type if required
  if (nestingLevel == 0) {
    if (! force) {
      TRI_ASSERT(trx->_status == TRI_TRANSACTION_CREATED);
    }

    if (accessType == TRI_TRANSACTION_WRITE &&
        trx->_type == TRI_TRANSACTION_READ) {
      // if one collection is written to, the whole transaction becomes a write-transaction
      trx->_type = TRI_TRANSACTION_WRITE;
    }
  }

  // check if we already have got this collection in the _collections vector
  size_t position = 0;
  TRI_transaction_collection_t* trxCollection = FindCollection(trx, cid, &position);

  if (trxCollection != nullptr) {
    // collection is already contained in vector

    if (accessType == TRI_TRANSACTION_WRITE && trxCollection->_accessType != accessType) {
      if (nestingLevel > 0) {
        // trying to write access a collection that is only marked with read-access
        return TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION;
      }

      TRI_ASSERT(nestingLevel == 0);

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
/// @brief make sure all declared collections are used & locked
////////////////////////////////////////////////////////////////////////////////

int TRI_EnsureCollectionsTransaction (TRI_transaction_t* trx) {
  return UseCollections(trx, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief request a lock for a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_LockCollectionTransaction (TRI_transaction_collection_t* trxCollection,
                                   TRI_transaction_type_e accessType,
                                   int nestingLevel) {

  if (accessType == TRI_TRANSACTION_WRITE && 
      trxCollection->_accessType != TRI_TRANSACTION_WRITE) {
    // wrong lock type
    return TRI_ERROR_INTERNAL;
  }

  if (IsLocked(trxCollection)) {
    // already locked
    return TRI_ERROR_NO_ERROR;
  }

  return LockCollection(trxCollection, accessType, nestingLevel);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief request an unlock for a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_UnlockCollectionTransaction (TRI_transaction_collection_t* trxCollection,
                                     TRI_transaction_type_e accessType,
                                     int nestingLevel) {

  if (accessType == TRI_TRANSACTION_WRITE && 
      trxCollection->_accessType != TRI_TRANSACTION_WRITE) {
    // wrong lock type: write-unlock requested but collection is read-only
    return TRI_ERROR_INTERNAL;
  }

  if (! IsLocked(trxCollection)) {
    // already unlocked
    return TRI_ERROR_NO_ERROR;
  }
  
  return UnlockCollection(trxCollection, accessType, nestingLevel);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a collection is locked in a transaction
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsLockedCollectionTransaction (TRI_transaction_collection_t const* trxCollection,
                                        TRI_transaction_type_e accessType,
                                        int nestingLevel) {

  if (accessType == TRI_TRANSACTION_WRITE &&
      trxCollection->_accessType != TRI_TRANSACTION_WRITE) {
    // wrong lock type
    LOG_WARNING("logic error. checking wrong lock type");
    return false;
  }

  return IsLocked(trxCollection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a collection is locked in a transaction
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsLockedCollectionTransaction (TRI_transaction_collection_t const* trxCollection) {
  return IsLocked(trxCollection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a WAL operation for a transaction collection
////////////////////////////////////////////////////////////////////////////////

int TRI_AddOperationTransaction (triagens::wal::DocumentOperation& operation,
                                 bool& waitForSync) {
  TRI_transaction_collection_t* trxCollection = operation.trxCollection;
  TRI_transaction_t* trx = trxCollection->_transaction;

  TRI_ASSERT(operation.header != nullptr);

  bool const isSingleOperationTransaction = IsSingleOperationTransaction(trx);

  // upgrade the info for the transaction
  if (waitForSync || trxCollection->_waitForSync) {
    trx->_waitForSync = true;
  }

  // default is false
  waitForSync = false;
  if (isSingleOperationTransaction) {
    waitForSync |= trxCollection->_waitForSync;
  }
  

  TRI_IF_FAILURE("TransactionOperationNoSlot") {
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("TransactionOperationNoSlotExcept") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
      
  if (! trx->_beginWritten) {
    int res = WriteBeginMarker(trx);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  TRI_voc_fid_t fid = 0;
  void const* position = nullptr;

  int64_t sizeChanged = 0;
  TRI_document_collection_t* document = operation.trxCollection->_collection->_collection;

  if (operation.marker->fid() == 0) {
    // this is a "real" marker that must be written into the logfiles
    char* oldmarker = static_cast<char*>(operation.marker->mem());
    auto oldm = reinterpret_cast<triagens::wal::document_marker_t*>(oldmarker);
    if ((oldm->_type == TRI_WAL_MARKER_DOCUMENT ||
         oldm->_type == TRI_WAL_MARKER_EDGE) &&
         ! triagens::wal::LogfileManager::instance()->suppressShapeInformation()) {
      // In this case we have to take care of the legend, we know that the
      // marker does not have a legend so far, so first try to get away 
      // with this:
      // (Note that the latter also works for edges!
      TRI_voc_cid_t cid = oldm->_collectionId;
      TRI_shape_sid_t sid = oldm->_shape;
      void* oldLegend;
      triagens::wal::SlotInfoCopy slotInfo = triagens::wal::LogfileManager::instance()->allocateAndWrite(oldmarker, operation.marker->size(), false, cid, sid, 0, oldLegend);
      if (slotInfo.errorCode == TRI_ERROR_LEGEND_NOT_IN_WAL_FILE) {
        // Oh dear, we have to build a legend and patch the marker:
        triagens::basics::JsonLegend legend(document->getShaper());  // PROTECTED by trx in trxCollection
        int res = legend.addShape(sid, oldmarker + oldm->_offsetJson,
                                       oldm->_size - oldm->_offsetJson);
        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }
        else {
          sizeChanged =   legend.getSize() 
                        - (oldm->_offsetJson - oldm->_offsetLegend);
          TRI_voc_size_t newMarkerSize = (TRI_voc_size_t) (oldm->_size + sizeChanged);

          // Now construct the new marker on the heap:
          char* newmarker = new char[newMarkerSize];
          memcpy(newmarker, oldmarker, oldm->_offsetLegend);
          legend.dump(newmarker + oldm->_offsetLegend);
          memcpy(newmarker + oldm->_offsetLegend + legend.getSize(), 
                 oldmarker + oldm->_offsetJson,
                 oldm->_size - oldm->_offsetJson);

          // And fix its entries:
          auto newm = reinterpret_cast<triagens::wal::document_marker_t*>(newmarker);
          newm->_size = newMarkerSize;
          newm->_offsetJson = (uint32_t) (oldm->_offsetLegend + legend.getSize());
          triagens::wal::SlotInfoCopy slotInfo2 = triagens::wal::LogfileManager::instance()->allocateAndWrite(newmarker, newMarkerSize, false, cid, sid, newm->_offsetLegend, oldLegend);
          delete[] newmarker;
          if (slotInfo2.errorCode != TRI_ERROR_NO_ERROR) {
            return slotInfo2.errorCode;
          }
          fid = slotInfo2.logfileId;
          position = slotInfo2.mem; 
          operation.tick = slotInfo2.tick;
        }
      }
      else if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
        return slotInfo.errorCode;
      }
      else {
        int64_t* legendPtr = reinterpret_cast<int64_t*>
                             (oldmarker + oldm->_offsetLegend);
        *legendPtr =  reinterpret_cast<char*>(oldLegend) 
                     -reinterpret_cast<char*>(legendPtr);
        // This means that we can find the old legend relative to
        // the new position in the same WAL file.
        operation.tick = slotInfo.tick;
        fid = slotInfo.logfileId;
        position = slotInfo.mem; 
      }

    }
    else {  
      // No document or edge marker, just append it to the WAL:
      triagens::wal::SlotInfoCopy slotInfo = triagens::wal::LogfileManager::instance()->allocateAndWrite(operation.marker->mem(), operation.marker->size(), false);
      if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
        // some error occurred
        return slotInfo.errorCode;
      }
      operation.tick = slotInfo.tick;
      fid = slotInfo.logfileId;
      position = slotInfo.mem; 
    }
  }
  else {
    // this is an envelope marker that has been written to the logfiles before
    // avoid writing it again!
    fid = operation.marker->fid();
    position = operation.marker->mem();
  }
   
  TRI_ASSERT(fid > 0);
  TRI_ASSERT(position != nullptr);

  if (operation.type == TRI_VOC_DOCUMENT_OPERATION_INSERT ||
      operation.type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
    // adjust the data position in the header
    operation.header->setDataPtr(position);  // PROTECTED by ongoing trx from operation
    if (operation.type == TRI_VOC_DOCUMENT_OPERATION_INSERT && sizeChanged) {
      document->_headersPtr->adjustTotalSize(0, sizeChanged);
    }
  }

  // set header file id
  operation.header->_fid = fid;

  TRI_ASSERT(operation.header->_fid > 0);

  if (isSingleOperationTransaction) {
    // operation is directly executed
    operation.handle();

    ++document->_uncollectedLogfileEntries;

    if (operation.type == TRI_VOC_DOCUMENT_OPERATION_UPDATE ||
        operation.type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
      // update datafile statistics for the old header
      TRI_ASSERT(operation.oldHeader._fid > 0);
       
      TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

      TRI_doc_datafile_info_t* dfi = TRI_FindDatafileInfoDocumentCollection(document, operation.oldHeader._fid, false);
      // the old header might point to the WAL. in this case, there'll be no stats update

      if (dfi != nullptr) {
        TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(operation.oldHeader.getDataPtr());  // PROTECTED by trx from above
        dfi->_numberDead += 1;
        dfi->_sizeDead += TRI_DF_ALIGN_BLOCK(marker->_size);
        dfi->_numberAlive -= 1;
        dfi->_sizeAlive -= TRI_DF_ALIGN_BLOCK(marker->_size);
      }
      
      TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
    }
  }
  else {
    // operation is buffered and might be rolled back
    if (trxCollection->_operations == nullptr) {
      trxCollection->_operations = new std::vector<triagens::wal::DocumentOperation*>;
      trx->_hasOperations = true;
    }

    triagens::wal::DocumentOperation* copy = operation.swap();
    trxCollection->_operations->push_back(copy);
    copy->handle();
  }

  TRI_UpdateRevisionDocumentCollection(document, operation.rid, false);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_BeginTransaction (TRI_transaction_t* trx,
                          TRI_transaction_hint_t hints,
                          int nestingLevel) {
  LOG_TRX(trx, nestingLevel, "%s %s transaction", "beginning", (trx->_type == TRI_TRANSACTION_READ ? "read" : "write"));

  if (nestingLevel == 0) {
    TRI_ASSERT(trx->_status == TRI_TRANSACTION_CREATED);
 
    if (! HasHint(trx, TRI_TRANSACTION_HINT_NO_THROTTLING) &&
        trx->_type == TRI_TRANSACTION_WRITE &&
        triagens::wal::LogfileManager::instance()->canBeThrottled()) {
      // write-throttling?
      static uint64_t const WaitTime = 50000;
      uint64_t const maxIterations = triagens::wal::LogfileManager::instance()->maxThrottleWait() / (WaitTime / 1000);
      uint64_t iterations = 0;
     
      while (triagens::wal::LogfileManager::instance()->isThrottled()) {
        if (++iterations == maxIterations) {
          return TRI_ERROR_ARANGO_WRITE_THROTTLE_TIMEOUT;
        }

        usleep(WaitTime);
      }
    }

    // get a new id
    trx->_id = TRI_NewTickServer();

    // set hints
    trx->_hints = hints;

    // register a protector
    triagens::wal::LogfileManager::instance()->registerTransaction(trx->_id);
  }
  else {
    TRI_ASSERT(trx->_status == TRI_TRANSACTION_RUNNING);
  }

  int res = UseCollections(trx, nestingLevel);

  if (res == TRI_ERROR_NO_ERROR) {
    // all valid
    if (nestingLevel == 0) {
      UpdateTransactionStatus(trx, TRI_TRANSACTION_RUNNING);
      
      // defer writing of the begin marker until necessary!
    }
  }
  else {
    // something is wrong
    if (nestingLevel == 0) {
      UpdateTransactionStatus(trx, TRI_TRANSACTION_ABORTED);
    }

    // free what we have got so far
    UnuseCollections(trx, nestingLevel);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_CommitTransaction (TRI_transaction_t* trx,
                           int nestingLevel) {
  LOG_TRX(trx, nestingLevel, "%s %s transaction", "committing", (trx->_type == TRI_TRANSACTION_READ ? "read" : "write"));

  TRI_ASSERT(trx->_status == TRI_TRANSACTION_RUNNING);

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

  UnuseCollections(trx, nestingLevel);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief abort and rollback a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_AbortTransaction (TRI_transaction_t* trx,
                          int nestingLevel) {
  LOG_TRX(trx, nestingLevel, "%s %s transaction", "aborting", (trx->_type == TRI_TRANSACTION_READ ? "read" : "write"));

  TRI_ASSERT(trx->_status == TRI_TRANSACTION_RUNNING);

  int res = TRI_ERROR_NO_ERROR;

  if (nestingLevel == 0) {
    res = WriteAbortMarker(trx);

    UpdateTransactionStatus(trx, TRI_TRANSACTION_ABORTED);

    FreeOperations(trx);
  }

  UnuseCollections(trx, nestingLevel);

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
