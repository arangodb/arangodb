////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Guarded.h"
#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedState/StateInterfaces.h"
#include "Replication2/ReplicatedState/StateStatus.h"

#include <iosfwd>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>
#include "Replication2/ReplicatedState/PersistedStateInfo.h"

namespace arangodb::cluster {
struct IFailureOracle;
}
namespace arangodb::replication2::replicated_log {
class LogFollower;
class LogLeader;
struct AbstractFollower;
struct LogCore;
}  // namespace arangodb::replication2::replicated_log

namespace arangodb::replication2::replicated_log {

struct IAbstractFollowerFactory {
  virtual ~IAbstractFollowerFactory() = default;
  virtual auto constructFollower(ParticipantId const&)
      -> std::shared_ptr<AbstractFollower> = 0;
  virtual auto constructLeaderCommunicator(ParticipantId const&)
      -> std::shared_ptr<replicated_log::ILeaderCommunicator> = 0;
};

struct IReplicatedLogMethodsBase {
  virtual ~IReplicatedLogMethodsBase() = default;
  virtual auto releaseIndex(LogIndex) -> void = 0;
  virtual auto getLogSnapshot() -> InMemoryLog = 0;
  virtual auto waitFor(LogIndex) -> ILogParticipant::WaitForFuture = 0;
  virtual auto waitForIterator(LogIndex)
      -> ILogParticipant::WaitForIteratorFuture = 0;
};

struct IReplicatedLogLeaderMethods : IReplicatedLogMethodsBase {
  // TODO waitForSync parameter is missing
  virtual auto insert(LogPayload) -> LogIndex = 0;
  // TODO waitForSync parameter is missing
  virtual auto insertDeferred(LogPayload)
      -> std::pair<LogIndex, DeferredAction> = 0;
};

struct IReplicatedLogFollowerMethods : IReplicatedLogMethodsBase {
  virtual auto snapshotCompleted() -> Result = 0;
};

// TODO Move to namespace replicated_state (and different file?)
struct IReplicatedStateHandle {
  virtual ~IReplicatedStateHandle() = default;
  [[nodiscard]] virtual auto resignCurrentState() noexcept
      -> std::unique_ptr<replicated_log::IReplicatedLogMethodsBase> = 0;
  virtual void leadershipEstablished(
      std::unique_ptr<IReplicatedLogLeaderMethods>) = 0;
  virtual void becomeFollower(
      std::unique_ptr<IReplicatedLogFollowerMethods>) = 0;
  virtual void acquireSnapshot(ServerID leader, LogIndex) = 0;
  virtual void updateCommitIndex(LogIndex) = 0;
  // TODO
  virtual void dropEntries() = 0;  // o.ä. (für waitForSync=false)
};

struct LeaderTermInfo {
  LogTerm term;
  ParticipantId myself;
  std::shared_ptr<agency::ParticipantsConfig const> initialConfig;
};

struct FollowerTermInfo {
  LogTerm term;
  ParticipantId myself;
  std::optional<ParticipantId> leader;
};

struct ParticipantContext {
  LoggerContext loggerContext;
  std::unique_ptr<IReplicatedStateHandle> stateHandle;
  std::shared_ptr<ReplicatedLogMetrics> metrics;
  std::shared_ptr<ReplicatedLogGlobalSettings const> options;
};

struct IParticipantsFactory {
  virtual ~IParticipantsFactory() = default;
  virtual auto constructFollower(
      std::unique_ptr<replicated_state::IStorageEngineMethods> methods,
      FollowerTermInfo info, ParticipantContext context)
      -> std::shared_ptr<ILogFollower> = 0;
  virtual auto constructLeader(
      std::unique_ptr<replicated_state::IStorageEngineMethods> methods,
      LeaderTermInfo info, ParticipantContext context)
      -> std::shared_ptr<ILogLeader> = 0;
};

struct ReplicatedLogConnection;

/**
 * @brief Container for a replicated log. These are managed by the responsible
 * vocbase. Exactly one instance exists for each replicated log this server is a
 * participant of.
 *
 * It holds a single ILogParticipant; starting with a
 * LogUnconfiguredParticipant, this will usually be either a LogLeader or a
 * LogFollower.
 *
 * The active participant is also responsible for the singular LogCore of this
 * log, providing access to the physical log. The fact that only one LogCore
 * exists, and only one participant has access to it, asserts that only the
 * active instance can write to (or read from) the physical log.
 *
 * ReplicatedLog is responsible for instantiating Participants, and moving the
 * LogCore from the previous active participant to a new one. This happens in
 * becomeLeader and becomeFollower.
 *
 * A mutex must be used to make sure that moving the LogCore from the old to
 * the new participant, and switching the participant pointer, happen
 * atomically.
 */
struct alignas(64) ReplicatedLog {
  explicit ReplicatedLog(
      std::unique_ptr<replicated_state::IStorageEngineMethods> storage,
      std::shared_ptr<ReplicatedLogMetrics> metrics,
      std::shared_ptr<ReplicatedLogGlobalSettings const> options,
      std::shared_ptr<IParticipantsFactory> participantsFactory,
      LoggerContext const& logContext, agency::ServerInstanceReference myself);

  ~ReplicatedLog();
  ReplicatedLog() = delete;
  ReplicatedLog(ReplicatedLog const&) = delete;
  ReplicatedLog(ReplicatedLog&&) = delete;
  auto operator=(ReplicatedLog const&) -> ReplicatedLog& = delete;
  auto operator=(ReplicatedLog&&) -> ReplicatedLog& = delete;

  [[nodiscard]] auto connect(std::unique_ptr<IReplicatedStateHandle>)
      -> ReplicatedLogConnection;
  auto disconnect(ReplicatedLogConnection)
      -> std::unique_ptr<IReplicatedStateHandle>;

  auto updateConfig(agency::LogPlanTermSpecification term,
                    agency::ParticipantsConfig config)
      -> futures::Future<futures::Unit>;

  [[nodiscard]] auto getId() const noexcept -> LogId;

  [[nodiscard]] auto getParticipant() const -> std::shared_ptr<ILogParticipant>;
  [[nodiscard]] auto getQuickStatus() const -> QuickLogStatus;
  [[nodiscard]] auto getStatus() const -> LogStatus;

  [[nodiscard]] auto
  resign() && -> std::unique_ptr<replicated_state::IStorageEngineMethods>;

  auto updateMyInstanceReference(agency::ServerInstanceReference)
      -> futures::Future<futures::Unit>;

 private:
  struct GuardedData {
    explicit GuardedData(
        std::unique_ptr<replicated_state::IStorageEngineMethods> methods,
        agency::ServerInstanceReference myself)
        : methods(std::move(methods)), _myself(std::move(myself)) {}

    struct LatestConfig {
      explicit LatestConfig(agency::LogPlanTermSpecification term,
                            agency::ParticipantsConfig config)
          : term(std::move(term)), config(std::move(config)) {}
      agency::LogPlanTermSpecification term;
      agency::ParticipantsConfig config;
    };

    bool resigned{false};
    std::unique_ptr<replicated_state::IStorageEngineMethods> methods;
    std::shared_ptr<ILogParticipant> participant = nullptr;
    agency::ServerInstanceReference _myself;
    std::optional<LatestConfig> latest;
    std::unique_ptr<IReplicatedStateHandle> stateHandle;
  };

  auto tryBuildParticipant(GuardedData& data) -> futures::Future<futures::Unit>;
  void resetParticipant(GuardedData& data);

  LoggerContext const _logContext;
  std::shared_ptr<ReplicatedLogMetrics> const _metrics;
  std::shared_ptr<ReplicatedLogGlobalSettings const> const _options;
  std::shared_ptr<IParticipantsFactory> const _participantsFactory;
  Guarded<GuardedData> _guarded;
};

struct ReplicatedLogConnection {
  ReplicatedLogConnection() = default;
  ReplicatedLogConnection(ReplicatedLogConnection const&) = delete;
  ReplicatedLogConnection(ReplicatedLogConnection&&) noexcept = default;
  auto operator=(ReplicatedLogConnection const&)
      -> ReplicatedLogConnection& = delete;
  auto operator=(ReplicatedLogConnection&&) noexcept
      -> ReplicatedLogConnection& = default;

  ~ReplicatedLogConnection();

  void disconnect();

 private:
  friend struct ReplicatedLog;
  explicit ReplicatedLogConnection(ReplicatedLog* log);

  struct nop {
    template<typename T>
    void operator()(T*) {}
  };

  std::unique_ptr<ReplicatedLog, nop> _log = nullptr;
};

struct IScheduler {
  virtual ~IScheduler() = default;
  virtual auto delayedFuture(std::chrono::steady_clock::duration duration)
      -> futures::Future<futures::Unit> = 0;

  struct WorkItem {
    virtual ~WorkItem() = default;
  };

  using WorkItemHandle = std::shared_ptr<WorkItem>;

  virtual auto queueDelayed(
      std::string_view name, std::chrono::steady_clock::duration delay,
      fu2::unique_function<void(bool canceled)> handler) noexcept
      -> WorkItemHandle = 0;
};

struct DefaultParticipantsFactory : IParticipantsFactory {
  explicit DefaultParticipantsFactory(
      std::shared_ptr<IAbstractFollowerFactory> followerFactory,
      std::shared_ptr<IScheduler> scheduler);
  auto constructFollower(
      std::unique_ptr<replicated_state::IStorageEngineMethods>,
      FollowerTermInfo info, ParticipantContext context)
      -> std::shared_ptr<ILogFollower> override;
  auto constructLeader(std::unique_ptr<replicated_state::IStorageEngineMethods>,
                       LeaderTermInfo info, ParticipantContext context)
      -> std::shared_ptr<ILogLeader> override;

  std::shared_ptr<IAbstractFollowerFactory> followerFactory;
  std::shared_ptr<IScheduler> scheduler;
};

}  // namespace arangodb::replication2::replicated_log
