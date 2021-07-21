////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBTrxMethods.h"

#include "Aql/QueryCache.h"
#include "Random/RandomGenerator.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Statistics/ServerStatistics.h"

#include <rocksdb/utilities/write_batch_with_index.h>

using namespace arangodb;

RocksDBTrxMethods::RocksDBTrxMethods(RocksDBTransactionState* state, rocksdb::TransactionDB* db)
    : RocksDBTransactionMethods(state),
      _db(db) {
  TRI_ASSERT(!_state->isReadOnlyTransaction());
  _readOptions.prefix_same_as_start = true;  // should always be true
  _readOptions.fill_cache = _state->options().fillBlockCache;
}

RocksDBTrxMethods::~RocksDBTrxMethods() {
  cleanupTransaction();
}

bool RocksDBTrxMethods::DisableIndexing() {
  if (!_indexingDisabled) {
    TRI_ASSERT(_rocksTransaction);
    _rocksTransaction->DisableIndexing();
    _indexingDisabled = true;
    return true;
  }
  return false;
}

bool RocksDBTrxMethods::EnableIndexing() {
  if (_indexingDisabled) {
    TRI_ASSERT(_rocksTransaction);
    _rocksTransaction->EnableIndexing();
    _indexingDisabled = false;
    return true;
  }
  return false;
}

Result RocksDBTrxMethods::beginTransaction() {
  TRI_ASSERT(_rocksTransaction == nullptr);

  createTransaction();
  TRI_ASSERT(_rocksTransaction != nullptr);
  _readOptions.snapshot = _rocksTransaction->GetSnapshot();

  TRI_ASSERT(_readSnapshot == nullptr);
  if (_state->hasHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS)) {
    TRI_ASSERT(_state->options().intermediateCommitCount != UINT64_MAX ||
               _state->options().intermediateCommitSize != UINT64_MAX);
    _readSnapshot = _db->GetSnapshot();  // must call ReleaseSnapshot later
    TRI_ASSERT(_readSnapshot != nullptr);
  }
  return {};
}

Result RocksDBTrxMethods::commitTransaction() {
  Result result = internalCommit();
  if (result.ok()) {
    cleanupTransaction();
  }
  return result;
}

Result RocksDBTrxMethods::abortTransaction() {
  Result result;
  if (_rocksTransaction != nullptr) {
    rocksdb::Status status = _rocksTransaction->Rollback();
    result = rocksutils::convertStatus(status);
  }
  return result;
}

rocksdb::ReadOptions RocksDBTrxMethods::iteratorReadOptions() const {
  if (_state->hasHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS)) {
    rocksdb::ReadOptions ro = _readOptions;
    TRI_ASSERT(_readSnapshot);
    ro.snapshot = _readSnapshot;
    return ro;
  }
  return _readOptions;
}

/// @brief acquire a database snapshot if we do not yet have one.
/// Returns true if a snapshot was acquired, otherwise false (i.e., if we already had a snapshot)
bool RocksDBTrxMethods::ensureSnapshot() {
  TRI_ASSERT(_readSnapshot != nullptr);
  return false;
}

rocksdb::SequenceNumber RocksDBTrxMethods::GetSequenceNumber() const {
  if (_rocksTransaction) {
    return _rocksTransaction->GetSnapshot()->GetSequenceNumber();
  }
  TRI_ASSERT(_readSnapshot == nullptr);
  return _db->GetLatestSequenceNumber();
}

void RocksDBTrxMethods::prepareOperation(DataSourceId cid, RevisionId rid,
                                         TRI_voc_document_operation_e operationType) {
  TRI_ASSERT(_rocksTransaction != nullptr);

  if (_state->isSingleOperation()) {
    // singleOp => no modifications yet
    TRI_ASSERT(_rocksTransaction->GetNumPuts() == 0 &&
               _rocksTransaction->GetNumDeletes() == 0);
    switch (operationType) {
      case TRI_VOC_DOCUMENT_OPERATION_UNKNOWN:
        break;

      case TRI_VOC_DOCUMENT_OPERATION_INSERT:
      case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
      case TRI_VOC_DOCUMENT_OPERATION_REPLACE: {
        auto logValue = RocksDBLogValue::SinglePut(_state->_vocbase.id(), cid);

        _rocksTransaction->PutLogData(logValue.slice());
        TRI_ASSERT(_numLogdata == 0);
        _numLogdata++;
        break;
      }
      case TRI_VOC_DOCUMENT_OPERATION_REMOVE: {
        TRI_ASSERT(rid.isSet());

        auto logValue = RocksDBLogValue::SingleRemoveV2(_state->_vocbase.id(), cid, rid);

        _rocksTransaction->PutLogData(logValue.slice());
        TRI_ASSERT(_numLogdata == 0);
        _numLogdata++;
        break;
      }
    }
  } else {
    if (operationType == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
      RocksDBLogValue logValue = RocksDBLogValue::DocumentRemoveV2(rid);
      _rocksTransaction->PutLogData(logValue.slice());
      ++_numLogdata;
    }
  }
}


/// @brief undo the effects of the previous prepareOperation call
void RocksDBTrxMethods::rollbackOperation(TRI_voc_document_operation_e operationType) {
  ++_numRollbacks;

  if (_state->isSingleOperation()) {
    switch (operationType) {
      case TRI_VOC_DOCUMENT_OPERATION_INSERT:
      case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
      case TRI_VOC_DOCUMENT_OPERATION_REPLACE:
      case TRI_VOC_DOCUMENT_OPERATION_REMOVE: {
        TRI_ASSERT(_numLogdata > 0);
        --_numLogdata;
        break;
      }
      default: { 
        break; 
      }
    }
  } else {
    if (operationType == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
      TRI_ASSERT(_numLogdata > 0);
      --_numLogdata;
    }
  }
}

/// @brief add an operation for a transaction collection
Result RocksDBTrxMethods::addOperation(DataSourceId cid, RevisionId revisionId,
                                       TRI_voc_document_operation_e operationType,
                                       bool& hasPerformedIntermediateCommit) {
  TRI_IF_FAILURE("addOperationSizeError") {
    return Result(TRI_ERROR_RESOURCE_LIMIT);
  }

  size_t currentSize = _rocksTransaction->GetWriteBatch()->GetWriteBatch()->GetDataSize();
  if (currentSize > _state->_options.maxTransactionSize) {
    // we hit the transaction size limit
    std::string message =
    "aborting transaction because maximal transaction size limit of " +
        std::to_string(_state->_options.maxTransactionSize) +
        " bytes is reached";
    return Result(TRI_ERROR_RESOURCE_LIMIT, message);
  }

  auto tcoll = static_cast<RocksDBTransactionCollection*>(_state->findCollection(cid));
  if (tcoll == nullptr) {
    std::string message = "collection '" + std::to_string(cid.id()) +
                          "' not found in transaction state";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
  }

  // should not fail or fail with exception
  tcoll->addOperation(operationType, revisionId);

  // clear the query cache for this collection
  auto queryCache = arangodb::aql::QueryCache::instance();

  if (queryCache->mayBeActive() && tcoll->collection()) {
    queryCache->invalidate(&_state->_vocbase, tcoll->collection()->guid());
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


rocksdb::Status RocksDBTrxMethods::Get(rocksdb::ColumnFamilyHandle* cf,
                                       rocksdb::Slice const& key,
                                       rocksdb::PinnableSlice* val) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  rocksdb::ReadOptions const& ro = _readOptions;
  TRI_ASSERT(ro.snapshot != nullptr);
  return _rocksTransaction->Get(ro, cf, key, val);
}

rocksdb::Status RocksDBTrxMethods::GetForUpdate(rocksdb::ColumnFamilyHandle* cf,
                                                rocksdb::Slice const& key,
                                                rocksdb::PinnableSlice* val) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  rocksdb::ReadOptions const& ro = _readOptions;
  TRI_ASSERT(ro.snapshot != nullptr);
  return _rocksTransaction->GetForUpdate(ro, cf, key, val);
}

rocksdb::Status RocksDBTrxMethods::Put(rocksdb::ColumnFamilyHandle* cf,
                                       RocksDBKey const& key,
                                       rocksdb::Slice const& val, bool assume_tracked) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  return _rocksTransaction->Put(cf, key.string(), val, assume_tracked);
}

rocksdb::Status RocksDBTrxMethods::PutUntracked(rocksdb::ColumnFamilyHandle* cf,
                                                RocksDBKey const& key,
                                                rocksdb::Slice const& val) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  return _rocksTransaction->PutUntracked(cf, key.string(), val);
}

rocksdb::Status RocksDBTrxMethods::Delete(rocksdb::ColumnFamilyHandle* cf,
                                          RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  return _rocksTransaction->Delete(cf, key.string());
}

rocksdb::Status RocksDBTrxMethods::SingleDelete(rocksdb::ColumnFamilyHandle* cf,
                                                RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  return _rocksTransaction->SingleDelete(cf, key.string());
}

void RocksDBTrxMethods::PutLogData(rocksdb::Slice const& blob) {
  TRI_ASSERT(_rocksTransaction);
  _rocksTransaction->PutLogData(blob);
}

std::unique_ptr<rocksdb::Iterator> RocksDBTrxMethods::NewIterator(
    rocksdb::ReadOptions const& opts, rocksdb::ColumnFamilyHandle* cf) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);

  std::unique_ptr<rocksdb::Iterator> iterator(_rocksTransaction->GetIterator(opts, cf));
  if (iterator == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid iterator in RocksDBTrxMethods");
  }
  return iterator;
}

void RocksDBTrxMethods::SetSavePoint() {
  TRI_ASSERT(_rocksTransaction);
  _rocksTransaction->SetSavePoint();
}

rocksdb::Status RocksDBTrxMethods::RollbackToSavePoint() {
  TRI_ASSERT(_rocksTransaction);
  return _rocksTransaction->RollbackToSavePoint();
}

rocksdb::Status RocksDBTrxMethods::RollbackToWriteBatchSavePoint() {
  TRI_ASSERT(_rocksTransaction);
  // this deserves some further explanation:
  // we are first trying to get rid of the last changes in the write batch,
  // but we don't want to pay the price for rebuilding the WBWI from scratch
  // with all that remains in the WB.
  // so what we do is the following:
  // we first revert the changes in the WB only. this will truncate the WB
  // to the position of the last SavePoint, and is cheap
  rocksdb::Status s =
      _rocksTransaction->GetWriteBatch()->GetWriteBatch()->RollbackToSavePoint();
  if (s.ok()) {
    // if this succeeds we now add a new SavePoint to the WB. this does nothing,
    // but we need it to have the same number of SavePoints in the WB and the
    // WBWI.
    _rocksTransaction->GetWriteBatch()->GetWriteBatch()->SetSavePoint();

    // finally, we pop off the SavePoint from the WBWI, which will remove the
    // latest changes from the WBWI and the WB (our dummy SavePoint), but it
    // will _not_ rebuild the entire WBWI from the WB
    this->PopSavePoint();
  }
  TRI_ASSERT(s.ok());
  return s;
}

void RocksDBTrxMethods::PopSavePoint() {
  TRI_ASSERT(_rocksTransaction);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  rocksdb::Status s = _rocksTransaction->PopSavePoint();
  TRI_ASSERT(s.ok());
#else
  _rocksTransaction->PopSavePoint();
#endif
}

void RocksDBTrxMethods::cleanupTransaction() {
  delete _rocksTransaction;
  _rocksTransaction = nullptr;
  
  if (_readSnapshot != nullptr) {
    TRI_ASSERT(_state->hasHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS));
    _db->ReleaseSnapshot(_readSnapshot);
    _readSnapshot = nullptr;
  }
}

void RocksDBTrxMethods::createTransaction() {
  // start rocks transaction
  rocksdb::TransactionOptions trxOpts;
  trxOpts.set_snapshot = true;

  // when trying to lock the same keys, we want to return quickly and not
  // spend the default 1000ms before giving up
  trxOpts.lock_timeout = 1;

  // unclear performance implications do not use for now
  // trxOpts.deadlock_detect = !hasHint(transaction::Hints::Hint::NO_DLD);
  if (_state->isOnlyExclusiveTransaction()) {
    // we are exclusively modifying collection data here, so we can turn off
    // all concurrency control checks to save time
    trxOpts.skip_concurrency_control = true;
  }

  TRI_ASSERT(_rocksTransaction == nullptr ||
             _rocksTransaction->GetState() == rocksdb::Transaction::COMMITED ||
             (_rocksTransaction->GetState() == rocksdb::Transaction::STARTED &&
              _rocksTransaction->GetNumKeys() == 0));
  rocksdb::WriteOptions wo;
  _rocksTransaction = _db->BeginTransaction(wo, trxOpts, _rocksTransaction);

  // add transaction begin marker
  if (!_state->isSingleOperation()) {
    auto header = RocksDBLogValue::BeginTransaction(_state->_vocbase.id(), _state->id());

    _rocksTransaction->PutLogData(header.slice());
    TRI_ASSERT(_numLogdata == 0);
    ++_numLogdata;
  }
}

arangodb::Result RocksDBTrxMethods::internalCommit() {
  if (!hasOperations()) { // bail out early
    TRI_ASSERT(_rocksTransaction == nullptr ||
               (_rocksTransaction->GetNumKeys() == 0 &&
                _rocksTransaction->GetNumPuts() == 0 &&
                _rocksTransaction->GetNumDeletes() == 0));
    // this is most likely the fill index case
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    for (auto& trxColl : _state->_collections) {
      auto* rcoll = static_cast<RocksDBTransactionCollection*>(trxColl);
      TRI_ASSERT(!rcoll->hasOperations());
      TRI_ASSERT(rcoll->stealTrackedOperations().empty());
      TRI_ASSERT(rcoll->stealTrackedIndexOperations().empty());
    }
    // don't write anything if the transaction is empty
#endif
    return Result();
  }

  // we may need to block intermediate commits
  ExecContext const& exec = ExecContext::current();
  bool cancelRW = ServerState::readOnly() && !exec.isSuperuser();
  if (exec.isCanceled() || cancelRW) {
    return Result(TRI_ERROR_ARANGO_READ_ONLY, "server is in read-only mode");
  }

  auto commitCounts = [this]() {
    _state->commitCollections(_state->_lastWrittenOperationTick);
  };

  // we are actually going to attempt a commit
  if (!_state->isSingleOperation()) {
    // add custom commit marker to increase WAL tailing reliability
    auto logValue = RocksDBLogValue::CommitTransaction(_state->_vocbase.id(), _state->id());

    _rocksTransaction->PutLogData(logValue.slice());
    _numLogdata++;
  }
  
  ++_numCommits;

  // integrity-check our on-disk WAL format
  uint64_t x = _numInserts + _numRemoves + _numUpdates;
  if (_state->isSingleOperation()) {
    TRI_ASSERT(x <= 1 && _numLogdata == x);
  } else {
    if (_numLogdata != (2 + _numRemoves)) {
      LOG_TOPIC("772e1", ERR, Logger::ENGINES) 
        << "inconsistent internal transaction state: " 
        << " numInserts: " << _numInserts
        << ", numRemoves: " << _numRemoves
        << ", numUpdates: " << _numUpdates
        << ", numLogdata: " << _numLogdata 
        << ", numRollbacks: " << _numRollbacks 
        << ", numCommits: " << _numCommits;
    }
    // begin transaction + commit transaction + n doc removes
    TRI_ASSERT(_numLogdata == (2 + _numRemoves));
  }
  TRI_ASSERT(x > 0);

  _state->prepareCollections();

  TRI_IF_FAILURE("TransactionChaos::randomSync") {
    if (RandomGenerator::interval(uint32_t(1000)) > 950) {
      auto& selector = _state->vocbase().server().getFeature<EngineSelectorFeature>();
      auto& engine = selector.engine<RocksDBEngine>();
      auto* sm = engine.settingsManager();
      if (sm) {
        sm->sync(true);  // force
      }
    }
  }

  // if we fail during commit, make sure we remove blockers, etc.
  auto cleanupCollTrx = scopeGuard([this]() { _state->cleanupCollections(); });

#ifdef _WIN32
  // set wait for sync flag if required
  // we do this only for Windows here, because all other platforms use the
  // RocksDB SyncThread to do the syncing
  if (_state->waitForSync()) {
    rocksdb::WriteOptions wo;
    wo.sync = true;
    _rocksTransaction->SetWriteOptions(wo);
  }
#endif

  // total number of sequence ID consuming records
  uint64_t numOps = _rocksTransaction->GetNumPuts() +
                    _rocksTransaction->GetNumDeletes() +
                    _rocksTransaction->GetNumMerges();

  rocksdb::Status s = _rocksTransaction->Commit();
  if (!s.ok()) { // cleanup performed by scope-guard
    return rocksutils::convertStatus(s);
  }

  TRI_ASSERT(numOps > 0);  // simon: should hold unless we're being stupid
  // the transaction id that is returned here is the seqno of the transaction's
  // first write operation in the WAL
  rocksdb::SequenceNumber postCommitSeq = _rocksTransaction->GetId();
  TRI_ASSERT(postCommitSeq != 0);
  if (ADB_LIKELY(numOps > 0)) {
    // we now need to add 1 for each write operation carried out in the trx
    // to get to the transaction's last operation's seqno
    postCommitSeq += numOps - 1;  // add to get to the next batch
  }
  // now use the transaction's last seqno for persisting revision trees
  _state->_lastWrittenOperationTick = postCommitSeq;


  TRI_ASSERT(postCommitSeq <= _db->GetLatestSequenceNumber());

  commitCounts();
  cleanupCollTrx.cancel();

#ifndef _WIN32
  // wait for sync if required, for all other platforms but Windows
  if (waitForSync()) {
    if (engine.syncThread()) {
      // we do have a sync thread
      return engine.syncThread()->syncWal();
    } else {
      // no sync thread present... this may be the case if automatic
      // syncing is completely turned off. in this case, use the
      // static sync method
      return RocksDBSyncThread::sync(engine.db()->GetBaseDB());
    }
  }
#endif

  return Result();
}


Result RocksDBTrxMethods::triggerIntermediateCommit(bool& hasPerformedIntermediateCommit) {
  TRI_ASSERT(!hasPerformedIntermediateCommit);

  TRI_IF_FAILURE("FailBeforeIntermediateCommit") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("SegfaultBeforeIntermediateCommit") {
    TRI_TerminateDebugging("SegfaultBeforeIntermediateCommit");
  }

  TRI_ASSERT(!_state->isSingleOperation());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC("0fe63", DEBUG, Logger::ENGINES) << "INTERMEDIATE COMMIT!";
#endif

  Result res = internalCommit();
  if (res.fail()) {
    // FIXME: do we abort the transaction ?
    return res;
  }

  hasPerformedIntermediateCommit = true;
  ++_state->statistics()._intermediateCommits;
  
  // reset counters for DML operations, but intentionally don't reset
  // the commit counter, as we need to track if we had intermediate commits
  _numInserts = 0;
  _numUpdates = 0;
  _numRemoves = 0;
  _numLogdata = 0;

  TRI_IF_FAILURE("FailAfterIntermediateCommit") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("SegfaultAfterIntermediateCommit") {
    TRI_TerminateDebugging("SegfaultAfterIntermediateCommit");
  }

  createTransaction();
  _readOptions.snapshot = _rocksTransaction->GetSnapshot();
  // TODO - TRI_ASSERT(_readSnapshot != nullptr);  // snapshots for iterators
  //        TRI_ASSERT(_rocksReadOptions.snapshot != nullptr);
  return TRI_ERROR_NO_ERROR;
}

Result RocksDBTrxMethods::checkIntermediateCommit(uint64_t newSize, bool& hasPerformedIntermediateCommit) {
  hasPerformedIntermediateCommit = false;
    
  TRI_IF_FAILURE("noIntermediateCommits") {
    return TRI_ERROR_NO_ERROR;
  }

  if (_state->hasHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS)) {
    auto numOperations = _numInserts + _numUpdates + _numRemoves;
    // perform an intermediate commit
    // this will be done if either the "number of operations" or the
    // "transaction size" counters have reached their limit
    if (_state->_options.intermediateCommitCount <= numOperations ||
        _state->_options.intermediateCommitSize <= newSize) {
      return triggerIntermediateCommit(hasPerformedIntermediateCommit);
    }
  }
  return TRI_ERROR_NO_ERROR;
}
