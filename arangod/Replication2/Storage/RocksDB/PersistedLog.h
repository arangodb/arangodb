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

#include "Replication2/ReplicatedLog/LogEntries.h"
#include "Replication2/Storage/ILogPersistor.h"
#include "Replication2/Storage/IStatePersistor.h"
#include "Replication2/Storage/IStorageEngineMethods.h"
#include "Replication2/Storage/RocksDB/AsyncLogWriteContext.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/LogScale.h"

#include <function2.hpp>
#include <Futures/Promise.h>

#include <array>
#include <variant>
#include <memory>

namespace arangodb {

struct WriteBatchSizeScale {
  using scale_t = metrics::LogScale<std::uint64_t>;
  static scale_t scale() {
    // values in bytes, smallest bucket is up to 1kb
    return {scale_t::kSupplySmallestBucket, 2, 0, 1024, 16};
  }
};
DECLARE_GAUGE(arangodb_replication2_rocksdb_num_persistor_worker, std::size_t,
              "Number of threads running in the log persistor");
DECLARE_GAUGE(arangodb_replication2_rocksdb_queue_length, std::size_t,
              "Number of replicated log storage operations queued");
DECLARE_HISTOGRAM(arangodb_replication2_rocksdb_write_batch_size,
                  WriteBatchSizeScale,
                  "Size of replicated log write batches in bytes");
struct ApplyEntriesRttScale {
  using scale_t = metrics::LogScale<std::uint64_t>;
  static scale_t scale() {
    // values in us, smallest bucket is up to 1ms, scales up to 2^16ms =~ 65s.
    return {scale_t::kSupplySmallestBucket, 2, 0, 1'000, 16};
  }
};
DECLARE_HISTOGRAM(arangodb_replication2_rocksdb_write_time,
                  ApplyEntriesRttScale,
                  "Replicated log batches write time[us]");
DECLARE_HISTOGRAM(arangodb_replication2_rocksdb_sync_time, ApplyEntriesRttScale,
                  "Replicated log batches sync time[us]");
DECLARE_HISTOGRAM(arangodb_replication2_storage_operation_latency,
                  ApplyEntriesRttScale,
                  "Replicated log storage operation latency[us]");

}  // namespace arangodb

namespace rocksdb {
class DB;
class ColumnFamilyHandle;
}  // namespace rocksdb

namespace arangodb::replication2::storage::rocksdb {

struct AsyncLogWriteBatcherMetrics;
struct IAsyncLogWriteBatcher;

struct RocksDBLogStorageMethods final
    : replication2::storage::IStorageEngineMethods {
  explicit RocksDBLogStorageMethods(
      uint64_t objectId, std::uint64_t vocbaseId, replication2::LogId logId,
      std::shared_ptr<IAsyncLogWriteBatcher> batcher, ::rocksdb::DB* db,
      ::rocksdb::ColumnFamilyHandle* metaCf,
      ::rocksdb::ColumnFamilyHandle* logCf,
      std::shared_ptr<AsyncLogWriteBatcherMetrics> metrics);

  [[nodiscard]] auto updateMetadata(
      replication2::replicated_state::PersistedStateInfo info)
      -> Result override;
  [[nodiscard]] auto readMetadata()
      -> ResultT<replication2::replicated_state::PersistedStateInfo> override;
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
      -> futures::Future<futures::Unit> override;

  [[nodiscard]] auto drop() -> Result;
  [[nodiscard]] auto compact() -> Result;

  void waitForCompletion() noexcept override;

  replication2::LogId const logId;

  ::rocksdb::DB* const db;
  ::rocksdb::ColumnFamilyHandle* const metaCf;
  ::rocksdb::ColumnFamilyHandle* const logCf;
  AsyncLogWriteContext ctx;
  std::shared_ptr<AsyncLogWriteBatcherMetrics> const _metrics;
  std::unique_ptr<ILogPersistor> _logPersistor;
  std::unique_ptr<IStatePersistor> _statePersistor;
};

}  // namespace arangodb::replication2::storage::rocksdb
