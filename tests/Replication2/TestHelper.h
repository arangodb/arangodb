
#ifndef ARANGODB3_TESTHELPER_H
#define ARANGODB3_TESTHELPER_H

#include <gtest/gtest.h>

#include "Replication2/ReplicatedLog.h"
#include "Replication2/LogManager.h"
#include "MockLog.h"

namespace arangodb::replication2 {
struct DelayedFollowerLog : ReplicatedLog {
  using ReplicatedLog::ReplicatedLog;
  auto appendEntries(AppendEntriesRequest)
      -> arangodb::futures::Future<AppendEntriesResult> override;
  void runAsyncAppendEntries();

  using WaitForAsyncPromise = futures::Promise<std::optional<AppendEntriesResult>>;

  struct AsyncRequest {
    explicit AsyncRequest(AppendEntriesRequest request)
        : request(std::move(request)) {}
    AppendEntriesRequest request;
    WaitForAsyncPromise promise;
  };
  [[nodiscard]] auto pendingAppendEntries() const -> std::deque<AsyncRequest> const& { return _asyncQueue; }
  [[nodiscard]] auto hasPendingAppendEntries() const -> bool { return !_asyncQueue.empty(); }

 private:
  std::deque<AsyncRequest> _asyncQueue;
};

struct LogTestBase;

struct MockExecutor : LogWorkerExecutor {
  void operator()(fu2::unique_function<void()> func) override {
    std::unique_lock guard(_mutex);
    _actions.emplace_back(std::move(func));
  }

  void executeAllActions() {
    while (true) {
      std::vector<fu2::unique_function<void()>> actions;
      {
        std::unique_lock guard(_mutex);
        if (_actions.empty()) {
          break;
        }
        std::swap(actions, _actions);
      }

      for (auto& action : actions) {
        action();
      }
    }
  }

  [[nodiscard]] auto hasPendingActions() const noexcept -> bool {
    std::unique_lock guard(_mutex);
    return _actions.empty();
  }

  std::mutex mutable _mutex;
  std::vector<fu2::unique_function<void()>> _actions;
};

struct MockLogManager : LogManager {

  using LogManager::LogManager;

  std::shared_ptr<PersistedLog> getPersistedLogById(LogId id) override {
    if (auto iter = _mocksById.find(id); iter != _mocksById.end()) {
      return iter->second;
    }
    return nullptr;
  }

  std::unordered_map<LogId, std::shared_ptr<MockLog>> _mocksById;
};

struct LogTestBase : ::testing::Test {

  LogTestBase() {
    _executor = std::make_shared<MockExecutor>();
    _manager = std::make_shared<MockLogManager>(_executor);
  }

  auto getNextLogId() -> LogId {
    return _nextLogId = LogId{_nextLogId.id() + 1};
  }

  auto createPersistedLog(LogId id) -> std::shared_ptr<MockLog> {
    auto log = std::make_shared<MockLog>(id);
    _manager->_mocksById[id] = log;
    return log;
  }

  auto addLogInstance(ParticipantId const& id) -> std::pair<std::shared_ptr<ReplicatedLog>, std::shared_ptr<LogManagerProxy>> {
    auto const state = std::make_shared<InMemoryState>(InMemoryState::state_container{});
    auto logId = getNextLogId();
    auto persistedLog = createPersistedLog(logId);
    auto log = std::make_shared<ReplicatedLog>(id, state, persistedLog);
    auto local = std::make_shared<LogManagerProxy>(logId, id, _manager);
    return std::make_pair(log, local);
  }

  auto addFollowerLogInstance(ParticipantId const& id) -> std::shared_ptr<DelayedFollowerLog> {
    auto const state = std::make_shared<InMemoryState>(InMemoryState::state_container{});
    auto persistedLog = createPersistedLog(getNextLogId());
    auto log = std::make_shared<DelayedFollowerLog>(id, state, persistedLog);
    return log;
  }


  LogId _nextLogId{0};
  std::shared_ptr<MockExecutor> _executor;
  std::shared_ptr<MockLogManager> _manager;
};

}



#endif  // ARANGODB3_TESTHELPER_H
