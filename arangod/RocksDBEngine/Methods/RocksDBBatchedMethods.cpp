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

#include "RocksDBBatchedMethods.h"

using namespace arangodb;

RocksDBBatchedMethods::RocksDBBatchedMethods(rocksdb::WriteBatch* wb)
    : RocksDBMethods(), _wb(wb) {}

rocksdb::Status RocksDBBatchedMethods::Get(rocksdb::ColumnFamilyHandle* cf,
                                           rocksdb::Slice const& key,
                                           rocksdb::PinnableSlice* val, ReadOwnWrites) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "BatchedMethods does not provide Get");
}

rocksdb::Status RocksDBBatchedMethods::GetForUpdate(rocksdb::ColumnFamilyHandle* cf,
                                                    rocksdb::Slice const& key,
                                                    rocksdb::PinnableSlice* val) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, "BatchedMethods does not provide GetForUpdate");
}

rocksdb::Status RocksDBBatchedMethods::Put(rocksdb::ColumnFamilyHandle* cf,
                                           RocksDBKey const& key,
                                           rocksdb::Slice const& val, bool assume_tracked) {
  TRI_ASSERT(cf != nullptr);
  return _wb->Put(cf, key.string(), val);
}

rocksdb::Status RocksDBBatchedMethods::PutUntracked(rocksdb::ColumnFamilyHandle* cf,
                                                    RocksDBKey const& key,
                                                    rocksdb::Slice const& val) {
  return RocksDBBatchedMethods::Put(cf, key, val, /*assume_tracked*/ false);
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
