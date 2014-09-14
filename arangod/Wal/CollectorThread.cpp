////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead log garbage collection thread
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "CollectorThread.h"
#include "Basics/MutexLocker.h"
#include "Basics/hashes.h"
#include "Basics/logging.h"
#include "Basics/ConditionLocker.h"
#include "Utils/CollectionGuard.h"
#include "Utils/DatabaseGuard.h"
#include "Utils/Exception.h"
#include "Utils/transactions.h"
#include "VocBase/document-collection.h"
#include "VocBase/server.h"
#include "VocBase/voc-shaper.h"
#include "Wal/Logfile.h"
#include "Wal/LogfileManager.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return a reference to an existing datafile statistics struct
////////////////////////////////////////////////////////////////////////////////

static inline TRI_doc_datafile_info_t& getDfi (CollectorCache* cache,
                                               TRI_voc_fid_t fid) {
  return cache->dfi[fid];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a reference to an existing datafile statistics struct,
/// create it if it does not exist
////////////////////////////////////////////////////////////////////////////////

static inline TRI_doc_datafile_info_t& createDfi (CollectorCache* cache,
                                                  TRI_voc_fid_t fid) {
  auto it = cache->dfi.find(fid);
  if (it != cache->dfi.end()) {
    return (*it).second;
  }

  TRI_doc_datafile_info_t dfi;
  memset(&dfi, 0, sizeof(TRI_doc_datafile_info_t));
  cache->dfi.insert(std::make_pair(fid, dfi));

  return getDfi(cache, fid);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    CollectorState
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief state that is built up when scanning a WAL logfile
////////////////////////////////////////////////////////////////////////////////

struct CollectorState {
  std::unordered_map<TRI_voc_cid_t, TRI_voc_tick_t>                           collections;
  std::unordered_map<TRI_voc_cid_t, int64_t>                                  operationsCount;
  std::unordered_map<TRI_voc_cid_t, CollectorThread::OperationsType>          structuralOperations;
  std::unordered_map<TRI_voc_cid_t, CollectorThread::DocumentOperationsType>  documentOperations;
  std::unordered_set<TRI_voc_tid_t>                                           failedTransactions;
  std::unordered_set<TRI_voc_tid_t>                                           handledTransactions;
  std::unordered_set<TRI_voc_cid_t>                                           droppedCollections;
  std::unordered_set<TRI_voc_tick_t>                                          droppedDatabases;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a collection can be ignored in the gc
////////////////////////////////////////////////////////////////////////////////

static bool ShouldIgnoreCollection (CollectorState const* state,
                                    TRI_voc_cid_t cid) {
  if (state->droppedCollections.find(cid) != state->droppedCollections.end()) {
    // collection was dropped
    return true;
  }

  // look up database id for collection
  auto it = state->collections.find(cid);
  if (it == state->collections.end()) {
    // no database found for collection - should not happen normally
    return true;
  }

  TRI_voc_tick_t databaseId = (*it).second;

  if (state->droppedDatabases.find(databaseId) != state->droppedDatabases.end()) {
    // database of the collection was already dropped
    return true;
  }

  // collection not dropped, database not dropped
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback to handle one marker during collection
////////////////////////////////////////////////////////////////////////////////

static bool ScanMarker (TRI_df_marker_t const* marker,
                        void* data,
                        TRI_datafile_t* datafile) {
  CollectorState* state = reinterpret_cast<CollectorState*>(data);

  TRI_ASSERT(marker != nullptr);

  switch (marker->_type) {
    case TRI_WAL_MARKER_ATTRIBUTE: {
      attribute_marker_t const* m = reinterpret_cast<attribute_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tick_t databaseId = m->_databaseId;

      state->collections[collectionId] = databaseId;

      // fill list of structural operations
      state->structuralOperations[collectionId].push_back(marker);
      // state->operationsCount[collectionId]++; // do not count this operation
      break;
    }

    case TRI_WAL_MARKER_SHAPE: {
      shape_marker_t const* m = reinterpret_cast<shape_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tick_t databaseId = m->_databaseId;

      state->collections[collectionId] = databaseId;

      // fill list of structural operations
      state->structuralOperations[collectionId].push_back(marker);
      // state->operationsCount[collectionId]++; // do not count this operation
      break;
    }

    case TRI_WAL_MARKER_DOCUMENT: {
      document_marker_t const* m = reinterpret_cast<document_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tid_t transactionId = m->_transactionId;

      if (state->failedTransactions.find(transactionId) != state->failedTransactions.end()) {
        // transaction had failed
        state->operationsCount[collectionId]++;
        break;
      }

      char const* key = reinterpret_cast<char const*>(m) + m->_offsetKey;
      state->documentOperations[collectionId][std::string(key)] = marker;
      state->operationsCount[collectionId]++;
      state->collections[collectionId] = m->_databaseId;
      break;
    }

    case TRI_WAL_MARKER_EDGE: {
      edge_marker_t const* m = reinterpret_cast<edge_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tid_t transactionId = m->_transactionId;

      if (state->failedTransactions.find(transactionId) != state->failedTransactions.end()) {
        // transaction had failed
        state->operationsCount[collectionId]++;
        break;
      }

      char const* key = reinterpret_cast<char const*>(m) + m->_offsetKey;
      state->documentOperations[collectionId][std::string(key)] = marker;
      state->operationsCount[collectionId]++;
      state->collections[collectionId] = m->_databaseId;
      break;
    }

    case TRI_WAL_MARKER_REMOVE: {
      remove_marker_t const* m = reinterpret_cast<remove_marker_t const*>(marker);
      TRI_voc_cid_t collectionId = m->_collectionId;
      TRI_voc_tid_t transactionId = m->_transactionId;

      if (state->failedTransactions.find(transactionId) != state->failedTransactions.end()) {
        // transaction had failed
        state->operationsCount[collectionId]++;
        break;
      }

      char const* key = reinterpret_cast<char const*>(m) + sizeof(remove_marker_t);
      state->documentOperations[collectionId][std::string(key)] = marker;
      state->operationsCount[collectionId]++;
      state->collections[collectionId] = m->_databaseId;
      break;
    }

    case TRI_WAL_MARKER_BEGIN_TRANSACTION:
    case TRI_WAL_MARKER_COMMIT_TRANSACTION: {
      break;
    }
    
    case TRI_WAL_MARKER_ABORT_TRANSACTION: {
      transaction_abort_marker_t const* m = reinterpret_cast<transaction_abort_marker_t const*>(marker);
      // note which abort markers we found
      state->handledTransactions.insert(m->_transactionId);
      break;
    }
    
    case TRI_WAL_MARKER_BEGIN_REMOTE_TRANSACTION:
    case TRI_WAL_MARKER_COMMIT_REMOTE_TRANSACTION: {
      break;
    }
    
    case TRI_WAL_MARKER_ABORT_REMOTE_TRANSACTION: {
      transaction_remote_abort_marker_t const* m = reinterpret_cast<transaction_remote_abort_marker_t const*>(marker);
      // note which abort markers we found
      state->handledTransactions.insert(m->_transactionId);
      break;
    }

    case TRI_WAL_MARKER_DROP_COLLECTION: {
      collection_drop_marker_t const* m = reinterpret_cast<collection_drop_marker_t const*>(marker);
      // note that the collection was dropped and doesn't need to be collected
      state->droppedCollections.insert(m->_collectionId);
      break;
    }

    case TRI_WAL_MARKER_DROP_DATABASE: {
      database_drop_marker_t const* m = reinterpret_cast<database_drop_marker_t const*>(marker);
      // note that the database was dropped and doesn't need to be collected
      state->droppedDatabases.insert(m->_databaseId);
      break;
    }

    default: {
      // do nothing intentionally
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                             class CollectorThread
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief wait interval for the collector thread when idle
////////////////////////////////////////////////////////////////////////////////

uint64_t const CollectorThread::Interval = 1000000;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the collector thread
////////////////////////////////////////////////////////////////////////////////

CollectorThread::CollectorThread (LogfileManager* logfileManager,
                                  TRI_server_t* server)
  : Thread("WalCollector"),
    _logfileManager(logfileManager),
    _server(server),
    _condition(),
    _operationsQueueLock(),
    _operationsQueue(),
    _numPendingOperations(0),
    _stop(0) {

  allowAsynchronousCancelation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the collector thread
////////////////////////////////////////////////////////////////////////////////

CollectorThread::~CollectorThread () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the collector thread
////////////////////////////////////////////////////////////////////////////////

void CollectorThread::stop () {
  if (_stop > 0) {
    return;
  }

  _stop = 1;
  _condition.signal();

  while (_stop != 2) {
    usleep(10000);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief signal the thread that there is something to do
////////////////////////////////////////////////////////////////////////////////

void CollectorThread::signal () {
  CONDITION_LOCKER(guard, _condition);
  guard.signal();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief main loop
////////////////////////////////////////////////////////////////////////////////

void CollectorThread::run () {
  int counter = 0;

  while (true) {
    int stop = (int) _stop;
    bool worked = false;

    try {
      // step 1: collect a logfile if any qualifies
      if (stop == 0) {
        // don't collect additional logfiles in case we want to shut down
        worked |= this->collectLogfiles();
      }

      // step 2: update master pointers
      worked |= this->processQueuedOperations();
    }
    catch (triagens::arango::Exception const& ex) {
      int res = ex.code();
      LOG_ERROR("got unexpected error in collectorThread::run: %s", TRI_errno_string(res));
    }
    catch (...) {
      LOG_ERROR("got unspecific error in collectorThread::run");
    }

    if (stop == 0 && ! worked) {
      // sleep only if there was nothing to do
      CONDITION_LOCKER(guard, _condition);

      if (! guard.wait(Interval)) {
        if (++counter > 10) {
          LOG_TRACE("wal collector has queued operations: %d", (int) _operationsQueue.size());
          counter = 0;
        }
      }
    }
    else if (stop == 1 && ! hasQueuedOperations()) {
      // no operations left to execute, we can exit
      break;
    }

    // next iteration
  }

  // all queues are empty, so we can exit
  TRI_ASSERT(! hasQueuedOperations());

  _stop = 2;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief step 1: perform collection of a logfile (if any)
////////////////////////////////////////////////////////////////////////////////

bool CollectorThread::collectLogfiles () {
  TRI_IF_FAILURE("CollectorThreadCollect") {
    return false;
  }

  Logfile* logfile = _logfileManager->getCollectableLogfile();

  if (logfile == nullptr) {
    return false;
  }
    
  _logfileManager->setCollectionRequested(logfile);

  int res = collect(logfile);

  if (res == TRI_ERROR_NO_ERROR) {
    _logfileManager->setCollectionDone(logfile);
    return true;
  }
  else {
    // return the logfile to the logfile manager in case of errors
    logfile->forceStatus(Logfile::StatusType::SEALED);
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief step 2: process all still-queued collection operations
////////////////////////////////////////////////////////////////////////////////

bool CollectorThread::processQueuedOperations () {
  TRI_IF_FAILURE("CollectorThreadProcessQueuedOperations") {
    return false;
  }

  MUTEX_LOCKER(_operationsQueueLock);

  if (_operationsQueue.empty()) {
    // nothing to do
    return false;
  }

  // process operations for each collection
  for (auto it = _operationsQueue.begin(); it != _operationsQueue.end(); ++it) {
    auto& operations = (*it).second;
    TRI_ASSERT(! operations.empty());

    for (auto it2 = operations.begin(); it2 != operations.end(); /* no hoisting */ ) {
      Logfile* logfile = (*it2)->logfile;

      int res = TRI_ERROR_INTERNAL;

      try {
        res = processCollectionOperations((*it2));
      }
      catch (triagens::arango::Exception const& ex) {
        res = ex.code();
      }

      if (res == TRI_ERROR_LOCK_TIMEOUT) {
        // could not acquire write-lock for collection in time
        // do not delete the operations
        ++it2;
        continue;
      }

      if (res == TRI_ERROR_NO_ERROR) {
        LOG_TRACE("queued operations applied successfully");
      }
      else if (res == TRI_ERROR_ARANGO_DATABASE_NOT_FOUND ||
               res == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
        // these are expected errors
        LOG_TRACE("removing queued operations for already deleted collection");
        res = TRI_ERROR_NO_ERROR;
      }
      else {
        LOG_WARNING("got unexpected error code while applying queued operations: %s", TRI_errno_string(res));
      }

      if (res == TRI_ERROR_NO_ERROR) {
        uint64_t numOperations = (*it2)->operations->size();
        uint64_t maxNumPendingOperations = _logfileManager->throttleWhenPending();

        if (maxNumPendingOperations > 0 && 
            _numPendingOperations >= maxNumPendingOperations &&
            (_numPendingOperations - numOperations) < maxNumPendingOperations) {
          // write-throttling was active, but can be turned off now
          _logfileManager->deactivateWriteThrottling();
          LOG_INFO("deactivating write-throttling");
        }

        _numPendingOperations -= numOperations;

        // delete the object
        delete (*it2);

        // delete the element from the vector while iterating over the vector
        it2 = operations.erase(it2);

        _logfileManager->decreaseCollectQueueSize(logfile);
      }
      else {
        // do not delete the object but advance in the operations vector
        ++it2;
      }
    }

    // next collection
  }

  // finally remove all entries from the map with empty vectors
  for (auto it = _operationsQueue.begin(); it != _operationsQueue.end(); /* no hoisting */) {
    if ((*it).second.empty()) {
      it = _operationsQueue.erase(it);
    }
    else {
      ++it;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether there are queued operations left
////////////////////////////////////////////////////////////////////////////////

bool CollectorThread::hasQueuedOperations () {
  MUTEX_LOCKER(_operationsQueueLock);

  return ! _operationsQueue.empty();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process all operations for a single collection
////////////////////////////////////////////////////////////////////////////////

int CollectorThread::processCollectionOperations (CollectorCache* cache) {
  triagens::arango::DatabaseGuard dbGuard(_server, cache->databaseId);
  TRI_vocbase_t* vocbase = dbGuard.database();
  TRI_ASSERT(vocbase != nullptr);

  triagens::arango::CollectionGuard collectionGuard(vocbase, cache->collectionId, true);
  TRI_vocbase_col_t* collection = collectionGuard.collection();

  TRI_ASSERT(collection != nullptr);

  // create a fake transaction while accessing the collection
  triagens::arango::TransactionBase trx(true);

  TRI_document_collection_t* document = collection->_collection;

  // first try to read-lock the compactor-lock, afterwards try to write-lock the collection
  // if any locking attempt fails, release and try again next time

  if (! TRI_TryReadLockReadWriteLock(&document->_compactionLock)) {
    return TRI_ERROR_LOCK_TIMEOUT;
  }

  // try to acquire the write lock on the collection
  if (! TRI_TRY_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document)) {
    TRI_ReadUnlockReadWriteLock(&document->_compactionLock);
    LOG_TRACE("wal collector couldn't acquire write lock for collection '%llu'", (unsigned long long) document->_info._cid);

    return TRI_ERROR_LOCK_TIMEOUT;
  }

  int res;

  try {
    // now we have the write lock on the collection
    LOG_TRACE("wal collector processing operations for collection '%s'", document->_info._name);

    TRI_ASSERT(! cache->operations->empty());

    for (auto it = cache->operations->begin(); it != cache->operations->end(); ++it) {
      auto operation = (*it);

      TRI_df_marker_t const* walMarker = reinterpret_cast<TRI_df_marker_t const*>(operation.walPosition);
      TRI_df_marker_t const* marker = reinterpret_cast<TRI_df_marker_t const*>(operation.datafilePosition);
      TRI_voc_size_t const datafileMarkerSize = operation.datafileMarkerSize;
      TRI_voc_fid_t const fid = operation.datafileId;

      TRI_ASSERT(walMarker != nullptr);
      TRI_ASSERT(marker != nullptr);

      if (walMarker->_type == TRI_WAL_MARKER_DOCUMENT) {
        wal::document_marker_t const* m = reinterpret_cast<wal::document_marker_t const*>(walMarker);
        char const* key = reinterpret_cast<char const*>(m) + m->_offsetKey;

        TRI_doc_mptr_t* found = static_cast<TRI_doc_mptr_t*>(TRI_LookupByKeyPrimaryIndex(&document->_primaryIndex, key));

        if (found == nullptr || found->_rid != m->_revisionId || found->getDataPtr() != walMarker) {
          // somebody inserted a new revision of the document or the revision was already moved by the compactor
          auto& dfi = createDfi(cache, fid);
          dfi._numberDead++;
          dfi._sizeDead += (int64_t) TRI_DF_ALIGN_BLOCK(datafileMarkerSize);
          dfi._numberAlive--;
          dfi._sizeAlive -= (int64_t) TRI_DF_ALIGN_BLOCK(datafileMarkerSize);
        }
        else {
          // update cap constraint info
          document->_headersPtr->adjustTotalSize(TRI_DF_ALIGN_BLOCK(walMarker->_size),
                                                  TRI_DF_ALIGN_BLOCK(datafileMarkerSize));

          // we can safely update the master pointer's dataptr value
          found->setDataPtr(static_cast<void*>(const_cast<char*>(operation.datafilePosition)));
          found->_fid = fid;
        }
      }
      else if (walMarker->_type == TRI_WAL_MARKER_EDGE) {
        wal::edge_marker_t const* m = reinterpret_cast<wal::edge_marker_t const*>(walMarker);
        char const* key = reinterpret_cast<char const*>(m) + m->_offsetKey;

        TRI_doc_mptr_t* found = static_cast<TRI_doc_mptr_t*>(TRI_LookupByKeyPrimaryIndex(&document->_primaryIndex, key));

        if (found == nullptr || found->_rid != m->_revisionId || found->getDataPtr() != walMarker) {
          // somebody inserted a new revision of the document or the revision was already moved by the compactor
          auto& dfi = createDfi(cache, fid);
          dfi._numberDead++;
          dfi._sizeDead += (int64_t) TRI_DF_ALIGN_BLOCK(datafileMarkerSize);
          dfi._numberAlive--;
          dfi._sizeAlive -= (int64_t) TRI_DF_ALIGN_BLOCK(datafileMarkerSize);
        }
        else {
          // update cap constraint info
          document->_headersPtr->adjustTotalSize(TRI_DF_ALIGN_BLOCK(walMarker->_size),
                                                  TRI_DF_ALIGN_BLOCK(datafileMarkerSize));

          // we can safely update the master pointer's dataptr value
          found->setDataPtr(static_cast<void*>(const_cast<char*>(operation.datafilePosition)));
          found->_fid = fid;
        }
      }
      else if (walMarker->_type == TRI_WAL_MARKER_REMOVE) {
        wal::remove_marker_t const* m = reinterpret_cast<wal::remove_marker_t const*>(walMarker);
        char const* key = reinterpret_cast<char const*>(m) + sizeof(wal::remove_marker_t);

        TRI_doc_mptr_t* found = static_cast<TRI_doc_mptr_t*>(TRI_LookupByKeyPrimaryIndex(&document->_primaryIndex, key));

        if (found != nullptr && found->_rid > m->_revisionId) {
          // somebody re-created the document with a newer revision
          auto& dfi = createDfi(cache, fid);
          dfi._numberDead++;
          dfi._sizeDead += (int64_t) TRI_DF_ALIGN_BLOCK(datafileMarkerSize);
          dfi._numberAlive--;
          dfi._sizeAlive -= (int64_t) TRI_DF_ALIGN_BLOCK(datafileMarkerSize);
        }
      }
      else if (walMarker->_type == TRI_WAL_MARKER_ATTRIBUTE) {
        // move the pointer to the attribute from WAL to the datafile
        TRI_MoveMarkerVocShaper(document->getShaper(), const_cast<TRI_df_marker_t*>(marker), (void*) walMarker);  // ONLY IN COLLECTOR, PROTECTED by COLLECTION LOCK and fake trx here
      }
      else if (walMarker->_type == TRI_WAL_MARKER_SHAPE) {
        // move the pointer to the shape from WAL to the datafile
        TRI_MoveMarkerVocShaper(document->getShaper(), const_cast<TRI_df_marker_t*>(marker), (void*) walMarker);  // ONLY IN COLLECTOR, PROTECTED by COLLECTION LOCK and fake trx here
      }
      else {
        // a marker we won't care about
      }
    }


    // finally update all datafile statistics
    LOG_TRACE("updating datafile statistics for collection '%s'", document->_info._name);
    updateDatafileStatistics(document, cache);
        
    document->_uncollectedLogfileEntries -= cache->totalOperationsCount;
    if (document->_uncollectedLogfileEntries < 0) {
      document->_uncollectedLogfileEntries = 0;
    }

    res = TRI_ERROR_NO_ERROR;
  }
  catch (triagens::arango::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  // always release the locks
  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  TRI_ReadUnlockReadWriteLock(&document->_compactionLock);

  LOG_TRACE("wal collector processed operations for collection '%s' with status: %s",
            document->_info._name,
            TRI_errno_string(res));

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief collect one logfile
////////////////////////////////////////////////////////////////////////////////

int CollectorThread::collect (Logfile* logfile) {
  TRI_ASSERT(logfile != nullptr);

  LOG_TRACE("collecting logfile %llu", (unsigned long long) logfile->id());

  TRI_datafile_t* df = logfile->df();

  TRI_ASSERT(df != nullptr);

  // create a state for the collector, beginning with the list of failed transactions
  CollectorState state;
  state.failedTransactions = _logfileManager->getFailedTransactions();
/*
  if (_inRecovery) {
    state.droppedCollections = _logfileManager->getDroppedCollections();
    state.droppedDatabases   = _logfileManager->getDroppedDatabases();
  }
*/

  // scan all markers in logfile, this will fill the state
  bool result = TRI_IterateDatafile(df, &ScanMarker, static_cast<void*>(&state));

  if (! result) {
    return TRI_ERROR_INTERNAL;
  }

  // get an aggregated list of all collection ids
  std::set<TRI_voc_cid_t> collectionIds;
  for (auto it = state.structuralOperations.begin(); it != state.structuralOperations.end(); ++it) {
    auto cid = (*it).first;

    if (! ShouldIgnoreCollection(&state, cid)) {
      collectionIds.insert((*it).first);
    }
  }

  for (auto it = state.documentOperations.begin(); it != state.documentOperations.end(); ++it) {
    auto cid = (*it).first;

    if (state.structuralOperations.find(cid) == state.structuralOperations.end() &&
        ! ShouldIgnoreCollection(&state, cid)) {
      collectionIds.insert(cid);
    }
  }

  // now for each collection, write all surviving markers into collection datafiles
  for (auto it = collectionIds.begin(); it != collectionIds.end(); ++it) {
    auto cid = (*it);

    OperationsType sortedOperations;

    // insert structural operations - those are already sorted by tick
    if (state.structuralOperations.find(cid) != state.structuralOperations.end()) {
      OperationsType const& ops = state.structuralOperations[cid];

      sortedOperations.insert(sortedOperations.begin(), ops.begin(), ops.end());
      TRI_ASSERT_EXPENSIVE(sortedOperations.size() == ops.size());
    }

    // insert document operations - those are sorted by key, not by tick
    if (state.documentOperations.find(cid) != state.documentOperations.end()) {
      DocumentOperationsType const& ops = state.documentOperations[cid];


      for (auto it2 = ops.begin(); it2 != ops.end(); ++it2) {
        sortedOperations.push_back((*it2).second);
      }

      // sort vector by marker tick
      std::sort(sortedOperations.begin(), sortedOperations.end(), [] (TRI_df_marker_t const* left, TRI_df_marker_t const* right) {
        return (left->_tick < right->_tick);
      });
    }

    if (! sortedOperations.empty()) {
      int res = TRI_ERROR_INTERNAL;

      try {
        res = transferMarkers(logfile, cid, state.collections[cid], state.operationsCount[cid], sortedOperations);
      }
      catch (triagens::arango::Exception const& ex) {
        res = ex.code();
      }
      catch (...) {
        res = TRI_ERROR_INTERNAL;
      }

      if (res != TRI_ERROR_NO_ERROR &&
          res != TRI_ERROR_ARANGO_DATABASE_NOT_FOUND &&
          res != TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
        LOG_WARNING("got unexpected error in CollectorThread::collect: %s", TRI_errno_string(res));
        // abort early
        return res;
      }
    }
  }

  // TODO: what to do if an error has occurred?

  // remove all handled transactions from failedTransactions list
  if (! state.handledTransactions.empty()) {
    _logfileManager->unregisterFailedTransactions(state.handledTransactions);
  }
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief transfer markers into a collection
////////////////////////////////////////////////////////////////////////////////

int CollectorThread::transferMarkers (Logfile* logfile,
                                      TRI_voc_cid_t collectionId,
                                      TRI_voc_tick_t databaseId,
                                      int64_t totalOperationsCount,
                                      OperationsType const& operations) {

  TRI_ASSERT(! operations.empty());

  // prepare database and collection
  triagens::arango::DatabaseGuard dbGuard(_server, databaseId);
  TRI_vocbase_t* vocbase = dbGuard.database();
  TRI_ASSERT(vocbase != nullptr);

  triagens::arango::CollectionGuard collectionGuard(vocbase, collectionId, true);
  TRI_vocbase_col_t* collection = collectionGuard.collection();
  TRI_ASSERT(collection != nullptr);

  TRI_document_collection_t* document = collection->_collection;
  TRI_ASSERT(document != nullptr);

  LOG_TRACE("collector transferring markers for '%s', totalOperationsCount: %llu",
            document->_info._name,
            (unsigned long long) totalOperationsCount);

  CollectorCache* cache = new CollectorCache(collectionId,
                                             databaseId,
                                             logfile,
                                             totalOperationsCount,
                                             operations.size());

  int res = TRI_ERROR_INTERNAL;

  try {
    res = executeTransferMarkers(document, cache, operations);

    if (res == TRI_ERROR_NO_ERROR && ! cache->operations->empty()) {
      // now sync the datafile
      res = syncDatafileCollection(document);

      // note: cache is passed by reference and can be modified by queueOperations
      // (i.e. set to nullptr!)
      queueOperations(logfile, cache);
    }
  }
  catch (triagens::arango::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }


  if (cache != nullptr) {
    // prevent memleak
    delete cache;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief transfer markers into a collection, actual work
/// the collection must have been prepared to call this function
////////////////////////////////////////////////////////////////////////////////

int CollectorThread::executeTransferMarkers (TRI_document_collection_t* document,
                                             CollectorCache* cache,
                                             OperationsType const& operations) {

  // used only for crash / recovery tests
  int numMarkers = 0;

  TRI_voc_tick_t const minTransferTick = document->_tickMax;
  TRI_ASSERT(! operations.empty());

  for (auto it2 = operations.begin(); it2 != operations.end(); ++it2) {
    TRI_df_marker_t const* source = (*it2);

    if (source->_tick <= minTransferTick) {
      // we have already transferred this marker in a previous run, nothing to do
      continue;
    }

    TRI_IF_FAILURE("CollectorThreadTransfer") {
      if (++numMarkers > 5) {
        // intentionally kill the server
        TRI_SegfaultDebugging("CollectorThreadTransfer");
      }
    }

    char const* base = reinterpret_cast<char const*>(source);

    switch (source->_type) {
      case TRI_WAL_MARKER_ATTRIBUTE: {
        char const* name = base + sizeof(attribute_marker_t);
        size_t n = strlen(name) + 1; // add NULL byte
        TRI_voc_size_t const totalSize = static_cast<TRI_voc_size_t>(sizeof(TRI_df_attribute_marker_t) + n);

        char* dst = nextFreeMarkerPosition(document, source->_tick, TRI_DF_MARKER_ATTRIBUTE, totalSize, cache);

        if (dst == nullptr) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }

        // set attribute id
        TRI_df_attribute_marker_t* m = reinterpret_cast<TRI_df_attribute_marker_t*>(dst);
        m->_aid = reinterpret_cast<attribute_marker_t const*>(source)->_attributeId;

        // copy attribute name into marker
        memcpy(dst + sizeof(TRI_df_attribute_marker_t), name, n);

        finishMarker(base, dst, document, source->_tick, cache);

        // update statistics
        auto& dfi = getDfi(cache, cache->lastFid);
        dfi._numberAttributes++;
        dfi._sizeAttributes += (int64_t) TRI_DF_ALIGN_BLOCK(totalSize);
        break;
      }

      case TRI_WAL_MARKER_SHAPE: {
        char const* shape = base + sizeof(shape_marker_t);
        ptrdiff_t shapeLength = source->_size - (shape - base);
        TRI_voc_size_t const totalSize = static_cast<TRI_voc_size_t>(sizeof(TRI_df_shape_marker_t) + shapeLength);

        char* dst = nextFreeMarkerPosition(document, source->_tick, TRI_DF_MARKER_SHAPE, totalSize, cache);

        if (dst == nullptr) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }

        // copy shape into marker
        memcpy(dst + sizeof(TRI_df_shape_marker_t), shape, shapeLength);

        finishMarker(base, dst, document, source->_tick, cache);

        // update statistics
        auto& dfi = getDfi(cache, cache->lastFid);
        dfi._numberShapes++;
        dfi._sizeShapes += (int64_t) TRI_DF_ALIGN_BLOCK(totalSize);
        break;
      }

      case TRI_WAL_MARKER_DOCUMENT: {
        document_marker_t const* orig = reinterpret_cast<document_marker_t const*>(source);
        char const* shape = base + orig->_offsetJson;
        ptrdiff_t shapeLength = source->_size - (shape - base);

        char const* key = base + orig->_offsetKey;
        size_t n = strlen(key) + 1; // add NULL byte
        TRI_voc_size_t const totalSize = static_cast<TRI_voc_size_t>(sizeof(TRI_doc_document_key_marker_t) + TRI_DF_ALIGN_BLOCK(n) + shapeLength);

        char* dst = nextFreeMarkerPosition(document, source->_tick, TRI_DOC_MARKER_KEY_DOCUMENT, totalSize, cache);

        if (dst == nullptr) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }

        TRI_doc_document_key_marker_t* m = reinterpret_cast<TRI_doc_document_key_marker_t*>(dst);
        m->_rid        = orig->_revisionId;
        m->_tid        = 0; // convert into standalone transaction
        m->_shape      = orig->_shape;
        m->_offsetKey  = sizeof(TRI_doc_document_key_marker_t);
        m->_offsetJson = static_cast<uint16_t>(m->_offsetKey + TRI_DF_ALIGN_BLOCK(n));

        // copy key into marker
        memcpy(dst + m->_offsetKey, key, n);

        // copy shape into marker
        memcpy(dst + m->_offsetJson, shape, shapeLength);

        finishMarker(base, dst, document, source->_tick, cache);

        // update statistics
        auto& dfi = getDfi(cache, cache->lastFid);
        dfi._numberAlive++;
        dfi._sizeAlive += (int64_t) TRI_DF_ALIGN_BLOCK(totalSize);
        break;
      }

      case TRI_WAL_MARKER_EDGE: {
        edge_marker_t const* orig = reinterpret_cast<edge_marker_t const*>(source);
        char const* shape = base + orig->_offsetJson;
        ptrdiff_t shapeLength = source->_size - (shape - base);

        char const* key = base + orig->_offsetKey;
        size_t n = strlen(key) + 1; // add NULL byte
        char const* toKey = base + orig->_offsetToKey;
        size_t to = strlen(toKey) + 1; // add NULL byte
        char const* fromKey = base + orig->_offsetFromKey;
        size_t from = strlen(fromKey) + 1; // add NULL byte
        TRI_voc_size_t const totalSize = static_cast<TRI_voc_size_t>(sizeof(TRI_doc_edge_key_marker_t) + TRI_DF_ALIGN_BLOCK(n) +  TRI_DF_ALIGN_BLOCK(to) +  TRI_DF_ALIGN_BLOCK(from) + shapeLength);

        char* dst = nextFreeMarkerPosition(document, source->_tick, TRI_DOC_MARKER_KEY_EDGE, totalSize, cache);

        if (dst == nullptr) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }

        size_t offsetKey = sizeof(TRI_doc_edge_key_marker_t);
        TRI_doc_edge_key_marker_t* m = reinterpret_cast<TRI_doc_edge_key_marker_t*>(dst);
        m->base._rid           = orig->_revisionId;
        m->base._tid           = 0; // convert into standalone transaction
        m->base._shape         = orig->_shape;
        m->base._offsetKey     = static_cast<uint16_t>(offsetKey);
        m->base._offsetJson    = static_cast<uint16_t>(offsetKey + TRI_DF_ALIGN_BLOCK(n) + TRI_DF_ALIGN_BLOCK(to) + TRI_DF_ALIGN_BLOCK(from));
        m->_toCid              = orig->_toCid;
        m->_fromCid            = orig->_fromCid;
        m->_offsetToKey        = static_cast<uint16_t>(offsetKey + TRI_DF_ALIGN_BLOCK(n));
        m->_offsetFromKey      = static_cast<uint16_t>(offsetKey + TRI_DF_ALIGN_BLOCK(n) + TRI_DF_ALIGN_BLOCK(to));

        // copy key into marker
        memcpy(dst + offsetKey, key, n);
        memcpy(dst + m->_offsetToKey, toKey, to);
        memcpy(dst + m->_offsetFromKey, fromKey, from);

        // copy shape into marker
        memcpy(dst + m->base._offsetJson, shape, shapeLength);

        finishMarker(base, dst, document, source->_tick, cache);

        // update statistics
        auto& dfi = getDfi(cache, cache->lastFid);
        dfi._numberAlive++;
        dfi._sizeAlive += (int64_t) TRI_DF_ALIGN_BLOCK(totalSize);
        break;
      }

      case TRI_WAL_MARKER_REMOVE: {
        remove_marker_t const* orig = reinterpret_cast<remove_marker_t const*>(source);

        char const* key = base + sizeof(remove_marker_t);
        size_t n = strlen(key) + 1; // add NULL byte
        TRI_voc_size_t const totalSize = static_cast<TRI_voc_size_t>(sizeof(TRI_doc_deletion_key_marker_t) + n);

        char* dst = nextFreeMarkerPosition(document, source->_tick, TRI_DOC_MARKER_KEY_DELETION, totalSize, cache);

        if (dst == nullptr) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }

        TRI_doc_deletion_key_marker_t* m = reinterpret_cast<TRI_doc_deletion_key_marker_t*>(dst);
        m->_rid       = orig->_revisionId;
        m->_tid       = 0; // convert into standalone transaction
        m->_offsetKey = sizeof(TRI_doc_deletion_key_marker_t);

        // copy key into marker
        memcpy(dst + m->_offsetKey, key, n);

        finishMarker(base, dst, document, source->_tick, cache);

        // update statistics
        auto& dfi = getDfi(cache, cache->lastFid);
        dfi._numberDeletion++;
        break;
      }
    }
  }

  TRI_IF_FAILURE("CollectorThreadTransferFinal") {
    // intentionally kill the server
    TRI_SegfaultDebugging("CollectorThreadTransferFinal");
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert the collect operations into a per-collection queue
////////////////////////////////////////////////////////////////////////////////

int CollectorThread::queueOperations (triagens::wal::Logfile* logfile,
                                      CollectorCache*& cache) {
  TRI_voc_cid_t cid = cache->collectionId;
  uint64_t maxNumPendingOperations = _logfileManager->throttleWhenPending();
 
  TRI_ASSERT(! cache->operations->empty());

  {
    MUTEX_LOCKER(_operationsQueueLock);

    auto it = _operationsQueue.find(cid);
    if (it == _operationsQueue.end()) {
      std::vector<CollectorCache*> ops;
      ops.push_back(cache);
      _operationsQueue.insert(it, std::make_pair(cid, ops));
      _logfileManager->increaseCollectQueueSize(logfile);
    }
    else {
      (*it).second.push_back(cache);
      _logfileManager->increaseCollectQueueSize(logfile);
    }
  }
  
  uint64_t numOperations = cache->operations->size();

  if (maxNumPendingOperations > 0 && 
      _numPendingOperations < maxNumPendingOperations &&
      (_numPendingOperations + numOperations) >= maxNumPendingOperations) {
    // activate write-throttling!
    _logfileManager->activateWriteThrottling();
    LOG_WARNING("queued more than %llu pending WAL collector operations. now activating write-throttling", 
                (unsigned long long) maxNumPendingOperations);
  }
  
  _numPendingOperations += numOperations;

  // we have put the object into the queue successfully
  // now set the original pointer to null so it isn't double-freed
  cache = nullptr;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update a collection's datafile information
////////////////////////////////////////////////////////////////////////////////

int CollectorThread::updateDatafileStatistics (TRI_document_collection_t* document,
                                               CollectorCache* cache) {
  // iterate over all datafile infos and update the collection's datafile stats
  for (auto it = cache->dfi.begin(); it != cache->dfi.end(); ++it) {
    TRI_voc_fid_t fid = (*it).first;

    TRI_doc_datafile_info_t* dst = TRI_FindDatafileInfoDocumentCollection(document, fid, true);

    if (dst != nullptr) {
      auto& dfi = (*it).second;

      dst->_numberAttributes   += dfi._numberAttributes;
      dst->_sizeAttributes     += dfi._sizeAttributes;
      dst->_numberShapes       += dfi._numberShapes;
      dst->_sizeShapes         += dfi._sizeShapes;
      dst->_numberAlive        += dfi._numberAlive;
      dst->_sizeAlive          += dfi._sizeAlive;
      dst->_numberDead         += dfi._numberDead;
      dst->_sizeDead           += dfi._sizeDead;
      dst->_numberTransactions += dfi._numberTransactions;
      dst->_sizeTransactions   += dfi._sizeTransactions;
      dst->_numberDeletion     += dfi._numberDeletion;

      // flush the local datafile info so we don't update the statistics twice
      // with the same values
      memset(&dfi, 0, sizeof(TRI_doc_datafile_info_t));
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sync all journals of a collection
////////////////////////////////////////////////////////////////////////////////

int CollectorThread::syncDatafileCollection (TRI_document_collection_t* document) {
  TRI_collection_t* collection = document;
  int res = TRI_ERROR_NO_ERROR;
      

  TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
  // note: only journals need to be handled here as the journal is the
  // only place that's ever written to. if a journal is full, it will have been
  // sealed and synced already
  size_t const n = collection->_journals._length;
      
  for (size_t i = 0; i < n; ++i) {
    TRI_datafile_t* datafile = static_cast<TRI_datafile_t*>(collection->_journals._buffer[i]);

    // we only need to care about physical datafiles
    if (! datafile->isPhysical(datafile)) {
      // anonymous regions do not need to be synced
      continue;
    }

    char const* synced = datafile->_synced;
    char* written      = datafile->_written;
      
    if (synced < written) {
      bool ok = datafile->sync(datafile, synced, written);

      if (ok) {
        LOG_TRACE("msync succeeded %p, size %lu", synced, (unsigned long) (written - synced));
        datafile->_synced = written;
      }
      else {
        res = TRI_errno();
        LOG_ERROR("msync failed with: %s", TRI_last_error());
        datafile->_state = TRI_DF_STATE_WRITE_ERROR;
        break;
      }
    }
  }

  TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the next position for a marker of the specified size
////////////////////////////////////////////////////////////////////////////////

char* CollectorThread::nextFreeMarkerPosition (TRI_document_collection_t* document,
                                               TRI_voc_tick_t tick,
                                               TRI_df_marker_type_e type,
                                               TRI_voc_size_t size,
                                               CollectorCache* cache) {
  TRI_collection_t* collection = document;
  size = TRI_DF_ALIGN_BLOCK(size);

  char* dst = nullptr;
  TRI_datafile_t* datafile = nullptr;

  TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
  // start with configured journal size
  TRI_voc_size_t targetSize = document->_info._maximalSize;
      
  // make sure that the document fits
  while (targetSize - 256 < size && targetSize < 512 * 1024 * 1024) { // TODO: remove magic number
    targetSize *= 2;
  }

  while (collection->_state == TRI_COL_STATE_WRITE) {
    size_t const n = collection->_journals._length;

    for (size_t i = 0;  i < n;  ++i) {
      // select datafile
      datafile = static_cast<TRI_datafile_t*>(collection->_journals._buffer[i]);

      // try to reserve space

      TRI_df_marker_t* position = nullptr;
      int res = TRI_ReserveElementDatafile(datafile, size, &position, targetSize);

      // found a datafile with enough space left
      if (res == TRI_ERROR_NO_ERROR) {
        datafile->_written = ((char*) position) + size;
        dst = reinterpret_cast<char*>(position);
        TRI_ASSERT(dst != nullptr);
        goto leave;
      }

      if (res != TRI_ERROR_ARANGO_DATAFILE_FULL) {
        // some other error
        LOG_ERROR("cannot select journal: '%s'", TRI_last_error());
        goto leave;
      }

      // journal is full, close it and sync
      LOG_DEBUG("closing full journal '%s'", datafile->getName(datafile));
      TRI_CloseDatafileDocumentCollection(document, i, false);
    }

    datafile = TRI_CreateDatafileDocumentCollection(document, tick, targetSize, false);

    if (datafile == nullptr) {
      int res = TRI_errno();
      // could not create a datafile, this is a serious error
      TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

      THROW_ARANGO_EXCEPTION(res);
    }
  } // next iteration

leave:
  TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  if (dst != nullptr) {
    initMarker(reinterpret_cast<TRI_df_marker_t*>(dst), type, size);

    TRI_ASSERT(datafile != nullptr);

    if (datafile->_fid != cache->lastFid) {
      // datafile has changed
      cache->lastDatafile = datafile;
      cache->lastFid = datafile->_fid;

      // create a local datafile info struct
      createDfi(cache, datafile->_fid);

      // we only need the barriers when we are outside the recovery
      // the compactor will not run during recovery
      TRI_barrier_t* barrier = TRI_CreateBarrierElement(&document->_barrierList);
        
      if (barrier == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      cache->addBarrier(barrier);
    }
  }
  else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_NO_JOURNAL);
  }

  return dst;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a marker
////////////////////////////////////////////////////////////////////////////////

void CollectorThread::initMarker (TRI_df_marker_t* marker,
                                  TRI_df_marker_type_e type,
                                  TRI_voc_size_t size) {
  TRI_ASSERT(marker != nullptr);

  marker->_size = size;
  marker->_type = (TRI_df_marker_type_t) type;
  marker->_crc  = 0;
  marker->_tick = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the tick of a marker and calculate its CRC value
////////////////////////////////////////////////////////////////////////////////

void CollectorThread::finishMarker (char const* walPosition,
                                    char* datafilePosition,
                                    TRI_document_collection_t* document,
                                    TRI_voc_tick_t tick,
                                    CollectorCache* cache) {
  TRI_df_marker_t* marker = reinterpret_cast<TRI_df_marker_t*>(datafilePosition);

  // re-use the original WAL marker's tick
  marker->_tick = tick;

  // calculate the CRC
  TRI_voc_crc_t crc = TRI_InitialCrc32();
  crc = TRI_BlockCrc32(crc, const_cast<char*>(datafilePosition), marker->_size);
  marker->_crc = TRI_FinalCrc32(crc);

  TRI_datafile_t* datafile = cache->lastDatafile;
  TRI_ASSERT(datafile != nullptr);

  // update ticks
  TRI_UpdateTicksDatafile(datafile, marker);

  TRI_ASSERT(document->_tickMax < tick);
  document->_tickMax = tick;

  cache->operations->emplace_back(CollectorOperation(datafilePosition, marker->_size, walPosition, cache->lastFid));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
