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

#include "RocksDBEngine/RocksDBTransactionState.h"

#include <rocksdb/utilities/write_batch_with_index.h>

using namespace arangodb;

RocksDBTrxMethods::RocksDBTrxMethods(RocksDBTransactionState* state, rocksdb::TransactionDB* db)
    : RocksDBTransactionMethods(state), _indexingDisabled(false), _db(db) {
}

RocksDBTrxMethods::~RocksDBTrxMethods() {
  releaseSnapshot();
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

Result RocksDBTrxMethods::beginTransaction() {
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
  releaseSnapshot();
  return {};
}

Result RocksDBTrxMethods::abortTransaction() {
  releaseSnapshot();
  return {};
}

rocksdb::ReadOptions RocksDBTrxMethods::iteratorReadOptions() const {
  if (_state->hasHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS)) {
    rocksdb::ReadOptions ro = _state->_rocksReadOptions;
    TRI_ASSERT(_readSnapshot);
    ro.snapshot = _readSnapshot;
    return ro;
  }
  return _state->_rocksReadOptions;
}

/// @brief acquire a database snapshot if we do not yet have one.
/// Returns true if a snapshot was acquired, otherwise false (i.e., if we already had a snapshot)
bool RocksDBTrxMethods::ensureSnapshot() {
  TRI_ASSERT(_readSnapshot != nullptr);
  return false;
}

rocksdb::SequenceNumber RocksDBTrxMethods::GetSequenceNumber() const {
  if (_readOptions.snapshot) {
    return _readOptions.snapshot->GetSequenceNumber();
  }
  return _db->GetLatestSequenceNumber();
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
                                       rocksdb::Slice const& val, bool assume_tracked) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->_rocksTransaction);
  return _state->_rocksTransaction->Put(cf, key.string(), val, assume_tracked);
}

rocksdb::Status RocksDBTrxMethods::PutUntracked(rocksdb::ColumnFamilyHandle* cf,
                                                RocksDBKey const& key,
                                                rocksdb::Slice const& val) {
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

  std::unique_ptr<rocksdb::Iterator> iterator(
      _state->_rocksTransaction->GetIterator(opts, cf));
  if (iterator == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid iterator in RocksDBTrxMethods");
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

rocksdb::Status RocksDBTrxMethods::RollbackToWriteBatchSavePoint() {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->_rocksTransaction);
  // this deserves some further explanation:
  // we are first trying to get rid of the last changes in the write batch,
  // but we don't want to pay the price for rebuilding the WBWI from scratch
  // with all that remains in the WB.
  // so what we do is the following:
  // we first revert the changes in the WB only. this will truncate the WB
  // to the position of the last SavePoint, and is cheap
  rocksdb::Status s =
      _state->_rocksTransaction->GetWriteBatch()->GetWriteBatch()->RollbackToSavePoint();
  if (s.ok()) {
    // if this succeeds we now add a new SavePoint to the WB. this does nothing,
    // but we need it to have the same number of SavePoints in the WB and the
    // WBWI.
    _state->_rocksTransaction->GetWriteBatch()->GetWriteBatch()->SetSavePoint();

    // finally, we pop off the SavePoint from the WBWI, which will remove the
    // latest changes from the WBWI and the WB (our dummy SavePoint), but it
    // will _not_ rebuild the entire WBWI from the WB
    this->PopSavePoint();
  }
  TRI_ASSERT(s.ok());
  return s;
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

void RocksDBTrxMethods::releaseSnapshot() {
  if (_readSnapshot != nullptr) {
    TRI_ASSERT(_state->isReadOnlyTransaction() ||
               _state->hasHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS));
    _db->ReleaseSnapshot(_readSnapshot); 
    _readSnapshot = nullptr;
  }
}