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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <map>

#include "Replication2/ReplicatedState/ReplicatedStateToken.h"
#include "Replication2/ReplicatedState/ReplicatedStateTraits.h"
#include "Replication2/ReplicatedState/StateStatus.h"
#include "Replication2/Streams/Streams.h"

#include "Basics/Guarded.h"
#include "Replication2/DeferredExecution.h"
#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"

namespace arangodb {
class Result;

namespace futures {
template<typename T>
class Future;
template<typename T>
class Promise;
struct Unit;
}  // namespace futures

namespace velocypack {
class SharedSlice;
}

}  // namespace arangodb

namespace arangodb::replication2 {
namespace replicated_log {
struct ReplicatedLog;
struct ILogFollower;
struct ILogLeader;
struct ILogParticipant;
struct LogUnconfiguredParticipant;
}  // namespace replicated_log

namespace replicated_state {
struct StatePersistorInterface;
struct ReplicatedStateMetrics;
struct IReplicatedLeaderStateBase;
struct IReplicatedFollowerStateBase;

struct IStateManagerBase {};
template<typename S>
struct IReplicatedStateImplBase;
template<typename S>
struct IReplicatedFollowerState;
template<typename S>
struct IReplicatedLeaderState;

/**
 * Common base class for all ReplicatedStates, hiding the type information.
 */
struct ReplicatedStateBase {
  virtual ~ReplicatedStateBase() = default;

  virtual void drop() && = 0;
  [[nodiscard]] virtual auto getStatus() -> std::optional<StateStatus> = 0;
  [[nodiscard]] auto getLeader()
      -> std::shared_ptr<IReplicatedLeaderStateBase> {
    return getLeaderBase();
  }
  [[nodiscard]] auto getFollower()
      -> std::shared_ptr<IReplicatedFollowerStateBase> {
    return getFollowerBase();
  }

  [[nodiscard]] virtual auto createStateHandle()
      -> std::unique_ptr<replicated_log::IReplicatedStateHandle> = 0;

 private:
  [[nodiscard]] virtual auto getLeaderBase()
      -> std::shared_ptr<IReplicatedLeaderStateBase> = 0;
  [[nodiscard]] virtual auto getFollowerBase()
      -> std::shared_ptr<IReplicatedFollowerStateBase> = 0;
};

template<typename S>
struct NewLeaderStateManager {
  using CoreType = typename ReplicatedStateTraits<S>::CoreType;
  explicit NewLeaderStateManager(
      std::shared_ptr<IReplicatedLeaderState<S>> leaderState,
      std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods> logMethods);

  void recoverEntries();
  void updateCommitIndex(LogIndex index);
  [[nodiscard]] auto resign() noexcept
      -> std::pair<std::unique_ptr<CoreType>,
                   std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>>;

 private:
  std::shared_ptr<IReplicatedLeaderState<S>> _leaderState;
  std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods> _logMethods;
};

template<typename S>
struct NewFollowerStateManager {
  using CoreType = typename ReplicatedStateTraits<S>::CoreType;
  explicit NewFollowerStateManager(
      std::shared_ptr<IReplicatedFollowerState<S>> followerState,
      std::unique_ptr<replicated_log::IReplicatedLogFollowerMethods>
          logMethods);
  void acquireSnapshot(ServerID leader, LogIndex index);
  void updateCommitIndex(LogIndex index);
  [[nodiscard]] auto resign() noexcept
      -> std::pair<std::unique_ptr<CoreType>,
                   std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>>;

 private:
  std::shared_ptr<IReplicatedFollowerState<S>> _followerState;
  std::unique_ptr<replicated_log::IReplicatedLogFollowerMethods> _logMethods;
  LogIndex _lastAppliedIndex = LogIndex{0};
};

template<typename S>
struct NewUnconfiguredStateManager {
  using CoreType = typename ReplicatedStateTraits<S>::CoreType;
  explicit NewUnconfiguredStateManager(std::unique_ptr<CoreType>) noexcept;
  [[nodiscard]] auto resign() noexcept
      -> std::pair<std::unique_ptr<CoreType>,
                   std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>>;

 private:
  std::unique_ptr<CoreType> _core;
};

template<typename S>
struct ReplicatedStateManager : replicated_log::IReplicatedStateHandle {
  using CoreType = typename ReplicatedStateTraits<S>::CoreType;
  using Factory = typename ReplicatedStateTraits<S>::FactoryType;
  using FollowerType = typename ReplicatedStateTraits<S>::FollowerType;
  using LeaderType = typename ReplicatedStateTraits<S>::LeaderType;

  ReplicatedStateManager(LoggerContext loggerContext,
                         std::unique_ptr<CoreType> logCore,
                         std::shared_ptr<Factory> factory);

  void acquireSnapshot(ServerID leader, LogIndex index) override;

  void updateCommitIndex(LogIndex index) override;

  [[nodiscard]] auto resign() noexcept
      -> std::unique_ptr<replicated_log::IReplicatedLogMethodsBase> override;

  void leadershipEstablished(
      std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods>) override;

  void becomeFollower(
      std::unique_ptr<replicated_log::IReplicatedLogFollowerMethods> methods)
      override;

  void dropEntries() override;

 private:
  struct GuardedData {
    std::variant<NewUnconfiguredStateManager<S>, NewLeaderStateManager<S>,
                 NewFollowerStateManager<S>>
        _currentManager;
  };

  Guarded<GuardedData> _guarded;

  LoggerContext const _loggerContext;
  std::shared_ptr<Factory> const _factory;
};

template<typename S>
struct ReplicatedState final
    : ReplicatedStateBase,
      std::enable_shared_from_this<ReplicatedState<S>> {
  using Factory = typename ReplicatedStateTraits<S>::FactoryType;
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using FollowerType = typename ReplicatedStateTraits<S>::FollowerType;
  using LeaderType = typename ReplicatedStateTraits<S>::LeaderType;
  using CoreType = typename ReplicatedStateTraits<S>::CoreType;

  explicit ReplicatedState(GlobalLogIdentifier gid,
                           std::shared_ptr<replicated_log::ReplicatedLog> log,
                           std::shared_ptr<Factory> factory,
                           LoggerContext loggerContext,
                           std::shared_ptr<ReplicatedStateMetrics>);
  ~ReplicatedState() override;

  void drop() && override;
  /**
   * Returns the follower state machine. Returns nullptr if no follower state
   * machine is present. (i.e. this server is not a follower)
   */
  [[nodiscard]] auto getFollower() const -> std::shared_ptr<FollowerType>;
  /**
   * Returns the leader state machine. Returns nullptr if no leader state
   * machine is present. (i.e. this server is not a leader)
   */
  [[nodiscard]] auto getLeader() const -> std::shared_ptr<LeaderType>;

  [[nodiscard]] auto getStatus() -> std::optional<StateStatus> final;

  auto createStateHandle()
      -> std::unique_ptr<replicated_log::IReplicatedStateHandle> override;

  struct IStateManager : IStateManagerBase {
    virtual ~IStateManager() = default;
    virtual void run() = 0;

    using WaitForAppliedPromise = futures::Promise<futures::Unit>;
    using WaitForAppliedQueue = std::multimap<LogIndex, WaitForAppliedPromise>;

    [[nodiscard]] virtual auto getStatus() const -> StateStatus = 0;
    [[nodiscard]] virtual auto resign() && noexcept
        -> std::tuple<std::unique_ptr<CoreType>,
                      std::unique_ptr<ReplicatedStateToken>,
                      DeferredAction> = 0;

    virtual auto resign2() && noexcept -> std::tuple<
        std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods>,
        std::unique_ptr<CoreType>> = 0;
  };

 private:
  auto buildCore(std::optional<velocypack::SharedSlice> const& coreParameter);
  auto getLeaderBase() -> std::shared_ptr<IReplicatedLeaderStateBase> final {
    return getLeader();
  }
  auto getFollowerBase()
      -> std::shared_ptr<IReplicatedFollowerStateBase> final {
    return getFollower();
  }

  std::shared_ptr<Factory> const factory;
  GlobalLogIdentifier const gid;
  std::shared_ptr<replicated_log::ReplicatedLog> const log{};

  struct GuardedData {
    auto rebuildMe(IStateManagerBase const* caller) noexcept -> DeferredAction;

    auto runLeader(std::shared_ptr<replicated_log::ILogLeader> logLeader,
                   std::unique_ptr<CoreType>,
                   std::unique_ptr<ReplicatedStateToken> token) noexcept
        -> DeferredAction;
    auto runFollower(std::shared_ptr<replicated_log::ILogFollower> logFollower,
                     std::unique_ptr<CoreType>,
                     std::unique_ptr<ReplicatedStateToken> token) noexcept
        -> DeferredAction;
    auto runUnconfigured(
        std::shared_ptr<replicated_log::LogUnconfiguredParticipant>
            unconfiguredParticipant,
        std::unique_ptr<CoreType> core,
        std::unique_ptr<ReplicatedStateToken> token) noexcept -> DeferredAction;

    auto rebuild(std::unique_ptr<CoreType> core,
                 std::unique_ptr<ReplicatedStateToken> token) noexcept
        -> DeferredAction;

    auto flush(StateGeneration planGeneration) -> DeferredAction;

    explicit GuardedData(ReplicatedState& self) : _self(self) {}

    ReplicatedState& _self;
    std::shared_ptr<IStateManager> currentManager = nullptr;
    std::unique_ptr<CoreType> oldCore = nullptr;
  };
  Guarded<GuardedData> guardedData;
  LoggerContext const loggerContext;
  DatabaseID const database;
  std::shared_ptr<ReplicatedStateMetrics> const metrics;
};

template<typename S>
using ReplicatedStateStreamSpec =
    streams::stream_descriptor_set<streams::stream_descriptor<
        streams::StreamId{1}, typename ReplicatedStateTraits<S>::EntryType,
        streams::tag_descriptor_set<streams::tag_descriptor<
            1, typename ReplicatedStateTraits<S>::Deserializer,
            typename ReplicatedStateTraits<S>::Serializer>>>>;

}  // namespace replicated_state
}  // namespace arangodb::replication2
