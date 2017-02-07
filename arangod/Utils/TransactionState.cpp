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

#include "TransactionState.h"
#include "Aql/QueryCache.h"
#include "Logger/Logger.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "MMFiles/MMFilesDatafileHelper.h"
#include "MMFiles/MMFilesDocumentOperation.h"
#include "MMFiles/MMFilesLogfileManager.h"
#include "MMFiles/MMFilesPersistentIndexFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "Utils/Transaction.h"
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
TransactionState::TransactionState(TRI_vocbase_t* vocbase, double timeout, bool waitForSync)
    : _vocbase(vocbase), 
      _id(0), 
      _type(AccessMode::Type::READ),
      _status(Transaction::Status::CREATED),
      _arena(),
      _collections{_arena}, // assign arena to vector 
      _rocksTransaction(nullptr),
      _hints(),
      _nestingLevel(0), 
      _allowImplicit(true),
      _hasOperations(false), 
      _waitForSync(waitForSync),
      _beginWritten(false), 
      _timeout(Transaction::DefaultLockTimeout) {
  
  if (timeout > 0.0) {
    _timeout = timeout;
  } 
}

/// @brief free a transaction container
TransactionState::~TransactionState() {
  TRI_ASSERT(_status != Transaction::Status::RUNNING);

  delete _rocksTransaction;

  releaseCollections();

  // free all collections
  for (auto it = _collections.rbegin(); it != _collections.rend(); ++it) {
    delete (*it);
  }
}
  
std::vector<std::string> TransactionState::collectionNames() const {
  std::vector<std::string> result;
  result.reserve(_collections.size());

  for (auto& trxCollection : _collections) {
    if (trxCollection->collection() != nullptr) {
      result.emplace_back(trxCollection->collectionName());
    }
  }

  return result;
}

/// @brief return the collection from a transaction
TransactionCollection* TransactionState::collection(TRI_voc_cid_t cid, AccessMode::Type accessType) {
  TRI_ASSERT(_status == Transaction::Status::CREATED ||
             _status == Transaction::Status::RUNNING);

  size_t unused;
  TransactionCollection* trxCollection = findCollection(cid, unused);

  if (trxCollection == nullptr || !trxCollection->canAccess(accessType)) {
    // not found or not accessible in the requested mode
    return nullptr;
  }

  return trxCollection;
}

/// @brief add a collection to a transaction
int TransactionState::addCollection(TRI_voc_cid_t cid,
                                    AccessMode::Type accessType,
                                    int nestingLevel, bool force,
                                    bool allowImplicitCollections) {
  LOG_TRX(this, nestingLevel) << "adding collection " << cid;

  allowImplicitCollections &= _allowImplicit;

  // LOG(TRACE) << "cid: " << cid 
  //            << ", accessType: " << accessType 
  //            << ", nestingLevel: " << nestingLevel 
  //            << ", force: " << force 
  //            << ", allowImplicitCollections: " << allowImplicitCollections;
  
  // upgrade transaction type if required
  if (nestingLevel == 0) {
    if (!force) {
      TRI_ASSERT(_status == Transaction::Status::CREATED);
    }

    if (AccessMode::isWriteOrExclusive(accessType) && !AccessMode::isWriteOrExclusive(_type)) {
      // if one collection is written to, the whole transaction becomes a
      // write-transaction
      _type = AccessMode::Type::WRITE;
    }
  }

  // check if we already have got this collection in the _collections vector
  size_t position = 0;
  TransactionCollection* trxCollection = findCollection(cid, position);
  
  if (trxCollection != nullptr) {
    // collection is already contained in vector
    return trxCollection->updateUsage(accessType, nestingLevel);
  }

  // collection not found.

  if (nestingLevel > 0 && AccessMode::isWriteOrExclusive(accessType)) {
    // trying to write access a collection in an embedded transaction
    return TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION;
  }

  if (!AccessMode::isWriteOrExclusive(accessType) && !allowImplicitCollections) {
    return TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION;
  }
  
  // collection was not contained. now create and insert it
  TRI_ASSERT(trxCollection == nullptr);
  try {
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    trxCollection = engine->createTransactionCollection(this, cid, accessType, nestingLevel);
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  TRI_ASSERT(trxCollection != nullptr);

  // insert collection at the correct position
  try {
    _collections.insert(_collections.begin() + position, trxCollection);
  } catch (...) {
    delete trxCollection;

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief make sure all declared collections are used & locked
int TransactionState::ensureCollections(int nestingLevel) {
  return useCollections(nestingLevel);
}

/// @brief use all participating collections of a transaction
int TransactionState::useCollections(int nestingLevel) {
  int res = TRI_ERROR_NO_ERROR;

  // process collections in forward order
  for (auto& trxCollection : _collections) {
    res = trxCollection->use(nestingLevel);

    if (res != TRI_ERROR_NO_ERROR) {
      break;
    }
  }

  return res;
}

/// @brief release collection locks for a transaction
int TransactionState::unuseCollections(int nestingLevel) {
  // process collections in reverse order
  for (auto it = _collections.rbegin(); it != _collections.rend(); ++it) {
    (*it)->unuse(nestingLevel);
  }

  return TRI_ERROR_NO_ERROR;
}
 
int TransactionState::lockCollections() { 
  for (auto& trxCollection : _collections) {
    int res = trxCollection->lock();

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }
  return TRI_ERROR_NO_ERROR;
}

/// @brief start a transaction
int TransactionState::beginTransaction(TransactionHints hints, int nestingLevel) {
  LOG_TRX(this, nestingLevel) << "beginning " << AccessMode::typeString(_type) << " transaction";

  if (nestingLevel == 0) {
    TRI_ASSERT(_status == Transaction::Status::CREATED);

    auto logfileManager = MMFilesLogfileManager::instance();

    if (!hasHint(TransactionHints::Hint::NO_THROTTLING) &&
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
    int res = logfileManager->registerTransaction(_id);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  
  } else {
    TRI_ASSERT(_status == Transaction::Status::RUNNING);
  }

  int res = useCollections(nestingLevel);

  if (res == TRI_ERROR_NO_ERROR) {
    // all valid
    if (nestingLevel == 0) {
      updateStatus(Transaction::Status::RUNNING);

      // defer writing of the begin marker until necessary!
    }
  } else {
    // something is wrong
    if (nestingLevel == 0) {
      updateStatus(Transaction::Status::ABORTED);
    }

    // free what we have got so far
    unuseCollections(nestingLevel);
  }

  return res;
}

/// @brief commit a transaction
int TransactionState::commitTransaction(arangodb::Transaction* activeTrx, int nestingLevel) {
  LOG_TRX(this, nestingLevel) << "committing " << AccessMode::typeString(_type) << " transaction";

  TRI_ASSERT(_status == Transaction::Status::RUNNING);

  int res = TRI_ERROR_NO_ERROR;

  if (nestingLevel == 0) {
    if (_rocksTransaction != nullptr) {
      auto status = _rocksTransaction->Commit();

      if (!status.ok()) {
        res = TRI_ERROR_INTERNAL;
        abortTransaction(activeTrx, nestingLevel);
        return res;
      }
    }

    res = writeCommitMarker();

    if (res != TRI_ERROR_NO_ERROR) {
      // TODO: revert rocks transaction somehow
      abortTransaction(activeTrx, nestingLevel);

      // return original error
      return res;
    }

    updateStatus(Transaction::Status::COMMITTED);

    // if a write query, clear the query cache for the participating collections
    if (AccessMode::isWriteOrExclusive(_type) &&
        !_collections.empty() &&
        arangodb::aql::QueryCache::instance()->mayBeActive()) {
      clearQueryCache();
    }

    freeOperations(activeTrx);
  }

  unuseCollections(nestingLevel);

  return res;
}

/// @brief abort and rollback a transaction
int TransactionState::abortTransaction(arangodb::Transaction* activeTrx, int nestingLevel) {
  LOG_TRX(this, nestingLevel) << "aborting " << AccessMode::typeString(_type) << " transaction";

  TRI_ASSERT(_status == Transaction::Status::RUNNING);

  int res = TRI_ERROR_NO_ERROR;

  if (nestingLevel == 0) {
    res = writeAbortMarker();

    updateStatus(Transaction::Status::ABORTED);

    freeOperations(activeTrx);
  }

  unuseCollections(nestingLevel);

  return res;
}

/// @brief add a WAL operation for a transaction collection
int TransactionState::addOperation(TRI_voc_rid_t revisionId,
                                   MMFilesDocumentOperation& operation,
                                   MMFilesWalMarker const* marker,
                                   bool& waitForSync) {
  LogicalCollection* collection = operation.collection();
  bool const isSingleOperationTransaction = isSingleOperation();

  if (hasHint(TransactionHints::Hint::RECOVERY)) {
    // turn off all waitForSync operations during recovery
    waitForSync = false;
  } else if (!waitForSync) {
    // upgrade the info for the transaction based on the collection's settings
    waitForSync |= collection->waitForSync();
  }

  if (waitForSync) {
    _waitForSync = true;
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
      RocksDBFeature::syncWal();
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

  if (operation.type() == TRI_VOC_DOCUMENT_OPERATION_INSERT ||
      operation.type() == TRI_VOC_DOCUMENT_OPERATION_UPDATE ||
      operation.type() == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
    // adjust the data position in the header
    uint8_t const* vpack = reinterpret_cast<uint8_t const*>(position) + MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT);
    TRI_ASSERT(fid > 0);
    operation.setVPack(vpack);
    collection->updateRevision(revisionId, vpack, fid, true); // always in WAL
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

    collection->increaseUncollectedLogfileEntries(1);
  } else {
    // operation is buffered and might be rolled back
    TransactionCollection* trxCollection = this->collection(collection->cid(), AccessMode::Type::WRITE);
    
    std::unique_ptr<MMFilesDocumentOperation> copy(operation.swap());
    
    TRI_IF_FAILURE("TransactionOperationPushBack") {
      // test what happens if reserve above failed
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG); 
    }
    
    trxCollection->addOperation(copy.get());
    copy->handled();
    copy.release();
    _hasOperations = true;
  }

  collection->setRevision(revisionId, false);

  TRI_IF_FAILURE("TransactionOperationAtEnd") { return TRI_ERROR_DEBUG; }

  return TRI_ERROR_NO_ERROR;
}

/// @brief whether or not a transaction consists of a single operation
bool TransactionState::isSingleOperation() const {
  return hasHint(TransactionHints::Hint::SINGLE_OPERATION);
}

/// @brief find a collection in the transaction's list of collections
TransactionCollection* TransactionState::findCollection(TRI_voc_cid_t cid, size_t& position) const {
  size_t const n = _collections.size();
  size_t i;

  for (i = 0; i < n; ++i) {
    auto trxCollection = _collections.at(i);

    if (cid < trxCollection->id()) {
      // collection not found
      break;
    }

    if (cid == trxCollection->id()) {
      // found
      return trxCollection;
    }
    // next
  }

  // update the insert position if required
  position = i;

  return nullptr;
}

/// @brief free all operations for a transaction
void TransactionState::freeOperations(arangodb::Transaction* activeTrx) {
  bool const mustRollback = (_status == Transaction::Status::ABORTED);
     
  TRI_ASSERT(activeTrx != nullptr);
   
  for (auto& trxCollection : _collections) {
    trxCollection->freeOperations(activeTrx, mustRollback);
  }
}

/// @brief release collection locks for a transaction
int TransactionState::releaseCollections() {
  if (hasHint(TransactionHints::Hint::LOCK_NEVER) ||
      hasHint(TransactionHints::Hint::NO_USAGE_LOCK)) {
    return TRI_ERROR_NO_ERROR;
  }

  // process collections in reverse order
  for (auto it = _collections.rbegin(); it != _collections.rend(); ++it) {
    (*it)->release();
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief clear the query cache for all collections that were modified by
/// the transaction
void TransactionState::clearQueryCache() {
  if (_collections.empty()) {
    return;
  }

  try {
    std::vector<std::string> collections;
    for (auto& trxCollection : _collections) {
      if (trxCollection->hasOperations()) {
        // we're only interested in collections that may have been modified
        collections.emplace_back(trxCollection->collectionName());
      }
    }

    if (!collections.empty()) {
      arangodb::aql::QueryCache::instance()->invalidate(_vocbase, collections);
    }
  } catch (...) {
    // in case something goes wrong, we have to remove all queries from the
    // cache
    arangodb::aql::QueryCache::instance()->invalidate(_vocbase);
  }
}

/// @brief update the status of a transaction
void TransactionState::updateStatus(Transaction::Status status) {
  TRI_ASSERT(_status == Transaction::Status::CREATED ||
             _status == Transaction::Status::RUNNING);

  if (_status == Transaction::Status::CREATED) {
    TRI_ASSERT(status == Transaction::Status::RUNNING ||
               status == Transaction::Status::ABORTED);
  } else if (_status == Transaction::Status::RUNNING) {
    TRI_ASSERT(status == Transaction::Status::COMMITTED ||
               status == Transaction::Status::ABORTED);
  }

  _status = status;
}

/// @brief write WAL begin marker
int TransactionState::writeBeginMarker() {
  if (!needWriteMarker(true)) {
    return TRI_ERROR_NO_ERROR;
  }

  if (hasHint(TransactionHints::Hint::NO_BEGIN_MARKER)) {
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
    LOG(WARN) << "could not save transaction begin marker in log: " << ex.what();
  } catch (std::exception const& ex) {
    res = TRI_ERROR_INTERNAL;
    LOG(WARN) << "could not save transaction begin marker in log: " << ex.what();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
    LOG(WARN) << "could not save transaction begin marker in log: unknown exception";
  }

  return res;
}

/// @brief write WAL abort marker
int TransactionState::writeAbortMarker() {
  if (!needWriteMarker(false)) {
    return TRI_ERROR_NO_ERROR;
  }

  if (hasHint(TransactionHints::Hint::NO_ABORT_MARKER)) {
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
    LOG(WARN) << "could not save transaction abort marker in log: " << ex.what();
  } catch (std::exception const& ex) {
    res = TRI_ERROR_INTERNAL;
    LOG(WARN) << "could not save transaction abort marker in log: " << ex.what();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
    LOG(WARN) << "could not save transaction abort marker in log: unknown exception";
  }

  return res;
}

/// @brief write WAL commit marker
int TransactionState::writeCommitMarker() {
  if (!needWriteMarker(false)) {
    return TRI_ERROR_NO_ERROR;
  }

  TRI_IF_FAILURE("TransactionWriteCommitMarker") { return TRI_ERROR_DEBUG; }

  TRI_ASSERT(_beginWritten);

  int res;

  try {
    MMFilesTransactionMarker marker(TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION, _vocbase->id(), _id);
    res = GetMMFilesLogfileManager()->allocateAndWrite(marker, _waitForSync).errorCode;
    
    TRI_IF_FAILURE("TransactionWriteCommitMarkerSegfault") { 
      TRI_SegfaultDebugging("crashing on commit");
    }

    TRI_IF_FAILURE("TransactionWriteCommitMarkerNoRocksSync") { return TRI_ERROR_NO_ERROR; }

    if (_waitForSync) {
      // also sync RocksDB WAL
      RocksDBFeature::syncWal();
    }
    
    TRI_IF_FAILURE("TransactionWriteCommitMarkerThrow") { 
      throw std::bad_alloc();
    }
    
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
    LOG(WARN) << "could not save transaction commit marker in log: " << ex.what();
  } catch (std::exception const& ex) {
    res = TRI_ERROR_INTERNAL;
    LOG(WARN) << "could not save transaction commit marker in log: " << ex.what();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
    LOG(WARN) << "could not save transaction commit marker in log: unknown exception";
  }

  return res;
}

