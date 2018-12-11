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
#include "RocksDBEngine/RocksDBSyncThread.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "StorageEngine/EngineSelectorFeature.h"
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
      _readSnapshot(nullptr),
      _rocksReadOptions(),
      _cacheTx(nullptr),
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


  TRI_ASSERT(!hasHint(transaction::Hints::Hint::NO_USAGE_LOCK) ||
             !AccessMode::isWriteOrExclusive(_type));

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
    TransactionManagerFeature::manager()->registerTransaction(_id, std::unique_ptr<RocksDBTransactionData>());

    TRI_ASSERT(_rocksTransaction == nullptr);
    TRI_ASSERT(_cacheTx == nullptr);

    // start cache transaction
    if (CacheManagerFeature::MANAGER != nullptr) {
      _cacheTx =
          CacheManagerFeature::MANAGER->beginTransaction(isReadOnlyTransaction());
    }

    rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
    _rocksReadOptions.prefix_same_as_start = true;  // should always be true

    if (isReadOnlyTransaction()) {
      if (_readSnapshot == nullptr) { // replication may donate a snapshot
        _readSnapshot = db->GetSnapshot(); // must call ReleaseSnapshot later
      }
      TRI_ASSERT(_readSnapshot != nullptr);
      _rocksReadOptions.snapshot = _readSnapshot;
      _rocksMethods.reset(new RocksDBReadOnlyMethods(this));
    } else {
      TRI_ASSERT(_readSnapshot == nullptr);
      createTransaction();
      _rocksReadOptions.snapshot = _rocksTransaction->GetSnapshot();
      if (hasHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS)) {
        TRI_ASSERT(_options.intermediateCommitCount != UINT64_MAX ||
                   _options.intermediateCommitSize != UINT64_MAX);
        _readSnapshot = db->GetSnapshot(); // must call ReleaseSnapshot later
        TRI_ASSERT(_readSnapshot != nullptr);
      }

      // with exlusive locking there is no chance of conflict
      // with other transactions -> we can use untracked< Put/Delete methods
      if (isOnlyExclusiveTransaction()) {
        _rocksMethods.reset(new RocksDBTrxUntrackedMethods(this));
      } else {
        _rocksMethods.reset(new RocksDBTrxMethods(this));
      }

      if (hasHint(transaction::Hints::Hint::NO_INDEXING)) {
        // do not track our own writes... we can only use this in very
        // specific scenarios, i.e. when we are sure that we will have a
        // single operation transaction or we are sure we are writing
        // unique keys

        // we must check if there is a unique secondary index for any of the collections
        // we write into
        // in case it is, we must disable NO_INDEXING here, as it wouldn't be safe
        bool disableIndexing = true;

        for (auto& trxCollection : _collections) {
          if (!AccessMode::isWriteOrExclusive(trxCollection->accessType())) {
            continue;
          }
          auto indexes = trxCollection->collection()->getIndexes();
          for (auto const& idx : indexes) {
            if (idx->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
              // primary index is unique, but we can ignore it here.
              // we are only looking for secondary indexes
              continue;
            }
            if (idx->unique()) {
              // found secondary unique index. we need to turn off the NO_INDEXING optimization now
              disableIndexing = false;
              break;
            }
          }
        }

        if (disableIndexing) {
          // only turn it on when safe...
          _rocksMethods->DisableIndexing();
        }
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
    TRI_ASSERT(CacheManagerFeature::MANAGER != nullptr);
    CacheManagerFeature::MANAGER->endTransaction(_cacheTx);
    _cacheTx = nullptr;
  }
  if (_readSnapshot != nullptr) {
    rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
    db->ReleaseSnapshot(_readSnapshot); // calls delete
    _readSnapshot = nullptr;
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
    // sanity check for our on-disk WAL format
    uint64_t x = _numInserts + _numRemoves + _numUpdates;
    if (hasHint(transaction::Hints::Hint::SINGLE_OPERATION)) {
      TRI_ASSERT(x <= 1 && _numLogdata == x);
    } else {
      if (_numLogdata != (2 + _numRemoves)) {
        LOG_TOPIC(ERR, Logger::ENGINES)
            << "_numInserts " << _numInserts << "  "
            << "_numRemoves " << _numRemoves << "  "
            << "_numUpdates " << _numUpdates << "  "
            << "_numLogdata " << _numLogdata;
      }
      // begin transaction + commit transaction + n doc removes
      TRI_ASSERT(_numLogdata == (2 + _numRemoves));
    }
    ++_numCommits;
#endif

    // prepare for commit on each collection, e.g. place blockers for estimators
    rocksdb::SequenceNumber preCommitSeq =
        rocksutils::globalRocksDB()->GetLatestSequenceNumber();
    for (auto& trxColl : _collections) {
      auto* coll = static_cast<RocksDBTransactionCollection*>(trxColl);
      coll->prepareCommit(id(), preCommitSeq);
    }
    bool committed = false;
    auto cleanupCollectionTransactions = scopeGuard([this, &committed]() {
      // if we didn't commit, make sure we remove blockers, etc.
      if (!committed) {
        for (auto& trxColl : _collections) {
          auto* coll = static_cast<RocksDBTransactionCollection*>(trxColl);
          coll->abortCommit(id());
        }
      }
    });

#ifdef _WIN32
    // set wait for sync flag if required
    // we do this only for Windows here, because all other platforms use the
    // RocksDB SyncThread to do the syncing
    if (waitForSync()) {
      _rocksWriteOptions.sync = true;
      _rocksTransaction->SetWriteOptions(_rocksWriteOptions);
    }
#endif

    // total number of sequence ID consuming records
    uint64_t numOps = _rocksTransaction->GetNumPuts() +
                      _rocksTransaction->GetNumDeletes() +
                      _rocksTransaction->GetNumMerges();
    // will invaliate all counts
    result = rocksutils::convertStatus(_rocksTransaction->Commit());
    
    if (result.ok()) {
      TRI_ASSERT(numOps > 0); // simon: should hold unless we're being stupid
      rocksdb::SequenceNumber postCommitSeq = _rocksTransaction->GetCommitedSeqNumber();
      if (ADB_LIKELY(numOps > 0)) {
        postCommitSeq += numOps - 1; // add to get to the next batch
      }
      TRI_ASSERT(postCommitSeq <= rocksutils::globalRocksDB()->GetLatestSequenceNumber());
      
      for (auto& trxColl : _collections) {
        auto* coll = static_cast<RocksDBTransactionCollection*>(trxColl);
        // we need this in case of an intermediate commit. The number of
        // initial documents is adjusted and numInserts / removes is set to 0
        // index estimator updates are buffered
        TRI_IF_FAILURE("RocksDBCommitCounts") {
          committed = true;
          continue;
        }
        coll->commitCounts(id(), postCommitSeq);
        committed = true;
      }

#ifndef _WIN32
      // wait for sync if required, for all other platforms but Windows
      if (waitForSync()) {
        RocksDBEngine* engine = static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);
        TRI_ASSERT(engine != nullptr);
        if (engine->syncThread()) {
          // we do have a sync thread
          result = engine->syncThread()->syncWal();
        } else {
          // no sync thread present... this may be the case if automatic
          // syncing is completely turned off. in this case, use the
          // static sync method
          result = RocksDBSyncThread::sync(engine->db()->GetBaseDB());
        }
      }
#endif
    }
  } else /* if (!hasOperations()) */ {
    TRI_ASSERT(_rocksTransaction->GetNumKeys() == 0 &&
               _rocksTransaction->GetNumPuts() == 0 &&
               _rocksTransaction->GetNumDeletes() == 0);
    
    rocksdb::SequenceNumber seq = 0;
    if (_rocksTransaction) {
      seq = _rocksTransaction->GetSnapshot()->GetSequenceNumber();
    } else {
      TRI_ASSERT(_readSnapshot);
      seq = _readSnapshot->GetSequenceNumber();
    }

    for (auto& trxColl : _collections) {
      TRI_IF_FAILURE("RocksDBCommitCounts") {
        continue;
      }
      auto* rcoll = static_cast<RocksDBTransactionCollection*>(trxColl);
      rcoll->prepareCommit(id(), seq);
      // We get here if we have filled indexes. So let us commit counts and
      // any buffered index estimator updates
      rcoll->commitCounts(id(), seq+1);
    }
    // don't write anything if the transaction is empty
    result = rocksutils::convertStatus(_rocksTransaction->Rollback());
  }

  return result;
}

/// @brief commit a transaction
Result RocksDBTransactionState::commitTransaction(transaction::Methods* activeTrx) {
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
    TRI_ASSERT(!_rocksTransaction && !_cacheTx && !_readSnapshot);
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
    TRI_ASSERT(!_rocksTransaction && !_cacheTx && !_readSnapshot);
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
      case TRI_VOC_DOCUMENT_OPERATION_UNKNOWN:
        break;

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
         break;
      }
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

/// @brief undo the effects of the previous prepareOperation call
void RocksDBTransactionState::rollbackOperation(TRI_voc_document_operation_e operationType) {
  bool singleOp = hasHint(transaction::Hints::Hint::SINGLE_OPERATION);
  if (singleOp) {
    switch (operationType) {
      case TRI_VOC_DOCUMENT_OPERATION_INSERT:
      case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
      case TRI_VOC_DOCUMENT_OPERATION_REPLACE:
      case TRI_VOC_DOCUMENT_OPERATION_REMOVE:
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        --_numLogdata;
#endif
        break;
      default: {
        break;
      }
    }
  } else {
    if (operationType == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      --_numLogdata;
#endif
    }
  }
}

/// @brief add an operation for a transaction collection
Result RocksDBTransactionState::addOperation(
    TRI_voc_cid_t cid, TRI_voc_rid_t revisionId,
    TRI_voc_document_operation_e operationType,
    bool& hasPerformedIntermediateCommit) {
  size_t currentSize = _rocksTransaction->GetWriteBatch()->GetWriteBatch()->GetDataSize();
  if (currentSize > _options.maxTransactionSize) {
    // we hit the transaction size limit
    std::string message =
        "aborting transaction because maximal transaction size limit of " +
        std::to_string(_options.maxTransactionSize) + " bytes is reached";
    return Result(Result(TRI_ERROR_RESOURCE_LIMIT, message));
  }

  auto tcoll = static_cast<RocksDBTransactionCollection*>(findCollection(cid));
  if (tcoll == nullptr) {
    std::string message = "collection '" + std::to_string(cid) +
                          "' not found in transaction state";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
  }

  // should not fail or fail with exception
  tcoll->addOperation(operationType, revisionId);

  // clear the query cache for this collection
  auto queryCache = arangodb::aql::QueryCache::instance();
  if (queryCache->mayBeActive()) {
    queryCache->invalidate(&_vocbase, tcoll->collectionName());
  }

  switch (operationType) {
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
    case TRI_VOC_DOCUMENT_OPERATION_UNKNOWN:
      break;
  }

  // perform an intermediate commit if necessary
  return checkIntermediateCommit(currentSize, hasPerformedIntermediateCommit);
}

RocksDBMethods* RocksDBTransactionState::rocksdbMethods() {
  TRI_ASSERT(_rocksMethods);
  return _rocksMethods.get();
}

uint64_t RocksDBTransactionState::sequenceNumber() const {
  if (_rocksTransaction) {
    return static_cast<uint64_t>(
        _rocksTransaction->GetSnapshot()->GetSequenceNumber());
  } else if (_readSnapshot != nullptr) {
    return static_cast<uint64_t>(_readSnapshot->GetSequenceNumber());
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "No snapshot set");
}

Result RocksDBTransactionState::triggerIntermediateCommit(bool& hasPerformedIntermediateCommit) {
  TRI_ASSERT(!hasPerformedIntermediateCommit);

  TRI_IF_FAILURE("FailBeforeIntermediateCommit") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("SegfaultBeforeIntermediateCommit") {
    TRI_SegfaultDebugging("SegfaultBeforeIntermediateCommit");
  }

  TRI_ASSERT(!hasHint(transaction::Hints::Hint::SINGLE_OPERATION));
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC(DEBUG, Logger::ENGINES) << "INTERMEDIATE COMMIT!";
#endif

  Result res = internalCommit();
  if (res.fail()) {
    // FIXME: do we abort the transaction ?
    return res;
  }

  hasPerformedIntermediateCommit = true;

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
  _rocksReadOptions.snapshot = _rocksTransaction->GetSnapshot();
  TRI_ASSERT(_readSnapshot != nullptr); // snapshots for iterators
  TRI_ASSERT(_rocksReadOptions.snapshot != nullptr);
  return TRI_ERROR_NO_ERROR;
}

Result RocksDBTransactionState::checkIntermediateCommit(uint64_t newSize, bool& hasPerformedIntermediateCommit) {
  hasPerformedIntermediateCommit = false;

  if (hasHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS)) {
    auto numOperations = _numInserts + _numUpdates + _numRemoves;
    // perform an intermediate commit
    // this will be done if either the "number of operations" or the
    // "transaction size" counters have reached their limit
    if (_options.intermediateCommitCount <= numOperations ||
        _options.intermediateCommitSize <= newSize) {
      return triggerIntermediateCommit(hasPerformedIntermediateCommit);
    }
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
    static_cast<RocksDBTransactionCollection*>(col)->trackIndexInsert(idxId, hash);
  } else {
    TRI_ASSERT(false);
  }
}

void RocksDBTransactionState::trackIndexRemove(TRI_voc_cid_t cid,
                                               TRI_idx_iid_t idxId,
                                               uint64_t hash) {
  auto col = findCollection(cid);
  if (col != nullptr) {
    static_cast<RocksDBTransactionCollection*>(col)->trackIndexRemove(idxId, hash);
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
