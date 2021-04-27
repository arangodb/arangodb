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
#include <mutex>

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/flex_vector.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

#include "PersistedLog.h"
#include "ReplicatedLog.h"

namespace arangodb::replication2 {
#if 0
struct LogInterface {
  virtual ~LogInterface() = default;
  virtual auto getPersistedLog() -> std::shared_ptr<PersistedLog> = 0;
  virtual auto getDirtyLogEntries(std::size_t limit) -> std::shared_ptr<LogIterator> = 0;
  virtual auto updatePersistedIndex(LogIndex newIndex) -> void = 0;
  virtual auto removeDirtyFlag() -> void = 0;
};

struct LogWorkerExecutor {
  virtual ~LogWorkerExecutor() = default;
  virtual void operator()(std::function<void()>) = 0;
};

struct LogManager {
  explicit LogManager(std::shared_ptr<LogWorkerExecutor> executor)
      : _executor(std::move(executor)) {}
  virtual ~LogManager() = default;
  void markLogAsDirty(std::shared_ptr<LogInterface> log);
 protected:
  virtual void workerEntryPoint() = 0;

 private:
  std::mutex _mutex;
  bool workerActive;
  immer::flex_vector<std::shared_ptr<LogInterface>> _logs;
  std::shared_ptr<LogWorkerExecutor> _executor;
};
#endif

struct LogWorkerExecutor {
  virtual ~LogWorkerExecutor() = default;
  virtual void operator()(fu2::unique_function<void()>) = 0;
};

struct LogManager;

struct LogManagerProxy : LogFollower {
  LogManagerProxy(const LogId& logId, ParticipantId id, std::shared_ptr<LogManager> manager);
  [[nodiscard]] auto participantId() const noexcept -> ParticipantId override;
  auto appendEntries(AppendEntriesRequest request)
      -> arangodb::futures::Future<AppendEntriesResult> override;

  [[nodiscard]] auto getLogId() const noexcept -> LogId { return _logId; }
 private:
  LogId _logId;
  ParticipantId _id;
  std::shared_ptr<LogManager> _manager;
};

struct LogManager : std::enable_shared_from_this<LogManager> {
  virtual ~LogManager() = default;
  explicit LogManager(std::shared_ptr<LogWorkerExecutor> executor);

  auto appendEntries(AppendEntriesRequest, LogId) -> arangodb::futures::Future<AppendEntriesResult>;

 protected:
  virtual void workerEntryPoint();
  virtual auto getPersistedLogById(LogId id) -> std::shared_ptr<PersistedLog> = 0;
 private:
  using ResultPromise = arangodb::futures::Promise<AppendEntriesResult>;

  struct RequestRecord {
    RequestRecord(AppendEntriesRequest request, LogId logId) : request(std::move(request)), logId(logId) {}

    AppendEntriesRequest request;
    ResultPromise promise;
    LogId logId;
  };
  using RequestVector = std::vector<RequestRecord>;
  std::mutex _mutex;
  bool _isWorkerActive = false;
  RequestVector _requests;
  std::shared_ptr<LogWorkerExecutor> _executor;
};

}  // namespace arangodb::replication2
