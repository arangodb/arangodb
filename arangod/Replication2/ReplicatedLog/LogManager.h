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

#include "Replication2/ReplicatedLog/types.h"

#include <Futures/Promise.h>

#include <function2.hpp>

#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace arangodb::replication2 {

struct LogWorkerExecutor {
  virtual ~LogWorkerExecutor() = default;
  virtual void operator()(fu2::unique_function<void()>) = 0;
};

struct LogManager;

struct LogManagerProxy : arangodb::replication2::replicated_log::AbstractFollower {
  LogManagerProxy(const LogId& logId, ParticipantId id, std::shared_ptr<LogManager> manager);
  [[nodiscard]] auto getParticipantId() const noexcept -> ParticipantId const& override;
  auto appendEntries(replicated_log::AppendEntriesRequest request)
      -> arangodb::futures::Future<replicated_log::AppendEntriesResult> override;

  [[nodiscard]] auto getLogId() const noexcept -> LogId { return _logId; }
 private:
  LogId _logId;
  ParticipantId _id;
  std::shared_ptr<LogManager> _manager;
};

struct LogManager : std::enable_shared_from_this<LogManager> {
  virtual ~LogManager() = default;
  explicit LogManager(std::shared_ptr<LogWorkerExecutor> executor);

  auto appendEntries(replicated_log::AppendEntriesRequest, LogId) -> arangodb::futures::Future<replicated_log::AppendEntriesResult>;

 protected:
  virtual void workerEntryPoint();
  // virtual auto getPersistedLogById(LogId id) -> std::shared_ptr<PersistedLog> = 0;
 private:
  using ResultPromise = arangodb::futures::Promise<replicated_log::AppendEntriesResult>;

  struct RequestRecord {
    RequestRecord(replicated_log::AppendEntriesRequest request, LogId logId) : request(std::move(request)), logId(logId) {}

    replicated_log::AppendEntriesRequest request;
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
