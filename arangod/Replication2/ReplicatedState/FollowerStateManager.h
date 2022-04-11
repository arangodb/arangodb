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

#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/ReplicatedState/StateInterfaces.h"
#include "Replication2/Streams/Streams.h"
#include "Replication2/Streams/LogMultiplexer.h"

namespace arangodb::replication2::replicated_state {
template<typename S>
struct FollowerStateManager
    : ReplicatedState<S>::StateManagerBase,
      std::enable_shared_from_this<FollowerStateManager<S>> {
  using Factory = typename ReplicatedStateTraits<S>::FactoryType;
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using FollowerType = typename ReplicatedStateTraits<S>::FollowerType;
  using LeaderType = typename ReplicatedStateTraits<S>::LeaderType;
  using CoreType = typename ReplicatedStateTraits<S>::CoreType;
  using WaitForAppliedQueue =
      typename ReplicatedState<S>::StateManagerBase::WaitForAppliedQueue;
  using WaitForAppliedPromise =
      typename ReplicatedState<S>::StateManagerBase::WaitForAppliedPromise;

  using Stream = streams::Stream<EntryType>;
  using Iterator = typename Stream::Iterator;

  FollowerStateManager(
      LoggerContext loggerContext, std::shared_ptr<ReplicatedStateBase> parent,
      std::shared_ptr<replicated_log::ILogFollower> logFollower,
      std::unique_ptr<CoreType> core,
      std::unique_ptr<ReplicatedStateToken> token,
      std::shared_ptr<Factory> factory) noexcept;

  void run() override;

  [[nodiscard]] auto getStatus() const -> StateStatus final;

  [[nodiscard]] auto getFollowerState() const
      -> std::shared_ptr<IReplicatedFollowerState<S>>;

  [[nodiscard]] auto resign() && noexcept
      -> std::tuple<std::unique_ptr<CoreType>,
                    std::unique_ptr<ReplicatedStateToken>,
                    DeferredAction> override;

  [[nodiscard]] auto getStream() const noexcept -> std::shared_ptr<Stream>;

  auto waitForApplied(LogIndex) -> futures::Future<futures::Unit>;

 private:
  void awaitLeaderShip();
  void ingestLogData();

  template<typename F>
  auto pollNewEntries(F&& fn);
  void checkSnapshot(std::shared_ptr<IReplicatedFollowerState<S>>);
  void tryTransferSnapshot(std::shared_ptr<IReplicatedFollowerState<S>>);
  void startService(std::shared_ptr<IReplicatedFollowerState<S>>);
  void retryTransferSnapshot(std::shared_ptr<IReplicatedFollowerState<S>>,
                             std::uint64_t retryCount);

  void applyEntries(std::unique_ptr<Iterator> iter) noexcept;

  using Demultiplexer = streams::LogDemultiplexer<ReplicatedStateStreamSpec<S>>;

  struct GuardedData {
    FollowerStateManager& self;
    LogIndex _nextWaitForIndex{1};
    std::shared_ptr<Stream> stream;
    std::shared_ptr<IReplicatedFollowerState<S>> state;

    FollowerInternalState internalState{
        FollowerInternalState::kUninitializedState};
    std::chrono::system_clock::time_point lastInternalStateChange;
    std::optional<LogRange> ingestionRange;
    std::optional<Result> lastError;
    std::uint64_t errorCounter{0};

    // core will be nullptr as soon as the FollowerState was created
    std::unique_ptr<CoreType> core;
    std::unique_ptr<ReplicatedStateToken> token;
    WaitForAppliedQueue waitForAppliedQueue;

    bool _didResign = false;

    GuardedData(FollowerStateManager& self, std::unique_ptr<CoreType> core,
                std::unique_ptr<ReplicatedStateToken> token);
    void updateInternalState(FollowerInternalState newState,
                             std::optional<LogRange> range = std::nullopt);
    void updateInternalState(FollowerInternalState newState, Result);
    auto updateNextIndex(LogIndex nextWaitForIndex) -> DeferredAction;
  };

  void handlePollResult(
      futures::Future<std::unique_ptr<Iterator>>&& pollFuture);
  void handleAwaitLeadershipResult(
      futures::Future<replicated_log::WaitForResult>&&);

  Guarded<GuardedData> _guardedData;
  std::weak_ptr<ReplicatedStateBase> const parent;
  std::shared_ptr<replicated_log::ILogFollower> const logFollower;
  std::shared_ptr<Factory> const factory;
  LoggerContext const loggerContext;
};
}  // namespace arangodb::replication2::replicated_state
