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

#include "RocksDBTransactionState.h"
#include "Aql/QueryCache.h"
#include "Basics/Exceptions.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/Manager.h"
#include "Cache/Transaction.h"
#include "Logger/Logger.h"
#include "RestServer/TransactionManagerFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBCounterManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionManager.h"
#include "Transaction/Methods.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/options.h>
#include <rocksdb/status.h>
#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/utilities/transaction.h>
#include <rocksdb/utilities/write_batch_with_index.h>

using namespace arangodb;

// for the RocksDB engine we do not need any additional data
struct RocksDBTransactionData final : public TransactionData {};

/// @brief transaction type
RocksDBTransactionState::RocksDBTransactionState(
    TRI_vocbase_t* vocbase, transaction::Options const& options)
    : TransactionState(vocbase, options),
      _rocksTransaction(nullptr),
      _snapshot(nullptr),
      _rocksWriteOptions(),
      _rocksReadOptions(),
      _cacheTx(nullptr),
      _numCommits(0),
      _numInternal(0),
      _numInserts(0),
      _numUpdates(0),
      _numRemoves(0),
      _lastUsedCollection(0),
      _keys{_arena},
      _parallel(false) {}

/// @brief free a transaction container
RocksDBTransactionState::~RocksDBTransactionState() {
  cleanupTransaction();
  for (auto& it : _keys) {
    delete it;
  }
}

/// @brief start a transaction
Result RocksDBTransactionState::beginTransaction(transaction::Hints hints) {
  LOG_TRX(this, _nestingLevel) << "beginning " << AccessMode::typeString(_type)
                               << " transaction";

  if (_nestingLevel == 0) {
    // set hints
    _hints = hints;
  }

  Result result = useCollections(_nestingLevel);
  if (result.ok()) {
    // all valid
    if (_nestingLevel == 0) {
      updateStatus(transaction::Status::RUNNING);
    }
  } else {
    // something is wrong
    if (_nestingLevel == 0) {
      updateStatus(transaction::Status::ABORTED);
    }

    // free what we have got so far
    unuseCollections(_nestingLevel);

    return result;
  }

  if (_nestingLevel == 0) {
    // get a new id
    _id = TRI_NewTickServer();

    // register a protector (intentionally empty)
    auto data = std::make_unique<RocksDBTransactionData>();
    TransactionManagerFeature::manager()->registerTransaction(_id,
                                                              std::move(data));

    TRI_ASSERT(_rocksTransaction == nullptr);
    TRI_ASSERT(_cacheTx == nullptr);

    // start cache transaction
    _cacheTx =
        CacheManagerFeature::MANAGER->beginTransaction(isReadOnlyTransaction());

    rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
    _rocksReadOptions.prefix_same_as_start = true; // should always be true

    if (isReadOnlyTransaction()) {
      // server wide replication may insert a snapshot
      if (_snapshot == nullptr) {
        // we must call ReleaseSnapshot at some point
        _snapshot = db->GetSnapshot();
      }
      TRI_ASSERT(_snapshot != nullptr);
      _rocksReadOptions.snapshot = _snapshot;
      _rocksMethods.reset(new RocksDBReadOnlyMethods(this));
    } else {
      TRI_ASSERT(_snapshot == nullptr);
      createTransaction();
      if (hasHint(transaction::Hints::Hint::SINGLE_OPERATION)) {
        _rocksReadOptions.snapshot = _rocksTransaction->GetSnapshot();
      } else {
        TRI_ASSERT(_options.intermediateCommitCount != UINT64_MAX ||
                   _options.intermediateCommitSize != UINT64_MAX);
        // we must call ReleaseSnapshot at some point
        _snapshot = db->GetSnapshot();
        _rocksReadOptions.snapshot = _snapshot;
        TRI_ASSERT(_snapshot != nullptr);
      }
      TRI_ASSERT(_rocksReadOptions.snapshot != nullptr);

      // under some circumstances we can use untracking Put/Delete methods,
      // but we need to be sure this does not cause any lost updates or other
      // inconsistencies. 
      // TODO: enable this optimization once these circumstances are clear
      // and fully covered by tests
      if (false && isExclusiveTransactionOnSingleCollection()) {
        _rocksMethods.reset(new RocksDBTrxUntrackedMethods(this));
      } else {
        _rocksMethods.reset(new RocksDBTrxMethods(this));
      }
    }
  } else {
    TRI_ASSERT(_status == transaction::Status::RUNNING);
  }

  return result;
}

// create a rocksdb transaction. will only be called for write transactions
void RocksDBTransactionState::createTransaction() {
  TRI_ASSERT(!isReadOnlyTransaction());

  // start rocks transaction
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  rocksdb::TransactionOptions trxOpts;
  trxOpts.set_snapshot = true;
  // unclear performance implications do not use for now
  //trxOpts.deadlock_detect = !hasHint(transaction::Hints::Hint::NO_DLD);
  
  TRI_ASSERT(_rocksTransaction == nullptr ||
             _rocksTransaction->GetState() == rocksdb::Transaction::COMMITED ||
             _rocksTransaction->GetState() == rocksdb::Transaction::ROLLEDBACK);
  _rocksTransaction = db->BeginTransaction(_rocksWriteOptions, trxOpts, _rocksTransaction);
  
  // add transaction begin marker
  if (!hasHint(transaction::Hints::Hint::SINGLE_OPERATION)) {
    RocksDBLogValue header =
        RocksDBLogValue::BeginTransaction(_vocbase->id(), _id);
    _rocksTransaction->PutLogData(header.slice());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_numLogdata == 0);
    ++_numLogdata;
#endif
  }
  TRI_ASSERT(_lastUsedCollection == 0);
}

void RocksDBTransactionState::cleanupTransaction() noexcept {
  delete _rocksTransaction;
  _rocksTransaction = nullptr;
  if (_cacheTx != nullptr) {
    // note: endTransaction() will delete _cacheTrx!
    CacheManagerFeature::MANAGER->endTransaction(_cacheTx);
    _cacheTx = nullptr;
  }
  if (_snapshot != nullptr) {
    rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
    db->ReleaseSnapshot(_snapshot);
    _snapshot = nullptr;
  }
}

arangodb::Result RocksDBTransactionState::internalCommit() {
  TRI_ASSERT(_rocksTransaction != nullptr);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  uint64_t x = _numInserts + _numRemoves + _numUpdates;
  if (hasHint(transaction::Hints::Hint::SINGLE_OPERATION)) {
    TRI_ASSERT(x <= 1 && _numLogdata == x);
  } else {
    if (_numLogdata < 1 + (x > 0 ? 1 : 0) + _numRemoves) {
      LOG_TOPIC(ERR, Logger::FIXME)
      << "_numInserts " << _numInserts << "  "
      << "_numRemoves " << _numRemoves << "  "
      << "_numUpdates " << _numUpdates << "  "
      << "_numLogdata " << _numLogdata;
    }
    // begin transaction + n DocumentOpsPrologue + m doc removes
    TRI_ASSERT(_numLogdata >= 1 + (x > 0 ? 1 : 0) + _numRemoves);
  }
#endif
  
  ExecContext const* exe = ExecContext::CURRENT;
  if (!isReadOnlyTransaction() && exe != nullptr) {
    bool cancelRW = !ServerState::writeOpsEnabled() && !exe->isSuperuser();
    if (exe->isCanceled() || cancelRW) {
      return Result(TRI_ERROR_ARANGO_READ_ONLY, "server is in read-only mode");
    }
  }

  Result result;
  if (_rocksTransaction->GetNumKeys() > 0) {
    // set wait for sync flag if required
    if (waitForSync()) {
      _rocksWriteOptions.sync = true;
      _rocksTransaction->SetWriteOptions(_rocksWriteOptions);
    }

    ++_numCommits;
    result = rocksutils::convertStatus(_rocksTransaction->Commit());
    rocksdb::SequenceNumber latestSeq = rocksutils::globalRocksDB()->GetLatestSequenceNumber();
    if (result.ok()) {
      for (auto& trxCollection : _collections) {
        RocksDBTransactionCollection* collection =
        static_cast<RocksDBTransactionCollection*>(trxCollection);
        int64_t adjustment = collection->numInserts() - collection->numRemoves();
        
        if (collection->numInserts() != 0 || collection->numRemoves() != 0 ||
            collection->revision() != 0) {
          RocksDBCollection* coll = static_cast<RocksDBCollection*>(trxCollection->collection()->getPhysical());
          coll->adjustNumberDocuments(adjustment);
          coll->setRevision(collection->revision());
          
          RocksDBEngine* engine = rocksutils::globalRocksEngine();
          RocksDBCounterManager::CounterAdjustment update(latestSeq, collection->numInserts(), collection->numRemoves(),
                                                          collection->revision());
          engine->counterManager()->updateCounter(coll->objectId(), update);
        }
        
        // we need this in case of an intermediate commit. The number of
        // initial documents is adjusted and numInserts / removes is set to 0
        collection->commitCounts();
      }
    }
  } else {
    // don't write anything if the transaction is empty
    result = rocksutils::convertStatus(_rocksTransaction->Rollback());
  }
  
  return result;
}

/// @brief commit a transaction
Result RocksDBTransactionState::commitTransaction(
    transaction::Methods* activeTrx) {
  LOG_TRX(this, _nestingLevel) << "committing " << AccessMode::typeString(_type)
                               << " transaction";

  TRI_ASSERT(_status == transaction::Status::RUNNING);
  TRI_IF_FAILURE("TransactionWriteCommitMarker") {
    return Result(TRI_ERROR_DEBUG);
  }

  arangodb::Result res;
  if (_nestingLevel == 0) {
    if (_rocksTransaction != nullptr) {
      res = internalCommit();
    }
    
    if (res.ok()) {
      updateStatus(transaction::Status::COMMITTED);
      cleanupTransaction(); // deletes trx
    } else {
      abortTransaction(activeTrx); // deletes trx
    }
    TRI_ASSERT(!_rocksTransaction && !_cacheTx && !_snapshot);
  }

  unuseCollections(_nestingLevel);
  return res;
}

/// @brief abort and rollback a transaction
Result RocksDBTransactionState::abortTransaction(
    transaction::Methods* activeTrx) {
  LOG_TRX(this, _nestingLevel) << "aborting " << AccessMode::typeString(_type)
                               << " transaction";
  TRI_ASSERT(_status == transaction::Status::RUNNING);
  Result result;
  if (_nestingLevel == 0) {
    if (_rocksTransaction != nullptr) {
      rocksdb::Status status = _rocksTransaction->Rollback();
      result = rocksutils::convertStatus(status);
    }
    cleanupTransaction(); // deletes trx

    updateStatus(transaction::Status::ABORTED);
    if (hasOperations()) {
      // must clean up the query cache because the transaction
      // may have queried something via AQL that is now rolled back
      clearQueryCache();
    }
    TRI_ASSERT(!_rocksTransaction && !_cacheTx && !_snapshot);
  }

  unuseCollections(_nestingLevel);
  return result;
}

void RocksDBTransactionState::prepareOperation(
    TRI_voc_cid_t collectionId, TRI_voc_rid_t revisionId, StringRef const& key,
    TRI_voc_document_operation_e operationType) {
  TRI_ASSERT(!isReadOnlyTransaction());

  bool singleOp = hasHint(transaction::Hints::Hint::SINGLE_OPERATION);
  if (collectionId != _lastUsedCollection) {
    // single operations should never call this method twice
    // singleOp => lastUsedColl == 0
    TRI_ASSERT(!singleOp || _lastUsedCollection == 0);
    _lastUsedCollection = collectionId;

    if (!singleOp) {
      if (operationType != TRI_VOC_DOCUMENT_OPERATION_UNKNOWN) {
        RocksDBLogValue logValue =
            RocksDBLogValue::DocumentOpsPrologue(collectionId);
        _rocksTransaction->PutLogData(logValue.slice());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        ++_numLogdata;
#endif
      }
    } else {
      // singleOp => no modifications yet
      TRI_ASSERT(!singleOp || (_rocksTransaction->GetNumPuts() == 0 &&
                               _rocksTransaction->GetNumDeletes() == 0));
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      TRI_ASSERT(_numLogdata == 0);
#endif

      switch (operationType) {
        case TRI_VOC_DOCUMENT_OPERATION_INSERT:
        case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
        case TRI_VOC_DOCUMENT_OPERATION_REPLACE: {
          RocksDBLogValue logValue =
              RocksDBLogValue::SinglePut(_vocbase->id(), collectionId);
          _rocksTransaction->PutLogData(logValue.slice());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          ++_numLogdata;
#endif
          break;
        }
        case TRI_VOC_DOCUMENT_OPERATION_REMOVE: {
          TRI_ASSERT(!key.empty());
          RocksDBLogValue logValue =
              RocksDBLogValue::SingleRemove(_vocbase->id(), collectionId, key);
          _rocksTransaction->PutLogData(logValue.slice());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          ++_numLogdata;
#endif
        } break;
        case TRI_VOC_DOCUMENT_OPERATION_UNKNOWN:
          break;
      }
    }
  }
  // we need to log the remove log entry, if we don't have the single
  // optimization
  if (!singleOp && operationType == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    RocksDBLogValue logValue = RocksDBLogValue::DocumentRemove(key);
    _rocksTransaction->PutLogData(logValue.slice());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    ++_numLogdata;
#endif
  }
}

/// @brief add an operation for a transaction collection
RocksDBOperationResult RocksDBTransactionState::addOperation(
    TRI_voc_cid_t cid, TRI_voc_rid_t revisionId,
    TRI_voc_document_operation_e operationType, uint64_t operationSize,
    uint64_t keySize) {

  size_t currentSize =
      _rocksTransaction->GetWriteBatch()->GetWriteBatch()->GetDataSize();
  uint64_t newSize = currentSize + operationSize + keySize;
  if (newSize > _options.maxTransactionSize) {
    // we hit the transaction size limit
    std::string message =
        "aborting transaction because maximal transaction size limit of " +
        std::to_string(_options.maxTransactionSize) + " bytes is reached";
    return RocksDBOperationResult(Result(TRI_ERROR_RESOURCE_LIMIT, message));
  }

  auto collection =
      static_cast<RocksDBTransactionCollection*>(findCollection(cid));

  if (collection == nullptr) {
    std::string message = "collection '" + std::to_string(cid) +
                          "' not found in transaction state";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
  }

  // should not fail or fail with exception
  collection->addOperation(operationType, operationSize, revisionId);

  // clear the query cache for this collection
  if (arangodb::aql::QueryCache::instance()->mayBeActive()) {
    arangodb::aql::QueryCache::instance()->invalidate(
        _vocbase, collection->collectionName());
  }

  switch (operationType) {
    case TRI_VOC_DOCUMENT_OPERATION_UNKNOWN:
      break;
    case TRI_VOC_DOCUMENT_OPERATION_INSERT:
      ++_numInserts;
      break;
    case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
    case TRI_VOC_DOCUMENT_OPERATION_REPLACE:
      ++_numUpdates;
      break;
    case TRI_VOC_DOCUMENT_OPERATION_REMOVE:
      ++_numRemoves;
      break;
  }

  // perform an intermediate commit if necessary
  checkIntermediateCommit(newSize);

  return RocksDBOperationResult();
}

/// @brief add an internal operation for a transaction
RocksDBOperationResult RocksDBTransactionState::addInternalOperation(
    uint64_t operationSize, uint64_t keySize) {

  size_t currentSize =
      _rocksTransaction->GetWriteBatch()->GetWriteBatch()->GetDataSize();
  uint64_t newSize = currentSize + operationSize + keySize;
  if (newSize > _options.maxTransactionSize) {
    // we hit the transaction size limit
    std::string message =
        "aborting transaction because maximal transaction size limit of " +
        std::to_string(_options.maxTransactionSize) + " bytes is reached";
    return RocksDBOperationResult(Result(TRI_ERROR_RESOURCE_LIMIT, message));
  }
  
  ++_numInternal;

  // perform an intermediate commit if necessary
  checkIntermediateCommit(newSize);

  return RocksDBOperationResult();
}

RocksDBMethods* RocksDBTransactionState::rocksdbMethods() {
  TRI_ASSERT(_rocksMethods);
  return _rocksMethods.get();
}

void RocksDBTransactionState::donateSnapshot(rocksdb::Snapshot const* snap) {
  TRI_ASSERT(_snapshot == nullptr);
  TRI_ASSERT(isReadOnlyTransaction());
  TRI_ASSERT(_status == transaction::Status::CREATED);
  _snapshot = snap;
}

rocksdb::Snapshot const* RocksDBTransactionState::stealSnapshot() {
  TRI_ASSERT(_snapshot != nullptr);
  TRI_ASSERT(isReadOnlyTransaction());
  TRI_ASSERT(_status == transaction::Status::COMMITTED || _status == transaction::Status::ABORTED);
  rocksdb::Snapshot const* snap = _snapshot;
  _snapshot = nullptr;
  return snap;
}

uint64_t RocksDBTransactionState::sequenceNumber() const {
  if (_snapshot != nullptr) {
    return static_cast<uint64_t>(_snapshot->GetSequenceNumber());
  } else if (_rocksTransaction) {
    return static_cast<uint64_t>(
        _rocksTransaction->GetSnapshot()->GetSequenceNumber());
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "No snapshot set");
}

void RocksDBTransactionState::checkIntermediateCommit(uint64_t newSize) {
  auto numOperations = _numInserts + _numUpdates + _numRemoves + _numInternal;
  // perform an intermediate commit
  // this will be done if either the "number of operations" or the
  // "transaction size" counters have reached their limit
  if (_options.intermediateCommitCount <= numOperations ||
      _options.intermediateCommitSize <= newSize) {
    TRI_ASSERT(!hasHint(transaction::Hints::Hint::SINGLE_OPERATION));
    LOG_TOPIC(DEBUG, Logger::ROCKSDB) << "INTERMEDIATE COMMIT!";
    internalCommit();
    _lastUsedCollection = 0;
    _numInternal = 0;
    _numInserts = 0;
    _numUpdates = 0;
    _numRemoves = 0;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    _numLogdata = 0;
#endif
    createTransaction();
  }
}

/// @brief temporarily lease a Builder object
RocksDBKey* RocksDBTransactionState::leaseRocksDBKey() {
  if (_keys.empty()) {
    // create a new key and return it
    return new RocksDBKey();
  }

  // re-use an existing builder
  RocksDBKey* k = _keys.back();
  _keys.pop_back();

  return k;
}

/// @brief return a temporary RocksDBKey object
void RocksDBTransactionState::returnRocksDBKey(RocksDBKey* key) {
  try {
    // put key back into our vector of keys
    _keys.emplace_back(key);
  } catch (...) {
    // no harm done. just wipe the key
    delete key;
  }
}

/// @brief constructor, leases a builder
RocksDBKeyLeaser::RocksDBKeyLeaser(transaction::Methods* trx)
      : _rtrx(RocksDBTransactionState::toState(trx)),
        _parallel(_rtrx->inParallelMode()),
        _key(_parallel ? &_internal : _rtrx->leaseRocksDBKey()) {
  TRI_ASSERT(_key != nullptr);
}

/// @brief destructor
RocksDBKeyLeaser::~RocksDBKeyLeaser() {
  if (!_parallel && _key != nullptr) {
    _rtrx->returnRocksDBKey(_key);
  }
}

