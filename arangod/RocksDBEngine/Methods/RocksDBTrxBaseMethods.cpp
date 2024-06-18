////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Logger/LogMacros.h"
#include "Random/RandomGenerator.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBSyncThread.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Statistics/ServerStatistics.h"
#include "StorageEngine/EngineSelectorFeature.h"

#include <absl/cleanup/cleanup.h>
#include <absl/strings/str_cat.h>
#include <rocksdb/utilities/write_batch_with_index.h>

using namespace arangodb;

namespace {
metrics::Gauge<std::uint64_t>* determineMetric(
    RocksDBTransactionState const* state) noexcept {
  TRI_ASSERT(state != nullptr);

  switch (state->operationOrigin().type) {
    case transaction::OperationOrigin::Type::kAQL:
      return nullptr;
    case transaction::OperationOrigin::Type::kREST:
      return &state->statistics()._restTransactionsMemoryUsage;
    case transaction::OperationOrigin::Type::kInternal:
      return &state->statistics()._internalTransactionsMemoryUsage;
  }
  // we should not get here at all
  return nullptr;
}
}  // namespace

RocksDBTrxBaseMethods::RocksDBTrxBaseMethods(
    RocksDBTransactionState* state, IRocksDBTransactionCallback& callback,
    rocksdb::TransactionDB* db)
    : RocksDBTransactionMethods(state),
      _callback(callback),
      _db(db),
      _memoryTracker(_state, ::determineMetric(_state),
                     /*reportGranularity*/
                     RocksDBMethodsMemoryTracker::kDefaultGranularity) {
  TRI_ASSERT(_state != nullptr);

  TRI_ASSERT(!_state->isReadOnlyTransaction());
  _readOptions.prefix_same_as_start = true;  // should always be true
  _readOptions.fill_cache = _state->options().fillBlockCache;
}

RocksDBTrxBaseMethods::~RocksDBTrxBaseMethods() {
  // cppcheck-suppress *
  RocksDBTrxBaseMethods::cleanupTransaction();
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
  TRI_ASSERT(_rocksTransaction->GetSnapshot() == nullptr);

  if (!_state->options().delaySnapshot) {
    // In some cases we delay acquiring the snapshot so we can lock the key(s)
    // _before_ we acquire the snapshot to prevent write-write conflicts. In all
    // other cases we acquire the snapshot right now to be consistent with the
    // old behavior (at least for now).
    ensureSnapshot();
  }
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
/// Returns true if a snapshot was acquired, otherwise false (i.e., if we
/// already had a snapshot)
bool RocksDBTrxBaseMethods::ensureSnapshot() {
  if (_rocksTransaction->GetSnapshot() == nullptr) {
    _rocksTransaction->SetSnapshot();
    _readOptions.snapshot = _rocksTransaction->GetSnapshot();
    TRI_ASSERT(_readOptions.snapshot != nullptr);
    // we at least are at this point
    _lastWrittenOperationTick = _readOptions.snapshot->GetSequenceNumber();
    return true;
  }
  return false;
}

rocksdb::SequenceNumber RocksDBTrxBaseMethods::GetSequenceNumber()
    const noexcept {
  if (_rocksTransaction) {
    return _rocksTransaction->GetSnapshot()->GetSequenceNumber();
  }
  return _db->GetLatestSequenceNumber();
}

/// @brief add an operation for a transaction
Result RocksDBTrxBaseMethods::addOperation(
    TRI_voc_document_operation_e operationType) {
  TRI_IF_FAILURE("addOperationSizeError") { return {TRI_ERROR_RESOURCE_LIMIT}; }

  if (_memoryTracker.memoryUsage() > _state->options().maxTransactionSize) {
    // we hit the transaction size limit
    return {TRI_ERROR_RESOURCE_LIMIT,
            absl::StrCat("Maximal transaction size limit of ",
                         _state->options().maxTransactionSize,
                         " bytes is reached")};
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
  TRI_ASSERT(ro.snapshot != nullptr || _state->options().delaySnapshot);
  if (readOwnWrites == ReadOwnWrites::yes) {
    return _rocksTransaction->Get(ro, cf, key, val);
  }
  return _db->Get(ro, cf, key, val);
}

rocksdb::Status RocksDBTrxBaseMethods::GetForUpdate(
    rocksdb::ColumnFamilyHandle* cf, rocksdb::Slice const& key,
    rocksdb::PinnableSlice* val) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  rocksdb::ReadOptions const& ro = _readOptions;
  TRI_ASSERT(ro.snapshot != nullptr || _state->options().delaySnapshot);
  rocksdb::Status s = _rocksTransaction->GetForUpdate(ro, cf, key, val);
  if (s.ok()) {
    _memoryTracker.increaseMemoryUsage(
        lockOverhead(!_state->isOnlyExclusiveTransaction(), key.size()));
  }
  return s;
}

rocksdb::Status RocksDBTrxBaseMethods::Put(rocksdb::ColumnFamilyHandle* cf,
                                           RocksDBKey const& key,
                                           rocksdb::Slice const& val,
                                           bool assume_tracked) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  std::uint64_t beforeSize = currentWriteBatchSize();
  rocksdb::Status s =
      _rocksTransaction->Put(cf, key.string(), val, assume_tracked);
  if (s.ok()) {
    // size of WriteBatch got increased. track memory usage of WriteBatch
    // plus potential overhead of locking and indexing
    _memoryTracker.increaseMemoryUsage(
        (currentWriteBatchSize() - beforeSize) +
        lockOverhead(!_state->isOnlyExclusiveTransaction(),
                     key.string().size()) +
        indexingOverhead(!_indexingDisabled, key.string().size()));
  }
  return s;
}

rocksdb::Status RocksDBTrxBaseMethods::PutUntracked(
    rocksdb::ColumnFamilyHandle* cf, RocksDBKey const& key,
    rocksdb::Slice const& val) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  std::uint64_t beforeSize = currentWriteBatchSize();
  rocksdb::Status s = _rocksTransaction->PutUntracked(cf, key.string(), val);
  if (s.ok()) {
    // size of WriteBatch got increased. track memory usage of WriteBatch
    // plus potential overhead of locking and indexing
    _memoryTracker.increaseMemoryUsage(
        (currentWriteBatchSize() - beforeSize) +
        lockOverhead(!_state->isOnlyExclusiveTransaction(),
                     key.string().size()) +
        indexingOverhead(!_indexingDisabled, key.string().size()));
  }
  return s;
}

rocksdb::Status RocksDBTrxBaseMethods::Delete(rocksdb::ColumnFamilyHandle* cf,
                                              RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  std::uint64_t beforeSize = currentWriteBatchSize();
  rocksdb::Status s = _rocksTransaction->Delete(cf, key.string());
  if (s.ok()) {
    // size of WriteBatch got increased. track memory usage of WriteBatch
    // plus potential overhead of locking and indexing
    _memoryTracker.increaseMemoryUsage(
        (currentWriteBatchSize() - beforeSize) +
        lockOverhead(!_state->isOnlyExclusiveTransaction(),
                     key.string().size()) +
        indexingOverhead(!_indexingDisabled, key.string().size()));
  }
  return s;
}

rocksdb::Status RocksDBTrxBaseMethods::SingleDelete(
    rocksdb::ColumnFamilyHandle* cf, RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  std::uint64_t beforeSize = currentWriteBatchSize();
  rocksdb::Status s = _rocksTransaction->SingleDelete(cf, key.string());
  if (s.ok()) {
    // size of WriteBatch got increased. track memory usage of WriteBatch
    // plus potential overhead of locking and indexing
    _memoryTracker.increaseMemoryUsage(
        (currentWriteBatchSize() - beforeSize) +
        lockOverhead(!_state->isOnlyExclusiveTransaction(),
                     key.string().size()) +
        indexingOverhead(!_indexingDisabled, key.string().size()));
  }
  return s;
}

void RocksDBTrxBaseMethods::PutLogData(rocksdb::Slice const& blob) {
  TRI_ASSERT(_rocksTransaction);
  // PutLogData does not have a return value, so we assume it was successful
  // when it returns.
  std::uint64_t beforeSize = currentWriteBatchSize();
  _rocksTransaction->PutLogData(blob);
  // size of WriteBatch got increased. track memory usage
  _memoryTracker.increaseMemoryUsage(currentWriteBatchSize() - beforeSize);
}

void RocksDBTrxBaseMethods::SetSavePoint() {
  TRI_ASSERT(_rocksTransaction);
  _rocksTransaction->SetSavePoint();
  _memoryTracker.setSavePoint();
}

rocksdb::Status RocksDBTrxBaseMethods::RollbackToSavePoint() {
  TRI_ASSERT(_rocksTransaction);
  rocksdb::Status s = _rocksTransaction->RollbackToSavePoint();
  if (s.ok()) {
    _memoryTracker.rollbackToSavePoint();
  }
  return s;
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
  rocksdb::Status s = _rocksTransaction->GetWriteBatch()
                          ->GetWriteBatch()
                          ->RollbackToSavePoint();
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
  rocksdb::Status s = _rocksTransaction->PopSavePoint();
  TRI_ASSERT(s.ok());
  if (s.ok()) {
    _memoryTracker.popSavePoint();
  }
}

void RocksDBTrxBaseMethods::beginQuery(
    std::shared_ptr<ResourceMonitor> resourceMonitor,
    bool /*isModificationQuery*/) {
  _memoryTracker.beginQuery(resourceMonitor);
}

void RocksDBTrxBaseMethods::endQuery(bool /*isModificationQuery*/) noexcept {
  _memoryTracker.endQuery();
}

void RocksDBTrxBaseMethods::cleanupTransaction() {
  delete _rocksTransaction;
  _rocksTransaction = nullptr;
  _memoryTracker.reset();
}

void RocksDBTrxBaseMethods::createTransaction() {
  // start rocks transaction
  rocksdb::TransactionOptions trxOpts;

  if (_state->hasHint(transaction::Hints::Hint::IS_FOLLOWER_TRX)) {
    // write operations for the same keys on followers should normally be
    // serialized by the key locks held on the leaders. so we don't expect
    // to run into lock conflicts on followers. however, the lock_timeout
    // set here also includes locking the striped mutex for _all_ key locks,
    // which may be contended under load. to avoid timeouts caused by
    // waiting for the contented striped mutex, increase it to a higher
    // value on followers that makes this situation unlikely.
    trxOpts.lock_timeout = 3000;
  } else if (_state->options().delaySnapshot) {
    // for single operations we delay acquiring the snapshot so we can lock
    // the key _before_ we acquire the snapshot to prevent write-write
    // conflicts. in this case we obviously also want to use a higher lock
    // timeout
    // TODO - make this configurable
    trxOpts.lock_timeout = 1000;
  } else {
    // when trying to lock the same keys, we want to return quickly and not
    // spend the default 1000ms before giving up
    trxOpts.lock_timeout = 1;
  }

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

Result RocksDBTrxBaseMethods::doCommit() {
  // We need to call callbacks always, even if hasOperations() == false,
  // because it is like this in recovery
  TRI_ASSERT(_state != nullptr);
  _state->applyBeforeCommitCallbacks();
  auto r = doCommitImpl();
  if (r.ok()) {
    TRI_ASSERT(_state != nullptr);
    _state->applyAfterCommitCallbacks();
  }
  return r;
}

Result RocksDBTrxBaseMethods::doCommitImpl() {
  if (!hasOperations()) {  // bail out early
    TRI_ASSERT(_rocksTransaction == nullptr ||
               (_rocksTransaction->GetNumPuts() == 0 &&
                _rocksTransaction->GetNumDeletes() == 0));
    // this is most likely the fill index case
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // TODO - for ReplicatedRocksDBTransactionState this is no longer the case
    // should we remove this or think of some other way to check the current
    // state? for (auto& trxColl : _state->_collections) {
    //   auto* rcoll = static_cast<RocksDBTransactionCollection*>(trxColl);
    //   TRI_ASSERT(!rcoll->hasOperations());
    //   TRI_ASSERT(rcoll->stealTrackedOperations().empty());
    //   TRI_ASSERT(rcoll->stealTrackedIndexOperations().empty());
    // }
    // don't write anything if the transaction is empty
#endif
    return {};
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
    auto logValue = RocksDBLogValue::CommitTransaction(_state->vocbase().id(),
                                                       _state->id());

    _rocksTransaction->PutLogData(logValue.slice());
    _numLogdata++;

    // integrity-check our on-disk WAL format
    if (_numLogdata != (2 + _numRemoves)) {
      LOG_TOPIC("772e1", ERR, Logger::ENGINES)
          << "inconsistent internal transaction state: "
          << " numInserts: " << _numInserts << ", numRemoves: " << _numRemoves
          << ", numUpdates: " << _numUpdates << ", numLogdata: " << _numLogdata
          << ", numRollbacks: " << _numRollbacks
          << ", numCommits: " << _numCommits
          << ", numIntermediateCommits: " << _numIntermediateCommits;
    }
    // begin transaction + commit transaction + n doc removes
    TRI_ASSERT(_numLogdata == (2 + _numRemoves));
  }
  TRI_ASSERT(numOperations > 0);

  rocksdb::SequenceNumber previousSeqNo = _callback.prepare();

  TRI_IF_FAILURE("TransactionChaos::randomSync") {
    if (RandomGenerator::interval(uint32_t(1000)) > 950) {
      auto& engine = _state->vocbase().engine<RocksDBEngine>();
      auto* sm = engine.settingsManager();
      if (sm) {
        sm->sync(/*force*/ true);
      }
    }
  }

  // if we fail during commit, make sure we remove blockers, etc.
  auto cleanupCollTrx = scopeGuard([this]() noexcept {
    try {
      _callback.cleanup();
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
  if (!s.ok()) {  // cleanup performed by scope-guard
    return rocksutils::convertStatus(s);
  }

  _memoryTracker.reset();

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

  _state->clearQueryCache();
  // This resets the counters in the collection(s), so we also need to reset
  // our counters here for consistency.
  _callback.commit(_lastWrittenOperationTick);
  _numInserts = 0;
  _numUpdates = 0;
  _numRemoves = 0;
  TRI_ASSERT(this->numOperations() == 0);

  cleanupCollTrx.cancel();

  // wait for sync if required
  if (_state->waitForSync()) {
    auto& engine = _state->vocbase().engine<RocksDBEngine>();
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
  return {};
}

rocksdb::Status RocksDBTrxBaseMethods::SingleGet(
    rocksdb::Snapshot const* snapshot, rocksdb::ColumnFamilyHandle& family,
    rocksdb::Slice const& key, rocksdb::PinnableSlice& value) {
  absl::Cleanup restore = [&, was = _readOptions.snapshot] {
    _readOptions.snapshot = was;
  };
  _readOptions.snapshot = snapshot;

  return _db->Get(_readOptions, &family, key, &value);
}

void RocksDBTrxBaseMethods::MultiGet(rocksdb::Snapshot const* snapshot,
                                     rocksdb::ColumnFamilyHandle& family,
                                     size_t count, rocksdb::Slice const* keys,
                                     rocksdb::PinnableSlice* values,
                                     rocksdb::Status* statuses) {
  // make a copy of the ReadOptions, as we are going to modify the snapshot
  ReadOptions ro = _readOptions;
  ro.snapshot = snapshot;

  // Timestamps and multiple ColumnFamilies are not necessary for us
  _db->MultiGet(ro, &family, count, keys, values, statuses, false);
}

void RocksDBTrxBaseMethods::MultiGet(rocksdb::ColumnFamilyHandle& family,
                                     size_t count, rocksdb::Slice const* keys,
                                     rocksdb::PinnableSlice* values,
                                     rocksdb::Status* statuses,
                                     ReadOwnWrites readOwnWrites) {
  if (readOwnWrites == ReadOwnWrites::yes) {
    _rocksTransaction->MultiGet(_readOptions, &family, count, keys, values,
                                statuses, false);
  } else {
    _db->MultiGet(_readOptions, &family, count, keys, values, statuses, false);
  }
}

size_t RocksDBTrxBaseMethods::currentWriteBatchSize() const noexcept {
  TRI_ASSERT(_rocksTransaction);
  return _rocksTransaction->GetWriteBatch()->GetWriteBatch()->Data().capacity();
}
