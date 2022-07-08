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

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace rocksdb {
class ColumnFamilyHandle;
class Comparator;
class DB;
}  // namespace rocksdb

namespace arangodb {
class RocksDBSortedRowsStorageContext;

class RocksDBTempStorage {
 public:
  explicit RocksDBTempStorage(std::string const& basePath);
  ~RocksDBTempStorage();

  Result init();

  void close();

  std::unique_ptr<RocksDBSortedRowsStorageContext>
  getSortedRowsStorageContext();

 private:
  uint64_t nextId() noexcept;

  std::string const _basePath;
  std::string _tempFilesPath;

  std::atomic<uint64_t> _nextId;

  rocksdb::DB* _db;
  std::unique_ptr<rocksdb::Comparator> _comparator;
  std::vector<rocksdb::ColumnFamilyHandle*> _cfHandles;
};

}  // namespace arangodb
