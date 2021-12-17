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

#include "Replication2/ReplicatedState/ReplicatedStateCore.h"
#include "Replication2/ReplicatedState/ReplicatedStateTraits.h"
#include "Replication2/ReplicatedState/StateStatus.h"
#include "Replication2/Streams/Streams.h"

namespace arangodb::futures {
template<typename T>
class Future;
}
namespace arangodb {
class Result;
}
namespace arangodb::replication2 {
namespace replicated_log {
struct ReplicatedLog;
struct ILogFollower;
struct ILogLeader;
}  // namespace replicated_log

namespace replicated_state {

struct IReplicatedLeaderStateBase;
struct IReplicatedFollowerStateBase;

/**
 * Common base class for all ReplicatedStates, hiding the type information.
 */
struct ReplicatedStateBase {
  virtual ~ReplicatedStateBase() = default;

  virtual void flush(std::unique_ptr<ReplicatedStateCore>) = 0;
  virtual auto getStatus() -> StateStatus = 0;
  auto getLeader() -> std::shared_ptr<IReplicatedLeaderStateBase> {
    return getLeaderBase();
  }
  auto getFollower() -> std::shared_ptr<IReplicatedFollowerStateBase> {
    return getFollowerBase();
  }
  virtual auto getSnapshotStatus() const -> SnapshotStatus = 0;

 private:
  virtual auto getLeaderBase()
      -> std::shared_ptr<IReplicatedLeaderStateBase> = 0;
  virtual auto getFollowerBase()
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

  explicit ReplicatedState(std::shared_ptr<replicated_log::ReplicatedLog> log,
                           std::shared_ptr<Factory> factory);

  /**
   * Forces to rebuild the state machine depending on the replicated log state.
   */
  void flush(std::unique_ptr<ReplicatedStateCore> =
                 std::make_unique<ReplicatedStateCore>()) override;

  /**
   * Returns the follower state machine. Returns nullptr if no follower state
   * machine is present. (i.e. this server is not a follower)
   */
  auto getFollower() const -> std::shared_ptr<FollowerType>;
  /**
   * Returns the leader state machine. Returns nullptr if no leader state
   * machine is present. (i.e. this server is not a leader)
   */
  auto getLeader() const -> std::shared_ptr<LeaderType>;

  auto getStatus() -> StateStatus final;

  auto getSnapshotStatus() const -> SnapshotStatus override;

  struct StateManagerBase {
    virtual ~StateManagerBase() = default;
    virtual auto getStatus() const -> StateStatus = 0;
    virtual auto getSnapshotStatus() const -> SnapshotStatus = 0;
  };

  std::shared_ptr<Factory> const factory;

 private:
  auto getLeaderBase() -> std::shared_ptr<IReplicatedLeaderStateBase> final {
    return getLeader();
  }
  auto getFollowerBase()
      -> std::shared_ptr<IReplicatedFollowerStateBase> final {
    return getFollower();
  }

  void runLeader(std::shared_ptr<replicated_log::ILogLeader> logLeader,
                 std::unique_ptr<ReplicatedStateCore>);
  void runFollower(std::shared_ptr<replicated_log::ILogFollower> logFollower,
                   std::unique_ptr<ReplicatedStateCore>);

  std::shared_ptr<StateManagerBase> currentManager;
  StateGeneration generation{0};
  std::shared_ptr<replicated_log::ReplicatedLog> const log{};
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
