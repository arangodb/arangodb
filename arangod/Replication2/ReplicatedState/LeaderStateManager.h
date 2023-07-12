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
#include "Replication2/ReplicatedState/StreamProxy.h"

#include "Basics/Guarded.h"

namespace arangodb::replication2::replicated_state {

struct ReplicatedStateMetrics;

template<typename S>
struct LeaderStateManager
    : std::enable_shared_from_this<LeaderStateManager<S>> {
  using CoreType = typename ReplicatedStateTraits<S>::CoreType;
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Deserializer = typename ReplicatedStateTraits<S>::Deserializer;
  using Serializer = typename ReplicatedStateTraits<S>::Serializer;
  using StreamImpl = ProducerStreamProxy<S>;

  explicit LeaderStateManager(
      LoggerContext loggerContext,
      std::shared_ptr<ReplicatedStateMetrics> metrics,
      std::shared_ptr<IReplicatedLeaderState<S>> leaderState,
      std::shared_ptr<StreamImpl> stream);

  void recoverEntries();
  void updateCommitIndex(LogIndex index) noexcept {
    // TODO do we have to do anything?
  }
  [[nodiscard]] auto resign() && noexcept
      -> std::pair<std::unique_ptr<CoreType>,
                   std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>>;
  [[nodiscard]] auto getInternalStatus() const -> Status::Leader;

  [[nodiscard]] auto getStateMachine() const
      -> std::shared_ptr<IReplicatedLeaderState<S>>;

 private:
  LoggerContext const _loggerContext;
  std::shared_ptr<ReplicatedStateMetrics> const _metrics;
  struct GuardedData {
    auto recoverEntries();
    [[nodiscard]] auto resign() && noexcept -> std::pair<
        std::unique_ptr<CoreType>,
        std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>>;

    LoggerContext const& _loggerContext;
    ReplicatedStateMetrics const& _metrics;
    std::shared_ptr<IReplicatedLeaderState<S>> _leaderState;
    std::shared_ptr<StreamImpl> _stream;
    bool _recoveryCompleted{false};
  };
  Guarded<GuardedData> _guardedData;
};

}  // namespace arangodb::replication2::replicated_state
