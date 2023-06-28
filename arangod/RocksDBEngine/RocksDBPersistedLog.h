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

#include "Basics/Guarded.h"
#include "Replication2/ReplicatedLog/LogEntries.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "RocksDBEngine/RocksDBSyncThread.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/LogScale.h"

#include <function2.hpp>
#include <Futures/Promise.h>

#include <array>
#include <variant>
#include <map>
#include <memory>

namespace arangodb {

struct AsyncLogWriteContext {
  AsyncLogWriteContext(std::uint64_t vocbaseId, std::uint64_t objectId)
      : vocbaseId(vocbaseId), objectId(objectId) {}
  std::uint64_t const vocbaseId;
  std::uint64_t const objectId;

  void addPendingAsyncOperation();
  void finishPendingAsyncOperation();
  void waitForCompletion();

 private:
// There are currently bugs in the libstdc++ implementation of atomic
// wait/notify, e.g. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=106183
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=101037
// . Using the following types should make sure we're not running into them.
#if !defined(_LIBCPP_VERSION) || defined(__linux__) || \
    (defined(_AIX) && !defined(__64BIT__))
  using wait_t = std::int32_t;
#else
  using wait_t = std::int64_t;
#endif
  std::atomic<wait_t> _pendingAsyncOperations{0};
};

struct AsyncLogOperationGuard {
  AsyncLogOperationGuard() = default;
  AsyncLogOperationGuard(AsyncLogOperationGuard&&) noexcept = default;
  AsyncLogOperationGuard& operator=(AsyncLogOperationGuard&&) noexcept =
      default;
  explicit AsyncLogOperationGuard(AsyncLogWriteContext& ctx) : _context(&ctx) {
    ctx.addPendingAsyncOperation();
  }

  ~AsyncLogOperationGuard() { fire(); }

  void fire() noexcept {
    if (_context) {
      _context->finishPendingAsyncOperation();
      _context = nullptr;
    }
  }

 private:
  struct nop {
    template<typename T>
    void operator()(T const&) const noexcept {}
  };

  std::unique_ptr<AsyncLogWriteContext, nop> _context = nullptr;
};

struct IRocksDBAsyncLogWriteBatcher {
  virtual ~IRocksDBAsyncLogWriteBatcher() = default;

  struct WriteOptions {
    bool waitForSync = false;
  };

  using SequenceNumber =
      replication2::replicated_state::IStorageEngineMethods::SequenceNumber;

  virtual auto queueInsert(
      AsyncLogWriteContext& ctx,
      std::unique_ptr<replication2::PersistedLogIterator> iter,
      WriteOptions const& opts) -> futures::Future<ResultT<SequenceNumber>> = 0;

  virtual auto queueRemoveFront(AsyncLogWriteContext& ctx,
                                replication2::LogIndex stop,
                                WriteOptions const& opts)
      -> futures::Future<ResultT<SequenceNumber>> = 0;

  virtual auto queueRemoveBack(AsyncLogWriteContext& ctx,
                               replication2::LogIndex start,
                               WriteOptions const& opts)
      -> futures::Future<ResultT<SequenceNumber>> = 0;

  virtual auto waitForSync(SequenceNumber seq) -> futures::Future<Result> = 0;
};

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

struct RocksDBAsyncLogWriteBatcherMetrics {
  metrics::Gauge<std::size_t>* numWorkerThreadsWaitForSync;
  metrics::Gauge<std::size_t>* numWorkerThreadsNoWaitForSync;
  metrics::Gauge<std::size_t>* queueLength;

  metrics::Histogram<metrics::LogScale<std::uint64_t>>* writeBatchSize;
  // metrics::Histogram<metrics::LogScale<std::uint64_t>>* writeBatchCount; TODO
  // required?

  metrics::Histogram<metrics::LogScale<std::uint64_t>>* rocksdbWriteTimeInUs;
  metrics::Histogram<metrics::LogScale<std::uint64_t>>* rocksdbSyncTimeInUs;

  metrics::Histogram<metrics::LogScale<std::uint64_t>>* operationLatencyInsert;
  metrics::Histogram<metrics::LogScale<std::uint64_t>>*
      operationLatencyRemoveFront;
  metrics::Histogram<metrics::LogScale<std::uint64_t>>*
      operationLatencyRemoveBack;
};

struct RocksDBAsyncLogWriteBatcher final
    : IRocksDBAsyncLogWriteBatcher,
      RocksDBSyncThread::ISyncListener,
      std::enable_shared_from_this<RocksDBAsyncLogWriteBatcher> {
  struct IAsyncExecutor {
    virtual ~IAsyncExecutor() = default;
    virtual void operator()(fu2::unique_function<void() noexcept>) = 0;
  };

  RocksDBAsyncLogWriteBatcher(
      rocksdb::ColumnFamilyHandle* cf, rocksdb::DB* db,
      std::shared_ptr<IAsyncExecutor> executor,
      std::shared_ptr<replication2::ReplicatedLogGlobalSettings const> options,
      std::shared_ptr<RocksDBAsyncLogWriteBatcherMetrics> metrics);

  ~RocksDBAsyncLogWriteBatcher() override;

  struct InsertEntries {
    std::unique_ptr<arangodb::replication2::PersistedLogIterator> iter;
  };

  struct RemoveFront {
    replication2::LogIndex stop;
  };

  struct RemoveBack {
    replication2::LogIndex start;
  };

  using Action = std::variant<InsertEntries, RemoveFront, RemoveBack>;

  struct Request {
    Request(AsyncLogWriteContext& ctx, Action action)
        : objectId(ctx.objectId), action(std::move(action)), asyncGuard(ctx) {}
    std::uint64_t objectId;
    Action action;
    AsyncLogOperationGuard asyncGuard;
    futures::Promise<ResultT<SequenceNumber>> promise;
  };

  static_assert(std::is_nothrow_move_constructible_v<Request>);

  auto prepareRequest(Request const& req, rocksdb::WriteBatch& wb) -> Result;
  auto queueInsert(AsyncLogWriteContext& ctx,
                   std::unique_ptr<replication2::PersistedLogIterator> iter,
                   const WriteOptions& opts)
      -> futures::Future<ResultT<SequenceNumber>> override;
  auto queueRemoveFront(AsyncLogWriteContext& ctx, replication2::LogIndex stop,
                        const WriteOptions& opts)
      -> futures::Future<ResultT<SequenceNumber>> override;
  auto queueRemoveBack(AsyncLogWriteContext& ctx, replication2::LogIndex start,
                       const WriteOptions& opts)
      -> futures::Future<ResultT<SequenceNumber>> override;
  auto waitForSync(SequenceNumber seq) -> futures::Future<Result> override;
  auto queue(AsyncLogWriteContext& ctx, Action action, WriteOptions const& wo)
      -> futures::Future<ResultT<SequenceNumber>>;
  void onSync(SequenceNumber seq) noexcept override;

  struct Lane {
    Lane() = delete;

    struct WaitForSync {};
    struct DontWaitForSync {};
    explicit Lane(WaitForSync) : _waitForSync(true) {}
    explicit Lane(DontWaitForSync) : _waitForSync(false) {}

    std::mutex _persistorMutex;
    std::vector<Request> _pendingPersistRequests;
    std::atomic<unsigned> _activePersistorThreads = 0;
    bool const _waitForSync;
    metrics::Gauge<std::size_t>* _numWorkerMetrics;
  };

  void runPersistorWorker(Lane& lane) noexcept;

  std::array<Lane, 2> _lanes = {Lane{Lane::WaitForSync{}},
                                Lane{Lane::DontWaitForSync{}}};

  rocksdb::ColumnFamilyHandle* const _cf;
  rocksdb::DB* const _db;
  std::shared_ptr<IAsyncExecutor> const _executor;
  std::shared_ptr<replication2::ReplicatedLogGlobalSettings const> const
      _options;
  std::shared_ptr<RocksDBAsyncLogWriteBatcherMetrics> const _metrics;

  struct SyncGuard {
    explicit SyncGuard() = default;

    SequenceNumber syncedSequenceNumber{0};
    std::multimap<SequenceNumber, futures::Promise<Result>> promises{};
  };
  Guarded<SyncGuard> _syncGuard;
};

struct RocksDBReplicatedStateInfo {
  replication2::LogId stateId;
  std::uint64_t objectId{0};
  std::uint64_t dataSourceId{0};
  replication2::replicated_state::PersistedStateInfo state;
};

template<class Inspector>
auto inspect(Inspector& f, RocksDBReplicatedStateInfo& x) {
  return f.object(x).fields(
      f.field("stateId", x.stateId),
      f.field(StaticStrings::ObjectId, x.objectId),
      f.field(StaticStrings::DataSourceId, x.dataSourceId),
      f.field("state", x.state));
}

struct RocksDBLogStorageMethods final
    : replication2::replicated_state::IStorageEngineMethods {
  explicit RocksDBLogStorageMethods(
      uint64_t objectId, std::uint64_t vocbaseId, replication2::LogId logId,
      std::shared_ptr<IRocksDBAsyncLogWriteBatcher> persistor, rocksdb::DB* db,
      rocksdb::ColumnFamilyHandle* metaCf, rocksdb::ColumnFamilyHandle* logCf,
      std::shared_ptr<RocksDBAsyncLogWriteBatcherMetrics> metrics);

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
      -> futures::Future<Result> override;

  [[nodiscard]] auto drop() -> Result;
  [[nodiscard]] auto compact() -> Result;

  void waitForCompletion() noexcept override;

  replication2::LogId const logId;
  std::shared_ptr<IRocksDBAsyncLogWriteBatcher> const batcher;
  rocksdb::DB* const db;
  rocksdb::ColumnFamilyHandle* const metaCf;
  rocksdb::ColumnFamilyHandle* const logCf;
  AsyncLogWriteContext ctx;
  std::shared_ptr<RocksDBAsyncLogWriteBatcherMetrics> const _metrics;
};

}  // namespace arangodb
