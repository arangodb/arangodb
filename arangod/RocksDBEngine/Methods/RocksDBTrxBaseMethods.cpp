////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ResourceUsage.h"
#include "Containers/SmallVector.h"
#include "Logger/LogMacros.h"
#include "Metrics/Gauge.h"
#include "Random/RandomGenerator.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBSyncThread.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBMethodsMemoryTracker.h"
#include "Statistics/ServerStatistics.h"
#include "StorageEngine/EngineSelectorFeature.h"

#include <absl/cleanup/cleanup.h>
#include <rocksdb/utilities/write_batch_with_index.h>

using namespace arangodb;

// abstract base class with common functionality for other memory usage
// trackers
class MemoryTrackerBase : public RocksDBMethodsMemoryTracker {
 public:
  MemoryTrackerBase(MemoryTrackerBase const&) = delete;
  MemoryTrackerBase& operator=(MemoryTrackerBase const&) = delete;

  MemoryTrackerBase() : _memoryUsage(0), _memoryUsageAtBeginQuery(0) {}

  ~MemoryTrackerBase() {
    TRI_ASSERT(_memoryUsage == 0);
    TRI_ASSERT(_memoryUsageAtBeginQuery == 0);
  }

  void reset() noexcept override {
    _memoryUsage = 0;
    _memoryUsageAtBeginQuery = 0;
    _savePoints.clear();
  }

  void increaseMemoryUsage(std::uint64_t value) override {
    _memoryUsage += value;
  }

  void decreaseMemoryUsage(std::uint64_t value) noexcept override {
    TRI_ASSERT(_memoryUsage >= value);
    _memoryUsage -= value;
  }

  void setSavePoint() override { _savePoints.push_back(_memoryUsage); }

  void rollbackToSavePoint() noexcept override {
    TRI_ASSERT(!_savePoints.empty());
    _memoryUsage = _savePoints.back();
    _savePoints.pop_back();
  }

  void popSavePoint() noexcept override {
    TRI_ASSERT(!_savePoints.empty());
    _savePoints.pop_back();
  }

  std::uint64_t memoryUsage() const noexcept override { return _memoryUsage; }

 protected:
  std::uint64_t _memoryUsage;
  std::uint64_t _memoryUsageAtBeginQuery;
  containers::SmallVector<std::uint64_t, 4> _savePoints;
};

// memory usage tracker for AQL transactions that tracks memory
// allocations during an AQL query. reports to the AQL query's
// ResourceMonitor.
class MemoryTrackerAqlQuery final : public MemoryTrackerBase {
 public:
  MemoryTrackerAqlQuery() : MemoryTrackerBase(), _resourceMonitor(nullptr) {}

  ~MemoryTrackerAqlQuery() { reset(); }

  void reset() noexcept override {
    // inform resource monitor first
    if (_resourceMonitor != nullptr && _memoryUsage != 0) {
      _resourceMonitor->decreaseMemoryUsage(_memoryUsage);
    }
    // reset everything
    MemoryTrackerBase::reset();
  }

  void increaseMemoryUsage(std::uint64_t value) override {
    if (value != 0) {
      // inform resource monitor first
      if (_resourceMonitor != nullptr) {
        // can throw. if it does, no harm is done
        _resourceMonitor->increaseMemoryUsage(value);
      }
      MemoryTrackerBase::increaseMemoryUsage(value);
    }
  }

  void decreaseMemoryUsage(std::uint64_t value) noexcept override {
    if (value != 0) {
      // inform resource monitor
      if (_resourceMonitor != nullptr) {
        _resourceMonitor->decreaseMemoryUsage(value);
      }
      MemoryTrackerBase::decreaseMemoryUsage(value);
    }
  }

  void rollbackToSavePoint() noexcept override {
    auto value = _memoryUsage;
    // this will reset _memoryUsage
    MemoryTrackerBase::rollbackToSavePoint();

    // inform resource monitor
    if (_resourceMonitor != nullptr) {
      if (value > _memoryUsage) {
        // this should be the usual case, that after popping a
        // SavePoint we will use _less_ memory
        _resourceMonitor->decreaseMemoryUsage(value - _memoryUsage);
      } else {
        // somewhat unexpected case
        _resourceMonitor->increaseMemoryUsage(_memoryUsage - value);
      }
    }
  }

  void beginQuery(ResourceMonitor* resourceMonitor) override {
    TRI_ASSERT(resourceMonitor != nullptr);
    TRI_ASSERT(_resourceMonitor == nullptr);
    TRI_ASSERT(_memoryUsageAtBeginQuery == 0);
    _resourceMonitor = resourceMonitor;
    if (_resourceMonitor != nullptr) {
      _memoryUsageAtBeginQuery = _memoryUsage;
    }
  }

  void endQuery() noexcept override {
    TRI_ASSERT(_resourceMonitor != nullptr);

    if (_resourceMonitor != nullptr) {
      if (_memoryUsage >= _memoryUsageAtBeginQuery) {
        _resourceMonitor->decreaseMemoryUsage(_memoryUsage -
                                              _memoryUsageAtBeginQuery);
      } else {
        _resourceMonitor->increaseMemoryUsage(_memoryUsageAtBeginQuery -
                                              _memoryUsage);
      }
      _memoryUsage = _memoryUsageAtBeginQuery;
      _memoryUsageAtBeginQuery = 0;
      _resourceMonitor = nullptr;
    }
  }

 private:
  ResourceMonitor* _resourceMonitor;
};

// memory usage tracker for transactions that update a particular
// memory usage metric. currently used for all transactions that
// are not top-level AQL queries, and for internal transactions
// (transactions that were not explicitly initiated by users).
class MemoryTrackerMetric final : public MemoryTrackerBase {
 public:
  MemoryTrackerMetric(metrics::Gauge<std::uint64_t>* metric)
      : MemoryTrackerBase(), _metric(metric), _lastPublishedValue(0) {
    TRI_ASSERT(_metric != nullptr);
  }

  ~MemoryTrackerMetric() { reset(); }

  void reset() noexcept override {
    // reset everything
    MemoryTrackerBase::reset();
    publish(true);
  }

  void increaseMemoryUsage(std::uint64_t value) override {
    if (value != 0) {
      // both of these will not throw, so we can execute them
      // in any order
      MemoryTrackerBase::increaseMemoryUsage(value);
      publish(false);
    }
  }

  void decreaseMemoryUsage(std::uint64_t value) noexcept override {
    if (value != 0) {
      MemoryTrackerBase::decreaseMemoryUsage(value);
      publish(false);
    }
  }

  void rollbackToSavePoint() noexcept override {
    // this will reset _memoryUsage
    MemoryTrackerBase::rollbackToSavePoint();
    publish(true);
  }

  void beginQuery(ResourceMonitor* /*resourceMonitor*/) override {
    // note: resourceMonitor can be a nullptr when we are called
    // from RocksDBCollection::truncateWithRemovals()
    TRI_ASSERT(_memoryUsageAtBeginQuery == 0);
    _memoryUsageAtBeginQuery = _memoryUsage;
  }

  void endQuery() noexcept override {
    _memoryUsage = _memoryUsageAtBeginQuery;
    _memoryUsageAtBeginQuery = 0;
    publish(true);
  }

 private:
  void publish(bool force) noexcept {
    if (!force) {
      if (_lastPublishedValue < _memoryUsage) {
        // current memory usage is higher
        force |=
            (_memoryUsage - _lastPublishedValue) >= kMemoryReportGranularity;
      } else if (_lastPublishedValue > _memoryUsage) {
        // current memory usage is lower
        force |=
            (_lastPublishedValue - _memoryUsage) >= kMemoryReportGranularity;
      }
    }
    if (force) {
      if (_lastPublishedValue < _memoryUsage) {
        // current memory usage is higher
        _metric->fetch_add(_memoryUsage - _lastPublishedValue);
      } else if (_lastPublishedValue > _memoryUsage) {
        // current memory usage is lower
        _metric->fetch_sub(_lastPublishedValue - _memoryUsage);
      }
      _lastPublishedValue = _memoryUsage;
    }
  }

  metrics::Gauge<std::uint64_t>* _metric;

  // last value we published to the metric ourselves. we keep track
  // of this so we only need to update the metric if our current
  // memory usage differs by more than kMemoryReportGranularity from
  // what we already posted to the metric. we do this to save lots
  // of metrics updates with very small increments/decrements, which
  // would provide little value and would only lead to contention on
  // the metric's underlying atomic value.
  std::uint64_t _lastPublishedValue;

  // publish only memory usage differences if memory usage changed
  // by this many bytes since our last update to the metric.
  // this is to avoid too frequent metrics updates and potential
  // contention on ths metric's underlying atomic value.
  static constexpr std::int64_t kMemoryReportGranularity = 4096;
};

RocksDBTrxBaseMethods::RocksDBTrxBaseMethods(
    RocksDBTransactionState* state, IRocksDBTransactionCallback& callback,
    rocksdb::TransactionDB* db)
    : RocksDBTransactionMethods(state), _callback(callback), _db(db) {
  TRI_ASSERT(_state != nullptr);

  switch (_state->getTrxTypeHint()) {
    case transaction::TrxType::kREST:
      _memoryTracker = std::make_unique<MemoryTrackerMetric>(
          &state->statistics()._restTransactionsMemoryUsage);
      break;
    case transaction::TrxType::kAQL:
      TRI_ASSERT(!_state->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED));
      _memoryTracker = std::make_unique<MemoryTrackerAqlQuery>();
      break;
    case transaction::TrxType::kInternal:
      _memoryTracker = std::make_unique<MemoryTrackerMetric>(
          &state->statistics()._internalTransactionsMemoryUsage);
      break;
  }

  TRI_ASSERT(_memoryTracker != nullptr);

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
  TRI_IF_FAILURE("addOperationSizeError") {
    return Result(TRI_ERROR_RESOURCE_LIMIT);
  }

  size_t currentSize = currentWriteBatchSize();
  if (currentSize > _state->options().maxTransactionSize) {
    // we hit the transaction size limit
    std::string message =
        "aborting transaction because maximal transaction size limit of " +
        std::to_string(_state->options().maxTransactionSize) +
        " bytes is reached";
    return Result(TRI_ERROR_RESOURCE_LIMIT, std::move(message));
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
    _memoryTracker->increaseMemoryUsage(lockOverhead(key.size()));
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
    _memoryTracker->increaseMemoryUsage((currentWriteBatchSize() - beforeSize) +
                                        lockOverhead(key.string().size()) +
                                        indexingOverhead(key.string().size()));
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
    _memoryTracker->increaseMemoryUsage((currentWriteBatchSize() - beforeSize) +
                                        lockOverhead(key.string().size()) +
                                        indexingOverhead(key.string().size()));
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
    _memoryTracker->increaseMemoryUsage((currentWriteBatchSize() - beforeSize) +
                                        lockOverhead(key.string().size()) +
                                        indexingOverhead(key.string().size()));
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
    _memoryTracker->increaseMemoryUsage((currentWriteBatchSize() - beforeSize) +
                                        lockOverhead(key.string().size()) +
                                        indexingOverhead(key.string().size()));
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
  _memoryTracker->increaseMemoryUsage(currentWriteBatchSize() - beforeSize);
}

void RocksDBTrxBaseMethods::SetSavePoint() {
  TRI_ASSERT(_rocksTransaction);
  _rocksTransaction->SetSavePoint();
  _memoryTracker->setSavePoint();
}

rocksdb::Status RocksDBTrxBaseMethods::RollbackToSavePoint() {
  TRI_ASSERT(_rocksTransaction);
  rocksdb::Status s = _rocksTransaction->RollbackToSavePoint();
  if (s.ok()) {
    _memoryTracker->rollbackToSavePoint();
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
    _memoryTracker->popSavePoint();
  }
}

void RocksDBTrxBaseMethods::beginQuery(ResourceMonitor* resourceMonitor,
                                       bool /*isModificationQuery*/) {
  _memoryTracker->beginQuery(resourceMonitor);
}

void RocksDBTrxBaseMethods::endQuery(bool /*isModificationQuery*/) noexcept {
  _memoryTracker->endQuery();
}

void RocksDBTrxBaseMethods::cleanupTransaction() {
  delete _rocksTransaction;
  _rocksTransaction = nullptr;
  _memoryTracker->reset();
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
      auto& selector =
          _state->vocbase().server().getFeature<EngineSelectorFeature>();
      auto& engine = selector.engine<RocksDBEngine>();
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

  _memoryTracker->reset();

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
    auto& selector =
        _state->vocbase().server().getFeature<EngineSelectorFeature>();
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
  return {};
}

rocksdb::Status RocksDBTrxBaseMethods::GetFromSnapshot(
    rocksdb::ColumnFamilyHandle* family, rocksdb::Slice const& slice,
    rocksdb::PinnableSlice* pinnable, ReadOwnWrites rw,
    rocksdb::Snapshot const* snapshot) {
  auto oldSnapshot = _readOptions.snapshot;
  auto restoreSnapshot = absl::Cleanup{
      [oldSnapshot, this]() { _readOptions.snapshot = oldSnapshot; }};
  _readOptions.snapshot = snapshot;
  return Get(family, slice, pinnable, rw);
}

size_t RocksDBTrxBaseMethods::indexingOverhead(size_t keySize) const noexcept {
  if (_indexingDisabled) {
    return 0;
  }
  return keySize + fixedIndexingEntryOverhead;
}

size_t RocksDBTrxBaseMethods::lockOverhead(size_t keySize) const noexcept {
  if (_state->isOnlyExclusiveTransaction()) {
    // current transaction is exclusive and thus does not acquire
    // per-key locks
    return 0;
  }
  // assumed overhead of the lock we acquired. note that RocksDB does not
  // report back here whether the current transaction had already
  // acquired the lock before. in that case it will still return ok().
  // because we do not want to track the acquired locks here in addition,
  // we simply assume here that for every invocation of this function we
  // acquire an additional lock.
  // each lock entry contains at least the string with the key. the
  // string may use SSO to store the key, but we don't want to dive into
  // the internals of std::string here. for storing the key, we assume
  // that we need to store at least the size of an std::string, or the
  // size of the key, whatever is larger. as locked keys are stored in a
  // hash table, we also need to assume overhead (as the hash table will
  // always have a load factor < 100%).
  return std::max(sizeof(std::string), keySize) + fixedLockEntryOverhead;
}

size_t RocksDBTrxBaseMethods::currentWriteBatchSize() const noexcept {
  TRI_ASSERT(_rocksTransaction);
  return _rocksTransaction->GetWriteBatch()->GetWriteBatch()->GetDataSize();
}
