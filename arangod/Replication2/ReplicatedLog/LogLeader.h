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
#include "Replication2/ReplicatedLog/LogParticipantI.h"
#include "Replication2/ReplicatedLog/types.h"

#include "Replication2/LogContext.h"

#include <Basics/Guarded.h>

#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

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

namespace arangodb::futures {
template <typename T>
class Try;
}

namespace arangodb::replication2::replicated_log {
struct LogCore;
struct ReplicatedLogMetrics;
}

namespace arangodb::replication2::replicated_log {

/**
 * @brief Leader instance of a replicated log.
 */
class LogLeader : public std::enable_shared_from_this<LogLeader>, public LogParticipantI {
 public:
  ~LogLeader() override;

  static auto construct(LogContext const& logContext, ReplicatedLogMetrics& logMetrics,
                        ParticipantId id, std::unique_ptr<LogCore> logCore, LogTerm term,
                        std::vector<std::shared_ptr<AbstractFollower>> const& followers,
                        std::size_t writeConcern) -> std::shared_ptr<LogLeader>;

  auto insert(LogPayload) -> LogIndex;

  [[nodiscard]] auto waitFor(LogIndex) -> WaitForFuture override;

  [[nodiscard]] auto waitForIterator(LogIndex index) -> WaitForIteratorFuture override;

  [[nodiscard]] auto getReplicatedLogSnapshot() const -> immer::flex_vector<LogEntry>;

  [[nodiscard]] auto readReplicatedEntryByIndex(LogIndex idx) const -> std::optional<LogEntry>;

  auto runAsyncStep() -> void;

  [[nodiscard]] auto getStatus() const -> LogStatus override;

  auto resign() && -> std::unique_ptr<LogCore> override;

  [[nodiscard]] auto getParticipantId() const noexcept -> ParticipantId const&;

 protected:
  // Use the named constructor construct() to create a leader!
  LogLeader(LogContext logContext, ParticipantId id, LogTerm term,
            std::size_t writeConcern, InMemoryLog inMemoryLog);

 private:
  struct GuardedLeaderData;
  using Guard = MutexGuard<GuardedLeaderData, std::unique_lock<std::mutex>>;
  using ConstGuard = MutexGuard<GuardedLeaderData const, std::unique_lock<std::mutex>>;

  struct alignas(64) FollowerInfo {
    explicit FollowerInfo(std::shared_ptr<AbstractFollower> impl, LogIndex lastLogIndex, LogContext logContext);

    std::chrono::steady_clock::duration _lastRequestLatency;
    std::shared_ptr<AbstractFollower> _impl;
    LogIndex lastAckedIndex = LogIndex{0};
    LogIndex lastAckedCommitIndex = LogIndex{0};
    MessageId lastSendMessageId{0};
    std::size_t numErrorsSinceLastAnswer = 0;
    AppendEntriesErrorReason lastErrorReason = AppendEntriesErrorReason::NONE;
    LogContext const logContext;
    bool requestInFlight = false;
  };

  struct LocalFollower final : AbstractFollower {
    LocalFollower(LogLeader& self, LogContext logContext, std::unique_ptr<LogCore> logCore);
    ~LocalFollower() override = default;

    LocalFollower(LocalFollower const&) = delete;
    LocalFollower(LocalFollower&&) noexcept = delete;
    auto operator=(LocalFollower const&) -> LocalFollower& = delete;
    auto operator=(LocalFollower&&) noexcept -> LocalFollower& = delete;

    [[nodiscard]] auto getParticipantId() const noexcept -> ParticipantId const& override;
    auto appendEntries(AppendEntriesRequest request)
        -> arangodb::futures::Future<AppendEntriesResult> override;

    auto resign() && -> std::unique_ptr<LogCore>;

   private:
    LogLeader& _self;
    LogContext const _logContext;
    Guarded<std::unique_ptr<LogCore>> _guardedLogCore;
  };

  struct PreparedAppendEntryRequest {
    // TODO Write a constructor, delete the default constructor
    FollowerInfo* _follower;
    AppendEntriesRequest _request;
    std::weak_ptr<LogLeader> _parentLog;
    LogIndex _lastIndex;
    LogIndex _currentCommitIndex;
    LogTerm _currentTerm;
  };

  struct ResolvedPromiseSet {
    WaitForQueue _set;
    std::shared_ptr<QuorumData> _quorum;
  };

  struct alignas(128) GuardedLeaderData {
    ~GuardedLeaderData() = default;
    GuardedLeaderData(LogLeader const& self, InMemoryLog inMemoryLog);

    GuardedLeaderData() = delete;
    GuardedLeaderData(GuardedLeaderData const&) = delete;
    GuardedLeaderData(GuardedLeaderData&&) = delete;
    auto operator=(GuardedLeaderData const&) -> GuardedLeaderData& = delete;
    auto operator=(GuardedLeaderData&&) -> GuardedLeaderData& = delete;

    [[nodiscard]] auto prepareAppendEntry(std::weak_ptr<LogLeader> const& parentLog,
                                          FollowerInfo& follower)
        -> std::optional<PreparedAppendEntryRequest>;
    [[nodiscard]] auto prepareAppendEntries(std::weak_ptr<LogLeader> const& parentLog)
        -> std::vector<std::optional<PreparedAppendEntryRequest>>;

    [[nodiscard]] auto handleAppendEntriesResponse(
        std::weak_ptr<LogLeader> const& parentLog, FollowerInfo& follower,
        LogIndex lastIndex, LogIndex currentCommitIndex, LogTerm currentTerm,
        futures::Try<AppendEntriesResult>&& res, std::chrono::steady_clock::duration latency,
        MessageId messageId)
        -> std::pair<std::vector<std::optional<PreparedAppendEntryRequest>>, ResolvedPromiseSet>;

    [[nodiscard]] auto checkCommitIndex(std::weak_ptr<LogLeader> const& parentLog)
        -> ResolvedPromiseSet;

    [[nodiscard]] auto updateCommitIndexLeader(std::weak_ptr<LogLeader> const& parentLog, LogIndex newIndex,
                                 std::shared_ptr<QuorumData> const& quorum) -> ResolvedPromiseSet;

    [[nodiscard]] auto getLogIterator(LogIndex fromIdx) const
        -> std::unique_ptr<LogIterator>;

    [[nodiscard]] auto getLocalStatistics() const -> LogStatistics;

    LogLeader const& _self;
    InMemoryLog _inMemoryLog{};
    std::vector<FollowerInfo> _follower{};
    WaitForQueue _waitForQueue{};
    std::shared_ptr<QuorumData> _lastQuorum{};
    LogIndex _commitIndex{0};
    bool _didResign{false};
  };

  LogContext const _logContext;
  ParticipantId const _participantId;
  LogTerm const _currentTerm;
  std::size_t const _writeConcern;
  // _localFollower is const after construction
  std::shared_ptr<LocalFollower> _localFollower;
  // make this thread safe in the most simple way possible, wrap everything in
  // a single mutex.
  Guarded<GuardedLeaderData> _guardedLeaderData;

  static auto instantiateFollowers(LogContext, std::vector<std::shared_ptr<AbstractFollower>> const& follower,
                                   std::shared_ptr<LocalFollower> const& localFollower,
                                   LogIndex lastIndex) -> std::vector<FollowerInfo>;

  auto acquireMutex() -> Guard;
  auto acquireMutex() const -> ConstGuard;

  static void executeAppendEntriesRequests(std::vector<std::optional<PreparedAppendEntryRequest>> requests);

  auto tryHardToClearQueue() noexcept -> void;
};

}  // namespace arangodb::replication2::replicated_log
