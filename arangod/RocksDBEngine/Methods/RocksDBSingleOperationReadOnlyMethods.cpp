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

#include "RocksDBSingleOperationReadOnlyMethods.h"

#include <rocksdb/db.h>

#include "RocksDBEngine/RocksDBTransactionState.h"

using namespace arangodb;

RocksDBSingleOperationReadOnlyMethods::RocksDBSingleOperationReadOnlyMethods(
    RocksDBTransactionState* state, rocksdb::TransactionDB* db)
    : RocksDBReadOnlyBaseMethods(state), _db(db) {
  TRI_ASSERT(_db != nullptr);
}

Result RocksDBSingleOperationReadOnlyMethods::beginTransaction() { return {}; }

Result RocksDBSingleOperationReadOnlyMethods::commitTransaction() { return {}; }

Result RocksDBSingleOperationReadOnlyMethods::abortTransaction() { return {}; }

rocksdb::ReadOptions
RocksDBSingleOperationReadOnlyMethods::iteratorReadOptions() const {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "should not call iteratorReadOptions for single operation methods");
}

rocksdb::SequenceNumber
RocksDBSingleOperationReadOnlyMethods::GetSequenceNumber() const noexcept {
  return _db->GetLatestSequenceNumber();
}

rocksdb::Status RocksDBSingleOperationReadOnlyMethods::Get(
    rocksdb::ColumnFamilyHandle* cf, rocksdb::Slice const& key,
    rocksdb::PinnableSlice* val, ReadOwnWrites) {
  TRI_ASSERT(cf != nullptr);
  rocksdb::ReadOptions ro;
  ro.prefix_same_as_start = true;  // should always be true
  ro.fill_cache = _state->options().fillBlockCache;
  return _db->Get(ro, cf, key, val);
}

std::unique_ptr<rocksdb::Iterator>
RocksDBSingleOperationReadOnlyMethods::NewIterator(rocksdb::ColumnFamilyHandle*,
                                                   ReadOptionsCallback) {
  // This should never be called for a single operation transaction.
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "should not call NewIterator for single operation methods");
}
