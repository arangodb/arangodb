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

#include <Basics/Guarded.h>
#include <Containers/ImmerMemoryPolicy.h>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>
#include <chrono>
#include <tuple>

#include "Replication2/ReplicatedLog/ILogParticipant.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/NetworkMessages.h"
#include "Replication2/ReplicatedLog/types.h"
#include "Replication2/LoggerContext.h"
#include "Basics/Result.h"
#include "Futures/Future.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogStatus.h"

namespace arangodb {
struct DeferredAction;
}  // namespace arangodb

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
}  // namespace arangodb::replication2::replicated_log

namespace arangodb::replication2::replicated_log {
struct PersistedLogIterator;

/**
 * @brief Leader instance of a replicated log.
 */
class LogLeader : public std::enable_shared_from_this<LogLeader>, public ILogParticipant {
 public:
  ~LogLeader() override;

  // Used in tests, forwards to overload below
  [[nodiscard]] static auto construct(
      LoggerContext const& logContext, std::shared_ptr<ReplicatedLogMetrics> logMetrics,
      ParticipantId id, std::unique_ptr<LogCore> logCore, LogTerm term,
      std::vector<std::shared_ptr<AbstractFollower>> const& followers,
      std::size_t writeConcern) -> std::shared_ptr<LogLeader>;

  [[nodiscard]] static auto construct(
      LogConfig config, std::unique_ptr<LogCore> logCore,
      std::vector<std::shared_ptr<AbstractFollower>> const& followers,
      ParticipantId id, LogTerm term, LoggerContext const& logContext,
      std::shared_ptr<ReplicatedLogMetrics> logMetrics) -> std::shared_ptr<LogLeader>;

  struct DoNotTriggerAsyncReplication {};
  constexpr static auto doNotTriggerAsyncReplication = DoNotTriggerAsyncReplication{};

  auto insert(LogPayload payload, bool waitForSync = false) -> LogIndex;

  // As opposed to the above insert methods, this one does not trigger the async
  // replication automatically, i.e. does not call triggerAsyncReplication after
  // the insert into the in-memory log. This is necessary for testing. It should
  // not be necessary in production code. It might seem useful for batching, but
  // in that case, it'd be even better to add an insert function taking a batch.
  //
  // This method will however not prevent the resulting log entry from being
  // replicated, if async replication is running in the background already, or
  // if it is triggered by someone else.
  auto insert(LogPayload payload, bool waitForSync, DoNotTriggerAsyncReplication) -> LogIndex;

  [[nodiscard]] auto waitFor(LogIndex) -> WaitForFuture override;

  [[nodiscard]] auto waitForIterator(LogIndex index) -> WaitForIteratorFuture override;

  [[nodiscard]] auto getReplicatedLogSnapshot() const -> InMemoryLog::log_type;

  [[nodiscard]] auto readReplicatedEntryByIndex(LogIndex idx) const
      -> std::optional<PersistingLogEntry>;

  // Triggers sending of appendEntries requests to all followers. This continues
  // until all participants are perfectly in sync, and will then stop.
  // Is usually called automatically after an insert, but can be called manually
  // from test code.
  auto triggerAsyncReplication() -> void;

  [[nodiscard]] auto getStatus() const -> LogStatus override;

  [[nodiscard]] auto resign() && -> std::tuple<std::unique_ptr<LogCore>, DeferredAction> override;

  [[nodiscard]] auto getParticipantId() const noexcept -> ParticipantId const&;

  [[nodiscard]] auto release(LogIndex doneWithIdx) -> Result override;

  [[nodiscard]] auto copyInMemoryLog() const -> InMemoryLog;

 protected:
  // Use the named constructor construct() to create a leader!
  LogLeader(LoggerContext logContext, std::shared_ptr<ReplicatedLogMetrics> logMetrics,
            LogConfig config, ParticipantId id, LogTerm term, InMemoryLog inMemoryLog);

 private:
  struct GuardedLeaderData;

  using Guard = MutexGuard<GuardedLeaderData, std::unique_lock<std::mutex>>;
  using ConstGuard = MutexGuard<GuardedLeaderData const, std::unique_lock<std::mutex>>;

  struct alignas(64) FollowerInfo {
    explicit FollowerInfo(std::shared_ptr<AbstractFollower> impl,
                          TermIndexPair lastLogIndex, LoggerContext const& logContext);

    std::chrono::steady_clock::duration _lastRequestLatency{};
    std::chrono::steady_clock::time_point _lastRequestStartTP{};
    std::chrono::steady_clock::time_point _errorBackoffEndTP{};
    std::shared_ptr<AbstractFollower> _impl;
    TermIndexPair lastAckedEntry = TermIndexPair{LogTerm{0}, LogIndex{0}};
    LogIndex lastAckedCommitIndex = LogIndex{0};
    LogIndex lastAckedLCI = LogIndex{0};
    MessageId lastSentMessageId{0};
    std::size_t numErrorsSinceLastAnswer = 0;
    AppendEntriesErrorReason lastErrorReason = AppendEntriesErrorReason::NONE;
    LoggerContext const logContext;

    enum class State {
      IDLE,
      PREPARE,
      ERROR_BACKOFF,
      REQUEST_IN_FLIGHT,
    } _state = State::IDLE;
  };

  struct LocalFollower final : AbstractFollower {
    // The LocalFollower assumes that the last entry of log core matches
    // lastIndex.
    LocalFollower(LogLeader& self, LoggerContext logContext,
                  std::unique_ptr<LogCore> logCore, TermIndexPair lastIndex);
    ~LocalFollower() override = default;

    LocalFollower(LocalFollower const&) = delete;
    LocalFollower(LocalFollower&&) noexcept = delete;
    auto operator=(LocalFollower const&) -> LocalFollower& = delete;
    auto operator=(LocalFollower&&) noexcept -> LocalFollower& = delete;

    [[nodiscard]] auto getParticipantId() const noexcept -> ParticipantId const& override;
    [[nodiscard]] auto appendEntries(AppendEntriesRequest request)
        -> arangodb::futures::Future<AppendEntriesResult> override;

    [[nodiscard]] auto resign() && noexcept -> std::unique_ptr<LogCore>;
    [[nodiscard]] auto release(LogIndex stop) const -> Result;

   private:
    LogLeader& _leader;
    LoggerContext const _logContext;
    Guarded<std::unique_ptr<LogCore>> _guardedLogCore;
  };

  struct PreparedAppendEntryRequest {
    PreparedAppendEntryRequest() = delete;
    PreparedAppendEntryRequest(std::shared_ptr<LogLeader> const& logLeader,
                               FollowerInfo& follower,
                               std::chrono::steady_clock::duration executionDelay);

    std::weak_ptr<LogLeader> _parentLog;
    // This weak_ptr will always be an alias of _parentLog. It's thus not
    // strictly necessary, but makes it more clear that we do share ownership
    // of this particular FollowerInfo.
    std::weak_ptr<FollowerInfo> _follower;
    std::chrono::steady_clock::duration _executionDelay;
  };

  struct ResolvedPromiseSet {
    WaitForQueue _set;
    WaitForResult result;
    ::immer::flex_vector<InMemoryLogEntry, arangodb::immer::arango_memory_policy> _commitedLogEntries;
  };

  struct alignas(128) GuardedLeaderData {
    ~GuardedLeaderData() = default;
    GuardedLeaderData(LogLeader& self, InMemoryLog inMemoryLog);

    GuardedLeaderData() = delete;
    GuardedLeaderData(GuardedLeaderData const&) = delete;
    GuardedLeaderData(GuardedLeaderData&&) = delete;
    auto operator=(GuardedLeaderData const&) -> GuardedLeaderData& = delete;
    auto operator=(GuardedLeaderData&&) -> GuardedLeaderData& = delete;

    [[nodiscard]] auto prepareAppendEntry(FollowerInfo& follower)
        -> std::optional<PreparedAppendEntryRequest>;
    [[nodiscard]] auto prepareAppendEntries()
        -> std::vector<std::optional<PreparedAppendEntryRequest>>;

    [[nodiscard]] auto handleAppendEntriesResponse(
        FollowerInfo& follower, TermIndexPair lastIndex, LogIndex currentCommitIndex,
        LogIndex currentLCI, LogTerm currentTerm, futures::Try<AppendEntriesResult>&& res,
        std::chrono::steady_clock::duration latency, MessageId messageId)
        -> std::pair<std::vector<std::optional<PreparedAppendEntryRequest>>, ResolvedPromiseSet>;

    [[nodiscard]] auto checkCommitIndex() -> ResolvedPromiseSet;
    [[nodiscard]] auto checkCompaction() -> Result;

    [[nodiscard]] auto updateCommitIndexLeader(LogIndex newIndex,
                                               std::shared_ptr<QuorumData> quorum)
        -> ResolvedPromiseSet;

    [[nodiscard]] auto getInternalLogIterator(LogIndex firstIdx) const
        -> std::unique_ptr<TypedLogIterator<InMemoryLogEntry>>;

    [[nodiscard]] auto getCommittedLogIterator(LogIndex firstIndex) const
        -> std::unique_ptr<LogRangeIterator>;

    [[nodiscard]] auto getLocalStatistics() const -> LogStatistics;

    [[nodiscard]] auto createAppendEntriesRequest(FollowerInfo& follower,
                                                  TermIndexPair const& lastAvailableIndex) const
        -> std::pair<AppendEntriesRequest, TermIndexPair>;

    [[nodiscard]] auto calculateCommitLag() const noexcept
        -> std::chrono::duration<double, std::milli>;

    LogLeader& _self;
    InMemoryLog _inMemoryLog;
    std::vector<FollowerInfo> _follower{};
    WaitForQueue _waitForQueue{};
    std::shared_ptr<QuorumData> _lastQuorum{};
    LogIndex _commitIndex{0};
    LogIndex _largestCommonIndex{0};
    LogIndex _releaseIndex{0};
    bool _didResign{false};
  };

  LoggerContext const _logContext;
  std::shared_ptr<ReplicatedLogMetrics> const _logMetrics;
  LogConfig const _config;
  ParticipantId const _id;
  LogTerm const _currentTerm;
  // _localFollower is const after construction
  std::shared_ptr<LocalFollower> _localFollower;
  // make this thread safe in the most simple way possible, wrap everything in
  // a single mutex.
  Guarded<GuardedLeaderData> _guardedLeaderData;

  [[nodiscard]] static auto instantiateFollowers(
      LoggerContext, std::vector<std::shared_ptr<AbstractFollower>> const& follower,
      std::shared_ptr<LocalFollower> const& localFollower,
      TermIndexPair lastEntry) -> std::vector<FollowerInfo>;

  auto acquireMutex() -> Guard;
  auto acquireMutex() const -> ConstGuard;

  static void executeAppendEntriesRequests(
      std::vector<std::optional<PreparedAppendEntryRequest>> requests,
      std::shared_ptr<ReplicatedLogMetrics> const& logMetrics);
  static void handleResolvedPromiseSet(ResolvedPromiseSet set,
                                       std::shared_ptr<ReplicatedLogMetrics> const& logMetrics);

  auto tryHardToClearQueue() noexcept -> void;
};

}  // namespace arangodb::replication2::replicated_log
