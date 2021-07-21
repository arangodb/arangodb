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

#include "RocksDBReadonlyMethods.h"

#include "RocksDBEngine/RocksDBTransactionState.h"

#include <rocksdb/db.h>
#include <rocksdb/status.h>

using namespace arangodb;

RocksDBReadOnlyMethods::RocksDBReadOnlyMethods(RocksDBTransactionState* state,
                                               rocksdb::TransactionDB* db)
    : RocksDBTransactionMethods(state), _db(db) {
  TRI_ASSERT(_db != nullptr);
  _readOptions.prefix_same_as_start = true;  // should always be true
  _readOptions.fill_cache = _state->options().fillBlockCache;
}

RocksDBReadOnlyMethods::~RocksDBReadOnlyMethods() {
  releaseSnapshot();
}

Result RocksDBReadOnlyMethods::beginTransaction() {
  TRI_ASSERT(_readOptions.snapshot == nullptr);
  if (!_state->isSingleOperation()) {
    _readOptions.snapshot = _db->GetSnapshot();  // must call ReleaseSnapshot later
  }
  return {};
}

Result RocksDBReadOnlyMethods::commitTransaction() {
  releaseSnapshot();
  return {};
}

Result RocksDBReadOnlyMethods::abortTransaction() {
  releaseSnapshot();
  return {};
}

rocksdb::ReadOptions RocksDBReadOnlyMethods::iteratorReadOptions() const {
  return _readOptions;
}

/// @brief acquire a database snapshot if we do not yet have one.
/// Returns true if a snapshot was acquired, otherwise false (i.e., if we already had a snapshot)
bool RocksDBReadOnlyMethods::ensureSnapshot() {
  if (_readOptions.snapshot == nullptr) {
    TRI_ASSERT(_state->isSingleOperation());
    _readOptions.snapshot = _db->GetSnapshot();
    return true;
  }
  return false;
}

rocksdb::SequenceNumber RocksDBReadOnlyMethods::GetSequenceNumber() const {
  if (_readOptions.snapshot) {
    return _readOptions.snapshot->GetSequenceNumber();
  }
  return _db->GetLatestSequenceNumber();
}
  
void RocksDBReadOnlyMethods::prepareOperation(DataSourceId cid, RevisionId rid,
                                              TRI_voc_document_operation_e operationType) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);                                          
}

void RocksDBReadOnlyMethods::rollbackOperation(TRI_voc_document_operation_e operationType) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);                                          
}

Result RocksDBReadOnlyMethods::addOperation(DataSourceId collectionId, RevisionId revisionId,
                                            TRI_voc_document_operation_e opType,
                                            bool& hasPerformedIntermediateCommit) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);                                          
}
                      
rocksdb::Status RocksDBReadOnlyMethods::Get(rocksdb::ColumnFamilyHandle* cf,
                                            rocksdb::Slice const& key,
                                            rocksdb::PinnableSlice* val) {
  TRI_ASSERT(cf != nullptr);
  rocksdb::ReadOptions const& ro = _readOptions;
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
                                            RocksDBKey const&,
                                            rocksdb::Slice const&, bool) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
}

rocksdb::Status RocksDBReadOnlyMethods::PutUntracked(rocksdb::ColumnFamilyHandle* cf,
                                                     RocksDBKey const&,
                                                     rocksdb::Slice const&) {
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
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "invalid iterator in RocksDBReadOnlyMethods");
  }
  return iterator;
}

void RocksDBReadOnlyMethods::releaseSnapshot() {
  if (_readOptions.snapshot != nullptr) {
    TRI_ASSERT(_state->isReadOnlyTransaction() ||
               _state->hasHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS));
    _db->ReleaseSnapshot(_readOptions.snapshot); 
    _readOptions.snapshot = nullptr;
  }
}
