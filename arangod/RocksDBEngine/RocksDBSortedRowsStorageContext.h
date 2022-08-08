////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"

#include <cstdint>
#include <memory>
#include <string>

#include <rocksdb/slice.h>

namespace rocksdb {
class ColumnFamilyHandle;
class DB;
class Iterator;
}  // namespace rocksdb

namespace arangodb {
class RocksDBKey;
class RocksDBSstFileMethods;
class StorageUsageTracker;

namespace velocypack {
class Slice;
}

class RocksDBSortedRowsStorageContext {
 public:
  RocksDBSortedRowsStorageContext(rocksdb::DB* db,
                                  rocksdb::ColumnFamilyHandle* cf,
                                  std::string const& path, uint64_t keyPrefix,
                                  StorageUsageTracker& usageTracker);
  ~RocksDBSortedRowsStorageContext();

  Result storeRow(RocksDBKey const& key, velocypack::Slice data);
  void ingestAll();
  std::unique_ptr<rocksdb::Iterator> getIterator();
  void cleanup();

  uint64_t keyPrefix() const noexcept { return _keyPrefix; }

  bool hasReachedMaxCapacity();

 private:
  rocksdb::DB* _db;
  rocksdb::ColumnFamilyHandle* _cf;

  std::string const _path;
  uint64_t const _keyPrefix;
  StorageUsageTracker& _usageTracker;

  std::string _lowerBoundPrefix;
  rocksdb::Slice _lowerBoundSlice;
  std::string _upperBoundPrefix;
  rocksdb::Slice _upperBoundSlice;

  uint64_t _bytesWrittenToDir;
  bool _needsCleanup;

  std::unique_ptr<RocksDBSstFileMethods> _methods;
};

}  // namespace arangodb
