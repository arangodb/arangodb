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

#include <utility>
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Replication2/coro-helper.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/ReplicatedLogIterator.h"

using namespace arangodb::replication2::replicated_log;

struct comp::StorageManagerTransaction : IStorageTransaction {
  using GuardType = Guarded<StorageManager::GuardedData>::mutex_guard_type;

  auto getInMemoryLog() const noexcept -> InMemoryLog override {
    return guard->spearheadLog;
  }
  auto getLogBounds() const noexcept -> LogRange override {
    return guard->spearheadLog.getIndexRange();
  }

  template<typename F>
  auto scheduleOperation(InMemoryLog result, F&& fn) noexcept {
    return manager.scheduleOperationLambda(std::move(guard), std::move(result),
                                           std::forward<F>(fn));
  }

  auto removeFront(LogIndex stop) noexcept -> futures::Future<Result> override {
    auto newLog = guard->spearheadLog.removeFront(stop);
    return scheduleOperation(
        std::move(newLog),
        [stop](StorageManager::IStorageEngineMethods& methods) noexcept {
          return methods.removeFront(stop, {}).thenValue(
              [](auto&& res) { return res.result(); });
        });
  }

  auto removeBack(LogIndex start) noexcept -> futures::Future<Result> override {
    auto newLog = guard->spearheadLog.removeBack(start);
    return scheduleOperation(
        std::move(newLog),
        [start](StorageManager::IStorageEngineMethods& methods) noexcept {
          return methods.removeBack(start, {}).thenValue(
              [](auto&& res) { return res.result(); });
        });
  }

  auto appendEntries(InMemoryLog slice) noexcept
      -> futures::Future<Result> override {
    ADB_PROD_ASSERT(slice.getFirstIndex() ==
                    guard->spearheadLog.getLastIndex() + 1)
        << "tried to append non matching slice - first entry in current log: "
        << guard->spearheadLog.getLastIndex() << " new piece starts at "
        << slice.getFirstIndex();
    auto iter = slice.getPersistedLogIterator();
    auto newLog = guard->spearheadLog.append(slice);
    return scheduleOperation(
        std::move(newLog),
        [slice = std::move(slice), iter = std::move(iter)](
            StorageManager::IStorageEngineMethods& methods) mutable noexcept {
          return methods.insert(std::move(iter), {}).thenValue([](auto&& res) {
            return res.result();
          });
        });
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
  // TODO wait for queue to be empty
  ADB_PROD_ASSERT(guard->queue.empty());
  return std::move(guard->core);
}

StorageManager::GuardedData::GuardedData(
    std::unique_ptr<IStorageEngineMethods> core)
    : core(std::move(core)) {
  spearheadLog = onDiskLog = InMemoryLog::loadFromMethods(*this->core);
}

auto StorageManager::scheduleOperation(
    GuardType&& guard, InMemoryLog result,
    std::unique_ptr<StorageOperation> operation)
    -> futures::Future<arangodb::Result> {
  guard->spearheadLog = result;
  auto f = guard->queue.emplace_back(std::move(operation), std::move(result))
               .promise.getFuture();
  triggerQueueWorker(std::move(guard));
  return f;
}

template<typename F>
auto StorageManager::scheduleOperationLambda(GuardType&& guard,
                                             InMemoryLog result, F&& fn)
    -> futures::Future<Result> {
  struct LambdaOperation : F, StorageOperation {
    explicit LambdaOperation(F&& fn) : F(std::move(fn)) {}
    static_assert(std::is_nothrow_invocable_r_v<futures::Future<Result>, F,
                                                IStorageEngineMethods&>);
    auto run(IStorageEngineMethods& methods) noexcept
        -> futures::Future<Result> override {
      return F::operator()(methods);
    }
  };

  return scheduleOperation(
      std::move(guard), std::move(result),
      std::make_unique<LambdaOperation>(std::forward<F>(fn)));
}

void StorageManager::triggerQueueWorker(GuardType guard) noexcept {
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
      Result result = co_await asResult(std::move(f));
      // lock guard to make effects visible
      if (result.ok()) {
        // resolve and continue
        req.promise.setValue(std::move(result));
        guard = self->guardedData.getLockedGuard();
        guard->onDiskLog = std::move(req.logResult);
      } else {
        // lock directly, and flush all the queue.
        // If we resolve directly (which could result in a retry) we would
        // immediately abort any retries with `precondition` failed
        guard = self->guardedData.getLockedGuard();
        // restore old state
        guard->spearheadLog = guard->onDiskLog;
        // clear queue
        auto queue = std::move(guard->queue);
        guard->queue.clear();
        guard.unlock();
        // resolve everything else
        req.promise.setValue(std::move(result));
        for (auto& r : queue) {
          r.promise.setValue(TRI_ERROR_ARANGO_CONFLICT);
        }
        // and lock again
        guard = self->guardedData.getLockedGuard();
      }
    }

    co_return futures::unit;
  };

  // check if a thread is working on the queue
  if (not guard->workerActive) {
    // otherwise start a worker
    guard->workerActive = true;
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
