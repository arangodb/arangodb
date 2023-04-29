////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Basics/Guarded.h>
#include <Containers/ImmerMemoryPolicy.h>
#include <chrono>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "Basics/Result.h"
#include "Futures/Future.h"
#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/NetworkMessages.h"
#include "Replication2/ReplicatedLog/WaitForBag.h"
#include "Replication2/ReplicatedLog/types.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Scheduler/Scheduler.h"

namespace arangodb {
struct DeferredAction;
}  // namespace arangodb

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of
// data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift
// intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/flex_vector.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

namespace arangodb::futures {
template<typename T>
class Try;
}
namespace arangodb::cluster {
struct IFailureOracle;
}
namespace arangodb::replication2::algorithms {
struct ParticipantState;
}
namespace arangodb::replication2::replicated_log {
struct LogCore;
struct ReplicatedLogMetrics;
}  // namespace arangodb::replication2::replicated_log

namespace arangodb::replication2::replicated_log {

/**
 * @brief Leader instance of a replicated log.
 */
class LogLeader : public std::enable_shared_from_this<LogLeader>,
                  public ILogLeader {
 public:
  ~LogLeader() override;

  [[nodiscard]] static auto construct(
      std::unique_ptr<LogCore> logCore,
      std::shared_ptr<agency::ParticipantsConfig const> participantsConfig,
      ParticipantId id, LogTerm term, LoggerContext const& logContext,
      std::shared_ptr<ReplicatedLogMetrics> logMetrics,
      std::shared_ptr<ReplicatedLogGlobalSettings const> options,
      std::shared_ptr<IReplicatedStateHandle>,
      std::shared_ptr<IAbstractFollowerFactory> followerFactory)
      -> std::shared_ptr<LogLeader>;

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
  struct DoNotTriggerAsyncReplication {};
  static constexpr DoNotTriggerAsyncReplication doNotTriggerAsyncReplication{};
  auto insert(LogPayload payload, bool waitForSync,
              DoNotTriggerAsyncReplication) -> LogIndex;

  [[nodiscard]] auto waitFor(LogIndex) -> WaitForFuture override;

  [[nodiscard]] auto waitForIterator(LogIndex index)
      -> WaitForIteratorFuture override;

  [[nodiscard]] auto getReplicatedLogSnapshot() const -> InMemoryLog::log_type;

  [[nodiscard]] auto readReplicatedEntryByIndex(LogIndex idx) const
      -> std::optional<PersistingLogEntry>;

  // Triggers sending of appendEntries requests to all followers. This continues
  // until all participants are perfectly in sync, and will then stop.
  // Is usually called automatically after an insert, but can be called manually
  // from test code.
  auto triggerAsyncReplication() -> void;

  [[nodiscard]] auto getStatus() const -> LogStatus override;

  [[nodiscard]] auto getQuickStatus() const -> QuickLogStatus override;

  [[nodiscard]] auto
  resign() && -> std::tuple<std::unique_ptr<LogCore>, DeferredAction> override;

  [[nodiscard]] auto getParticipantId() const noexcept -> ParticipantId const&;

  [[nodiscard]] auto release(LogIndex doneWithIdx) -> Result override;
  [[nodiscard]] auto compact() -> ResultT<CompactionResult> override;

  [[nodiscard]] auto copyInMemoryLog() const -> InMemoryLog override;

  // Returns true if the leader has established its leadership: at least one
  // entry within its term has been committed.
  [[nodiscard]] auto isLeadershipEstablished() const noexcept -> bool;

  auto waitForLeadership() -> WaitForFuture override;
  auto ping(std::optional<std::string> message) -> LogIndex override;

  // This function returns the current commit index. Do NOT poll this function,
  // use waitFor(idx) instead. This function is used in tests.
  [[nodiscard]] auto getCommitIndex() const noexcept -> LogIndex;

  // Updates the flags of the participants.
  auto updateParticipantsConfig(
      std::shared_ptr<agency::ParticipantsConfig const> const& config)
      -> LogIndex override;

  // Returns [acceptedConfig.generation, committedConfig.?generation]
  auto getParticipantConfigGenerations() const noexcept
      -> std::pair<std::size_t, std::optional<std::size_t>>;

  auto setSnapshotAvailable(ParticipantId const& participantId,
                            SnapshotAvailableReport report) -> Result;

 protected:
  // Use the named constructor construct() to create a leader!
  LogLeader(LoggerContext logContext,
            std::shared_ptr<ReplicatedLogMetrics> logMetrics,
            std::shared_ptr<ReplicatedLogGlobalSettings const> options,
            ParticipantId id, LogTerm term, LogIndex firstIndexOfCurrentTerm,
            InMemoryLog inMemoryLog, std::shared_ptr<IReplicatedStateHandle>,
            std::shared_ptr<IAbstractFollowerFactory> followerFactory);

 private:
  struct GuardedLeaderData;

  using Guard = MutexGuard<GuardedLeaderData, std::unique_lock<std::mutex>>;
  using ConstGuard =
      MutexGuard<GuardedLeaderData const, std::unique_lock<std::mutex>>;

  struct alignas(64) FollowerInfo {
    explicit FollowerInfo(std::shared_ptr<AbstractFollower> impl,
                          LogIndex lastLogIndex,
                          LoggerContext const& logContext);

    std::chrono::steady_clock::duration _lastRequestLatency{};
    std::chrono::steady_clock::time_point _lastRequestStartTP{};
    std::chrono::steady_clock::time_point _errorBackoffEndTP{};
    std::shared_ptr<AbstractFollower> _impl;
    TermIndexPair lastAckedIndex = TermIndexPair{LogTerm{0}, LogIndex{0}};
    LogIndex nextPrevLogIndex = LogIndex{0};
    LogIndex lastAckedCommitIndex = LogIndex{0};
    LogIndex lastAckedLowestIndexToKeep = LogIndex{0};
    MessageId lastSentMessageId{0};
    std::size_t numErrorsSinceLastAnswer = 0;
    AppendEntriesErrorReason lastErrorReason;
    bool snapshotAvailable{true};
    MessageId snapshotAvailableMessageId;
    LoggerContext const logContext;
    Scheduler::WorkHandle lastRequestHandle;

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

    [[nodiscard]] auto getParticipantId() const noexcept
        -> ParticipantId const& override;
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
    PreparedAppendEntryRequest(
        std::shared_ptr<LogLeader> const& logLeader,
        std::shared_ptr<FollowerInfo> follower,
        std::chrono::steady_clock::duration executionDelay);

    std::weak_ptr<LogLeader> _parentLog;
    std::weak_ptr<FollowerInfo> _follower;
    std::chrono::steady_clock::duration _executionDelay;
  };

  struct ResolvedPromiseSet {
    LogIndex commitIndex;
    WaitForQueue _set;
    WaitForResult result;
    ::immer::flex_vector<InMemoryLogEntry,
                         arangodb::immer::arango_memory_policy>
        _commitedLogEntries;
  };

  struct alignas(128) GuardedLeaderData {
    ~GuardedLeaderData() = default;
    GuardedLeaderData(LogLeader& self, InMemoryLog inMemoryLog);

    GuardedLeaderData() = delete;
    GuardedLeaderData(GuardedLeaderData const&) = delete;
    GuardedLeaderData(GuardedLeaderData&&) = delete;
    auto operator=(GuardedLeaderData const&) -> GuardedLeaderData& = delete;
    auto operator=(GuardedLeaderData&&) -> GuardedLeaderData& = delete;

    [[nodiscard]] auto prepareAppendEntry(
        std::shared_ptr<FollowerInfo> follower)
        -> std::optional<PreparedAppendEntryRequest>;
    [[nodiscard]] auto prepareAppendEntries()
        -> std::vector<std::optional<PreparedAppendEntryRequest>>;

    [[nodiscard]] auto handleAppendEntriesResponse(
        FollowerInfo& follower, TermIndexPair lastIndex,
        LogIndex currentCommitIndex, LogIndex currentLITK, LogTerm currentTerm,
        futures::Try<AppendEntriesResult>&& res,
        std::chrono::steady_clock::duration latency, MessageId messageId)
        -> std::pair<std::vector<std::optional<PreparedAppendEntryRequest>>,
                     ResolvedPromiseSet>;

    [[nodiscard]] auto checkCommitIndex() -> ResolvedPromiseSet;

    [[nodiscard]] auto collectFollowerStates() const
        -> std::pair<LogIndex, std::vector<algorithms::ParticipantState>>;
    [[nodiscard]] auto checkCompaction() -> ResultT<CompactionResult>;
    [[nodiscard]] auto runCompaction(LogIndex compactionStop)
        -> ResultT<CompactionResult>;

    [[nodiscard]] auto updateCommitIndexLeader(
        LogIndex newIndex, std::shared_ptr<QuorumData> quorum)
        -> ResolvedPromiseSet;

    [[nodiscard]] auto getInternalLogIterator(LogIndex firstIdx) const
        -> std::unique_ptr<TypedLogIterator<InMemoryLogEntry>>;

    [[nodiscard]] auto getCommittedLogIterator(LogIndex firstIndex) const
        -> std::unique_ptr<LogRangeIterator>;

    [[nodiscard]] auto getLocalStatistics() const -> LogStatistics;

    [[nodiscard]] auto createAppendEntriesRequest(
        FollowerInfo& follower, TermIndexPair const& lastAvailableIndex) const
        -> std::pair<AppendEntriesRequest, TermIndexPair>;

    [[nodiscard]] auto calculateCommitLag() const noexcept
        -> std::chrono::duration<double, std::milli>;

    auto insertInternal(
        std::variant<LogMetaPayload, LogPayload>, bool waitForSync,
        std::optional<InMemoryLogEntry::clock::time_point> insertTp)
        -> LogIndex;

    [[nodiscard]] auto waitForResign()
        -> std::pair<futures::Future<futures::Unit>, DeferredAction>;

    LogLeader& _self;
    InMemoryLog _inMemoryLog;
    std::unordered_map<ParticipantId, std::shared_ptr<FollowerInfo>>
        _follower{};
    WaitForQueue _waitForQueue{};
    WaitForBag _waitForResignQueue;
    std::shared_ptr<QuorumData> _lastQuorum{};
    LogIndex _commitIndex{0};
    LogIndex _lowestIndexToKeep{0};
    LogIndex _releaseIndex{0};
    bool _didResign{false};
    bool _leadershipEstablished{false};
    CommitFailReason _lastCommitFailReason;

    // active - that is currently used to check for committed entries
    std::shared_ptr<agency::ParticipantsConfig const> activeParticipantsConfig;
    // committed - latest active config that has committed at least one entry
    // Note that this will be nullptr until leadership is established!
    std::shared_ptr<agency::ParticipantsConfig const>
        committedParticipantsConfig;
  };

  LoggerContext const _logContext;
  std::shared_ptr<ReplicatedLogMetrics> const _logMetrics;
  std::shared_ptr<ReplicatedLogGlobalSettings const> const _options;
  std::shared_ptr<IReplicatedStateHandle> _stateHandle;
  std::shared_ptr<IAbstractFollowerFactory> const _followerFactory;
  ParticipantId const _id;
  LogTerm const _currentTerm;
  LogIndex const _firstIndexOfCurrentTerm;
  // _localFollower is const after construction
  std::shared_ptr<LocalFollower> _localFollower;
  // make this thread safe in the most simple way possible, wrap everything in
  // a single mutex.
  Guarded<GuardedLeaderData> _guardedLeaderData;

  void establishLeadership(
      std::shared_ptr<agency::ParticipantsConfig const> config);

  [[nodiscard]] static auto instantiateFollowers(
      LoggerContext const& logContext,
      std::shared_ptr<IAbstractFollowerFactory> followerFactory,
      std::shared_ptr<LocalFollower> const& localFollower,
      TermIndexPair lastEntry,
      std::shared_ptr<agency::ParticipantsConfig const> const&
          participantsConfig)
      -> std::unordered_map<ParticipantId, std::shared_ptr<FollowerInfo>>;

  auto acquireMutex() -> Guard;
  auto acquireMutex() const -> ConstGuard;

  static void executeAppendEntriesRequests(
      std::vector<std::optional<PreparedAppendEntryRequest>> requests,
      std::shared_ptr<ReplicatedLogMetrics> const& logMetrics);
  static void handleResolvedPromiseSet(
      ResolvedPromiseSet set,
      std::shared_ptr<ReplicatedLogMetrics> const& logMetrics);
};

}  // namespace arangodb::replication2::replicated_log
