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

namespace arangodb::futures {
template<typename T>
class Future;
template<typename T>
class Promise;
struct Unit;
}  // namespace arangodb::futures
namespace arangodb {
class Result;
}
namespace arangodb::replication2 {
namespace replicated_log {
struct ReplicatedLog;
struct ILogFollower;
struct ILogLeader;
struct ILogParticipant;
struct LogUnconfiguredParticipant;
}  // namespace replicated_log

namespace replicated_state {

struct IReplicatedLeaderStateBase;
struct IReplicatedFollowerStateBase;

/**
 * Common base class for all ReplicatedStates, hiding the type information.
 */
struct ReplicatedStateBase {
  virtual ~ReplicatedStateBase() = default;

  virtual void flush(StateGeneration plannedGeneration) = 0;
  virtual void start(std::unique_ptr<ReplicatedStateToken> token) = 0;
  virtual void forceRebuild() = 0;
  [[nodiscard]] virtual auto getStatus() -> std::optional<StateStatus> = 0;
  [[nodiscard]] auto getLeader()
      -> std::shared_ptr<IReplicatedLeaderStateBase> {
    return getLeaderBase();
  }
  [[nodiscard]] auto getFollower()
      -> std::shared_ptr<IReplicatedFollowerStateBase> {
    return getFollowerBase();
  }

 private:
  [[nodiscard]] virtual auto getLeaderBase()
      -> std::shared_ptr<IReplicatedLeaderStateBase> = 0;
  [[nodiscard]] virtual auto getFollowerBase()
      -> std::shared_ptr<IReplicatedFollowerStateBase> = 0;
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

  explicit ReplicatedState(std::shared_ptr<replicated_log::ReplicatedLog> log,
                           std::shared_ptr<Factory> factory,
                           LoggerContext loggerContext);

  /**
   * Forces to rebuild the state machine depending on the replicated log state.
   */
  void flush(StateGeneration planGeneration) override;
  void start(std::unique_ptr<ReplicatedStateToken> token) override;

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

  /**
   * Rebuilds the managers. Called when the managers participant is gone.
   */
  void forceRebuild() override;

  struct StateManagerBase {
    virtual ~StateManagerBase() = default;
    virtual void run() = 0;

    using WaitForAppliedPromise = futures::Promise<futures::Unit>;
    using WaitForAppliedQueue = std::multimap<LogIndex, WaitForAppliedPromise>;

    [[nodiscard]] virtual auto getStatus() const -> StateStatus = 0;
    [[nodiscard]] virtual auto resign() && noexcept
        -> std::tuple<std::unique_ptr<CoreType>,
                      std::unique_ptr<ReplicatedStateToken>,
                      DeferredAction> = 0;
  };

 private:
  auto getLeaderBase() -> std::shared_ptr<IReplicatedLeaderStateBase> final {
    return getLeader();
  }
  auto getFollowerBase()
      -> std::shared_ptr<IReplicatedFollowerStateBase> final {
    return getFollower();
  }

  std::shared_ptr<Factory> const factory;
  std::shared_ptr<replicated_log::ReplicatedLog> const log{};

  struct GuardedData {
    auto forceRebuild() -> DeferredAction;

    auto runLeader(std::shared_ptr<replicated_log::ILogLeader> logLeader,
                   std::unique_ptr<CoreType>,
                   std::unique_ptr<ReplicatedStateToken> token)
        -> DeferredAction;
    auto runFollower(std::shared_ptr<replicated_log::ILogFollower> logFollower,
                     std::unique_ptr<CoreType>,
                     std::unique_ptr<ReplicatedStateToken> token)
        -> DeferredAction;
    auto runUnconfigured(
        std::shared_ptr<replicated_log::LogUnconfiguredParticipant>
            unconfiguredParticipant,
        std::unique_ptr<CoreType> core,
        std::unique_ptr<ReplicatedStateToken> token) -> DeferredAction;

    auto rebuild(std::unique_ptr<CoreType> core,
                 std::unique_ptr<ReplicatedStateToken> token) -> DeferredAction;

    auto flush(StateGeneration planGeneration) -> DeferredAction;

    explicit GuardedData(ReplicatedState& self) : _self(self) {}

    ReplicatedState& _self;
    std::shared_ptr<StateManagerBase> currentManager = nullptr;
  };
  Guarded<GuardedData> guardedData;
  LoggerContext const loggerContext;
  DatabaseID const database;
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
