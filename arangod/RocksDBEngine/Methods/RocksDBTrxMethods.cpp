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

std::unique_ptr<rocksdb::Iterator> RocksDBTrxMethods::NewIterator(
    rocksdb::ColumnFamilyHandle* cf, ReadOptionsCallback readOptionsCallback) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);
  
  rocksdb::ReadOptions opts = _readOptions;
  if (hasIntermediateCommitsEnabled()) {
    TRI_ASSERT(_iteratorReadSnapshot);
    opts.snapshot = _iteratorReadSnapshot;
  }
  if (readOptionsCallback) {
    readOptionsCallback(opts);
  }
  
   std::unique_ptr<rocksdb::Iterator> iterator;
  if (!iteratorMustCheckBounds()) {
    // we are in a write transaction, but only run a single query in it. 
    // in this case, we can get away with the DB snapshot iterator
    iterator.reset(_db->NewIterator(opts, cf));
  } else {
    // in case we have intermediate commits enabled, the transaction's WBWI
    // may get invalidated on an intermediate commit. we must create our own
    // copy of the transaction's WBWI to prevent pointing into invalid memory
    // after an intermediate commit
    rocksdb::WriteBatchWithIndex* wbwi = ensureWriteBatch();
    if (wbwi == nullptr) {
      // when we don't have intermediate commits enabled, we can create an
      // iterator based on the transaction's own WBWI
      iterator.reset(_rocksTransaction->GetIterator(opts, cf));
    } else {
      iterator.reset(wbwi->NewIteratorWithBase(cf, _db->NewIterator(opts, cf)));
    }
  }

  if (iterator == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid iterator in RocksDBTrxMethods");
  }
  return iterator;
}

std::unique_ptr<rocksdb::Iterator> RocksDBTrxMethods::NewReadOwnWritesIterator(
    rocksdb::ColumnFamilyHandle* cf, ReadOptionsCallback readOptionsCallback) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_rocksTransaction);

  rocksdb::ReadOptions opts = _readOptions;
  if (hasIntermediateCommitsEnabled()) {
    TRI_ASSERT(_iteratorReadSnapshot);
    opts.snapshot = _iteratorReadSnapshot;
  }
  if (readOptionsCallback) {
    readOptionsCallback(opts);
  }
  
  std::unique_ptr<rocksdb::Iterator> iterator(_rocksTransaction->GetIterator(opts, cf));
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

void RocksDBTrxMethods::pushQuery(bool responsibleForCommit) {
  // push query information, still without a WriteBatchWithIndex clone
  _queries.emplace_back(responsibleForCommit, nullptr);
}

void RocksDBTrxMethods::popQuery() noexcept {
  TRI_ASSERT(!_queries.empty());
  _queries.pop_back();
}

/// @brief whether or not a RocksDB iterator in this transaction must check its
/// bounds during iteration in addition to setting iterator_lower_bound or
/// iterate_upper_bound. this is currently true for all iterators that are based
/// on in-flight writes of the current transaction. it is never necessary to
/// check bounds for read-only transactions
bool RocksDBTrxMethods::iteratorMustCheckBounds() const {
  if (_queries.size() == 1 && _queries[0].first) {
    // only one query, and it is the main query!
    // in this case, the iterator should always be based on the db snapshot
    return false;
  }

  return true;
}

rocksdb::WriteBatchWithIndex* RocksDBTrxMethods::ensureWriteBatch() {
  if (_queries.empty()) {
    return nullptr;
  }

  auto& top = _queries.back();
  if (top.second == nullptr) {
    top.second = copyWriteBatch();
  }
  TRI_ASSERT(top.second != nullptr);
  return top.second.get();
}

std::unique_ptr<rocksdb::WriteBatchWithIndex> RocksDBTrxMethods::copyWriteBatch() const {
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

    rocksdb::ColumnFamilyHandle* familyId(uint32_t id) {
      for (auto const& it : RocksDBColumnFamilyManager::allHandles()) {
        if (it->GetID() == id) {
          return it;
        }
      }
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unknown column family id");
    }

    rocksdb::WriteBatchWithIndex& wbwi;
  };

  auto wbwi = std::make_unique<rocksdb::WriteBatchWithIndex>(rocksdb::BytewiseComparator(), 0, true, 0);
  WBCloner cloner(*wbwi);
  
  // get the transaction's current WriteBatch
  TRI_ASSERT(_rocksTransaction != nullptr);
  rocksdb::WriteBatch* wb = _rocksTransaction->GetWriteBatch()->GetWriteBatch();
  rocksdb::Status s = wb->Iterate(&cloner);

  if (!s.ok()) {
    THROW_ARANGO_EXCEPTION(rocksutils::convertStatus(s));
  }

  return wbwi;
}
