////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <list>
#include <memory>

#include "Replication2/ReplicatedLog/Components/LogFollower.h"
#include "Replication2/ReplicatedLog/ILogInterfaces.h"

#include "Replication2/Mocks/SchedulerMocks.h"

namespace arangodb::replication2::test {

// Queues appendEntries calls on its own DelayedScheduler. Keeps a queue of
// pending requests to inspect.
struct DelayedLogFollower : replicated_log::ILogFollower {
  explicit DelayedLogFollower(
      std::shared_ptr<replicated_log::ILogFollower> follower)
      : _follower(std::move(follower)) {
    // Let's not accidentally wrap a DelayedLogFollower in another
    ADB_PROD_ASSERT(std::dynamic_pointer_cast<DelayedLogFollower>(follower) ==
                    nullptr);
  }

  auto appendEntries(replicated_log::AppendEntriesRequest req)
      -> arangodb::futures::Future<
          replicated_log::AppendEntriesResult> override {
    auto p = _asyncQueue.emplace_back(
        std::make_shared<AsyncRequest>(std::move(req)));

    scheduler.queue([p, this]() {
      p->promise.setValue(std::move(p->request));
      auto it = std::find(_asyncQueue.begin(), _asyncQueue.end(), p);
      if (it != _asyncQueue.end()) {
        _asyncQueue.erase(it);
      }
    });

    return p->promise.getFuture().thenValue([this](auto&& result) mutable {
      return _follower->appendEntries(std::forward<decltype(result)>(result));
    });
  }

  auto runAsyncAppendEntries() { return scheduler.runAllCurrent(); }

  void runAllAsyncAppendEntries() {
    while (hasPendingAppendEntries()) {
      runAsyncAppendEntries();
    }
  }

  using WaitForAsyncPromise =
      futures::Promise<replicated_log::AppendEntriesRequest>;

  struct AsyncRequest {
    explicit AsyncRequest(replicated_log::AppendEntriesRequest request)
        : request(std::move(request)) {}
    replicated_log::AppendEntriesRequest request;
    WaitForAsyncPromise promise;
  };
  [[nodiscard]] auto pendingAppendEntries() const
      -> std::list<std::shared_ptr<AsyncRequest>> {
    return _asyncQueue;
  }
  [[nodiscard]] auto hasPendingAppendEntries() const -> bool {
    return scheduler.hasWork();
  }

  auto getParticipantId() const noexcept -> ParticipantId const& override {
    return _follower->getParticipantId();
  }

  auto getStatus() const -> replicated_log::LogStatus override {
    return _follower->getStatus();
  }

  auto getQuickStatus() const -> replicated_log::QuickLogStatus override {
    return _follower->getQuickStatus();
  }

  [[nodiscard]] auto resign() && -> std::tuple<
      std::unique_ptr<storage::IStorageEngineMethods>,
      std::unique_ptr<replicated_log::IReplicatedStateHandle>,
      DeferredAction> override {
    return std::move(*_follower).resign();
  }

  auto waitFor(LogIndex index) -> WaitForFuture override {
    return _follower->waitFor(index);
  }

  auto waitForIterator(LogIndex index) -> WaitForIteratorFuture override {
    return _follower->waitForIterator(index);
  }
  auto release(LogIndex doneWithIdx) -> Result override {
    return _follower->release(doneWithIdx);
  }
  auto compact() -> ResultT<replicated_log::CompactionResult> override {
    return _follower->compact();
  }

  auto getInternalLogIterator(std::optional<LogRange> bounds) const
      -> std::unique_ptr<LogIterator> override {
    return _follower->getInternalLogIterator(bounds);
  }

  DelayedScheduler scheduler;

 private:
  std::list<std::shared_ptr<AsyncRequest>> _asyncQueue;
  std::shared_ptr<replicated_log::ILogFollower> _follower;
};

}  // namespace arangodb::replication2::test
