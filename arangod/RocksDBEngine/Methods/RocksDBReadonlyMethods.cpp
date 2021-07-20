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
    : RocksDBMethods(state), _db(db) {
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
