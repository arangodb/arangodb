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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication2/ReplicatedLog/Common.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogParticipantI.h"
#include "Replication2/ReplicatedLog/types.h"

#include <Basics/Guarded.h>
#include <Futures/Future.h>

#include <Replication2/ReplicatedLogMetrics.h>
#include <map>
#include <memory>
#include <mutex>

namespace arangodb::replication2::replicated_log {

/**
 * @brief Follower isntance of a replicated log.
 */
class LogFollower : public LogParticipantI, public AbstractFollower {
 public:
  ~LogFollower() override = default;
  LogFollower(ReplicatedLogMetrics& logMetrics, ParticipantId id,
              std::unique_ptr<LogCore> logCore, LogTerm term,
              ParticipantId leaderId, InMemoryLog inMemoryLog);

  // follower only
  auto appendEntries(AppendEntriesRequest) -> futures::Future<AppendEntriesResult> override;

  [[nodiscard]] auto getStatus() const -> LogStatus override;
  auto resign() && -> std::unique_ptr<LogCore> override;

  [[nodiscard]] auto waitFor(LogIndex) -> WaitForFuture override;

  [[nodiscard]] auto getParticipantId() const noexcept -> ParticipantId const& override;

 private:
  struct GuardedFollowerData;
  using Guard = MutexGuard<GuardedFollowerData, std::unique_lock<std::mutex>>;
  using ConstGuard = MutexGuard<GuardedFollowerData const, std::unique_lock<std::mutex>>;

  struct GuardedFollowerData {
    GuardedFollowerData() = delete;
    GuardedFollowerData(LogFollower const& self, std::unique_ptr<LogCore> logCore,
                        InMemoryLog inMemoryLog);

    [[nodiscard]] auto getLocalStatistics() const -> LogStatistics;

    [[nodiscard]] auto waitFor(LogIndex) -> WaitForFuture;

    LogFollower const& _self;
    InMemoryLog _inMemoryLog;
    std::unique_ptr<LogCore> _logCore;
    std::multimap<LogIndex, WaitForPromise> _waitForQueue{};
    LogIndex _commitIndex{0};
    MessageId _lastRecvMessageId{0};
  };
  [[maybe_unused]] ReplicatedLogMetrics& _logMetrics;
  ParticipantId const _participantId;
  ParticipantId const _leaderId;
  LogTerm const _currentTerm;
  Guarded<GuardedFollowerData> _guardedFollowerData;

  auto acquireMutex() -> Guard;
  auto acquireMutex() const -> ConstGuard;
};

}  // namespace arangodb::replication2::replicated_log
