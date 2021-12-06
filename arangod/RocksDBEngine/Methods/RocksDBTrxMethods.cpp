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
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Statistics/ServerStatistics.h"
#include "Metrics/Counter.h"

#include <rocksdb/utilities/write_batch_with_index.h>

using namespace arangodb;

RocksDBTrxMethods::RocksDBTrxMethods(RocksDBTransactionState* state, rocksdb::TransactionDB* db)
    : RocksDBTrxBaseMethods(state, db) {
  TRI_ASSERT(!_state->isSingleOperation());
}

RocksDBTrxMethods::~RocksDBTrxMethods() {
  // cleanupTransaction is a virtual function, so we need to override the dtor and
  // call it ourselves here, because the call in the base class would only execute
  // the base class implementation, therefore leaking the iteratorReadSnapshot.
  cleanupTransaction();
}

Result RocksDBTrxMethods::beginTransaction() {
  Result result = RocksDBTrxBaseMethods::beginTransaction();

  TRI_ASSERT(_iteratorReadSnapshot == nullptr);
  if (result.ok() && hasIntermediateCommitsEnabled()) {
    TRI_ASSERT(_state->options().intermediateCommitCount != UINT64_MAX ||
               _state->options().intermediateCommitSize != UINT64_MAX);
    _iteratorReadSnapshot = _db->GetSnapshot();  // must call ReleaseSnapshot later
    TRI_ASSERT(_iteratorReadSnapshot != nullptr);
  }

  if (_state->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
    TRI_ASSERT(_readWriteBatch == nullptr && !_ownsReadWriteBatch);
    _readWriteBatch = _rocksTransaction->GetWriteBatch();
  }
  return result;
}

rocksdb::ReadOptions RocksDBTrxMethods::iteratorReadOptions() const {
  if (hasIntermediateCommitsEnabled()) {
    rocksdb::ReadOptions ro = _readOptions;
    TRI_ASSERT(_iteratorReadSnapshot);
    ro.snapshot = _iteratorReadSnapshot;
    return ro;
  }
  return _readOptions;
}

void RocksDBTrxMethods::prepareOperation(DataSourceId cid, RevisionId rid,
                                         TRI_voc_document_operation_e operationType) {
  TRI_ASSERT(_rocksTransaction != nullptr);
  if (operationType == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    RocksDBLogValue logValue = RocksDBLogValue::DocumentRemoveV2(rid);
    _rocksTransaction->PutLogData(logValue.slice());
    ++_numLogdata;
  }
}

/// @brief undo the effects of the previous prepareOperation call
void RocksDBTrxMethods::rollbackOperation(TRI_voc_document_operation_e operationType) {
  ++_numRollbacks;
  if (operationType == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    TRI_ASSERT(_numLogdata > 0);
    --_numLogdata;
  }
}

Result RocksDBTrxMethods::checkIntermediateCommit(bool& hasPerformedIntermediateCommit) {
  // perform an intermediate commit if necessary
  size_t currentSize = _rocksTransaction->GetWriteBatch()->GetWriteBatch()->GetDataSize();
  return checkIntermediateCommit(currentSize, hasPerformedIntermediateCommit);
}

rocksdb::Status RocksDBTrxMethods::Get(rocksdb::ColumnFamilyHandle* cf,
                                       rocksdb::Slice const& key,
                                       rocksdb::PinnableSlice* val,
                                       ReadOwnWrites readOwnWrites) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  rocksdb::ReadOptions const& ro = _readOptions;
  if (readOwnWrites == ReadOwnWrites::no) {
    if (_readWriteBatch) {
      return _readWriteBatch->GetFromBatchAndDB(_db, ro, cf, key, val);
    } else {
      return _db->Get(ro, cf, key, val);
    }
  }
  return _rocksTransaction->Get(ro, cf, key, val);
}

std::unique_ptr<rocksdb::Iterator> RocksDBTrxMethods::NewIterator(
    rocksdb::ColumnFamilyHandle* cf, ReadOptionsCallback readOptionsCallback) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);

  ReadOptions opts = _readOptions;
  if (hasIntermediateCommitsEnabled()) {
    TRI_ASSERT(_iteratorReadSnapshot);
    opts.snapshot = _iteratorReadSnapshot;
  }
  if (readOptionsCallback) {
    readOptionsCallback(opts);
  }

  std::unique_ptr<rocksdb::Iterator> iterator;
  if (opts.readOwnWrites) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // Even though the assertion is only evaluated in maintainer mode, it must at
    // least compile. But since checkIntermediateCommits is only defined in maintainer
    // mode, we have to wrap this assert in another ifdef.
    TRI_ASSERT(!opts.checkIntermediateCommits || !hasIntermediateCommitsEnabled());
#endif
    iterator.reset(_rocksTransaction->GetIterator(opts, cf));
  } else {
    if (iteratorMustCheckBounds(ReadOwnWrites::no)) {
      iterator.reset(_readWriteBatch->NewIteratorWithBase(cf, _db->NewIterator(opts, cf)));
    } else {
      // we either have an empty _readWriteBatch or no _readWriteBatch at all
      // in this case, we can get away with the DB snapshot iterator
      iterator.reset(_db->NewIterator(opts, cf));
    }
  }

  if (iterator == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid iterator in RocksDBTrxMethods");
  }
  return iterator;
}

void RocksDBTrxMethods::cleanupTransaction() {
  RocksDBTrxBaseMethods::cleanupTransaction();
  if (_iteratorReadSnapshot != nullptr) {
    TRI_ASSERT(hasIntermediateCommitsEnabled());
    _db->ReleaseSnapshot(_iteratorReadSnapshot);
    _iteratorReadSnapshot = nullptr;
  }
  releaseReadWriteBatch();
}

void RocksDBTrxMethods::createTransaction() {
  RocksDBTrxBaseMethods::createTransaction();
  // add transaction begin marker
  auto header =
      RocksDBLogValue::BeginTransaction(_state->vocbase().id(), _state->id());

  _rocksTransaction->PutLogData(header.slice());
  TRI_ASSERT(_numLogdata == 0);
  ++_numLogdata;
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

  Result res = doCommit();
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
  TRI_ASSERT(_iteratorReadSnapshot != nullptr);
  TRI_ASSERT(_readOptions.snapshot != nullptr);
  return TRI_ERROR_NO_ERROR;
}

Result RocksDBTrxMethods::checkIntermediateCommit(uint64_t newSize, bool& hasPerformedIntermediateCommit) {
  hasPerformedIntermediateCommit = false;

  TRI_IF_FAILURE("noIntermediateCommits") {
    return TRI_ERROR_NO_ERROR;
  }

  if (hasIntermediateCommitsEnabled()) {
    auto numOperations = this->numOperations();
    // perform an intermediate commit
    // this will be done if either the "number of operations" or the
    // "transaction size" counters have reached their limit
    if (_state->options().intermediateCommitCount <= numOperations ||
        _state->options().intermediateCommitSize <= newSize) {
      return triggerIntermediateCommit(hasPerformedIntermediateCommit);
    }
  }
  return TRI_ERROR_NO_ERROR;
}

bool RocksDBTrxMethods::hasIntermediateCommitsEnabled() const noexcept {
  return _state->hasHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
}

/// @brief whether or not a RocksDB iterator in this transaction must check its
/// bounds during iteration in addition to setting iterator_lower_bound or
/// iterate_upper_bound. this is currently true for all iterators that are based
/// on in-flight writes of the current transaction. for read-only transactions it
/// is only necessary to check bounds if we have local changes in the WriteBatch.
bool RocksDBTrxMethods::iteratorMustCheckBounds(ReadOwnWrites readOwnWrites) const {
  // If we have a non-empty _readWriteBatch we always need to check the bounds,
  // because we need to consider the WriteBatch for read operations, even if
  // we don't need to read own writes.
  return readOwnWrites == ReadOwnWrites::yes || (_readWriteBatch != nullptr && _readWriteBatch->GetWriteBatch()->GetDataSize() > 0);
}

void RocksDBTrxMethods::beginQuery(bool isModificationQuery) {
  if (!_state->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
    // don't bother with query tracking in non globally managed trx
    return;
  }

  if (isModificationQuery) {
    TRI_ASSERT(_hasActiveModificationQuery.load() == false && _numActiveReadOnlyQueries.load() == 0);
    if (_numActiveReadOnlyQueries.load(std::memory_order_relaxed) > 0) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
        "cannot run modification query and read-only query concurrently on the same transaction");
    }
    if (_hasActiveModificationQuery.load(std::memory_order_relaxed) ||
        _hasActiveModificationQuery.exchange(true, std::memory_order_relaxed)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
        "cannot run concurrent modification queries in the same transaction");
    }
    TRI_ASSERT(_hasActiveModificationQuery.load());
    TRI_ASSERT(_ownsReadWriteBatch == false);
    initializeReadWriteBatch();
  } else {
    TRI_ASSERT(_ownsReadWriteBatch == false);
    TRI_ASSERT(_hasActiveModificationQuery.load() == false);
    if (_hasActiveModificationQuery.load(std::memory_order_relaxed)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
        "cannot run modification query and read-only query concurrently on the same transaction");
    }
    _numActiveReadOnlyQueries.fetch_add(1, std::memory_order_relaxed);
  }
}

void RocksDBTrxMethods::endQuery(bool isModificationQuery) noexcept {
  if (!_state->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
    // don't bother with query tracking in non globally managed trx
    return;
  }

  if (isModificationQuery) {
    TRI_ASSERT(_readWriteBatch != nullptr);
    TRI_ASSERT(_hasActiveModificationQuery.load());
    TRI_ASSERT(_numActiveReadOnlyQueries.load() == 0);
    _hasActiveModificationQuery.store(false, std::memory_order_relaxed);
    releaseReadWriteBatch();
    TRI_ASSERT(_readWriteBatch == nullptr && !_ownsReadWriteBatch);
    _readWriteBatch = _rocksTransaction->GetWriteBatch();
  } else {
    TRI_ASSERT(_ownsReadWriteBatch == false);
    TRI_ASSERT(_hasActiveModificationQuery.load() == false);
    TRI_ASSERT(_numActiveReadOnlyQueries.load() > 0);
    _numActiveReadOnlyQueries.fetch_sub(1, std::memory_order_relaxed);
  }
}

void RocksDBTrxMethods::initializeReadWriteBatch() {
  TRI_ASSERT(_ownsReadWriteBatch == false);
  _readWriteBatch = new rocksdb::WriteBatchWithIndex(rocksdb::BytewiseComparator(), 0, true, 0);
  _ownsReadWriteBatch = true;

  struct WBCloner final : public rocksdb::WriteBatch::Handler {
    explicit WBCloner(rocksdb::WriteBatchWithIndex& target)
        : wbwi(target) {}

    rocksdb::Status PutCF(uint32_t column_family_id, const rocksdb::Slice& key,
                          const rocksdb::Slice& value) override {
      return wbwi.Put(familyId(column_family_id), key, value);
    }

    rocksdb::Status DeleteCF(uint32_t column_family_id, const rocksdb::Slice& key) override {
      return wbwi.Delete(familyId(column_family_id), key);
    }

    rocksdb::Status SingleDeleteCF(uint32_t column_family_id, const rocksdb::Slice& key) override {
      return wbwi.SingleDelete(familyId(column_family_id), key);
    }

    rocksdb::Status DeleteRangeCF(uint32_t column_family_id, const rocksdb::Slice& begin_key,
                                  const rocksdb::Slice& end_key) override {
      return wbwi.DeleteRange(familyId(column_family_id), begin_key, end_key);
    }

    rocksdb::Status MergeCF(uint32_t column_family_id, const rocksdb::Slice& key,
                            const rocksdb::Slice& value) override {
      // should never be used by our code
      TRI_ASSERT(false);
      return wbwi.Merge(familyId(column_family_id), key, value);
    }

    void LogData(const rocksdb::Slice& blob) override {
      wbwi.PutLogData(blob);
    }
  
    rocksdb::Status MarkBeginPrepare(bool = false) override {
      TRI_ASSERT(false);
      return rocksdb::Status::InvalidArgument("MarkBeginPrepare() handler not defined.");
    }
  
    rocksdb::Status MarkEndPrepare(rocksdb::Slice const& /*xid*/) override {
      TRI_ASSERT(false);
      return rocksdb::Status::InvalidArgument("MarkEndPrepare() handler not defined.");
    }
    
    rocksdb::Status MarkNoop(bool /*empty_batch*/) override {
      return rocksdb::Status::OK();
    }
    
    rocksdb::Status MarkRollback(rocksdb::Slice const& /*xid*/) override {
      TRI_ASSERT(false);
      return rocksdb::Status::InvalidArgument(
          "MarkRollbackPrepare() handler not defined.");
    }
    
    rocksdb::Status MarkCommit(rocksdb::Slice const& /*xid*/) override {
      TRI_ASSERT(false);
      return rocksdb::Status::InvalidArgument("MarkCommit() handler not defined.");
    }

    rocksdb::ColumnFamilyHandle* familyId(uint32_t id) {
      for (auto const& it : RocksDBColumnFamilyManager::allHandles()) {
        if (it->GetID() == id) {
          return it;
        }
      }
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unknown column family id");
    }

    rocksdb::WriteBatchWithIndex& wbwi;
  } cloner(*_readWriteBatch);

  // get the transaction's current WriteBatch
  TRI_ASSERT(_rocksTransaction != nullptr);
  rocksdb::WriteBatch* wb = _rocksTransaction->GetWriteBatch()->GetWriteBatch();
  rocksdb::Status s = wb->Iterate(&cloner);

  if (!s.ok()) {
    THROW_ARANGO_EXCEPTION(rocksutils::convertStatus(s));
  }
}

void RocksDBTrxMethods::releaseReadWriteBatch() noexcept {
  if (_ownsReadWriteBatch) {
    delete _readWriteBatch;
    _readWriteBatch = nullptr;
    _ownsReadWriteBatch = false;
  }
}
