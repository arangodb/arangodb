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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/SortedRowsStorageBackend.h"
#include "Aql/SortExecutor.h"

#include <cstddef>
#include <memory>

namespace rocksdb {
class Iterator;
}

namespace arangodb {
class RocksDBSortedRowsStorageContext;
class RocksDBTempStorage;

namespace aql {
class SortExecutorInfos;
}

class SortedRowsStorageBackendRocksDB final
    : public aql::SortedRowsStorageBackend {
 public:
  explicit SortedRowsStorageBackendRocksDB(RocksDBTempStorage& storage,
                                           aql::SortExecutorInfos& infos);

  ~SortedRowsStorageBackendRocksDB();

  aql::ExecutorState consumeInputRange(
      aql::AqlItemBlockInputRange& inputRange) final;

  bool hasReachedCapacityLimit() const noexcept final;
  bool hasMore() const final;
  void produceOutputRow(aql::OutputAqlItemRow& output) final;
  void skipOutputRow() noexcept final;
  void seal() final;
  void spillOver(aql::SortedRowsStorageBackend& other) final;

 private:
  void cleanup();

  RocksDBTempStorage& _tempStorage;

  aql::SortExecutorInfos& _infos;

  std::unique_ptr<RocksDBSortedRowsStorageContext> _context;

  // iterator for reading data
  std::unique_ptr<rocksdb::Iterator> _iterator;

  // next row number that we generate on insert
  size_t _rowNumberForInsert;
};

}  // namespace arangodb
