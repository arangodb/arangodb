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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBTrxBaseMethods.h"

#include "Random/RandomGenerator.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBSyncThread.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Statistics/ServerStatistics.h"

#include <rocksdb/utilities/write_batch_with_index.h>

using namespace arangodb;

RocksDBTrxBaseMethods::RocksDBTrxBaseMethods(RocksDBTransactionState* state, rocksdb::TransactionDB* db)
    : RocksDBTransactionMethods(state),
      _db(db) {
  TRI_ASSERT(!_state->isReadOnlyTransaction());
  _readOptions.prefix_same_as_start = true;  // should always be true
  _readOptions.fill_cache = _state->options().fillBlockCache;
}

RocksDBTrxBaseMethods::~RocksDBTrxBaseMethods() {
  cleanupTransaction();
}

bool RocksDBTrxBaseMethods::DisableIndexing() {
  if (!_indexingDisabled) {
    TRI_ASSERT(_rocksTransaction);
    _rocksTransaction->DisableIndexing();
    _indexingDisabled = true;
    return true;
  }
  return false;
}

bool RocksDBTrxBaseMethods::EnableIndexing() {
  if (_indexingDisabled) {
    TRI_ASSERT(_rocksTransaction);
    _rocksTransaction->EnableIndexing();
    _indexingDisabled = false;
    return true;
  }
  return false;
}

Result RocksDBTrxBaseMethods::beginTransaction() {
  TRI_ASSERT(_rocksTransaction == nullptr);

  createTransaction();
  TRI_ASSERT(_rocksTransaction != nullptr);
  _readOptions.snapshot = _rocksTransaction->GetSnapshot();

  return {};
}

Result RocksDBTrxBaseMethods::commitTransaction() {
  Result result = doCommit();
  if (result.ok()) {
    cleanupTransaction();
  }
  return result;
}

Result RocksDBTrxBaseMethods::abortTransaction() {
  Result result;
  if (_rocksTransaction != nullptr) {
    rocksdb::Status status = _rocksTransaction->Rollback();
    result = rocksutils::convertStatus(status);
  }
  cleanupTransaction();
  return result;
}

TRI_voc_tick_t RocksDBTrxBaseMethods::lastOperationTick() const noexcept {
  return _lastWrittenOperationTick;
}

/// @brief acquire a database snapshot if we do not yet have one.
/// Returns true if a snapshot was acquired, otherwise false (i.e., if we already had a snapshot)
bool RocksDBTrxBaseMethods::ensureSnapshot() {
  return false;
}

rocksdb::SequenceNumber RocksDBTrxBaseMethods::GetSequenceNumber() const noexcept {
  if (_rocksTransaction) {
    return _rocksTransaction->GetSnapshot()->GetSequenceNumber();
  }
  return _db->GetLatestSequenceNumber();
}

/// @brief add an operation for a transaction collection
Result RocksDBTrxBaseMethods::addOperation(DataSourceId cid, RevisionId revisionId,
                                           TRI_voc_document_operation_e operationType) {
  TRI_IF_FAILURE("addOperationSizeError") {
    return Result(TRI_ERROR_RESOURCE_LIMIT);
  }

  size_t currentSize = _rocksTransaction->GetWriteBatch()->GetWriteBatch()->GetDataSize();
  if (currentSize > _state->options().maxTransactionSize) {
    // we hit the transaction size limit
    std::string message =
    "aborting transaction because maximal transaction size limit of " +
        std::to_string(_state->options().maxTransactionSize) +
        " bytes is reached";
    return Result(TRI_ERROR_RESOURCE_LIMIT, message);
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

  return {};
}

rocksdb::Status RocksDBTrxBaseMethods::Get(rocksdb::ColumnFamilyHandle* cf,
                                           rocksdb::Slice const& key,
                                           rocksdb::PinnableSlice* val,
                                           ReadOwnWrites readOwnWrites) {
  TRI_ASSERT(cf != nullptr);
  rocksdb::ReadOptions const& ro = _readOptions;
  TRI_ASSERT(ro.snapshot != nullptr);
  if (readOwnWrites == ReadOwnWrites::yes) {
    return _rocksTransaction->Get(ro, cf, key, val);
  } else {
    return _db->Get(ro, cf, key, val);
  }
}

rocksdb::Status RocksDBTrxBaseMethods::GetForUpdate(rocksdb::ColumnFamilyHandle* cf,
                                                    rocksdb::Slice const& key,
                                                    rocksdb::PinnableSlice* val) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  rocksdb::ReadOptions const& ro = _readOptions;
  TRI_ASSERT(ro.snapshot != nullptr);
  return _rocksTransaction->GetForUpdate(ro, cf, key, val);
}

rocksdb::Status RocksDBTrxBaseMethods::Put(rocksdb::ColumnFamilyHandle* cf,
                                           RocksDBKey const& key,
                                           rocksdb::Slice const& val,
                                           bool assume_tracked) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  return _rocksTransaction->Put(cf, key.string(), val, assume_tracked);
}

rocksdb::Status RocksDBTrxBaseMethods::PutUntracked(rocksdb::ColumnFamilyHandle* cf,
                                                    RocksDBKey const& key,
                                                    rocksdb::Slice const& val) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  return _rocksTransaction->PutUntracked(cf, key.string(), val);
}

rocksdb::Status RocksDBTrxBaseMethods::Delete(rocksdb::ColumnFamilyHandle* cf,
                                              RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  return _rocksTransaction->Delete(cf, key.string());
}

rocksdb::Status RocksDBTrxBaseMethods::SingleDelete(rocksdb::ColumnFamilyHandle* cf,
                                                    RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  return _rocksTransaction->SingleDelete(cf, key.string());
}

void RocksDBTrxBaseMethods::PutLogData(rocksdb::Slice const& blob) {
  TRI_ASSERT(_rocksTransaction);
  _rocksTransaction->PutLogData(blob);
}

void RocksDBTrxBaseMethods::SetSavePoint() {
  TRI_ASSERT(_rocksTransaction);
  _rocksTransaction->SetSavePoint();
}

rocksdb::Status RocksDBTrxBaseMethods::RollbackToSavePoint() {
  TRI_ASSERT(_rocksTransaction);
  return _rocksTransaction->RollbackToSavePoint();
}

rocksdb::Status RocksDBTrxBaseMethods::RollbackToWriteBatchSavePoint() {
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

void RocksDBTrxBaseMethods::PopSavePoint() {
  TRI_ASSERT(_rocksTransaction);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  rocksdb::Status s = _rocksTransaction->PopSavePoint();
  TRI_ASSERT(s.ok());
#else
  _rocksTransaction->PopSavePoint();
#endif
}

void RocksDBTrxBaseMethods::cleanupTransaction() {
  delete _rocksTransaction;
  _rocksTransaction = nullptr;
}

void RocksDBTrxBaseMethods::createTransaction() {
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
}

arangodb::Result RocksDBTrxBaseMethods::doCommit() {
  if (!hasOperations()) { // bail out early
    TRI_ASSERT(_rocksTransaction == nullptr ||
               (_rocksTransaction->GetNumKeys() == 0 &&
                _rocksTransaction->GetNumPuts() == 0 &&
                _rocksTransaction->GetNumDeletes() == 0));
    // this is most likely the fill index case
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // TODO - for ReplicatedRocksDBTransactionState this is no longer the case
    // should we remove this or think of some other way to check the current state?
    // for (auto& trxColl : _state->_collections) {
    //   auto* rcoll = static_cast<RocksDBTransactionCollection*>(trxColl);
    //   TRI_ASSERT(!rcoll->hasOperations());
    //   TRI_ASSERT(rcoll->stealTrackedOperations().empty());
    //   TRI_ASSERT(rcoll->stealTrackedIndexOperations().empty());
    // }
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
  
  // we are actually going to attempt a commit

  ++_numCommits;
  uint64_t numOperations = this->numOperations();
  
  if (_state->isSingleOperation()) {
    // integrity-check our on-disk WAL format
    TRI_ASSERT(numOperations <= 1 && _numLogdata == numOperations);
  } else {
    // add custom commit marker to increase WAL tailing reliability
    auto logValue =
        RocksDBLogValue::CommitTransaction(_state->vocbase().id(), _state->id());

    _rocksTransaction->PutLogData(logValue.slice());
    _numLogdata++;
    
    // integrity-check our on-disk WAL format
    if (_numLogdata != (2 + _numRemoves)) {
      LOG_TOPIC("772e1", ERR, Logger::ENGINES)
          << "inconsistent internal transaction state: "
          << " numInserts: " << _numInserts << ", numRemoves: " << _numRemoves
          << ", numUpdates: " << _numUpdates << ", numLogdata: " << _numLogdata
          << ", numRollbacks: " << _numRollbacks << ", numCommits: " << _numCommits;
    }
    // begin transaction + commit transaction + n doc removes
    TRI_ASSERT(_numLogdata == (2 + _numRemoves));
  }
  TRI_ASSERT(numOperations > 0);

  rocksdb::SequenceNumber previousSeqNo = _state->prepareCollections();

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
  auto cleanupCollTrx = scopeGuard([this]() noexcept {
    try {
      _state->cleanupCollections();
    } catch (std::exception const& e) {
      LOG_TOPIC("62772", ERR, Logger::ENGINES)
          << "failed to cleanup collections: " << e.what();
    }
  });

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
  TRI_ASSERT(postCommitSeq >= previousSeqNo);

  if (ADB_LIKELY(numOps > 0)) {
    // we now need to add 1 for each write operation carried out in the trx
    // to get to the transaction's last operation's seqno
    postCommitSeq += numOps - 1;  // add to get to the next batch
  }
  // now use the transaction's last seqno for persisting revision trees
  _lastWrittenOperationTick = postCommitSeq;

  TRI_ASSERT(postCommitSeq <= _db->GetLatestSequenceNumber());

  _state->commitCollections(_lastWrittenOperationTick);
  cleanupCollTrx.cancel();

  // wait for sync if required
  if (_state->waitForSync()) {
    auto& selector = _state->vocbase().server().getFeature<EngineSelectorFeature>();
    auto& engine = selector.engine<RocksDBEngine>();
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

  return Result();
}
