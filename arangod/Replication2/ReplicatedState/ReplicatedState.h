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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedState/ReplicatedStateTraits.h"
#include "Replication2/ReplicatedState/ReplicatedStateManager.h"
#include "Replication2/Streams/StreamSpecification.h"

#include "Basics/Guarded.h"

struct TRI_vocbase_t;

namespace arangodb::velocypack {
class SharedSlice;
}

namespace arangodb::replication2 {
struct IScheduler;
namespace replicated_log {
struct ReplicatedLog;
}  // namespace replicated_log

namespace replicated_state {
struct ReplicatedStateMetrics;
struct IReplicatedLeaderStateBase;
struct IReplicatedFollowerStateBase;

template<typename S>
using ReplicatedStateStreamSpec =
    streams::stream_descriptor_set<streams::stream_descriptor<
        streams::StreamId{1}, typename ReplicatedStateTraits<S>::EntryType,
        streams::tag_descriptor_set<streams::tag_descriptor<
            1, typename ReplicatedStateTraits<S>::Deserializer,
            typename ReplicatedStateTraits<S>::Serializer>>>>;

/**
 * Common base class for all ReplicatedStates, hiding the type information.
 */
struct ReplicatedStateBase {
  virtual ~ReplicatedStateBase() = default;

  virtual void drop(
      std::shared_ptr<replicated_log::IReplicatedStateHandle>) && = 0;
  [[nodiscard]] auto getLeader()
      -> std::shared_ptr<IReplicatedLeaderStateBase> {
    return getLeaderBase();
  }
  [[nodiscard]] auto getFollower()
      -> std::shared_ptr<IReplicatedFollowerStateBase> {
    return getFollowerBase();
  }

  [[nodiscard]] virtual auto createStateHandle(
      TRI_vocbase_t& vocbase,
      std::optional<velocypack::SharedSlice> const& coreParameters)
      -> std::unique_ptr<replicated_log::IReplicatedStateHandle> = 0;

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

  explicit ReplicatedState(GlobalLogIdentifier gid,
                           std::shared_ptr<replicated_log::ReplicatedLog> log,
                           std::shared_ptr<Factory> factory,
                           LoggerContext loggerContext,
                           std::shared_ptr<ReplicatedStateMetrics> metrics,
                           std::shared_ptr<IScheduler> scheduler);
  ~ReplicatedState() override;

  void drop(std::shared_ptr<replicated_log::IReplicatedStateHandle>) &&
      override;
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

  auto createStateHandle(
      TRI_vocbase_t& vocbase,
      std::optional<velocypack::SharedSlice> const& coreParameter)
      -> std::unique_ptr<replicated_log::IReplicatedStateHandle> override;

 private:
  auto buildCore(TRI_vocbase_t& vocbase,
                 std::optional<velocypack::SharedSlice> const& coreParameter);
  auto getLeaderBase() -> std::shared_ptr<IReplicatedLeaderStateBase> final {
    return getLeader();
  }
  auto getFollowerBase()
      -> std::shared_ptr<IReplicatedFollowerStateBase> final {
    return getFollower();
  }

  std::optional<ReplicatedStateManager<S>> manager;

  std::shared_ptr<Factory> const factory;
  GlobalLogIdentifier const gid;
  std::shared_ptr<replicated_log::ReplicatedLog> const log{};

  LoggerContext const loggerContext;
  DatabaseID const database;
  std::shared_ptr<ReplicatedStateMetrics> const metrics;
  std::shared_ptr<IScheduler> const scheduler;
};

}  // namespace replicated_state
}  // namespace arangodb::replication2
