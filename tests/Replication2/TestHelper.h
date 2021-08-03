////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#include "ReplicatedLogMetricsMock.h"

#include "Replication2/ReplicatedLog/ILogParticipant.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogFollower.h"
#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/PersistedLog.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/types.h"

#include <gtest/gtest.h>

#include <deque>
#include <memory>
#include <utility>

namespace arangodb::replication2 {

using namespace replicated_log;

struct MockLog : replication2::replicated_log::PersistedLog {
  using storeType = std::map<replication2::LogIndex, replication2::PersistingLogEntry>;

  explicit MockLog(replication2::LogId id);
  MockLog(replication2::LogId id, storeType storage);

  auto insert(replication2::replicated_log::PersistedLogIterator& iter, WriteOptions const&) -> Result override;
  auto insertAsync(std::unique_ptr<replication2::replicated_log::PersistedLogIterator> iter,
                   WriteOptions const&) -> futures::Future<Result> override;
  auto read(replication2::LogIndex start)
      -> std::unique_ptr<replication2::replicated_log::PersistedLogIterator> override;
  auto removeFront(replication2::LogIndex stop) -> Result override;
  auto removeBack(replication2::LogIndex start) -> Result override;
  auto drop() -> Result override;

  void setEntry(replication2::LogIndex idx, replication2::LogTerm term,
                replication2::LogPayload payload);
  void setEntry(replication2::PersistingLogEntry);

  [[nodiscard]] storeType getStorage() const { return _storage; }
 private:
  using iteratorType = storeType::iterator;
  storeType _storage;
};

struct AsyncMockLog : MockLog {

  explicit AsyncMockLog(replication2::LogId id);

  ~AsyncMockLog() noexcept;

  auto insertAsync(std::unique_ptr<replication2::replicated_log::PersistedLogIterator> iter,
                   WriteOptions const&) -> futures::Future<Result> override;

  auto stop() noexcept -> void {
    if (!_stopping) {
      {
        std::unique_lock guard(_mutex);
        _stopping = true;
        _cv.notify_all();
      }
      _asyncWorker.join();
    }
  }

 private:
  struct QueueEntry {
    WriteOptions opts;
    std::unique_ptr<replication2::replicated_log::PersistedLogIterator> iter;
    futures::Promise<Result> promise;
  };

  void runWorker();

  std::mutex _mutex;
  std::vector<std::shared_ptr<QueueEntry>> _queue;
  std::condition_variable _cv;
  std::atomic<bool> _stopping = false;
  bool _stopped = false;
  // _asyncWorker *must* be initialized last, otherwise starting the thread
  // races with initializing the coordination variables.
  std::thread _asyncWorker;
};

struct DelayedFollowerLog : AbstractFollower {
  explicit DelayedFollowerLog(std::shared_ptr<LogFollower> follower)
      : _follower(std::move(follower)) {}

  DelayedFollowerLog(LoggerContext const& logContext,
                     std::shared_ptr<ReplicatedLogMetricsMock> logMetricsMock,
                     ParticipantId const& id, std::unique_ptr<LogCore> logCore,
                     LogTerm term, ParticipantId leaderId)
      : DelayedFollowerLog([&] {
          auto inMemoryLog = InMemoryLog{logContext, *logCore};
          return std::make_shared<LogFollower>(logContext, std::move(logMetricsMock),
                                               id, std::move(logCore), term,
                                               std::move(leaderId),
                                               std::move(inMemoryLog));
        }()) {}

  auto appendEntries(AppendEntriesRequest req)
      -> arangodb::futures::Future<AppendEntriesResult> override {
    auto future = _asyncQueue.doUnderLock([&](auto& queue) {
      return queue.emplace_back(std::make_shared<AsyncRequest>(std::move(req)))
          ->promise.getFuture();
    });
    return std::move(future).thenValue(
        [this](auto&& result) mutable {
          return _follower->appendEntries(std::forward<decltype(result)>(result));
        });
  }

  void runAsyncAppendEntries() {
    auto asyncQueue = _asyncQueue.doUnderLock([](auto& _queue) {
      auto queue = std::move(_queue);
      _queue.clear();
      return queue;
    });

    for (auto& p : asyncQueue) {
      p->promise.setValue(std::move(p->request));
    }
  }

  using WaitForAsyncPromise = futures::Promise<AppendEntriesRequest>;

  struct AsyncRequest {
    explicit AsyncRequest(AppendEntriesRequest request)
        : request(std::move(request)) {}
    AppendEntriesRequest request;
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

  auto getStatus() const -> LogStatus {
    return _follower->getStatus();
  }

  auto resign() && {
    return std::move(*_follower).resign();
  }

  auto waitFor(LogIndex index) {
    return _follower->waitFor(index);
  }

  auto waitForIterator(LogIndex index) {
    return _follower->waitForIterator(index);
  }
 private:
  Guarded<std::deque<std::shared_ptr<AsyncRequest>>> _asyncQueue;
  std::shared_ptr<LogFollower> _follower;
};

struct TestReplicatedLog : ReplicatedLog {
  using ReplicatedLog::ReplicatedLog;
  auto becomeFollower(ParticipantId const& id, LogTerm term, ParticipantId leaderId)
      -> std::shared_ptr<DelayedFollowerLog>;
  auto becomeLeader(ParticipantId const& id, LogTerm term,
                    std::vector<std::shared_ptr<AbstractFollower>> const& follower,
                    std::size_t writeConcern) -> std::shared_ptr<LogLeader>;
  auto becomeLeader(LogConfig config, ParticipantId id, LogTerm term,
                    std::vector<std::shared_ptr<AbstractFollower>> const& follower)
      -> std::shared_ptr<LogLeader>;
};

struct ReplicatedLogTest : ::testing::Test {

  auto makeLogCore(LogId id) -> std::unique_ptr<LogCore> {
    auto persisted = makePersistedLog(id);
    return std::make_unique<LogCore>(persisted);
  }

  auto getPersistedLogById(LogId id) -> std::shared_ptr<MockLog> {
    return _persistedLogs.at(id);
  }

  auto makePersistedLog(LogId id) -> std::shared_ptr<MockLog> {
    auto persisted = std::make_shared<MockLog>(id);
    _persistedLogs[id] = persisted;
    return persisted;
  }

  auto makeReplicatedLog(LogId id) -> std::shared_ptr<TestReplicatedLog> {
    auto core = makeLogCore(id);
    return std::make_shared<TestReplicatedLog>(std::move(core), _logMetricsMock,
                                               LoggerContext(Logger::FIXME));
  }

  auto makeReplicatedLogWithAsyncMockLog(LogId id) -> std::shared_ptr<TestReplicatedLog> {
    auto persisted = std::make_shared<AsyncMockLog>(id);
    _persistedLogs[id] = persisted;
    auto core = std::make_unique<LogCore>(persisted);
    return std::make_shared<TestReplicatedLog>(std::move(core), _logMetricsMock,
                                               LoggerContext(Logger::FIXME));
  }

  auto defaultLogger() {
    return LoggerContext(Logger::REPLICATION2);
  }

  auto stopAsyncMockLogs() -> void {
    for (auto const& it : _persistedLogs) {
      if (auto log = std::dynamic_pointer_cast<AsyncMockLog>(it.second); log != nullptr) {
        log->stop();
      }
    }
  }

  std::unordered_map<LogId, std::shared_ptr<MockLog>> _persistedLogs;
  std::shared_ptr<ReplicatedLogMetricsMock> _logMetricsMock = std::make_shared<ReplicatedLogMetricsMock>();
};


}
