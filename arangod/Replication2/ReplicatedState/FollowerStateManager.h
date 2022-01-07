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

  using Stream = streams::Stream<EntryType>;
  using Iterator = typename Stream::Iterator;

  FollowerStateManager(
      std::shared_ptr<ReplicatedStateBase> parent,
      std::shared_ptr<replicated_log::ILogFollower> logFollower,
      std::unique_ptr<ReplicatedStateCore> core,
      std::shared_ptr<Factory> factory) noexcept;

  void run();
  auto getStatus() const -> StateStatus final;
  auto getSnapshotStatus() const -> SnapshotStatus final;

  auto getFollowerState() -> std::shared_ptr<IReplicatedFollowerState<S>>;

 private:
  void awaitLeaderShip();
  void ingestLogData();
  void pollNewEntries();
  void checkSnapshot(std::shared_ptr<IReplicatedFollowerState<S>>);
  void tryTransferSnapshot(std::shared_ptr<IReplicatedFollowerState<S>>);
  void startService(std::shared_ptr<IReplicatedFollowerState<S>>);

  void applyEntries(std::unique_ptr<Iterator> iter) noexcept;

  using Demultiplexer = streams::LogDemultiplexer<ReplicatedStateStreamSpec<S>>;
  LogIndex nextEntry{1};

  // TODO locking

  std::shared_ptr<Stream> stream;
  std::shared_ptr<IReplicatedFollowerState<S>> state;
  std::weak_ptr<ReplicatedStateBase> parent;
  std::shared_ptr<replicated_log::ILogFollower> logFollower;

  FollowerInternalState internalState{
      FollowerInternalState::kUninitializedState};
  std::chrono::system_clock::time_point lastInternalStateChange;
  std::optional<LogRange> ingestionRange;

  std::unique_ptr<ReplicatedStateCore> core;
  std::shared_ptr<Factory> const factory;

 private:
  void updateInternalState(FollowerInternalState newState,
                           std::optional<LogRange> range = std::nullopt) {
    internalState = newState;
    lastInternalStateChange = std::chrono::system_clock::now();
    ingestionRange = range;
  }
};
}  // namespace arangodb::replication2::replicated_state
