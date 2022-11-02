////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Replication2/ReplicatedState/ReplicatedState.h"

#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

#include <memory>

namespace arangodb::replication2::replicated_log {
struct LogUnconfiguredParticipant;
}

namespace arangodb::replication2::replicated_state {

template<typename S>
struct UnconfiguredStateManager
    : ReplicatedState<S>::IStateManager,
      std::enable_shared_from_this<UnconfiguredStateManager<S>> {
  using Factory = typename ReplicatedStateTraits<S>::FactoryType;
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using FollowerType = typename ReplicatedStateTraits<S>::FollowerType;
  using LeaderType = typename ReplicatedStateTraits<S>::LeaderType;
  using CoreType = typename ReplicatedStateTraits<S>::CoreType;

  using WaitForAppliedQueue =
      typename ReplicatedState<S>::IStateManager::WaitForAppliedQueue;
  using WaitForAppliedPromise =
      typename ReplicatedState<S>::IStateManager::WaitForAppliedQueue;

  UnconfiguredStateManager(
      std::shared_ptr<ReplicatedState<S>> const& parent,
      std::shared_ptr<replicated_log::LogUnconfiguredParticipant>
          unconfiguredParticipant,
      std::unique_ptr<CoreType> core,
      std::unique_ptr<ReplicatedStateToken> token) noexcept;

  void run() noexcept override;

  [[nodiscard]] auto getStatus() const -> StateStatus override;

  [[nodiscard]] auto resign() && noexcept
      -> std::tuple<std::unique_ptr<CoreType>,
                    std::unique_ptr<ReplicatedStateToken>,
                    DeferredAction> override;

 private:
  std::weak_ptr<ReplicatedState<S>> _parent;
  std::shared_ptr<replicated_log::LogUnconfiguredParticipant>
      _unconfiguredParticipant;
  std::unique_ptr<CoreType> _core;
  std::unique_ptr<ReplicatedStateToken> _token;
};
}  // namespace arangodb::replication2::replicated_state
