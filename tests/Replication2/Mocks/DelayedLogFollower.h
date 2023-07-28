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

#include <deque>
#include <memory>

#include "Replication2/ReplicatedLog/Components/LogFollower.h"
#include "Replication2/ReplicatedLog/ILogInterfaces.h"

namespace arangodb::replication2::test {

struct DelayedLogFollower : replicated_log::AbstractFollower,
                            replicated_log::ILogParticipant {
  explicit DelayedLogFollower(
      std::shared_ptr<replicated_log::ILogFollower> follower)
      : _follower(std::move(follower)) {}

  /*
explicit LogFollowerImpl(
ParticipantId myself,
std::unique_ptr<storage::IStorageEngineMethods> methods,
std::unique_ptr<IReplicatedStateHandle> stateHandlePtr,
std::shared_ptr<FollowerTermInformation const> termInfo,
std::shared_ptr<ReplicatedLogGlobalSettings const> options,
std::shared_ptr<ReplicatedLogMetrics> metrics,
std::shared_ptr<ILeaderCommunicator> leaderComm,
std::shared_ptr<IScheduler> scheduler, LoggerContext logContext);
  */
  // DelayedFollowerLog(LoggerContext const& logContext,
  //                    std::shared_ptr<ReplicatedLogMetricsMock>
  //                    logMetricsMock,
  //                    std::shared_ptr<ReplicatedLogGlobalSettings const>
  //                    options, ParticipantId const& id,
  //                    std::unique_ptr<storage::IStorageEngineMethods> methods,
  //                    LogTerm term, ParticipantId leaderId)
  //     : DelayedFollowerLog([&] {
  //         return std::make_shared<replicated_log::LogFollowerImpl>(
  //             id, std::move(methods), nullptr /* TODO state handle */,
  //             nullptr /* TODO terminfo */, std::move(options),
  //             std::move(logMetricsMock), nullptr /* TODO leadercomm */,
  //             nullptr /* TODO scheduler */, logContext);
  //         // logContext, std::move(logMetricsMock), std::move(options), id,
  //         // std::move(methods), term, std::move(leaderId));
  //       }()) {}

  auto appendEntries(replicated_log::AppendEntriesRequest req)
      -> arangodb::futures::Future<
          replicated_log::AppendEntriesResult> override {
    auto future = _asyncQueue.doUnderLock([&](auto& queue) {
      return queue.emplace_back(std::make_shared<AsyncRequest>(std::move(req)))
          ->promise.getFuture();
    });
    return std::move(future).thenValue([this](auto&& result) mutable {
      return _follower->appendEntries(std::forward<decltype(result)>(result));
    });
  }

  auto runAsyncAppendEntries() {
    auto asyncQueue = _asyncQueue.doUnderLock([](auto& _queue) {
      auto queue = std::move(_queue);
      _queue.clear();
      return queue;
    });

    for (auto& p : asyncQueue) {
      p->promise.setValue(std::move(p->request));
    }

    return asyncQueue.size();
  }

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
      -> std::deque<std::shared_ptr<AsyncRequest>> {
    return _asyncQueue.copy();
  }
  [[nodiscard]] auto hasPendingAppendEntries() const -> bool {
    return _asyncQueue.doUnderLock(
        [](auto const& queue) { return !queue.empty(); });
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

 private:
  Guarded<std::deque<std::shared_ptr<AsyncRequest>>> _asyncQueue;
  std::shared_ptr<replicated_log::ILogFollower> _follower;
};
}  // namespace arangodb::replication2::test