////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBMethods.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Transaction/Methods.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/status.h>
#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/utilities/transaction.h>
#include <rocksdb/utilities/write_batch_with_index.h>

using namespace arangodb;

// ================= RocksDBSavePoint ==================

RocksDBSavePoint::RocksDBSavePoint(transaction::Methods* trx,
                                   TRI_voc_document_operation_e operationType)
    : _trx(trx),
      _operationType(operationType),
      _handled(_trx->isSingleOperationTransaction()) {
  TRI_ASSERT(trx != nullptr);
  if (!_handled) {
    auto mthds = RocksDBTransactionState::toMethods(_trx);
    // only create a savepoint when necessary
    mthds->SetSavePoint();
  }
}

RocksDBSavePoint::~RocksDBSavePoint() {
  if (!_handled) {
    try {
      // only roll back if we create a savepoint and have
      // not performed an intermediate commit in-between
      rollback();
    } catch (std::exception const& ex) {
      LOG_TOPIC("519ed", ERR, Logger::ENGINES)
          << "caught exception during rollback to savepoint: " << ex.what();
    } catch (...) {
      // whatever happens during rollback, no exceptions are allowed to escape
      // from here
    }
  }
}

void RocksDBSavePoint::finish(bool hasPerformedIntermediateCommit) {
  if (!_handled && !hasPerformedIntermediateCommit) {
    // pop the savepoint from the transaction in order to
    // save some memory for transactions with many operations
    // this is only safe to do when we have a created a savepoint
    // when creating the guard, and when there hasn't been an
    // intermediate commit in the transaction
    // when there has been an intermediate commit, we must
    // leave the savepoint alone, because it belonged to another
    // transaction, and the current transaction will not have any
    // savepoint
    auto mthds = RocksDBTransactionState::toMethods(_trx);
    mthds->PopSavePoint();
  }

  // this will prevent the rollback call in the destructor
  _handled = true;
}

void RocksDBSavePoint::rollback() {
  TRI_ASSERT(!_handled);
  auto mthds = RocksDBTransactionState::toMethods(_trx);
  mthds->RollbackToSavePoint();

  auto state = RocksDBTransactionState::toState(_trx);
  state->rollbackOperation(_operationType);

  _handled = true;  // in order to not roll back again by accident
}

// =================== RocksDBMethods ===================

rocksdb::ReadOptions RocksDBMethods::iteratorReadOptions() {
  if (_state->hasHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS)) {
    rocksdb::ReadOptions ro = _state->_rocksReadOptions;
    TRI_ASSERT(_state->_readSnapshot);
    ro.snapshot = _state->_readSnapshot;
    return ro;
  }
  return _state->_rocksReadOptions;
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
std::size_t RocksDBMethods::countInBounds(RocksDBKeyBounds const& bounds, bool isElementInRange) {
  std::size_t count = 0;

  // iterator is from read only / trx / writebatch
  std::unique_ptr<rocksdb::Iterator> iter =
      this->NewIterator(iteratorReadOptions(), bounds.columnFamily());
  iter->Seek(bounds.start());
  auto end = bounds.end();
  rocksdb::Comparator const* cmp = bounds.columnFamily()->GetComparator();

  // extra check to avoid extra comparisons with isElementInRage later;
  if (iter->Valid() && cmp->Compare(iter->key(), end) < 0) {
    ++count;
    if (isElementInRange) {
      return count;
    }
    iter->Next();
  }

  while (iter->Valid() && cmp->Compare(iter->key(), end) < 0) {
    iter->Next();
    ++count;
  }
  return count;
};
#endif

// =================== RocksDBReadOnlyMethods ====================

RocksDBReadOnlyMethods::RocksDBReadOnlyMethods(RocksDBTransactionState* state)
    : RocksDBMethods(state) {
  _db = rocksutils::globalRocksDB();
}

rocksdb::Status RocksDBReadOnlyMethods::Get(rocksdb::ColumnFamilyHandle* cf,
                                            rocksdb::Slice const& key,
                                            rocksdb::PinnableSlice* val) {
  TRI_ASSERT(cf != nullptr);
  rocksdb::ReadOptions const& ro = _state->_rocksReadOptions;
  TRI_ASSERT(ro.snapshot != nullptr ||
             (_state->isReadOnlyTransaction() && _state->isSingleOperation()));
  return _db->Get(ro, cf, key, val);
}

rocksdb::Status RocksDBReadOnlyMethods::GetForUpdate(rocksdb::ColumnFamilyHandle* cf,
                                                     rocksdb::Slice const& key,
                                                     rocksdb::PinnableSlice* val) {
  return this->Get(cf, key, val);
}

rocksdb::Status RocksDBReadOnlyMethods::Put(rocksdb::ColumnFamilyHandle* cf,
                                            RocksDBKey const&, rocksdb::Slice const&, bool) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

rocksdb::Status RocksDBReadOnlyMethods::PutUntracked(rocksdb::ColumnFamilyHandle* cf,
                                            RocksDBKey const&, rocksdb::Slice const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

rocksdb::Status RocksDBReadOnlyMethods::Delete(rocksdb::ColumnFamilyHandle* cf,
                                               RocksDBKey const& key) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

rocksdb::Status RocksDBReadOnlyMethods::SingleDelete(rocksdb::ColumnFamilyHandle*,
                                                     RocksDBKey const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

void RocksDBReadOnlyMethods::PutLogData(rocksdb::Slice const& blob) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

std::unique_ptr<rocksdb::Iterator> RocksDBReadOnlyMethods::NewIterator(
    rocksdb::ReadOptions const& opts, rocksdb::ColumnFamilyHandle* cf) {
  TRI_ASSERT(cf != nullptr);
  return std::unique_ptr<rocksdb::Iterator>(_db->NewIterator(opts, cf));
}

// =================== RocksDBTrxMethods ====================

bool RocksDBTrxMethods::DisableIndexing() {
  if (!_indexingDisabled) {
    TRI_ASSERT(_state != nullptr);
    _state->_rocksTransaction->DisableIndexing();
    _indexingDisabled = true;
    return true;
  }
  return false;
}

bool RocksDBTrxMethods::EnableIndexing() {
  if (_indexingDisabled) {
    _state->_rocksTransaction->EnableIndexing();
    _indexingDisabled = false;
    return true;
  }
  return false;
}

RocksDBTrxMethods::RocksDBTrxMethods(RocksDBTransactionState* state)
    : RocksDBMethods(state), _indexingDisabled(false) {
  TRI_ASSERT(_state != nullptr);
}

rocksdb::Status RocksDBTrxMethods::Get(rocksdb::ColumnFamilyHandle* cf,
                                       rocksdb::Slice const& key,
                                       rocksdb::PinnableSlice* val) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->_rocksTransaction);
  rocksdb::ReadOptions const& ro = _state->_rocksReadOptions;
  TRI_ASSERT(ro.snapshot != nullptr);
  return _state->_rocksTransaction->Get(ro, cf, key, val);
}

rocksdb::Status RocksDBTrxMethods::GetForUpdate(rocksdb::ColumnFamilyHandle* cf,
                                                rocksdb::Slice const& key,
                                                rocksdb::PinnableSlice* val) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->_rocksTransaction);
  rocksdb::ReadOptions const& ro = _state->_rocksReadOptions;
  TRI_ASSERT(ro.snapshot != nullptr);
  return _state->_rocksTransaction->GetForUpdate(ro, cf, key, val);
}

rocksdb::Status RocksDBTrxMethods::Put(rocksdb::ColumnFamilyHandle* cf,
                                       RocksDBKey const& key,
                                       rocksdb::Slice const& val,
                                       bool assume_tracked) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->_rocksTransaction);
  return _state->_rocksTransaction->Put(cf, key.string(), val, assume_tracked);
}

rocksdb::Status RocksDBTrxMethods::PutUntracked(rocksdb::ColumnFamilyHandle* cf,
                                       RocksDBKey const& key, rocksdb::Slice const& val) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->_rocksTransaction);
  return _state->_rocksTransaction->PutUntracked(cf, key.string(), val);
}

rocksdb::Status RocksDBTrxMethods::Delete(rocksdb::ColumnFamilyHandle* cf,
                                          RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->_rocksTransaction);
  return _state->_rocksTransaction->Delete(cf, key.string());
}

rocksdb::Status RocksDBTrxMethods::SingleDelete(rocksdb::ColumnFamilyHandle* cf,
                                                RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->_rocksTransaction);
  return _state->_rocksTransaction->SingleDelete(cf, key.string());
}

void RocksDBTrxMethods::PutLogData(rocksdb::Slice const& blob) {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->_rocksTransaction);
  _state->_rocksTransaction->PutLogData(blob);
}

std::unique_ptr<rocksdb::Iterator> RocksDBTrxMethods::NewIterator(
    rocksdb::ReadOptions const& opts, rocksdb::ColumnFamilyHandle* cf) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->_rocksTransaction);
  return std::unique_ptr<rocksdb::Iterator>(_state->_rocksTransaction->GetIterator(opts, cf));
}

void RocksDBTrxMethods::SetSavePoint() {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->_rocksTransaction);
  _state->_rocksTransaction->SetSavePoint();
}

rocksdb::Status RocksDBTrxMethods::RollbackToSavePoint() {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->_rocksTransaction);
  return _state->_rocksTransaction->RollbackToSavePoint();
}

void RocksDBTrxMethods::PopSavePoint() {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->_rocksTransaction);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  rocksdb::Status s = _state->_rocksTransaction->PopSavePoint();
  TRI_ASSERT(s.ok());
#else
  _state->_rocksTransaction->PopSavePoint();
#endif
}

// =================== RocksDBBatchedMethods ====================

RocksDBBatchedMethods::RocksDBBatchedMethods(RocksDBTransactionState* state,
                                             rocksdb::WriteBatch* wb)
    : RocksDBMethods(state), _wb(wb) {}

rocksdb::Status RocksDBBatchedMethods::Get(rocksdb::ColumnFamilyHandle* cf,
                                           rocksdb::Slice const& key,
                                           rocksdb::PinnableSlice* val) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "BatchedMethods does not provide Get");
}

rocksdb::Status RocksDBBatchedMethods::GetForUpdate(rocksdb::ColumnFamilyHandle* cf,
                                                    rocksdb::Slice const& key,
                                                    rocksdb::PinnableSlice* val) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "BatchedMethods does not provide GetForUpdate");
}

rocksdb::Status RocksDBBatchedMethods::Put(rocksdb::ColumnFamilyHandle* cf,
                                           RocksDBKey const& key,
                                           rocksdb::Slice const& val,
                                           bool assume_tracked) {
  TRI_ASSERT(cf != nullptr);
  return _wb->Put(cf, key.string(), val);
}

rocksdb::Status RocksDBBatchedMethods::PutUntracked(rocksdb::ColumnFamilyHandle* cf,
                                                    RocksDBKey const& key,
                                                    rocksdb::Slice const& val) {
  return RocksDBBatchedMethods::Put(cf, key, val, /*assume_tracked*/false);
}

rocksdb::Status RocksDBBatchedMethods::Delete(rocksdb::ColumnFamilyHandle* cf,
                                              RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  return _wb->Delete(cf, key.string());
}

rocksdb::Status RocksDBBatchedMethods::SingleDelete(rocksdb::ColumnFamilyHandle* cf,
                                                    RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  return _wb->SingleDelete(cf, key.string());
}

void RocksDBBatchedMethods::PutLogData(rocksdb::Slice const& blob) {
  _wb->PutLogData(blob);
}

std::unique_ptr<rocksdb::Iterator> RocksDBBatchedMethods::NewIterator(
    rocksdb::ReadOptions const&, rocksdb::ColumnFamilyHandle*) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "BatchedMethods does not provide NewIterator");
}

// =================== RocksDBBatchedWithIndexMethods ====================

RocksDBBatchedWithIndexMethods::RocksDBBatchedWithIndexMethods(RocksDBTransactionState* state,
                                                               rocksdb::WriteBatchWithIndex* wb)
    : RocksDBMethods(state), _wb(wb) {
  _db = rocksutils::globalRocksDB();
}

rocksdb::Status RocksDBBatchedWithIndexMethods::Get(rocksdb::ColumnFamilyHandle* cf,
                                                    rocksdb::Slice const& key,
                                                    rocksdb::PinnableSlice* val) {
  TRI_ASSERT(cf != nullptr);
  rocksdb::ReadOptions ro;
  return _wb->GetFromBatchAndDB(_db, ro, cf, key, val);
}

rocksdb::Status RocksDBBatchedWithIndexMethods::GetForUpdate(rocksdb::ColumnFamilyHandle* cf,
                                                             rocksdb::Slice const& key,
                                                             rocksdb::PinnableSlice* val) {
  return this->Get(cf, key, val);
}

rocksdb::Status RocksDBBatchedWithIndexMethods::Put(rocksdb::ColumnFamilyHandle* cf,
                                                    RocksDBKey const& key,
                                                    rocksdb::Slice const& val,
                                                    bool assume_tracked) {
  TRI_ASSERT(cf != nullptr);
  return _wb->Put(cf, key.string(), val);
}

rocksdb::Status RocksDBBatchedWithIndexMethods::PutUntracked(rocksdb::ColumnFamilyHandle* cf,
                                                             RocksDBKey const& key,
                                                             rocksdb::Slice const& val) {
  return RocksDBBatchedWithIndexMethods::Put(cf, key, val, /*assume_tracked*/false);
}

rocksdb::Status RocksDBBatchedWithIndexMethods::Delete(rocksdb::ColumnFamilyHandle* cf,
                                                       RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  return _wb->Delete(cf, key.string());
}

rocksdb::Status RocksDBBatchedWithIndexMethods::SingleDelete(rocksdb::ColumnFamilyHandle* cf,
                                                             RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  return _wb->SingleDelete(cf, key.string());
}

void RocksDBBatchedWithIndexMethods::PutLogData(rocksdb::Slice const& blob) {
  _wb->PutLogData(blob);
}

std::unique_ptr<rocksdb::Iterator> RocksDBBatchedWithIndexMethods::NewIterator(
    rocksdb::ReadOptions const& ro, rocksdb::ColumnFamilyHandle* cf) {
  TRI_ASSERT(cf != nullptr);
  return std::unique_ptr<rocksdb::Iterator>(
      _wb->NewIteratorWithBase(_db->NewIterator(ro, cf)));
}
