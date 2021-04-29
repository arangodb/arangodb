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

struct AppendEntriesResult {
  bool success = false;
  LogTerm logTerm = LogTerm{};

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

struct OldLogFollower {
  virtual ~OldLogFollower() = default;

  [[nodiscard]] virtual auto participantId() const noexcept -> ParticipantId = 0;
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

  [[nodiscard]] auto getLastIndex() const -> LogIndex {
    return LogIndex{_log.size()};
  }
  [[nodiscard]] auto getNextIndex() const -> LogIndex {
    return LogIndex{_log.size() + 1};
  }

  std::shared_ptr<PersistedLog> const _persistedLog;
  immer::flex_vector<LogEntry> _log{};
};

struct LogParticipantI {
  [[nodiscard]] virtual auto getStatus() const -> LogStatus = 0;
  virtual ~LogParticipantI() = default;
  virtual auto resign() && -> std::unique_ptr<LogCore> = 0;
  // TODO waitFor() ?
};

struct LogParticipant {
 public:
  explicit LogParticipant(ParticipantId const& id, std::unique_ptr<LogCore> logCore, LogTerm term)
      : _id(id), _currentTerm(term), _logCore(std::move(logCore)) {}

  [[nodiscard]] auto getLocalStatistics() const -> LogStatistics;

  [[nodiscard]] auto participantId() const noexcept -> ParticipantId const&;

  [[nodiscard]] auto getEntryByIndex(LogIndex) const -> std::optional<LogEntry>;

  [[nodiscard]] auto getPersistedLog() const noexcept -> std::shared_ptr<PersistedLog>;

  // private:
  ParticipantId _id{};
  LogTerm const _currentTerm = LogTerm{};
  std::unique_ptr<LogCore> _logCore;
  LogIndex _commitIndex{0};
};

class LogLeader : public std::enable_shared_from_this<LogLeader>, public LogParticipantI {
 public:
  ~LogLeader() override = default;
  LogLeader(ParticipantId const& id, std::unique_ptr<LogCore> logCore, LogTerm term,
            std::vector<std::shared_ptr<OldLogFollower>> const& follower,
            std::size_t writeConcern)
      : _guardedLeaderData(id, std::move(logCore), term, follower, writeConcern) {}

  auto insert(LogPayload) -> LogIndex;

  auto waitFor(LogIndex) -> arangodb::futures::Future<std::shared_ptr<QuorumData>>;

  [[nodiscard]] auto getReplicatedLogSnapshot() const -> immer::flex_vector<LogEntry>;

  [[nodiscard]] auto readReplicatedEntryByIndex(LogIndex idx) const -> std::optional<LogEntry>;

  auto runAsyncStep() -> void;

  [[nodiscard]] auto getStatus() const -> LogStatus override;

  auto resign() && -> std::unique_ptr<LogCore> override;

 private:
  using WaitForPromise = futures::Promise<std::shared_ptr<QuorumData>>;
  struct GuardedLeaderData;
  using Guard = MutexGuard<GuardedLeaderData, std::unique_lock<std::mutex>>;
  using ConstGuard = MutexGuard<GuardedLeaderData const, std::unique_lock<std::mutex>>;

  struct alignas(64) Follower {
    explicit Follower(std::shared_ptr<OldLogFollower> impl, LogIndex lastLogIndex)
        : _impl(std::move(impl)), lastAckedIndex(lastLogIndex) {}

    std::shared_ptr<OldLogFollower> _impl;
    LogIndex lastAckedIndex = LogIndex{0};
    LogIndex lastAckedCommitIndex = LogIndex{0};
    std::size_t numErrorsSinceLastAnswer = 0;
    bool requestInFlight = false;
  };

  struct alignas(128) GuardedLeaderData {
    ~GuardedLeaderData() = default;
    GuardedLeaderData(ParticipantId const& id, std::unique_ptr<LogCore> logCore, LogTerm term,
                      std::vector<std::shared_ptr<OldLogFollower>> const& follower,
                      std::size_t writeConcern)
        : _follower(instantiateFollowers(follower, logCore->getLastIndex())),
          _writeConcern(writeConcern),
          _participant(id, std::move(logCore), term) {}

    GuardedLeaderData() = delete;
    GuardedLeaderData(GuardedLeaderData const&) = delete;
    GuardedLeaderData(GuardedLeaderData&&) = delete;
    auto operator=(GuardedLeaderData const&) -> GuardedLeaderData& = delete;
    auto operator=(GuardedLeaderData&&) -> GuardedLeaderData& = delete;

    auto runAsyncStep(std::weak_ptr<LogLeader> const& leader) -> void;

    void sendAppendEntries(std::weak_ptr<LogLeader> const& parentLog, Follower& follower);

    void handleAppendEntriesResponse(std::weak_ptr<LogLeader> const& parentLog,
                                     Follower& follower, LogIndex lastIndex,
                                     LogIndex currentCommitIndex, LogTerm currentTerm,
                                     futures::Try<AppendEntriesResult>&& res);

    void checkCommitIndex();

    void updateCommitIndexLeader(LogIndex newIndex,
                                 const std::shared_ptr<QuorumData>& quorum);

    auto insert(LogPayload payload) -> LogIndex;

    auto waitFor(LogIndex index) -> futures::Future<std::shared_ptr<QuorumData>>;

    [[nodiscard]] auto getLogIterator(LogIndex fromIdx) const
        -> std::shared_ptr<LogIterator>;

    std::vector<Follower> _follower;
    // TODO as writeConcern is really const, it doesn't have to be under the mutex
    std::size_t const _writeConcern{};

    std::multimap<LogIndex, WaitForPromise> _waitForQueue{};
    std::shared_ptr<QuorumData> _lastQuorum{};

    LogParticipant _participant;

   private:
    static auto instantiateFollowers(std::vector<std::shared_ptr<OldLogFollower>> const& follower,
                                     LogIndex lastIndex) -> std::vector<Follower> {
      std::vector<Follower> follower_vec;
      follower_vec.reserve(follower.size());
      std::transform(follower.cbegin(), follower.cend(), std::back_inserter(follower_vec),
                     [&](std::shared_ptr<OldLogFollower> const& impl) -> Follower {
                       return Follower{impl, lastIndex == LogIndex{0}
                                                 ? LogIndex{0}
                                                 : LogIndex{lastIndex.value - 1}};
                     });
      return follower_vec;
    }
  };

  // make this thread safe in the most simple way possible, wrap everything in
  // a single mutex.
  Guarded<GuardedLeaderData> _guardedLeaderData;

  auto acquireMutex() -> Guard;
  auto acquireMutex() const -> ConstGuard;
};

class LogFollower : public LogParticipantI {
 public:
  ~LogFollower() override = default;
  LogFollower(ParticipantId const& id, std::unique_ptr<LogCore> logCore,
              LogTerm term, ParticipantId leaderId)
      : _leaderId(std::move(leaderId)),
        _guardedParticipant(id, std::move(logCore), term) {}

  // follower only
  auto appendEntries(AppendEntriesRequest) -> arangodb::futures::Future<AppendEntriesResult>;

  [[nodiscard]] auto getStatus() const -> LogStatus override;
  auto resign() && -> std::unique_ptr<LogCore> override;

  [[nodiscard]] auto getParticipantId() const noexcept -> ParticipantId;

 private:
  using Guard = MutexGuard<LogParticipant, std::unique_lock<std::mutex>>;
  using ConstGuard = MutexGuard<LogParticipant const, std::unique_lock<std::mutex>>;

  ParticipantId const _leaderId{};
  Guarded<LogParticipant> _guardedParticipant;

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

  auto becomeLeader(ParticipantId const& id, LogTerm term,
                    std::vector<std::shared_ptr<OldLogFollower>> const& follower,
                    std::size_t writeConcern) -> std::shared_ptr<LogLeader>;
  auto becomeFollower(ParticipantId const& id, LogTerm term, ParticipantId leaderId)
      -> std::shared_ptr<LogFollower>;

  std::shared_ptr<LogParticipantI> _participant;
};

}  // namespace replicated_log

}  // namespace arangodb::replication2

#endif  // ARANGOD_REPLICATION2_REPLICATEDLOG_H
