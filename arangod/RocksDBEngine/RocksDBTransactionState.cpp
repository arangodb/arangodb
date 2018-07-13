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

#include <rocksdb/options.h>
#include <rocksdb/status.h>
#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/utilities/transaction.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>

using namespace arangodb;

// for the RocksDB engine we do not need any additional data
struct RocksDBTransactionData final : public TransactionData {};

/// @brief transaction type
RocksDBTransactionState::RocksDBTransactionState(
    TRI_vocbase_t& vocbase,
    TRI_voc_tid_t tid,
    transaction::Options const& options
): TransactionState(vocbase, tid, options),
      _rocksTransaction(nullptr),
      _snapshot(nullptr),
      _rocksWriteOptions(),
      _rocksReadOptions(),
      _cacheTx(nullptr),
      _numCommits(0),
      _numInserts(0),
      _numUpdates(0),
      _numRemoves(0),
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
  LOG_TRX(this, _nestingLevel)
      << "beginning " << AccessMode::typeString(_type) << " transaction";


  TRI_ASSERT(!hasHint(transaction::Hints::Hint::NO_USAGE_LOCK) || !AccessMode::isWriteOrExclusive(_type));
  
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
    _rocksReadOptions.prefix_same_as_start = true;  // should always be true

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
  // trxOpts.deadlock_detect = !hasHint(transaction::Hints::Hint::NO_DLD);

  TRI_ASSERT(_rocksTransaction == nullptr ||
             _rocksTransaction->GetState() == rocksdb::Transaction::COMMITED ||
             (_rocksTransaction->GetState() == rocksdb::Transaction::STARTED &&
             _rocksTransaction->GetNumKeys() == 0));
  _rocksTransaction =
      db->BeginTransaction(_rocksWriteOptions, trxOpts, _rocksTransaction);

  // add transaction begin marker
  if (!hasHint(transaction::Hints::Hint::SINGLE_OPERATION)) {
    auto header =
      RocksDBLogValue::BeginTransaction(_vocbase.id(), _id);

    _rocksTransaction->PutLogData(header.slice());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_numLogdata == 0);
    ++_numLogdata;
#endif
  }
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

  // we may need to block intermediate commits
  ExecContext const* exe = ExecContext::CURRENT;
  if (!isReadOnlyTransaction() && exe != nullptr) {
    bool cancelRW = ServerState::readOnly() && !exe->isSuperuser();
    if (exe->isCanceled() || cancelRW) {
      return Result(TRI_ERROR_ARANGO_READ_ONLY, "server is in read-only mode");
    }
  }

  Result result;
  if (hasOperations()) {
    // we are actually going to attempt a commit
    if (!hasHint(transaction::Hints::Hint::SINGLE_OPERATION)) {
      // add custom commit marker to increase WAL tailing reliability
      auto logValue =
        RocksDBLogValue::CommitTransaction(_vocbase.id(), id());

      _rocksTransaction->PutLogData(logValue.slice());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      _numLogdata++;
#endif
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    uint64_t x = _numInserts + _numRemoves + _numUpdates;

    if (hasHint(transaction::Hints::Hint::SINGLE_OPERATION)) {
      TRI_ASSERT(x <= 1 && _numLogdata == x);
    } else {
      if (_numLogdata != (2 + _numRemoves)) {
        LOG_TOPIC(ERR, Logger::FIXME) << "_numInserts " << _numInserts << "  "
        << "_numRemoves " << _numRemoves << "  "
        << "_numUpdates " << _numUpdates << "  "
        << "_numLogdata " << _numLogdata;
      }

      // begin transaction + commit transaction + n doc removes
      TRI_ASSERT(_numLogdata == (2 + _numRemoves));
    }
#endif
    
    // set wait for sync flag if required
    if (waitForSync()) {
      _rocksWriteOptions.sync = true;
      _rocksTransaction->SetWriteOptions(_rocksWriteOptions);
    }

    // prepare for commit on each collection, e.g. place blockers for estimators
    rocksdb::SequenceNumber preCommitSeq =
        rocksutils::globalRocksDB()->GetLatestSequenceNumber();
    for (auto& trxCollection : _collections) {
      RocksDBTransactionCollection* collection =
          static_cast<RocksDBTransactionCollection*>(trxCollection);
      collection->prepareCommit(id(), preCommitSeq);
    }
    bool committed = false;
    auto cleanupCollectionTransactions = scopeGuard([this, &committed]() {
      // if we didn't commit, make sure we remove blockers, etc.
      if (!committed) {
        for (auto& trxCollection : _collections) {
          RocksDBTransactionCollection* collection =
              static_cast<RocksDBTransactionCollection*>(trxCollection);
          collection->abortCommit(id());
        }
      }
    });

    ++_numCommits;
    result = rocksutils::convertStatus(_rocksTransaction->Commit());
    rocksdb::SequenceNumber latestSeq =
        rocksutils::globalRocksDB()->GetLatestSequenceNumber();

    if (result.ok()) {
      for (auto& trxCollection : _collections) {
        RocksDBTransactionCollection* collection =
            static_cast<RocksDBTransactionCollection*>(trxCollection);
        // we need this in case of an intermediate commit. The number of
        // initial documents is adjusted and numInserts / removes is set to 0
        // index estimator updates are buffered
        collection->commitCounts(id(), latestSeq);
        committed = true;
      }
    }
  } else {
    TRI_ASSERT(_rocksTransaction->GetNumKeys() == 0 &&
               _rocksTransaction->GetNumPuts() == 0 &&
               _rocksTransaction->GetNumDeletes() == 0);
    
    for (auto& trxCollection : _collections) {
      RocksDBTransactionCollection* collection =
          static_cast<RocksDBTransactionCollection*>(trxCollection);
      // We get here if we have filled indexes. So let us commit counts and
      // any buffered index estimator updates
      collection->commitCounts(id(), 0);
    }
    // don't write anything if the transaction is empty
    result = rocksutils::convertStatus(_rocksTransaction->Rollback());
  }

  return result;
}

/// @brief commit a transaction
Result RocksDBTransactionState::commitTransaction(
    transaction::Methods* activeTrx) {
  LOG_TRX(this, _nestingLevel)
      << "committing " << AccessMode::typeString(_type) << " transaction";

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
      cleanupTransaction();  // deletes trx
    } else {
      abortTransaction(activeTrx);  // deletes trx
    }
    TRI_ASSERT(!_rocksTransaction && !_cacheTx && !_snapshot);
  }

  unuseCollections(_nestingLevel);
  return res;
}

/// @brief abort and rollback a transaction
Result RocksDBTransactionState::abortTransaction(
    transaction::Methods* activeTrx) {
  LOG_TRX(this, _nestingLevel)
      << "aborting " << AccessMode::typeString(_type) << " transaction";
  TRI_ASSERT(_status == transaction::Status::RUNNING);
  Result result;
  if (_nestingLevel == 0) {
    if (_rocksTransaction != nullptr) {
      rocksdb::Status status = _rocksTransaction->Rollback();
      result = rocksutils::convertStatus(status);
    }
    cleanupTransaction();  // deletes trx

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

void RocksDBTransactionState::prepareOperation(TRI_voc_cid_t cid, TRI_voc_rid_t rid,
    TRI_voc_document_operation_e operationType) {
  TRI_ASSERT(!isReadOnlyTransaction());

  bool singleOp = hasHint(transaction::Hints::Hint::SINGLE_OPERATION);
  if (singleOp) {
    // singleOp => no modifications yet
    TRI_ASSERT(_rocksTransaction->GetNumPuts() == 0 &&
               _rocksTransaction->GetNumDeletes() == 0);
    switch (operationType) {
      case TRI_VOC_DOCUMENT_OPERATION_INSERT:
      case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
      case TRI_VOC_DOCUMENT_OPERATION_REPLACE: {
        auto logValue =
          RocksDBLogValue::SinglePut(_vocbase.id(), cid);

        _rocksTransaction->PutLogData(logValue.slice());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        TRI_ASSERT(_numLogdata == 0);
        _numLogdata++;
#endif
        break;
      }
      case TRI_VOC_DOCUMENT_OPERATION_REMOVE: {
        TRI_ASSERT(rid != 0);

        auto logValue =
          RocksDBLogValue::SingleRemoveV2(_vocbase.id(), cid, rid);

        _rocksTransaction->PutLogData(logValue.slice());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        TRI_ASSERT(_numLogdata == 0);
        _numLogdata++;
#endif
      } break;
      case TRI_VOC_DOCUMENT_OPERATION_UNKNOWN:
        break;
    }
  } else {
    if (operationType == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
      RocksDBLogValue logValue = RocksDBLogValue::DocumentRemoveV2(rid);
      _rocksTransaction->PutLogData(logValue.slice());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      ++_numLogdata;
#endif
    }
  }
}

/// @brief add an operation for a transaction collection
Result RocksDBTransactionState::addOperation(
    TRI_voc_cid_t cid, TRI_voc_rid_t revisionId,
    TRI_voc_document_operation_e operationType) {
  size_t currentSize = _rocksTransaction->GetWriteBatch()->GetWriteBatch()->GetDataSize();
  if (currentSize > _options.maxTransactionSize) {
    // we hit the transaction size limit
    std::string message =
        "aborting transaction because maximal transaction size limit of " +
        std::to_string(_options.maxTransactionSize) + " bytes is reached";
    return Result(Result(TRI_ERROR_RESOURCE_LIMIT, message));
  }

  auto collection =
      static_cast<RocksDBTransactionCollection*>(findCollection(cid));

  if (collection == nullptr) {
    std::string message = "collection '" + std::to_string(cid) +
                          "' not found in transaction state";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
  }

  // should not fail or fail with exception
  collection->addOperation(operationType, revisionId);

  // clear the query cache for this collection
  if (arangodb::aql::QueryCache::instance()->mayBeActive()) {
    arangodb::aql::QueryCache::instance()->invalidate(
      &_vocbase, collection->collectionName()
    );
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
  return checkIntermediateCommit(currentSize);
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
  TRI_ASSERT(_status == transaction::Status::RUNNING);
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

Result RocksDBTransactionState::triggerIntermediateCommit() {
  TRI_IF_FAILURE("FailBeforeIntermediateCommit") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("SegfaultBeforeIntermediateCommit") {
    TRI_SegfaultDebugging("SegfaultBeforeIntermediateCommit");
  }

  TRI_ASSERT(!hasHint(transaction::Hints::Hint::SINGLE_OPERATION));
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC(DEBUG, Logger::ROCKSDB) << "INTERMEDIATE COMMIT!";
#endif

  Result res = internalCommit();
  if (res.fail()) {
    // FIXME: do we abort the transaction ?
    return res;
  }

  TRI_IF_FAILURE("FailAfterIntermediateCommit") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("SegfaultAfterIntermediateCommit") {
    TRI_SegfaultDebugging("SegfaultAfterIntermediateCommit");
  }

  _numInserts = 0;
  _numUpdates = 0;
  _numRemoves = 0;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _numLogdata = 0;
#endif
  createTransaction();
  return TRI_ERROR_NO_ERROR;
}

Result RocksDBTransactionState::checkIntermediateCommit(uint64_t newSize) {
  auto numOperations = _numInserts + _numUpdates + _numRemoves;
  // perform an intermediate commit
  // this will be done if either the "number of operations" or the
  // "transaction size" counters have reached their limit
  if (_options.intermediateCommitCount <= numOperations ||
      _options.intermediateCommitSize <= newSize) {
    return triggerIntermediateCommit();
  }
  return TRI_ERROR_NO_ERROR;
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

void RocksDBTransactionState::trackIndexInsert(TRI_voc_cid_t cid,
                                               TRI_idx_iid_t idxId,
                                               uint64_t hash) {
  auto col = findCollection(cid);
  if (col != nullptr) {
    static_cast<RocksDBTransactionCollection*>(col)->trackIndexInsert(idxId,
                                                                      hash);
  } else {
    TRI_ASSERT(false);
  }
}

void RocksDBTransactionState::trackIndexRemove(TRI_voc_cid_t cid,
                                               TRI_idx_iid_t idxId,
                                               uint64_t hash) {
  auto col = findCollection(cid);
  if (col != nullptr) {
    static_cast<RocksDBTransactionCollection*>(col)->trackIndexRemove(idxId,
                                                                      hash);
  } else {
    TRI_ASSERT(false);
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
