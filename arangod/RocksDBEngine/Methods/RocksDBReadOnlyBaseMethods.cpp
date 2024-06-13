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

#include "RocksDBReadOnlyBaseMethods.h"

#include "RocksDBEngine/RocksDBTransactionState.h"

#include <absl/cleanup/cleanup.h>
#include <rocksdb/db.h>
#include <rocksdb/status.h>

using namespace arangodb;

RocksDBReadOnlyBaseMethods::RocksDBReadOnlyBaseMethods(
    RocksDBTransactionState* state, rocksdb::TransactionDB* db)
    : RocksDBTransactionMethods(state), _db(db) {
  TRI_ASSERT(_db != nullptr);
  _readOptions.prefix_same_as_start = true;  // should always be true
  _readOptions.fill_cache = _state->options().fillBlockCache;
}

RocksDBReadOnlyBaseMethods::~RocksDBReadOnlyBaseMethods() { releaseSnapshot(); }

/// @brief acquire a database snapshot if we do not yet have one.
/// Returns true if a snapshot was acquired, otherwise false (i.e., if we
/// already had a snapshot)
bool RocksDBReadOnlyBaseMethods::ensureSnapshot() {
  if (_readOptions.snapshot == nullptr) {
    _readOptions.snapshot = _db->GetSnapshot();
    return true;
  }
  return false;
}

rocksdb::SequenceNumber RocksDBReadOnlyBaseMethods::GetSequenceNumber()
    const noexcept {
  if (_readOptions.snapshot) {
    return _readOptions.snapshot->GetSequenceNumber();
  }
  return _db->GetLatestSequenceNumber();
}

void RocksDBReadOnlyBaseMethods::releaseSnapshot() {
  if (_readOptions.snapshot != nullptr) {
    _db->ReleaseSnapshot(_readOptions.snapshot);
    _readOptions.snapshot = nullptr;
  }
}

void RocksDBReadOnlyBaseMethods::prepareOperation(
    DataSourceId cid, RevisionId rid,
    TRI_voc_document_operation_e operationType) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

void RocksDBReadOnlyBaseMethods::rollbackOperation(
    TRI_voc_document_operation_e operationType) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

Result RocksDBReadOnlyBaseMethods::addOperation(
    TRI_voc_document_operation_e opType) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

rocksdb::Status RocksDBReadOnlyBaseMethods::GetForUpdate(
    rocksdb::ColumnFamilyHandle* cf, rocksdb::Slice const& key,
    rocksdb::PinnableSlice* val) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

rocksdb::Status RocksDBReadOnlyBaseMethods::Put(rocksdb::ColumnFamilyHandle* cf,
                                                RocksDBKey const&,
                                                rocksdb::Slice const&, bool) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

rocksdb::Status RocksDBReadOnlyBaseMethods::PutUntracked(
    rocksdb::ColumnFamilyHandle* cf, RocksDBKey const&, rocksdb::Slice const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

rocksdb::Status RocksDBReadOnlyBaseMethods::Delete(
    rocksdb::ColumnFamilyHandle* cf, RocksDBKey const& key) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

rocksdb::Status RocksDBReadOnlyBaseMethods::SingleDelete(
    rocksdb::ColumnFamilyHandle*, RocksDBKey const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

rocksdb::Status arangodb::RocksDBReadOnlyBaseMethods::SingleGet(
    rocksdb::Snapshot const* snapshot, rocksdb::ColumnFamilyHandle& family,
    rocksdb::Slice const& key, rocksdb::PinnableSlice& value) {
  absl::Cleanup restore = [&, was = _readOptions.snapshot] {
    _readOptions.snapshot = was;
  };
  _readOptions.snapshot = snapshot;

  return _db->Get(_readOptions, &family, key, &value);
}

void RocksDBReadOnlyBaseMethods::MultiGet(rocksdb::Snapshot const* snapshot,
                                          rocksdb::ColumnFamilyHandle& family,
                                          size_t count,
                                          rocksdb::Slice const* keys,
                                          rocksdb::PinnableSlice* values,
                                          rocksdb::Status* statuses) {
  // make a copy of the ReadOptions, as we are going to modify the snapshot
  ReadOptions ro = _readOptions;
  ro.snapshot = snapshot;

  // Timestamps and multiple ColumnFamilies are not necessary for us
  _db->MultiGet(ro, &family, count, keys, values, statuses, false);
}

void RocksDBReadOnlyBaseMethods::MultiGet(rocksdb::ColumnFamilyHandle& family,
                                          size_t count,
                                          rocksdb::Slice const* keys,
                                          rocksdb::PinnableSlice* values,
                                          rocksdb::Status* statuses,
                                          ReadOwnWrites) {
  _db->MultiGet(_readOptions, &family, count, keys, values, statuses, false);
}

void RocksDBReadOnlyBaseMethods::PutLogData(rocksdb::Slice const& blob) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}
