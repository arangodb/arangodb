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

#include "MMFilesTransactionState.h"
#include "Aql/QueryCache.h"
#include "Basics/Exceptions.h"
#include "Basics/RocksDBUtils.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesDatafileHelper.h"
#include "MMFiles/MMFilesDocumentOperation.h"
#include "MMFiles/MMFilesLogfileManager.h"
#include "MMFiles/MMFilesPersistentIndexFeature.h"
#include "MMFiles/MMFilesTransactionCollection.h"
#include "StorageEngine/TransactionCollection.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/modes.h"
#include "VocBase/ticks.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/utilities/transaction.h>

using namespace arangodb;

/// @brief return the logfile manager
static inline MMFilesLogfileManager* GetMMFilesLogfileManager() {
  return MMFilesLogfileManager::instance();
}

/// @brief transaction type
MMFilesTransactionState::MMFilesTransactionState(TRI_vocbase_t* vocbase, transaction::Options const& options)
    : TransactionState(vocbase, options),
      _rocksTransaction(nullptr),
      _beginWritten(false),
      _hasOperations(false) {}

/// @brief free a transaction container
MMFilesTransactionState::~MMFilesTransactionState() {
  delete _rocksTransaction;
}

/// @brief get (or create) a rocksdb WriteTransaction
rocksdb::Transaction* MMFilesTransactionState::rocksTransaction() {
  if (_rocksTransaction == nullptr) {
    _rocksTransaction = MMFilesPersistentIndexFeature::instance()->db()->BeginTransaction(
      rocksdb::WriteOptions(), rocksdb::OptimisticTransactionOptions());
  }
  return _rocksTransaction;
}
  
/// @brief start a transaction
Result MMFilesTransactionState::beginTransaction(transaction::Hints hints) {
  LOG_TRX(this, _nestingLevel) << "beginning " << AccessMode::typeString(_type) << " transaction";
  Result result;

  if (_nestingLevel == 0) {
    TRI_ASSERT(_status == transaction::Status::CREATED);

    auto logfileManager = MMFilesLogfileManager::instance();

    if (!hasHint(transaction::Hints::Hint::NO_THROTTLING) &&
        AccessMode::isWriteOrExclusive(_type) &&
        logfileManager->canBeThrottled()) {
      // write-throttling?
      static uint64_t const WaitTime = 50000;
      uint64_t const maxIterations =
          logfileManager->maxThrottleWait() / (WaitTime / 1000);
      uint64_t iterations = 0;

      while (logfileManager->isThrottled()) {
        if (++iterations == maxIterations) {
          return TRI_ERROR_ARANGO_WRITE_THROTTLE_TIMEOUT;
        }

        usleep(WaitTime);
      }
    }

    // set hints
    _hints = hints;

    // get a new id
    _id = TRI_NewTickServer();

    // register a protector
    int res = logfileManager->registerTransaction(_id, isReadOnlyTransaction());
    result.reset(res);
 
    if (!result.ok()) {
      return result;
    }
  
  } else {
    TRI_ASSERT(_status == transaction::Status::RUNNING);
  }

  result = useCollections(_nestingLevel);

  if (result.ok()) {
    // all valid
    if (_nestingLevel == 0) {
      updateStatus(transaction::Status::RUNNING);

      // defer writing of the begin marker until necessary!
    }
  } else {
    // something is wrong
    if (_nestingLevel == 0) {
      updateStatus(transaction::Status::ABORTED);
    }

    // free what we have got so far
    unuseCollections(_nestingLevel);
  }

  return result;
}

/// @brief commit a transaction
Result MMFilesTransactionState::commitTransaction(transaction::Methods* activeTrx) {
  LOG_TRX(this, _nestingLevel) << "committing " << AccessMode::typeString(_type) << " transaction";
  TRI_ASSERT(_status == transaction::Status::RUNNING);

  Result result;
  if (_nestingLevel == 0) {
    if (_rocksTransaction != nullptr) {
      auto status = _rocksTransaction->Commit();
      result = rocksutils::convertStatus(status);

      if (!result.ok()) {
        abortTransaction(activeTrx);
        return result;
      }
    }

    int res = writeCommitMarker();
    result.reset(res);

    if (!result.ok()) {
      // TODO: revert rocks transaction somehow
      abortTransaction(activeTrx);

      // return original error
      return result;
    }

    updateStatus(transaction::Status::COMMITTED);

    // if a write query, clear the query cache for the participating collections
    if (AccessMode::isWriteOrExclusive(_type) &&
        !_collections.empty() &&
        !isSingleOperation() && 
        arangodb::aql::QueryCache::instance()->mayBeActive()) {
      clearQueryCache();
    }

    freeOperations(activeTrx);
  }

  unuseCollections(_nestingLevel);

  return result;
}

/// @brief abort and rollback a transaction
Result MMFilesTransactionState::abortTransaction(transaction::Methods* activeTrx) {
  LOG_TRX(this, _nestingLevel) << "aborting " << AccessMode::typeString(_type) << " transaction";

  TRI_ASSERT(_status == transaction::Status::RUNNING);

  Result result;

  if (_nestingLevel == 0) {
    int res = writeAbortMarker();
    result.reset(res);

    updateStatus(transaction::Status::ABORTED);

    if (_hasOperations) {
      // must clean up the query cache because the transaction
      // may have queried something via AQL that is now rolled back
      clearQueryCache();
    }

    freeOperations(activeTrx);
  }

  unuseCollections(_nestingLevel);

  return result;
}

/// @brief add a WAL operation for a transaction collection
int MMFilesTransactionState::addOperation(TRI_voc_rid_t revisionId,
                                   MMFilesDocumentOperation& operation,
                                   MMFilesWalMarker const* marker,
                                   bool& waitForSync) {
  LogicalCollection* collection = operation.collection();
  bool const isSingleOperationTransaction = isSingleOperation();

  if (hasHint(transaction::Hints::Hint::RECOVERY)) {
    // turn off all waitForSync operations during recovery
    waitForSync = false;
  } else if (!waitForSync) {
    // upgrade the info for the transaction based on the collection's settings
    waitForSync |= collection->waitForSync();
  }

  if (waitForSync) {
    _options.waitForSync = true;
  }

  TRI_IF_FAILURE("TransactionOperationNoSlot") { return TRI_ERROR_DEBUG; }

  TRI_IF_FAILURE("TransactionOperationNoSlotExcept") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (!isSingleOperationTransaction && !_beginWritten) {
    int res = writeBeginMarker();

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  TRI_voc_fid_t fid = 0;
  void const* position = nullptr;

  if (marker->fid() == 0) {
    // this is a "real" marker that must be written into the logfiles
    // just append it to the WAL:

    // we only need to set waitForSync to true here if waitForSync was requested
    // for the operation AND the operation is a standalone operation. In case the
    // operation belongs to a transaction, the transaction's commit marker will
    // be written with waitForSync, and we don't need to request a sync ourselves
    bool const localWaitForSync = (isSingleOperationTransaction && waitForSync);

    // never wait until our marker was synced, even when an operation was tagged
    // waitForSync=true. this is still safe because inside a transaction, the final
    // commit marker will be written with waitForSync=true then, and in a standalone
    // operation the transaction will wait until everything was synced before returning
    // to the caller
    bool const waitForTick = false;

    // we should wake up the synchronizer in case this is a single operation
    //
    bool const wakeUpSynchronizer = isSingleOperationTransaction;

    MMFilesWalSlotInfoCopy slotInfo =
        MMFilesLogfileManager::instance()->allocateAndWrite(
            _vocbase->id(), collection->cid(), 
            marker, wakeUpSynchronizer,
            localWaitForSync, waitForTick);
    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      // some error occurred
      return slotInfo.errorCode;
    }
    if (localWaitForSync) {
      // also sync RocksDB WAL
      if (collection->getPhysical()->hasIndexOfType(arangodb::Index::TRI_IDX_TYPE_PERSISTENT_INDEX)) {
        MMFilesPersistentIndexFeature::syncWal();
      }
    }
    operation.setTick(slotInfo.tick);
    fid = slotInfo.logfileId;
    position = slotInfo.mem;
  } else {
    // this is an envelope marker that has been written to the logfiles before.
    // avoid writing it again!
    fid = marker->fid();
    position = static_cast<MMFilesMarkerEnvelope const*>(marker)->mem();
  }

  TRI_ASSERT(fid > 0);
  TRI_ASSERT(position != nullptr);

  auto physical = static_cast<MMFilesCollection*>(collection->getPhysical());
  if (operation.type() == TRI_VOC_DOCUMENT_OPERATION_INSERT ||
      operation.type() == TRI_VOC_DOCUMENT_OPERATION_UPDATE ||
      operation.type() == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
    // adjust the data position in the header
    uint8_t const* vpack = reinterpret_cast<uint8_t const*>(position) + MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT);
    TRI_ASSERT(fid > 0);
    operation.setVPack(vpack);
    physical->updateRevision(revisionId, vpack, fid, true); // always in WAL
  }

  TRI_IF_FAILURE("TransactionOperationAfterAdjust") { return TRI_ERROR_DEBUG; }

  if (isSingleOperationTransaction) {
    // operation is directly executed
    if (_rocksTransaction != nullptr) {
      auto status = _rocksTransaction->Commit();

      if (!status.ok()) { 
        // TODO: what to do here?
      }
    }
    operation.handled();

    arangodb::aql::QueryCache::instance()->invalidate(
        _vocbase, collection->name());

    physical->increaseUncollectedLogfileEntries(1);
  } else {
    // operation is buffered and might be rolled back
    TransactionCollection* trxCollection = this->collection(collection->cid(), AccessMode::Type::WRITE);
    
    std::unique_ptr<MMFilesDocumentOperation> copy(operation.clone());
   
    TRI_IF_FAILURE("TransactionOperationPushBack") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG); 
    }
      
    static_cast<MMFilesTransactionCollection*>(trxCollection)->addOperation(copy.get());
    
    TRI_IF_FAILURE("TransactionOperationPushBack2") {
      copy.release();
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG); 
    }

    copy->handled();
    copy.release();
    operation.swapped();
    _hasOperations = true;
    
    arangodb::aql::QueryCache::instance()->invalidate(
        _vocbase, collection->name());
  }

  physical->setRevision(revisionId, false);

  TRI_IF_FAILURE("TransactionOperationAtEnd") { return TRI_ERROR_DEBUG; }

  return TRI_ERROR_NO_ERROR;
}

/// @brief free all operations for a transaction
void MMFilesTransactionState::freeOperations(transaction::Methods* activeTrx) {
  bool const mustRollback = (_status == transaction::Status::ABORTED);
     
  TRI_ASSERT(activeTrx != nullptr);
   
  for (auto& trxCollection : _collections) {
    trxCollection->freeOperations(activeTrx, mustRollback);
  }
}

/// @brief write WAL begin marker
int MMFilesTransactionState::writeBeginMarker() {
  if (!needWriteMarker(true)) {
    return TRI_ERROR_NO_ERROR;
  }

  if (hasHint(transaction::Hints::Hint::NO_BEGIN_MARKER)) {
    return TRI_ERROR_NO_ERROR;
  }

  TRI_IF_FAILURE("TransactionWriteBeginMarker") { return TRI_ERROR_DEBUG; }

  TRI_ASSERT(!_beginWritten);

  int res;

  try {
    MMFilesTransactionMarker marker(TRI_DF_MARKER_VPACK_BEGIN_TRANSACTION, _vocbase->id(), _id);
    res = GetMMFilesLogfileManager()->allocateAndWrite(marker, false).errorCode;
    
    TRI_IF_FAILURE("TransactionWriteBeginMarkerThrow") { 
      throw std::bad_alloc();
    }

    if (res == TRI_ERROR_NO_ERROR) {
      _beginWritten = true;
    } else {
      THROW_ARANGO_EXCEPTION(res);
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "could not save transaction begin marker in log: " << ex.what();
  } catch (std::exception const& ex) {
    res = TRI_ERROR_INTERNAL;
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "could not save transaction begin marker in log: " << ex.what();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "could not save transaction begin marker in log: unknown exception";
  }

  return res;
}

/// @brief write WAL abort marker
int MMFilesTransactionState::writeAbortMarker() {
  if (!needWriteMarker(false)) {
    return TRI_ERROR_NO_ERROR;
  }

  if (hasHint(transaction::Hints::Hint::NO_ABORT_MARKER)) {
    return TRI_ERROR_NO_ERROR;
  }

  TRI_ASSERT(_beginWritten);

  TRI_IF_FAILURE("TransactionWriteAbortMarker") { return TRI_ERROR_DEBUG; }

  int res;

  try {
    MMFilesTransactionMarker marker(TRI_DF_MARKER_VPACK_ABORT_TRANSACTION, _vocbase->id(), _id);
    res = GetMMFilesLogfileManager()->allocateAndWrite(marker, false).errorCode;
    
    TRI_IF_FAILURE("TransactionWriteAbortMarkerThrow") { 
      throw std::bad_alloc();
    }
  
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "could not save transaction abort marker in log: " << ex.what();
  } catch (std::exception const& ex) {
    res = TRI_ERROR_INTERNAL;
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "could not save transaction abort marker in log: " << ex.what();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "could not save transaction abort marker in log: unknown exception";
  }

  return res;
}

/// @brief write WAL commit marker
int MMFilesTransactionState::writeCommitMarker() {
  if (!needWriteMarker(false)) {
    return TRI_ERROR_NO_ERROR;
  }

  TRI_IF_FAILURE("TransactionWriteCommitMarker") { return TRI_ERROR_DEBUG; }

  TRI_ASSERT(_beginWritten);

  int res;

  try {
    MMFilesTransactionMarker marker(TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION, _vocbase->id(), _id);
    res = GetMMFilesLogfileManager()->allocateAndWrite(marker, _options.waitForSync).errorCode;
    
    TRI_IF_FAILURE("TransactionWriteCommitMarkerSegfault") { 
      TRI_SegfaultDebugging("crashing on commit");
    }

    TRI_IF_FAILURE("TransactionWriteCommitMarkerNoRocksSync") { return TRI_ERROR_NO_ERROR; }

    if (_options.waitForSync) {
      // also sync RocksDB WAL if required
      bool hasPersistentIndex = false;
      allCollections([&hasPersistentIndex](TransactionCollection* collection) {
        auto c = static_cast<MMFilesTransactionCollection*>(collection);
        if (c->canAccess(AccessMode::Type::WRITE) && 
            c->collection()->getPhysical()->hasIndexOfType(arangodb::Index::TRI_IDX_TYPE_PERSISTENT_INDEX)) {
          hasPersistentIndex = true;
          // abort iteration
          return false;
        }

        return true;
      });
      if (hasPersistentIndex) {
        MMFilesPersistentIndexFeature::syncWal();
      }
    }
    
    TRI_IF_FAILURE("TransactionWriteCommitMarkerThrow") { 
      throw std::bad_alloc();
    }
    
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "could not save transaction commit marker in log: " << ex.what();
  } catch (std::exception const& ex) {
    res = TRI_ERROR_INTERNAL;
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "could not save transaction commit marker in log: " << ex.what();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "could not save transaction commit marker in log: unknown exception";
  }

  return res;
}

