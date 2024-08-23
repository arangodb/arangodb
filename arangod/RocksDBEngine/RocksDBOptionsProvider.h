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

#pragma once

#include <optional>

#include <rocksdb/options.h>
#include <rocksdb/table.h>
#include <rocksdb/utilities/transaction_db.h>

#include "RocksDBEngine/RocksDBColumnFamilyManager.h"

namespace arangodb {

struct RocksDBOptionsProvider {
  RocksDBOptionsProvider();
  virtual ~RocksDBOptionsProvider() = default;

  virtual rocksdb::TransactionDBOptions getTransactionDBOptions() const = 0;
  rocksdb::Options const& getOptions() const;
  rocksdb::BlockBasedTableOptions const& getTableOptions() const;
  virtual rocksdb::ColumnFamilyOptions getColumnFamilyOptions(
      RocksDBColumnFamilyManager::Family family) const;
  void resetVPackComparator(
      std::unique_ptr<rocksdb::Comparator> newComparator) {
    _vpackCmp = std::move(newComparator);
  }

  virtual bool useFileLogging() const noexcept { return false; }
  virtual bool limitOpenFilesAtStartup() const noexcept { return false; }
  virtual uint64_t maxTotalWalSize() const noexcept = 0;
  virtual uint32_t numThreadsHigh() const noexcept = 0;
  virtual uint32_t numThreadsLow() const noexcept = 0;
  virtual uint64_t periodicCompactionTtl() const noexcept = 0;

 protected:
  virtual rocksdb::Options doGetOptions() const = 0;
  virtual rocksdb::BlockBasedTableOptions doGetTableOptions() const = 0;

 private:
  /// arangodb comparator - required because of vpack in keys
  std::unique_ptr<rocksdb::Comparator> _vpackCmp;
  mutable std::optional<rocksdb::Options> _options;
  mutable std::optional<rocksdb::BlockBasedTableOptions> _tableOptions;
};

}  // namespace arangodb
