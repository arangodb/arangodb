////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "MMFilesCollectorThread.h"
#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/encoding.h"
#include "Basics/hashes.h"
#include "Basics/memory-map.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesCompactionLocker.h"
#include "MMFiles/MMFilesDatafileHelper.h"
#include "MMFiles/MMFilesEngine.h"
#include "MMFiles/MMFilesIndexElement.h"
#include "MMFiles/MMFilesLogfileManager.h"
#include "MMFiles/MMFilesPersistentIndex.h"
#include "MMFiles/MMFilesPrimaryIndex.h"
#include "MMFiles/MMFilesWalLogfile.h"
#include "RestServer/TransactionManagerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Helpers.h"
#include "Utils/CollectionGuard.h"
#include "Utils/DatabaseGuard.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/Hints.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

/// @brief state that is built up when scanning a WAL logfile
struct CollectorState {
  std::unordered_map<TRI_voc_cid_t, TRI_voc_tick_t> collections;
  std::unordered_map<TRI_voc_cid_t, int64_t> operationsCount;
  std::unordered_map<TRI_voc_cid_t, MMFilesOperationsType>
      structuralOperations;
  std::unordered_map<TRI_voc_cid_t, MMFilesDocumentOperationsType>
      documentOperations;
  std::unordered_set<TRI_voc_tid_t> failedTransactions;
  std::unordered_set<TRI_voc_tid_t> handledTransactions;
  std::unordered_set<TRI_voc_cid_t> droppedCollections;
  std::unordered_set<TRI_voc_tick_t> droppedDatabases;

  TRI_voc_tick_t lastDatabaseId;
  TRI_voc_cid_t lastCollectionId;

  CollectorState() : lastDatabaseId(0), lastCollectionId(0) {}

  void resetCollection() {
    return resetCollection(0, 0);
  }

  void resetCollection(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId) {
    lastDatabaseId = databaseId;
    lastCollectionId = collectionId;
  }
};

/// @brief whether or not a collection can be ignored in the gc
static bool ShouldIgnoreCollection(CollectorState const* state,
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

  if (state->droppedDatabases.find(databaseId) !=
      state->droppedDatabases.end()) {
    // database of the collection was already dropped
    return true;
  }

  // collection not dropped, database not dropped
  return false;
}

/// @brief callback to handle one marker during collection
static bool ScanMarker(MMFilesMarker const* marker, void* data,
                       MMFilesDatafile* datafile) {
  CollectorState* state = static_cast<CollectorState*>(data);

  TRI_ASSERT(marker != nullptr);
  MMFilesMarkerType const type = marker->getType();
  
  switch (type) {
    case TRI_DF_MARKER_PROLOGUE: {
      // simply note the last state
      TRI_voc_tick_t const databaseId = MMFilesDatafileHelper::DatabaseId(marker);
      TRI_voc_cid_t const collectionId = MMFilesDatafileHelper::CollectionId(marker);
      state->resetCollection(databaseId, collectionId);
      break;
    }

    case TRI_DF_MARKER_VPACK_DOCUMENT: 
    case TRI_DF_MARKER_VPACK_REMOVE: {
      TRI_voc_tick_t const databaseId = state->lastDatabaseId;
      TRI_voc_cid_t const collectionId = state->lastCollectionId;
      TRI_ASSERT(databaseId > 0);
      TRI_ASSERT(collectionId > 0);

      TRI_voc_tid_t transactionId = MMFilesDatafileHelper::TransactionId(marker);

      state->collections[collectionId] = databaseId;

      if (state->failedTransactions.find(transactionId) !=
          state->failedTransactions.end()) {
        // transaction had failed
        state->operationsCount[collectionId]++;
        break;
      }

      if (ShouldIgnoreCollection(state, collectionId)) {
        break;
      }

      VPackSlice slice(reinterpret_cast<char const*>(marker) + MMFilesDatafileHelper::VPackOffset(type));
      state->documentOperations[collectionId][transaction::helpers::extractKeyFromDocument(slice).copyString()] = marker;
      state->operationsCount[collectionId]++;
      break;
    }

    case TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION:
    case TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION: {
      break;
    }

    case TRI_DF_MARKER_VPACK_ABORT_TRANSACTION: {
      TRI_voc_tid_t const tid = MMFilesDatafileHelper::TransactionId(marker);

      // note which abort markers we found
      state->handledTransactions.emplace(tid);
      break;
    }

    case TRI_DF_MARKER_VPACK_CREATE_COLLECTION: {
      TRI_voc_cid_t const collectionId = MMFilesDatafileHelper::CollectionId(marker);
      // note that the collection is now considered not dropped
      state->droppedCollections.erase(collectionId);
      break;
    }

    case TRI_DF_MARKER_VPACK_DROP_COLLECTION: {
      TRI_voc_cid_t const collectionId = MMFilesDatafileHelper::CollectionId(marker);
      // note that the collection was dropped and doesn't need to be collected
      state->droppedCollections.emplace(collectionId);
      state->structuralOperations.erase(collectionId);
      state->documentOperations.erase(collectionId);
      state->operationsCount.erase(collectionId);
      state->collections.erase(collectionId);
      break;
    }

    case TRI_DF_MARKER_VPACK_CREATE_DATABASE: {
      TRI_voc_tick_t const database = MMFilesDatafileHelper::DatabaseId(marker);
      // note that the database is now considered not dropped
      state->droppedDatabases.erase(database);
      break;
    }

    case TRI_DF_MARKER_VPACK_DROP_DATABASE: {
      TRI_voc_tick_t const database = MMFilesDatafileHelper::DatabaseId(marker);
      // note that the database was dropped and doesn't need to be collected
      state->droppedDatabases.emplace(database);

      // find all collections for the same database and erase their state, too
      for (auto it = state->collections.begin(); it != state->collections.end();
           /* no hoisting */) {
        if ((*it).second == database) {
          state->droppedCollections.emplace((*it).first);
          state->structuralOperations.erase((*it).first);
          state->documentOperations.erase((*it).first);
          state->operationsCount.erase((*it).first);
          it = state->collections.erase(it);
        } else {
          ++it;
        }
      }
      break;
    }

    case TRI_DF_MARKER_HEADER: 
    case TRI_DF_MARKER_FOOTER: {
      // new datafile or end of datafile. forget state!
      state->resetCollection();
      break;
    }

    default: {
      // do nothing intentionally
    }
  }

  return true;
}

/// @brief wait interval for the collector thread when idle
uint64_t const MMFilesCollectorThread::Interval = 1000000;

/// @brief create the collector thread
MMFilesCollectorThread::MMFilesCollectorThread(MMFilesLogfileManager* logfileManager)
    : Thread("WalCollector"),
      _logfileManager(logfileManager),
      _condition(),
      _forcedStopIterations(-1),
      _operationsQueueLock(),
      _operationsQueue(),
      _operationsQueueInUse(false),
      _numPendingOperations(0),
      _collectorResultCondition(),
      _collectorResult(TRI_ERROR_NO_ERROR) {}

/// @brief wait for the collector result
int MMFilesCollectorThread::waitForResult(uint64_t timeout) {
  CONDITION_LOCKER(guard, _collectorResultCondition);

  if (_collectorResult == TRI_ERROR_NO_ERROR) {
    if (!guard.wait(timeout)) {
      return TRI_ERROR_LOCK_TIMEOUT;
    }
  }

  return _collectorResult;
}

/// @brief begin shutdown sequence
void MMFilesCollectorThread::beginShutdown() {
  Thread::beginShutdown();

  // deactivate write-throttling on shutdown
  _logfileManager->throttleWhenPending(0); 

  CONDITION_LOCKER(guard, _condition);
  guard.signal();
}

/// @brief signal the thread that there is something to do
void MMFilesCollectorThread::signal() {
  CONDITION_LOCKER(guard, _condition);
  guard.signal();
}

/// @brief signal the thread that there is something to do
void MMFilesCollectorThread::forceStop() {
  CONDITION_LOCKER(guard, _condition);
  _forcedStopIterations = 0;
  guard.signal();
}

/// @brief main loop
void MMFilesCollectorThread::run() {
  int counter = 0;

  while (true) {
    bool hasWorked = false;
    bool doDelay = false;

    try {
      // step 1: collect a logfile if any qualifies
      if (!isStopping()) {
        // don't collect additional logfiles in case we want to shut down
        bool worked;
        int res = this->collectLogfiles(worked);

        if (res == TRI_ERROR_NO_ERROR) {
          hasWorked |= worked;
        } else if (res == TRI_ERROR_ARANGO_FILESYSTEM_FULL) {
          doDelay = true;
        }
      }

      // step 2: update master pointers
      bool worked;
      int res = this->processQueuedOperations(worked);

      if (res == TRI_ERROR_NO_ERROR) {
        hasWorked |= worked;
      } else if (res == TRI_ERROR_ARANGO_FILESYSTEM_FULL) {
        doDelay = true;
      }
    } catch (arangodb::basics::Exception const& ex) {
      int res = ex.code();
      LOG_TOPIC(ERR, Logger::COLLECTOR) << "got unexpected error in collectorThread::run: "
               << TRI_errno_string(res);
    } catch (...) {
      LOG_TOPIC(ERR, Logger::COLLECTOR) << "got unspecific error in collectorThread::run";
    }

    uint64_t interval = Interval;

    if (doDelay) {
      hasWorked = false;
      // wait longer before retrying in case disk is full
      interval *= 2;
    }

    CONDITION_LOCKER(guard, _condition);

    if (!isStopping() && !hasWorked) {
      // sleep only if there was nothing to do

      if (!guard.wait(interval)) {
        if (++counter > 10) {
          LOG_TOPIC(TRACE, Logger::COLLECTOR) << "wal collector has queued operations: "
                     << numQueuedOperations();
          counter = 0;
        }
      }
    } else if (isStopping()) {
      if (!hasQueuedOperations()) {
        // no operations left to execute, we can exit
        break;
      }
      if (_forcedStopIterations >= 0) {
        if (++_forcedStopIterations == 10) {
          // forceful exit
          break;
        } else {
          guard.wait(interval);
        }
      }
    } 
  }

  // all queues are empty, so we can exit
  TRI_ASSERT(!hasQueuedOperations());
}

/// @brief check whether there are queued operations left
bool MMFilesCollectorThread::hasQueuedOperations() {
  MUTEX_LOCKER(mutexLocker, _operationsQueueLock);

  return !_operationsQueue.empty();
}

/// @brief check whether there are queued operations left
bool MMFilesCollectorThread::hasQueuedOperations(TRI_voc_cid_t cid) {
  MUTEX_LOCKER(mutexLocker, _operationsQueueLock);

  return (_operationsQueue.find(cid) != _operationsQueue.end());
}

// execute a callback during a phase in which the collector has nothing
// queued. This is used in the DatabaseManagerThread when dropping
// a database to avoid existence of ditches of type DOCUMENT.
bool MMFilesCollectorThread::executeWhileNothingQueued(std::function<void()> const& cb) {
  MUTEX_LOCKER(mutexLocker, _operationsQueueLock);
  if (!_operationsQueue.empty()) {
    return false;
  }
  cb();
  return true;
}

/// @brief step 1: perform collection of a logfile (if any)
int MMFilesCollectorThread::collectLogfiles(bool& worked) {
  // always init result variable
  worked = false;

  TRI_IF_FAILURE("CollectorThreadCollect") { return TRI_ERROR_NO_ERROR; }

  MMFilesWalLogfile* logfile = _logfileManager->getCollectableLogfile();

  if (logfile == nullptr) {
    return TRI_ERROR_NO_ERROR;
  }

  worked = true;
  _logfileManager->setCollectionRequested(logfile);

  try {
    int res = collect(logfile);
    // LOG_TOPIC(TRACE, Logger::COLLECTOR) << "collected logfile: " << logfile->id() << ". result: " << res;

    if (res == TRI_ERROR_NO_ERROR) {
      // reset collector status
      broadcastCollectorResult(res);

      MMFilesPersistentIndexFeature::syncWal();

      _logfileManager->setCollectionDone(logfile);
    } else {
      // return the logfile to the logfile manager in case of errors
      _logfileManager->forceStatus(logfile, MMFilesWalLogfile::StatusType::SEALED);

      // set error in collector
      broadcastCollectorResult(res);
    }

    return res;
  } catch (arangodb::basics::Exception const& ex) {
    _logfileManager->forceStatus(logfile, MMFilesWalLogfile::StatusType::SEALED);

    int res = ex.code();

    LOG_TOPIC(DEBUG, Logger::COLLECTOR) << "collecting logfile " << logfile->id()
               << " failed: " << TRI_errno_string(res);

    return res;
  } catch (...) {
    _logfileManager->forceStatus(logfile, MMFilesWalLogfile::StatusType::SEALED);

    LOG_TOPIC(DEBUG, Logger::COLLECTOR) << "collecting logfile " << logfile->id() << " failed";

    return TRI_ERROR_INTERNAL;
  }
}

/// @brief step 2: process all still-queued collection operations
int MMFilesCollectorThread::processQueuedOperations(bool& worked) {
  // always init result variable
  worked = false;

  TRI_IF_FAILURE("CollectorThreadProcessQueuedOperations") {
    return TRI_ERROR_NO_ERROR;
  }

  {
    MUTEX_LOCKER(mutexLocker, _operationsQueueLock);
    TRI_ASSERT(!_operationsQueueInUse);

    if (_operationsQueue.empty()) {
      // nothing to do
      return TRI_ERROR_NO_ERROR;
    }

    // this flag indicates that no one else must write to the queue
    _operationsQueueInUse = true;
  }

  // go on without the mutex!

  try {
    // process operations for each collection
    for (auto it = _operationsQueue.begin(); it != _operationsQueue.end(); ++it) {
      auto& operations = (*it).second;
      TRI_ASSERT(!operations.empty());

      for (auto it2 = operations.begin(); it2 != operations.end();
          /* no hoisting */) {
        MMFilesWalLogfile* logfile = (*it2)->logfile;

        int res = TRI_ERROR_INTERNAL;

        try {
          res = processCollectionOperations((*it2));
        } catch (arangodb::basics::Exception const& ex) {
          res = ex.code();
          LOG_TOPIC(TRACE, Logger::COLLECTOR) << "caught exception while applying queued operations: " << ex.what();
        } catch (std::exception const& ex) {
          res = TRI_ERROR_INTERNAL;
          LOG_TOPIC(TRACE, Logger::COLLECTOR) << "caught exception while applying queued operations: " << ex.what();
        } catch (...) {
          res = TRI_ERROR_INTERNAL;
          LOG_TOPIC(TRACE, Logger::COLLECTOR) << "caught unknown exception while applying queued operations";
        }

        if (res == TRI_ERROR_LOCK_TIMEOUT) {
          // could not acquire write-lock for collection in time
          // do not delete the operations
          LOG_TOPIC(TRACE, Logger::COLLECTOR) << "got lock timeout while trying to apply queued operations";
          ++it2;
          continue;
        }

        worked = true;

        if (res == TRI_ERROR_NO_ERROR) {
          LOG_TOPIC(TRACE, Logger::COLLECTOR) << "queued operations applied successfully";
        } else if (res == TRI_ERROR_ARANGO_DATABASE_NOT_FOUND ||
                  res == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
          // these are expected errors
          LOG_TOPIC(TRACE, Logger::COLLECTOR)
              << "removing queued operations for already deleted collection";
          res = TRI_ERROR_NO_ERROR;
        } else {
          LOG_TOPIC(WARN, Logger::COLLECTOR)
              << "got unexpected error code while applying queued operations: "
              << TRI_errno_string(res);
        }

        if (res == TRI_ERROR_NO_ERROR) {
          uint64_t numOperations = (*it2)->operations->size();
          uint64_t maxNumPendingOperations =
              _logfileManager->throttleWhenPending();

          if (maxNumPendingOperations > 0 &&
              _numPendingOperations >= maxNumPendingOperations &&
              (_numPendingOperations - numOperations) < maxNumPendingOperations) {
            // write-throttling was active, but can be turned off now
            _logfileManager->deactivateWriteThrottling();
            LOG_TOPIC(INFO, Logger::COLLECTOR) << "deactivating write-throttling";
          }

          _numPendingOperations -= numOperations;

          // delete the object
          delete (*it2);

          // delete the element from the vector while iterating over the vector
          it2 = operations.erase(it2);

          _logfileManager->decreaseCollectQueueSize(logfile);
        } else {
          // do not delete the object but advance in the operations vector
          ++it2;
        }
      }

      // next collection
    }

    // finally remove all entries from the map with empty vectors
    {
      MUTEX_LOCKER(mutexLocker, _operationsQueueLock);
      TRI_ASSERT(_operationsQueueInUse); 

      if (worked) {
        for (auto it = _operationsQueue.begin(); it != _operationsQueue.end();
            /* no hoisting */) {
          if ((*it).second.empty()) {
            it = _operationsQueue.erase(it);
          } else {
            ++it;
          }
        }
      }

      // the queue can now be used by others, too
      _operationsQueueInUse = false;
    }
  } catch (...) {
    {
      MUTEX_LOCKER(mutexLocker, _operationsQueueLock);
      // always make sure the queue can now be used by others, too
      TRI_ASSERT(_operationsQueueInUse); 
      _operationsQueueInUse = false;
    }
 
    throw;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief return the number of queued operations
size_t MMFilesCollectorThread::numQueuedOperations() {
  MUTEX_LOCKER(mutexLocker, _operationsQueueLock);

  return _operationsQueue.size();
}

/// @brief process a single marker in collector step 2
void MMFilesCollectorThread::processCollectionMarker(
    arangodb::SingleCollectionTransaction& trx,
    LogicalCollection* collection, MMFilesCollectorCache* cache,
    MMFilesCollectorOperation const& operation) {
  auto physical = static_cast<MMFilesCollection*>(collection->getPhysical());
  TRI_ASSERT(physical != nullptr);
  auto const* walMarker = reinterpret_cast<MMFilesMarker const*>(operation.walPosition);
  TRI_ASSERT(walMarker != nullptr);
  TRI_ASSERT(reinterpret_cast<MMFilesMarker const*>(operation.datafilePosition));
  TRI_voc_size_t const datafileMarkerSize = operation.datafileMarkerSize;
  TRI_voc_fid_t const fid = operation.datafileId;

  MMFilesMarkerType const type = walMarker->getType();

  if (type == TRI_DF_MARKER_VPACK_DOCUMENT) {
    auto& dfi = cache->createDfi(fid);
    dfi.numberUncollected--;

    VPackSlice slice(reinterpret_cast<char const*>(walMarker) + MMFilesDatafileHelper::VPackOffset(type));
    TRI_ASSERT(slice.isObject());
    
    VPackSlice keySlice;
    TRI_voc_rid_t revisionId = 0;
    transaction::helpers::extractKeyAndRevFromDocument(slice, keySlice, revisionId);
  
    bool wasAdjusted = false;
    MMFilesSimpleIndexElement element = physical->primaryIndex()->lookupKey(&trx, keySlice);

    if (element &&
        element.revisionId() == revisionId) { 
      // make it point to datafile now
      MMFilesMarker const* newPosition = reinterpret_cast<MMFilesMarker const*>(operation.datafilePosition);
      wasAdjusted = physical->updateRevisionConditional(element.revisionId(), walMarker, newPosition, fid, false); 
    }
      
    if (wasAdjusted) {
      // revision is still active
      dfi.numberAlive++;
      dfi.sizeAlive += encoding::alignedSize<int64_t>(datafileMarkerSize);
    } else {
      // somebody inserted a new revision of the document or the revision
      // was already moved by the compactor
      dfi.numberDead++;
      dfi.sizeDead += encoding::alignedSize<int64_t>(datafileMarkerSize);
    }
  } else if (type == TRI_DF_MARKER_VPACK_REMOVE) {
    auto& dfi = cache->createDfi(fid);
    dfi.numberUncollected--;
    dfi.numberDeletions++;

    VPackSlice slice(reinterpret_cast<char const*>(walMarker) + MMFilesDatafileHelper::VPackOffset(type));
    TRI_ASSERT(slice.isObject());
    
    VPackSlice keySlice;
    TRI_voc_rid_t revisionId = 0;
    transaction::helpers::extractKeyAndRevFromDocument(slice, keySlice, revisionId);

    MMFilesSimpleIndexElement found = physical->primaryIndex()->lookupKey(&trx, keySlice);

    if (found && 
        found.revisionId() > revisionId) {
      // somebody re-created the document with a newer revision
      dfi.numberDead++;
      dfi.sizeDead += encoding::alignedSize<int64_t>(datafileMarkerSize);
    }
  }
}

/// @brief process all operations for a single collection
int MMFilesCollectorThread::processCollectionOperations(MMFilesCollectorCache* cache) {
  arangodb::DatabaseGuard dbGuard(cache->databaseId);
  TRI_vocbase_t* vocbase = dbGuard.database();
  TRI_ASSERT(vocbase != nullptr);

  arangodb::CollectionGuard collectionGuard(vocbase, cache->collectionId, true);
  arangodb::LogicalCollection* collection = collectionGuard.collection();

  TRI_ASSERT(collection != nullptr);

  auto physical = static_cast<MMFilesCollection*>(collection->getPhysical());
  TRI_ASSERT(physical != nullptr);

  // first try to read-lock the compactor-lock, afterwards try to write-lock the
  // collection
  // if any locking attempt fails, release and try again next time
  MMFilesTryCompactionPreventer compactionPreventer(physical);
  
  if (!compactionPreventer.isLocked()) {
    return TRI_ERROR_LOCK_TIMEOUT;
  }

  arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(collection->vocbase()),
      collection->cid(), AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::NO_USAGE_LOCK);  // already locked by guard above
  trx.addHint(transaction::Hints::Hint::NO_COMPACTION_LOCK);  // already locked above
  trx.addHint(transaction::Hints::Hint::NO_THROTTLING);
  trx.addHint(transaction::Hints::Hint::NO_BEGIN_MARKER);
  trx.addHint(transaction::Hints::Hint::NO_ABORT_MARKER);
  trx.addHint(transaction::Hints::Hint::TRY_LOCK);
  trx.addHint(transaction::Hints::Hint::NO_DLD);

  TRI_IF_FAILURE("CollectorThreadProcessCollectionOperationsLockTimeout") {
    return TRI_ERROR_LOCK_TIMEOUT;
  }

  Result res = trx.begin();

  if (!res.ok()) {
    // this includes TRI_ERROR_LOCK_TIMEOUT!
    LOG_TOPIC(TRACE, Logger::COLLECTOR) << "wal collector couldn't acquire write lock for collection '"
               << collection->name() << "': " << res.errorMessage();

    return res.errorNumber();
  }

  try {
    // now we have the write lock on the collection
    LOG_TOPIC(TRACE, Logger::COLLECTOR) << "wal collector processing operations for collection '"
               << collection->name() << "'";

    TRI_ASSERT(!cache->operations->empty());

    for (auto const& it : *(cache->operations)) {
      processCollectionMarker(trx, collection, cache, it);
    }

    // finally update all datafile statistics
    LOG_TOPIC(TRACE, Logger::COLLECTOR) << "updating datafile statistics for collection '"
               << collection->name() << "'";
    updateDatafileStatistics(collection, cache);

    static_cast<arangodb::MMFilesCollection*>(collection->getPhysical())->decreaseUncollectedLogfileEntries(cache->totalOperationsCount);

    res = TRI_ERROR_NO_ERROR;
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
    LOG_TOPIC(TRACE, Logger::COLLECTOR) << "wal collector caught exception: " << ex.what();
  } catch (std::exception const& ex) {
    res = TRI_ERROR_INTERNAL;
    LOG_TOPIC(TRACE, Logger::COLLECTOR) << "wal collector caught exception: " << ex.what();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
    LOG_TOPIC(TRACE, Logger::COLLECTOR) << "wal collector caught unknown exception";
  }

  // always release the locks
  trx.finish(res);

  LOG_TOPIC(TRACE, Logger::COLLECTOR) << "wal collector processed operations for collection '"
             << collection->name() << "' with status: " << res.errorMessage();

  return res.errorNumber();
}

/// @brief collect one logfile
int MMFilesCollectorThread::collect(MMFilesWalLogfile* logfile) {
  TRI_ASSERT(logfile != nullptr);

  LOG_TOPIC(TRACE, Logger::COLLECTOR) << "collecting logfile " << logfile->id();

  MMFilesDatafile* df = logfile->df();

  TRI_ASSERT(df != nullptr);

  TRI_IF_FAILURE("CollectorThreadCollectException") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // We will sequentially scan the logfile for collection:
  df->sequentialAccess();
  df->willNeed();
  TRI_DEFER(df->randomAccess());

  // create a state for the collector, beginning with the list of failed
  // transactions
  CollectorState state;
  state.failedTransactions = TransactionManagerFeature::manager()->getFailedTransactions();

  // scan all markers in logfile, this will fill the state
  bool result =
      TRI_IterateDatafile(df, &ScanMarker, static_cast<void*>(&state));

  if (!result) {
    return TRI_ERROR_INTERNAL;
  }

  // get an aggregated list of all collection ids
  std::set<TRI_voc_cid_t> collectionIds;
  for (auto it = state.structuralOperations.begin();
       it != state.structuralOperations.end(); ++it) {
    auto cid = (*it).first;

    if (!ShouldIgnoreCollection(&state, cid)) {
      collectionIds.emplace((*it).first);
    }
  }

  for (auto it = state.documentOperations.begin();
       it != state.documentOperations.end(); ++it) {
    auto cid = (*it).first;

    if (state.structuralOperations.find(cid) ==
            state.structuralOperations.end() &&
        !ShouldIgnoreCollection(&state, cid)) {
      collectionIds.emplace(cid);
    }
  }
    
  MMFilesOperationsType sortedOperations;

  // now for each collection, write all surviving markers into collection
  // datafiles
  for (auto it = collectionIds.begin(); it != collectionIds.end(); ++it) {
    auto cid = (*it);

    // calculate required size for sortedOperations vector
    sortedOperations.clear();
    {
      size_t requiredSize = 0;

      auto it1 = state.structuralOperations.find(cid);
      if (it1 != state.structuralOperations.end()) {
        requiredSize += (*it1).second.size();
      }
      auto it2 = state.documentOperations.find(cid);
      if (it2 != state.documentOperations.end()) {
        requiredSize += (*it2).second.size();
      }
      sortedOperations.reserve(requiredSize);
    }
  
    // insert structural operations - those are already sorted by tick
    if (state.structuralOperations.find(cid) !=
        state.structuralOperations.end()) {
      MMFilesOperationsType const& ops = state.structuralOperations[cid];

      sortedOperations.insert(sortedOperations.begin(), ops.begin(), ops.end());
      TRI_ASSERT(sortedOperations.size() == ops.size());
    }

    // insert document operations - those are sorted by key, not by tick
    if (state.documentOperations.find(cid) != state.documentOperations.end()) {
      MMFilesDocumentOperationsType const& ops = state.documentOperations[cid];

      for (auto it2 = ops.begin(); it2 != ops.end(); ++it2) {
        sortedOperations.push_back((*it2).second);
      }

      // sort vector by marker tick
      std::sort(sortedOperations.begin(), sortedOperations.end(),
                [](MMFilesMarker const* left, MMFilesMarker const* right) {
                  return (left->getTick() < right->getTick());
                });
    }

    if (!sortedOperations.empty()) {
      int res = TRI_ERROR_INTERNAL;

      try {
        res = transferMarkers(logfile, cid, state.collections[cid],
                              state.operationsCount[cid], sortedOperations);

        TRI_IF_FAILURE("failDuringCollect") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

      } catch (arangodb::basics::Exception const& ex) {
        res = ex.code();
        LOG_TOPIC(TRACE, Logger::COLLECTOR) << "caught exception in collect: " << ex.what();
      } catch (std::exception const& ex) {
        res = TRI_ERROR_INTERNAL;
        LOG_TOPIC(TRACE, Logger::COLLECTOR) << "caught exception in collect: " << ex.what();
      } catch (...) {
        res = TRI_ERROR_INTERNAL;
        LOG_TOPIC(TRACE, Logger::COLLECTOR) << "caught unknown exception in collect";
      }

      if (res != TRI_ERROR_NO_ERROR &&
          res != TRI_ERROR_ARANGO_DATABASE_NOT_FOUND &&
          res != TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
        if (res != TRI_ERROR_ARANGO_FILESYSTEM_FULL) {
          // other places already log this error, and making the logging
          // conditional here
          // prevents the log message from being shown over and over again in
          // case the
          // file system is full
          LOG_TOPIC(WARN, Logger::COLLECTOR) << "got unexpected error in MMFilesCollectorThread::collect: "
                    << TRI_errno_string(res);
        }
        // abort early
        return res;
      }
    }
  }

  // Error conditions TRI_ERROR_ARANGO_DATABASE_NOT_FOUND and
  // TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND are intentionally ignored
  // here since this can actually happen if someone has dropped things
  // in between.

  // remove all handled transactions from failedTransactions list
  if (!state.handledTransactions.empty()) {
    TransactionManagerFeature::manager()->unregisterFailedTransactions(state.handledTransactions);
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief transfer markers into a collection
int MMFilesCollectorThread::transferMarkers(MMFilesWalLogfile* logfile,
                                     TRI_voc_cid_t collectionId,
                                     TRI_voc_tick_t databaseId,
                                     int64_t totalOperationsCount,
                                     MMFilesOperationsType const& operations) {
  TRI_ASSERT(!operations.empty());

  // prepare database and collection
  arangodb::DatabaseGuard dbGuard(databaseId);
  TRI_vocbase_t* vocbase = dbGuard.database();
  TRI_ASSERT(vocbase != nullptr);

  arangodb::CollectionGuard collectionGuard(vocbase, collectionId, true);
  arangodb::LogicalCollection* collection = collectionGuard.collection();
  TRI_ASSERT(collection != nullptr);
  
  // no need to go on if the collection is already deleted
  if (collection->status() == TRI_VOC_COL_STATUS_DELETED) {
    return TRI_ERROR_NO_ERROR;
  }

  LOG_TOPIC(TRACE, Logger::COLLECTOR) << "collector transferring markers for '"
             << collection->name()
             << "', totalOperationsCount: " << totalOperationsCount;
    
  auto cache = std::make_unique<MMFilesCollectorCache>(collectionId, databaseId, logfile,
                         totalOperationsCount, operations.size());

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  int res = TRI_ERROR_INTERNAL;

  try {
    auto en = static_cast<MMFilesEngine*>(engine);
    res = en->transferMarkers(collection, cache.get(), operations);
    
    if (res == TRI_ERROR_NO_ERROR && !cache->operations->empty()) {
      queueOperations(logfile, cache);
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
    LOG_TOPIC(TRACE, Logger::COLLECTOR) << "caught exception in transferMarkers: " << ex.what();
  } catch (std::exception const& ex) {
    LOG_TOPIC(TRACE, Logger::COLLECTOR) << "caught exception in transferMarkers: " << ex.what();
    res = TRI_ERROR_INTERNAL;
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
    LOG_TOPIC(TRACE, Logger::COLLECTOR) << "caught unknown exception in transferMarkers";
  }

  return res;
}

/// @brief insert the collect operations into a per-collection queue
int MMFilesCollectorThread::queueOperations(arangodb::MMFilesWalLogfile* logfile,
                                     std::unique_ptr<MMFilesCollectorCache>& cache) {
  TRI_ASSERT(cache != nullptr);

  TRI_voc_cid_t cid = cache->collectionId;
  uint64_t numOperations = cache->operations->size();
  uint64_t maxNumPendingOperations = _logfileManager->throttleWhenPending();

  TRI_ASSERT(!cache->operations->empty());

  while (true) {
    {
      MUTEX_LOCKER(mutexLocker, _operationsQueueLock);

      if (!_operationsQueueInUse) {
        // it is only safe to access the queue if this flag is not set
        auto it = _operationsQueue.find(cid);
        if (it == _operationsQueue.end()) {
          _operationsQueue.emplace(cid, std::vector<MMFilesCollectorCache*>({cache.get()}));
          _logfileManager->increaseCollectQueueSize(logfile);
        } else {
          (*it).second.push_back(cache.get());
          _logfileManager->increaseCollectQueueSize(logfile);
        }
        // now _operationsQueue is responsible for managing the cache entry
        cache.release();

        // exit the loop
        break;
      }
    }

    // wait outside the mutex for the flag to be cleared
    usleep(10000);
  }

  if (maxNumPendingOperations > 0 &&
      _numPendingOperations < maxNumPendingOperations &&
      (_numPendingOperations + numOperations) >= maxNumPendingOperations &&
      !isStopping()) {
    // activate write-throttling!
    _logfileManager->activateWriteThrottling();
    LOG_TOPIC(WARN, Logger::COLLECTOR)
        << "queued more than " << maxNumPendingOperations
        << " pending WAL collector operations." 
        << " current queue size: " << (_numPendingOperations + numOperations) 
        << ". now activating write-throttling";
  }

  _numPendingOperations += numOperations;

  return TRI_ERROR_NO_ERROR;
}

/// @brief update a collection's datafile information
int MMFilesCollectorThread::updateDatafileStatistics(
    LogicalCollection* collection, MMFilesCollectorCache* cache) {
  // iterate over all datafile infos and update the collection's datafile stats
  for (auto it = cache->dfi.begin(); it != cache->dfi.end();
       /* no hoisting */) {
    MMFilesCollection* mmfiles = static_cast<MMFilesCollection*>(collection->getPhysical());
    TRI_ASSERT(mmfiles);
    mmfiles->updateStats((*it).first, (*it).second);

    // flush the local datafile info so we don't update the statistics twice
    // with the same values
    (*it).second.reset();
    it = cache->dfi.erase(it);
  }

  return TRI_ERROR_NO_ERROR;
}

void MMFilesCollectorThread::broadcastCollectorResult(int res) { 
  CONDITION_LOCKER(guard, _collectorResultCondition);
  _collectorResult = res;
  _collectorResultCondition.broadcast();
}
