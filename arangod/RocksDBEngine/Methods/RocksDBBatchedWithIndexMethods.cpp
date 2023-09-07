////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBBatchedWithIndexMethods.h"
#include "RocksDBEngine/RocksDBMethodsMemoryTracker.h"

using namespace arangodb;

RocksDBBatchedWithIndexMethods::RocksDBBatchedWithIndexMethods(
    rocksdb::TransactionDB* db, rocksdb::WriteBatchWithIndex* wb,
    RocksDBMethodsMemoryTracker& memoryTracker)
    : RocksDBBatchedBaseMethods(memoryTracker), _db(db), _wb(wb) {
  TRI_ASSERT(_db != nullptr);
  TRI_ASSERT(_wb != nullptr);
}

rocksdb::Status RocksDBBatchedWithIndexMethods::Get(
    rocksdb::ColumnFamilyHandle* cf, rocksdb::Slice const& key,
    rocksdb::PinnableSlice* val, ReadOwnWrites readOwnWrites) {
  TRI_ASSERT(cf != nullptr);
  rocksdb::ReadOptions ro;
  if (readOwnWrites == ReadOwnWrites::yes) {
    return _wb->GetFromBatchAndDB(_db, ro, cf, key, val);
  }
  return _db->Get(ro, cf, key, val);
}

rocksdb::Status RocksDBBatchedWithIndexMethods::GetForUpdate(
    rocksdb::ColumnFamilyHandle* cf, rocksdb::Slice const& key,
    rocksdb::PinnableSlice* val) {
  return this->Get(
      cf, key, val,
      ReadOwnWrites::yes);  // update operations always have to read own writes
}

rocksdb::Status RocksDBBatchedWithIndexMethods::Put(
    rocksdb::ColumnFamilyHandle* cf, RocksDBKey const& key,
    rocksdb::Slice const& val, bool assume_tracked) {
  TRI_ASSERT(cf != nullptr);
  std::uint64_t beforeSize = currentWriteBatchSize();
  rocksdb::Status s = _wb->Put(cf, key.string(), val);
  if (s.ok()) {
    // size of WriteBatch got increased. track memory usage of WriteBatch
    _memoryTracker.increaseMemoryUsage((currentWriteBatchSize() - beforeSize) +
                                       indexingOverhead(key.string().size()));
  }
  return s;
}

rocksdb::Status RocksDBBatchedWithIndexMethods::PutUntracked(
    rocksdb::ColumnFamilyHandle* cf, RocksDBKey const& key,
    rocksdb::Slice const& val) {
  std::uint64_t beforeSize = currentWriteBatchSize();
  rocksdb::Status s =
      RocksDBBatchedWithIndexMethods::Put(cf, key, val,
                                          /*assume_tracked*/ false);
  if (s.ok()) {
    // size of WriteBatch got increased. track memory usage of WriteBatch
    // plus potential overhead of indexing
    _memoryTracker.increaseMemoryUsage((currentWriteBatchSize() - beforeSize) +
                                       indexingOverhead(key.string().size()));
  }
  return s;
}

rocksdb::Status RocksDBBatchedWithIndexMethods::Delete(
    rocksdb::ColumnFamilyHandle* cf, RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  std::uint64_t beforeSize = currentWriteBatchSize();
  rocksdb::Status s = _wb->Delete(cf, key.string());
  if (s.ok()) {
    // size of WriteBatch got increased. track memory usage of WriteBatch
    // plus potential overhead of indexing
    _memoryTracker.increaseMemoryUsage((currentWriteBatchSize() - beforeSize) +
                                       indexingOverhead(key.string().size()));
  }
  return s;
}

rocksdb::Status RocksDBBatchedWithIndexMethods::SingleDelete(
    rocksdb::ColumnFamilyHandle* cf, RocksDBKey const& key) {
  TRI_ASSERT(cf != nullptr);
  std::uint64_t beforeSize = currentWriteBatchSize();
  rocksdb::Status s = _wb->SingleDelete(cf, key.string());
  if (s.ok()) {
    // size of WriteBatch got increased. track memory usage of WriteBatch
    // plus potential overhead of indexing
    _memoryTracker.increaseMemoryUsage((currentWriteBatchSize() - beforeSize) +
                                       indexingOverhead(key.string().size()));
  }
  return s;
}

void RocksDBBatchedWithIndexMethods::PutLogData(rocksdb::Slice const& blob) {
  std::uint64_t beforeSize = currentWriteBatchSize();
  _wb->PutLogData(blob);
  _memoryTracker.increaseMemoryUsage(currentWriteBatchSize() - beforeSize);
}

size_t RocksDBBatchedWithIndexMethods::currentWriteBatchSize() const noexcept {
  return _wb->GetWriteBatch()->Data().capacity();
}
