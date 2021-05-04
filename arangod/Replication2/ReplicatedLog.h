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

#ifndef ARANGOD_REPLICATION2_REPLICATEDLOG_H
#define ARANGOD_REPLICATION2_REPLICATEDLOG_H

#include <Basics/Guarded.h>
#include <Basics/voc-errors.h>
#include <Futures/Future.h>
#include <velocypack/Builder.h>
#include <velocypack/SharedSlice.h>

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/map.hpp>
#include <immer/flex_vector.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

#include <deque>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Replication2/PersistedLog.h"

#include "Common.h"

namespace arangodb::replication2 {

enum class AppendEntriesErrorReason {
  NONE,
  INVALID_LEADER_ID,
  LOST_LOG_CORE,
  WRONG_TERM,
  NO_PREV_LOG_MATCH
};

std::string to_string(AppendEntriesErrorReason reason);

struct AppendEntriesResult {
  LogTerm const logTerm = LogTerm{};
  ErrorCode const errorCode = TRI_ERROR_NO_ERROR;
  AppendEntriesErrorReason const reason = AppendEntriesErrorReason::NONE;

  [[nodiscard]] auto isSuccess() const noexcept -> bool {
    return errorCode == TRI_ERROR_NO_ERROR;
  }

  explicit AppendEntriesResult(LogTerm);
  AppendEntriesResult(LogTerm logTerm, ErrorCode errorCode, AppendEntriesErrorReason reason);
  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> AppendEntriesResult;
};

struct AppendEntriesRequest {
  LogTerm leaderTerm;
  ParticipantId leaderId;
  // TODO assert index == 0 <=> term == 0
  LogTerm prevLogTerm;
  LogIndex prevLogIndex;
  LogIndex leaderCommit;
  immer::flex_vector<LogEntry> entries{};

  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> AppendEntriesRequest;
};

struct LogStatistics {
  LogIndex spearHead{};
  LogIndex commitIndex{};

  void toVelocyPack(velocypack::Builder& builder) const;
};

struct LeaderStatus {
  struct FollowerStatistics : LogStatistics {
    AppendEntriesErrorReason lastErrorReason;
    void toVelocyPack(velocypack::Builder& builder) const;
  };

  LogStatistics local;
  LogTerm term;
  std::unordered_map<ParticipantId, FollowerStatistics> follower;

  void toVelocyPack(velocypack::Builder& builder) const;
};

struct FollowerStatus {
  LogStatistics local;
  ParticipantId leader;
  LogTerm term;

  void toVelocyPack(velocypack::Builder& builder) const;
};

struct UnconfiguredStatus {
  void toVelocyPack(velocypack::Builder& builder) const;
};

using LogStatus = std::variant<UnconfiguredStatus, LeaderStatus, FollowerStatus>;

struct AbstractFollower {
  virtual ~AbstractFollower() = default;
  [[nodiscard]] virtual auto getParticipantId() const noexcept -> ParticipantId const& = 0;
  virtual auto appendEntries(AppendEntriesRequest)
      -> arangodb::futures::Future<AppendEntriesResult> = 0;
};

struct QuorumData {
  QuorumData(const LogIndex& index, LogTerm term, std::vector<ParticipantId> quorum);

  LogIndex index;
  LogTerm term;
  std::vector<ParticipantId> quorum;
};

namespace replicated_log {

struct alignas(64) LogCore {
  explicit LogCore(std::shared_ptr<PersistedLog> persistedLog);

  // There must only be one LogCore per physical log
  LogCore() = delete;
  LogCore(LogCore const&) = delete;
  LogCore(LogCore&&) = delete;
  auto operator=(LogCore const&) -> LogCore& = delete;
  auto operator=(LogCore&&) -> LogCore& = delete;
  std::shared_ptr<PersistedLog> const _persistedLog;
};

struct InMemoryLog {
  [[nodiscard]] auto getLastIndex() const -> LogIndex;
  [[nodiscard]] auto getNextIndex() const -> LogIndex;
  [[nodiscard]] auto getEntryByIndex(LogIndex idx) const -> std::optional<LogEntry>;

  immer::flex_vector<LogEntry> _log{};
};

struct LogParticipantI {
  [[nodiscard]] virtual auto getStatus() const -> LogStatus = 0;
  virtual ~LogParticipantI() = default;
  [[nodiscard]] virtual auto resign() && -> std::unique_ptr<LogCore> = 0;

  using WaitForPromise = futures::Promise<std::shared_ptr<QuorumData>>;
  using WaitForFuture = futures::Future<std::shared_ptr<QuorumData>>;
  using WaitForQueue = std::multimap<LogIndex, WaitForPromise>;

  [[nodiscard]] virtual auto waitFor(LogIndex index) -> WaitForFuture = 0;

  using WaitForIteratorFuture = futures::Future<std::unique_ptr<LogIterator>>;
  [[nodiscard]] virtual auto waitForIterator(LogIndex index) -> WaitForIteratorFuture;
};

class LogLeader : public std::enable_shared_from_this<LogLeader>, public LogParticipantI {
 public:
  ~LogLeader() override = default;

  static auto construct(ParticipantId id,
                        std::unique_ptr<LogCore> logCore, LogTerm term,
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
  LogLeader(ParticipantId id, LogTerm term, std::size_t writeConcern,
            InMemoryLog inMemoryLog);

 private:
  struct GuardedLeaderData;
  using Guard = MutexGuard<GuardedLeaderData, std::unique_lock<std::mutex>>;
  using ConstGuard = MutexGuard<GuardedLeaderData const, std::unique_lock<std::mutex>>;

  struct alignas(64) FollowerInfo {
    explicit FollowerInfo(std::shared_ptr<AbstractFollower> impl, LogIndex lastLogIndex)
        : _impl(std::move(impl)), lastAckedIndex(lastLogIndex) {}

    std::shared_ptr<AbstractFollower> _impl;
    LogIndex lastAckedIndex = LogIndex{0};
    LogIndex lastAckedCommitIndex = LogIndex{0};
    std::size_t numErrorsSinceLastAnswer = 0;
    AppendEntriesErrorReason lastErrorReason = AppendEntriesErrorReason::NONE;
    bool requestInFlight = false;
  };

  struct LocalFollower final : AbstractFollower {
    LocalFollower(LogLeader& self, std::unique_ptr<LogCore> logCore);
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
        futures::Try<AppendEntriesResult>&& res)
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

  ParticipantId const _participantId;
  LogTerm const _currentTerm;
  std::size_t const _writeConcern;
  // _localFollower is const after construction
  std::shared_ptr<LocalFollower> _localFollower;
  // make this thread safe in the most simple way possible, wrap everything in
  // a single mutex.
  Guarded<GuardedLeaderData> _guardedLeaderData;

  static auto instantiateFollowers(std::vector<std::shared_ptr<AbstractFollower>> const& follower,
                                   std::shared_ptr<LocalFollower> const& localFollower,
                                   LogIndex lastIndex) -> std::vector<FollowerInfo>;

  auto acquireMutex() -> Guard;
  auto acquireMutex() const -> ConstGuard;

  static void executeAppendEntriesRequests(std::vector<std::optional<PreparedAppendEntryRequest>> requests);
};

class LogFollower : public LogParticipantI, public AbstractFollower {
 public:
  ~LogFollower() override = default;
  LogFollower(ParticipantId id, std::unique_ptr<LogCore> logCore,
              LogTerm term, ParticipantId leaderId, InMemoryLog inMemoryLog);

  // follower only
  auto appendEntries(AppendEntriesRequest)
      -> arangodb::futures::Future<AppendEntriesResult> override;

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
  };
  ParticipantId const _participantId;
  ParticipantId const _leaderId;
  LogTerm const _currentTerm;
  Guarded<GuardedFollowerData> _guardedFollowerData;

  auto acquireMutex() -> Guard;
  auto acquireMutex() const -> ConstGuard;
};

struct LogUnconfiguredParticipant
    : std::enable_shared_from_this<LogUnconfiguredParticipant>,
      LogParticipantI {
  ~LogUnconfiguredParticipant() override = default;
  explicit LogUnconfiguredParticipant(std::unique_ptr<LogCore> logCore);

  [[nodiscard]] auto getStatus() const -> LogStatus override;
  auto resign() && -> std::unique_ptr<LogCore> override;
  [[nodiscard]] auto waitFor(LogIndex) -> WaitForFuture override;
  std::unique_ptr<LogCore> _logCore;
};

struct alignas(16) ReplicatedLog {
  ReplicatedLog() = delete;
  ReplicatedLog(ReplicatedLog const&) = delete;
  ReplicatedLog(ReplicatedLog&&) = delete;
  auto operator=(ReplicatedLog const&) -> ReplicatedLog& = delete;
  auto operator=(ReplicatedLog&&) -> ReplicatedLog& = delete;
  explicit ReplicatedLog(std::shared_ptr<LogParticipantI> participant)
      : _participant(std::move(participant)) {}
  explicit ReplicatedLog(std::unique_ptr<LogCore> core)
      : _participant(std::make_shared<LogUnconfiguredParticipant>(std::move(core))) {}

  auto becomeLeader(ParticipantId id, LogTerm term,
                    std::vector<std::shared_ptr<AbstractFollower>> const& follower,
                    std::size_t writeConcern) -> std::shared_ptr<LogLeader>;
  auto becomeFollower(ParticipantId id, LogTerm term, ParticipantId leaderId)
      -> std::shared_ptr<LogFollower>;

  auto getParticipant() const -> std::shared_ptr<LogParticipantI>;

  auto getLeader() const -> std::shared_ptr<LogLeader>;
  auto getFollower() const -> std::shared_ptr<LogFollower>;

 private:
  mutable std::mutex _mutex;
  std::shared_ptr<LogParticipantI> _participant;
};

}  // namespace replicated_log

}  // namespace arangodb::replication2

#endif  // ARANGOD_REPLICATION2_REPLICATEDLOG_H
