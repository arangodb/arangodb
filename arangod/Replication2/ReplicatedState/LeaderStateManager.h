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
#include <memory>

#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/StateInterfaces.h"
#include "Replication2/Streams/Streams.h"
#include "Replication2/Streams/LogMultiplexer.h"

namespace arangodb::replication2::replicated_state {

template<typename S>
struct LeaderStateManager
    : ReplicatedState<S>::StateManagerBase,
      std::enable_shared_from_this<LeaderStateManager<S>> {
  using Factory = typename ReplicatedStateTraits<S>::FactoryType;
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using FollowerType = typename ReplicatedStateTraits<S>::FollowerType;
  using LeaderType = typename ReplicatedStateTraits<S>::LeaderType;
  using CoreType = typename ReplicatedStateTraits<S>::CoreType;

  using WaitForAppliedQueue =
      typename ReplicatedState<S>::StateManagerBase::WaitForAppliedQueue;
  using WaitForAppliedPromise =
      typename ReplicatedState<S>::StateManagerBase::WaitForAppliedQueue;

  explicit LeaderStateManager(
      LoggerContext loggerContext,
      std::shared_ptr<ReplicatedState<S>> const& parent,
      std::shared_ptr<replicated_log::ILogLeader> leader,
      std::unique_ptr<CoreType> core,
      std::unique_ptr<ReplicatedStateToken> token,
      std::shared_ptr<Factory> factory) noexcept;

  using Stream = streams::ProducerStream<EntryType>;
  using Iterator = typename Stream::Iterator;

  [[nodiscard]] auto getStatus() const -> StateStatus final;

  void run() override;

  [[nodiscard]] auto resign() && noexcept
      -> std::tuple<std::unique_ptr<CoreType>,
                    std::unique_ptr<ReplicatedStateToken>,
                    DeferredAction> override;

  using Multiplexer = streams::LogMultiplexer<ReplicatedStateStreamSpec<S>>;

  auto getImplementationState() -> std::shared_ptr<IReplicatedLeaderState<S>>;

  struct GuardedData {
    explicit GuardedData(LeaderStateManager& self,
                         LeaderInternalState internalState,
                         std::unique_ptr<CoreType> core,
                         std::unique_ptr<ReplicatedStateToken> token);
    LeaderStateManager& self;
    std::shared_ptr<IReplicatedLeaderState<S>> state;
    std::shared_ptr<Stream> stream;

    LeaderInternalState internalState{LeaderInternalState::kUninitializedState};
    std::chrono::system_clock::time_point lastInternalStateChange;
    std::optional<LogRange> recoveryRange;

    std::unique_ptr<CoreType> core;
    std::unique_ptr<ReplicatedStateToken> token;
    bool _didResign = false;

    void updateInternalState(LeaderInternalState newState,
                             std::optional<LogRange> range = std::nullopt) {
      internalState = newState;
      lastInternalStateChange = std::chrono::system_clock::now();
      recoveryRange = range;
    }
  };

  Guarded<GuardedData> guardedData;
  std::weak_ptr<ReplicatedState<S>> const parent;
  std::shared_ptr<replicated_log::ILogLeader> const logLeader;
  LoggerContext const loggerContext;
  std::shared_ptr<Factory> const factory;

 private:
  void beginWaitingForParticipantResigned();
};

template<typename S>
LeaderStateManager<S>::GuardedData::GuardedData(
    LeaderStateManager& self, LeaderInternalState internalState,
    std::unique_ptr<CoreType> core, std::unique_ptr<ReplicatedStateToken> token)
    : self(self),
      internalState(internalState),
      core(std::move(core)),
      token(std::move(token)) {
  TRI_ASSERT(this->core != nullptr);
  TRI_ASSERT(this->token != nullptr);
}
}  // namespace arangodb::replication2::replicated_state
