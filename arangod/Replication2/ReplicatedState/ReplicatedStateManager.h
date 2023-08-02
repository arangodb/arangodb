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
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedState/FollowerStateManager.h"
#include "Replication2/ReplicatedState/LeaderStateManager.h"
#include "Replication2/ReplicatedState/UnconfiguredStateManager.h"
#include "Replication2/ReplicatedState/ReplicatedStateTraits.h"
#include "Replication2/ReplicatedState/StateStatus.h"

#include "Basics/Guarded.h"

namespace arangodb::replication2 {
struct IScheduler;

namespace replicated_state {

struct ReplicatedStateMetrics;

template<typename S>
struct ReplicatedStateManager : replicated_log::IReplicatedStateHandle {
  using CoreType = typename ReplicatedStateTraits<S>::CoreType;
  using Factory = typename ReplicatedStateTraits<S>::FactoryType;
  using FollowerType = typename ReplicatedStateTraits<S>::FollowerType;
  using LeaderType = typename ReplicatedStateTraits<S>::LeaderType;
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Serializer = typename ReplicatedStateTraits<S>::Serializer;
  using Deserializer = typename ReplicatedStateTraits<S>::Deserializer;

  ReplicatedStateManager(LoggerContext loggerContext,
                         std::shared_ptr<ReplicatedStateMetrics> metrics,
                         std::unique_ptr<CoreType> logCore,
                         std::shared_ptr<Factory> factory,
                         std::shared_ptr<IScheduler> scheduler);

  void acquireSnapshot(ServerID leader, LogIndex index,
                       std::uint64_t version) override;

  void updateCommitIndex(LogIndex index) override;

  [[nodiscard]] auto resignCurrentState() noexcept
      -> std::unique_ptr<replicated_log::IReplicatedLogMethodsBase> override;

  void leadershipEstablished(
      std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods>) override;

  void becomeFollower(
      std::unique_ptr<replicated_log::IReplicatedLogFollowerMethods> methods)
      override;

  auto resign() && -> std::unique_ptr<CoreType>;

  [[nodiscard]] auto getInternalStatus() const -> Status override;
  // We could, more specifically, return pointers to FollowerType/LeaderType.
  // But I currently don't see that it's needed, and would have to do one of
  // the stunts for covariance here.
  [[nodiscard]] auto getFollower() const
      -> std::shared_ptr<IReplicatedFollowerStateBase>;
  [[nodiscard]] auto getLeader() const
      -> std::shared_ptr<IReplicatedLeaderStateBase>;

 private:
  LoggerContext const _loggerContext;
  std::shared_ptr<ReplicatedStateMetrics> const _metrics;
  std::shared_ptr<Factory> const _factory;
  std::shared_ptr<IScheduler> const _scheduler;

  struct GuardedData {
    template<typename... Args>
    explicit GuardedData(Args&&... args)
        : _currentManager(std::forward<Args>(args)...) {}

    std::variant<std::shared_ptr<UnconfiguredStateManager<S>>,
                 std::shared_ptr<LeaderStateManager<S>>,
                 std::shared_ptr<FollowerStateManager<S>>>
        _currentManager;
  };
  Guarded<GuardedData> _guarded;
};

}  // namespace replicated_state
}  // namespace arangodb::replication2
