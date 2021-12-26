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

#include "RocksDBReadOnlyMethods.h"

#include "RocksDBEngine/RocksDBTransactionState.h"

#include <rocksdb/db.h>

using namespace arangodb;

Result RocksDBReadOnlyMethods::beginTransaction() {
  TRI_ASSERT(_readOptions.snapshot == nullptr);
  _readOptions.snapshot = _db->GetSnapshot();  // must call ReleaseSnapshot later
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

rocksdb::Status RocksDBReadOnlyMethods::Get(rocksdb::ColumnFamilyHandle* cf,
                                            rocksdb::Slice const& key,
                                            rocksdb::PinnableSlice* val, ReadOwnWrites) {
  TRI_ASSERT(cf != nullptr);
  TRI_ASSERT(_readOptions.snapshot != nullptr);
  return _db->Get(_readOptions, cf, key, val);
}

std::unique_ptr<rocksdb::Iterator> RocksDBReadOnlyMethods::NewIterator(
    rocksdb::ColumnFamilyHandle* cf, ReadOptionsCallback readOptionsCallback) {
  TRI_ASSERT(cf != nullptr);

  ReadOptions opts = _readOptions;
  if (readOptionsCallback) {
    readOptionsCallback(opts);
  }

  std::unique_ptr<rocksdb::Iterator> iterator(_db->NewIterator(opts, cf));
  if (iterator == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "invalid iterator in RocksDBReadOnlyMethods");
  }
  return iterator;
}
