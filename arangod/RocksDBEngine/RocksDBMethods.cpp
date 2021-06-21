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

#include "RocksDBMethods.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/status.h>
#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/utilities/transaction.h>
#include <rocksdb/utilities/write_batch_with_index.h>

using namespace arangodb;

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
  auto& selector = state->vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  _db = engine.db();
  TRI_ASSERT(_db != nullptr);
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
  
  std::unique_ptr<rocksdb::Iterator> iterator(_db->NewIterator(opts, cf));
  if (iterator == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid iterator in RocksDBReadOnlyMethods");
  }
  return iterator;
}

// =================== RocksDBTrxMethods ====================

RocksDBTrxMethods::RocksDBTrxMethods(RocksDBTransactionState* state)
    : RocksDBMethods(state), _indexingDisabled(false) {
  TRI_ASSERT(_state != nullptr);
}

bool RocksDBTrxMethods::DisableIndexing() {
  if (!_indexingDisabled) {
    TRI_ASSERT(_state != nullptr);
    TRI_ASSERT(_state->_rocksTransaction);
    _state->_rocksTransaction->DisableIndexing();
    _indexingDisabled = true;
    return true;
  }
  return false;
}

bool RocksDBTrxMethods::EnableIndexing() {
  if (_indexingDisabled) {
    TRI_ASSERT(_state->_rocksTransaction);
    _state->_rocksTransaction->EnableIndexing();
    _indexingDisabled = false;
    return true;
  }
  return false;
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

  std::unique_ptr<rocksdb::Iterator> iterator(_state->_rocksTransaction->GetIterator(opts, cf));
  if (iterator == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid iterator in RocksDBTrxMethods");
  }
  return iterator;
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
  auto& selector = state->vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  _db = engine.db();
  TRI_ASSERT(_db != nullptr);
  TRI_ASSERT(_wb != nullptr);
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
  std::unique_ptr<rocksdb::Iterator> iterator(_wb->NewIteratorWithBase(_db->NewIterator(ro, cf)));

  if (iterator == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid iterator in RocksDBBatchedWithIndexMethods");
  }
  return iterator;
}
