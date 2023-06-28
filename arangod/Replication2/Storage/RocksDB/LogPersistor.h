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

namespace arangodb::replication2::storage::rocksdb {

struct AsyncLogWriteBatcherMetrics;
struct AsyncLogWriteContext;
struct IAsyncLogWriteBatcher;

struct LogPersistor final : ILogPersistor {
  LogPersistor(LogId logId, AsyncLogWriteContext& ctx, ::rocksdb::DB* const db,
               ::rocksdb::ColumnFamilyHandle* const logCf,
               std::shared_ptr<IAsyncLogWriteBatcher> batcher,
               std::shared_ptr<AsyncLogWriteBatcherMetrics> metrics);

  [[nodiscard]] auto read(replication2::LogIndex first)
      -> std::unique_ptr<replication2::PersistedLogIterator> override;
  [[nodiscard]] auto insert(
      std::unique_ptr<replication2::PersistedLogIterator> ptr,
      WriteOptions const&) -> futures::Future<ResultT<SequenceNumber>> override;
  [[nodiscard]] auto removeFront(replication2::LogIndex stop,
                                 WriteOptions const&)
      -> futures::Future<ResultT<SequenceNumber>> override;
  [[nodiscard]] auto removeBack(replication2::LogIndex start,
                                WriteOptions const&)
      -> futures::Future<ResultT<SequenceNumber>> override;
  [[nodiscard]] auto getObjectId() -> std::uint64_t override;
  [[nodiscard]] auto getLogId() -> replication2::LogId override;

  [[nodiscard]] auto getSyncedSequenceNumber() -> SequenceNumber override;
  [[nodiscard]] auto waitForSync(SequenceNumber number)
      -> futures::Future<Result> override;

  void waitForCompletion() noexcept override;

 private:
  LogId const logId;
  AsyncLogWriteContext& ctx;
  std::shared_ptr<IAsyncLogWriteBatcher> const batcher;
  std::shared_ptr<AsyncLogWriteBatcherMetrics> const _metrics;
  ::rocksdb::DB* const db;
  ::rocksdb::ColumnFamilyHandle* const logCf;
};

}  // namespace arangodb::replication2::storage::rocksdb
