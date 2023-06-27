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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <function2.hpp>
#include <Futures/Promise.h>

#include "Metrics/Fwd.h"
#include "Replication2/Storage/RocksDB/AsyncLogWriteContext.h"
#include "Replication2/Storage/RocksDB/IAsyncLogWriteBatcher.h"

namespace rocksdb {
class DB;
class ColumnFamilyHandle;
class WriteBatch;
}  // namespace rocksdb

namespace arangodb {
template<typename T>
class ResultT;
}

namespace arangodb::replication2::storage::rocksdb {

struct AsyncLogWriteBatcherMetrics;

struct AsyncLogOperationGuard {
  AsyncLogOperationGuard() = default;
  AsyncLogOperationGuard(AsyncLogOperationGuard&&) noexcept = default;
  AsyncLogOperationGuard& operator=(AsyncLogOperationGuard&&) noexcept =
      default;
  explicit AsyncLogOperationGuard(AsyncLogWriteContext& ctx);

  ~AsyncLogOperationGuard();

  void fire() noexcept;

 private:
  struct nop {
    template<typename T>
    void operator()(T const&) const noexcept {}
  };

  std::unique_ptr<AsyncLogWriteContext, nop> _context = nullptr;
};

struct AsyncLogWriteBatcher final
    : IAsyncLogWriteBatcher,
      std::enable_shared_from_this<AsyncLogWriteBatcher> {
  struct IAsyncExecutor {
    virtual ~IAsyncExecutor() = default;
    virtual void operator()(fu2::unique_function<void() noexcept>) = 0;
  };

  AsyncLogWriteBatcher(
      ::rocksdb::ColumnFamilyHandle* cf, ::rocksdb::DB* db,
      std::shared_ptr<IAsyncExecutor> executor,
      std::shared_ptr<ReplicatedLogGlobalSettings const> options,
      std::shared_ptr<AsyncLogWriteBatcherMetrics> metrics);

  struct InsertEntries {
    std::unique_ptr<PersistedLogIterator> iter;
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

  auto prepareRequest(Request const& req, ::rocksdb::WriteBatch& wb) -> Result;
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
  auto queue(AsyncLogWriteContext& ctx, Action action, WriteOptions const& wo)
      -> futures::Future<ResultT<SequenceNumber>>;

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

  ::rocksdb::ColumnFamilyHandle* const _cf;
  ::rocksdb::DB* const _db;
  std::shared_ptr<IAsyncExecutor> const _executor;
  std::shared_ptr<replication2::ReplicatedLogGlobalSettings const> const
      _options;
  std::shared_ptr<AsyncLogWriteBatcherMetrics> const _metrics;
};
}  // namespace arangodb::replication2::storage::rocksdb