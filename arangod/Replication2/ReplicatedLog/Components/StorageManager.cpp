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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#include "StorageManager.h"

#include <Futures/Future.h>
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Replication2/coro-helper.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

using namespace arangodb::replication2::replicated_log;

struct comp::StorageManagerTransaction : IStorageTransaction {
  using GuardType = Guarded<StorageManager::GuardedData>::mutex_guard_type;

  auto getInMemoryLog() const noexcept -> InMemoryLog override {
    return guard->inMemoryLog;
  }
  auto getLogBounds() const noexcept -> LogRange override {
    return guard->inMemoryLog.getIndexRange();
  }

  auto removeFront(LogIndex stop) noexcept -> futures::Future<Result> override {
    struct RemoveFrontAction : StorageManager::StorageOperation {
      explicit RemoveFrontAction(LogIndex stop) : stop(stop) {}
      auto run(StorageManager::IStorageEngineMethods& methods) noexcept
          -> arangodb::futures::Future<arangodb::Result> override {
        return methods.removeFront(stop, {}).thenValue(
            [](auto&& res) { return res.result(); });
      }
      LogIndex stop;
    };
    return manager.scheduleOperation(std::move(guard),
                                     std::make_unique<RemoveFrontAction>(stop));
  }

  auto removeBack(LogIndex start) noexcept -> futures::Future<Result> override {
    struct RemoveBackAction : StorageManager::StorageOperation {
      explicit RemoveBackAction(LogIndex start) : start(start) {}
      auto run(StorageManager::IStorageEngineMethods& methods) noexcept
          -> arangodb::futures::Future<arangodb::Result> override {
        return methods.removeBack(start, {}).thenValue(
            [](auto&& res) { return res.result(); });
      }
      LogIndex start;
    };
    return manager.scheduleOperation(std::move(guard),
                                     std::make_unique<RemoveBackAction>(start));
  }

  auto appendEntries(LogIndex appendAfter, InMemoryLogSlice slice) noexcept
      -> futures::Future<Result> override {
    return Result{};
  }

  explicit StorageManagerTransaction(GuardType guard, StorageManager& manager)
      : guard(std::move(guard)), manager(manager) {}

  GuardType guard;
  StorageManager& manager;
};

StorageManager::StorageManager(std::unique_ptr<IStorageEngineMethods> core)
    : guardedData(std::move(core)) {}

auto StorageManager::resign() -> std::unique_ptr<IStorageEngineMethods> {
  auto guard = guardedData.getLockedGuard();
  ADB_PROD_ASSERT(guard->queue.empty());
  return std::move(guard->core);
}

StorageManager::GuardedData::GuardedData(
    std::unique_ptr<IStorageEngineMethods> core)
    : core(std::move(core)) {}

auto StorageManager::scheduleOperation(
    GuardType&& guard, std::unique_ptr<StorageOperation> operation)
    -> futures::Future<arangodb::Result> {
  auto f = guard->queue.emplace_back(std::move(operation)).promise.getFuture();
  triggerQueueWorker(std::move(guard));
  return f;
}

void StorageManager::triggerQueueWorker(GuardType&& guard) noexcept {
  auto const worker = [](GuardType guard, std::shared_ptr<StorageManager> self)
      -> futures::Future<futures::Unit> {
    while (true) {
      if (guard->queue.empty()) {
        guard->workerActive = false;
        break;
      }

      auto req = std::move(guard->queue.front());
      guard->queue.pop_front();
      auto f = req.operation->run(*guard->core);
      guard.unlock();
      auto result = co_await asTry(std::move(f));
      req.promise.setTry(std::move(result));
      guard = self->guardedData.getLockedGuard();
    }

    co_return futures::unit;
  };

  // check if a thread is working on the queue
  if (not guard->workerActive) {
    // otherwise start a worker
    worker(std::move(guard), shared_from_this());
  }
}

auto StorageManager::transaction() -> std::unique_ptr<IStorageTransaction> {
  auto guard = guardedData.getLockedGuard();
  if (guard->core == nullptr) {
    THROW_ARANGO_EXCEPTION(
        TRI_ERROR_REPLICATION_REPLICATED_LOG_PARTICIPANT_GONE);
  }
  return std::make_unique<StorageManagerTransaction>(std::move(guard), *this);
}
