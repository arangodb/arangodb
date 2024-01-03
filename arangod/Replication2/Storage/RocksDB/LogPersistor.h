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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <rocksdb/db.h>

#include "Replication2/Storage/ILogPersistor.h"
#include "Replication2/Storage/IteratorPosition.h"
#include "Replication2/Storage/RocksDB/AsyncLogWriteContext.h"

namespace arangodb {
struct ICompactKeyRange;
}

namespace arangodb::replication2::storage::rocksdb {

struct AsyncLogWriteBatcherMetrics;
struct AsyncLogWriteContext;
struct IAsyncLogWriteBatcher;

struct LogPersistor final : ILogPersistor {
  LogPersistor(LogId logId, uint64_t objectId, std::uint64_t vocbaseId,
               ::rocksdb::DB* const db,
               ::rocksdb::ColumnFamilyHandle* const logCf,
               std::shared_ptr<IAsyncLogWriteBatcher> batcher,
               std::shared_ptr<AsyncLogWriteBatcherMetrics> metrics,
               arangodb::ICompactKeyRange* keyrangeCompactor);

  [[nodiscard]] auto getIterator(IteratorPosition position)
      -> std::unique_ptr<PersistedLogIterator> override;
  [[nodiscard]] auto insert(std::unique_ptr<LogIterator> ptr,
                            WriteOptions const&)
      -> futures::Future<ResultT<SequenceNumber>> override;
  [[nodiscard]] auto removeFront(LogIndex stop, WriteOptions const&)
      -> futures::Future<ResultT<SequenceNumber>> override;
  [[nodiscard]] auto removeBack(LogIndex start, WriteOptions const&)
      -> futures::Future<ResultT<SequenceNumber>> override;
  [[nodiscard]] auto getLogId() -> LogId override;

  [[nodiscard]] auto waitForSync(SequenceNumber number)
      -> futures::Future<Result> override;

  void waitForCompletion() noexcept override;

  auto drop() -> Result override;
  auto compact() -> Result override;

 private:
  LogId const logId;
  AsyncLogWriteContext ctx;
  std::shared_ptr<IAsyncLogWriteBatcher> const batcher;
  std::shared_ptr<AsyncLogWriteBatcherMetrics> const _metrics;
  ::rocksdb::DB* const db;
  ::rocksdb::ColumnFamilyHandle* const logCf;
  arangodb::ICompactKeyRange* _keyrangeCompactor;
};

}  // namespace arangodb::replication2::storage::rocksdb
