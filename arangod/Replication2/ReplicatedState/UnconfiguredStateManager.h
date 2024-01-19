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
#include "Replication2/ReplicatedState/ReplicatedStateTraits.h"

#include "Basics/Guarded.h"

namespace arangodb::replication2::replicated_state {

template<typename S>
struct UnconfiguredStateManager
    : std::enable_shared_from_this<UnconfiguredStateManager<S>> {
  using CoreType = typename ReplicatedStateTraits<S>::CoreType;
  explicit UnconfiguredStateManager(LoggerContext loggerContext,
                                    std::unique_ptr<CoreType>) noexcept;
  [[nodiscard]] auto resign() && noexcept
      -> std::pair<std::unique_ptr<CoreType>,
                   std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>>;
  [[nodiscard]] auto getInternalStatus() const -> Status::Unconfigured;

 private:
  LoggerContext const _loggerContext;
  struct GuardedData {
    [[nodiscard]] auto resign() && noexcept -> std::unique_ptr<CoreType>;

    std::unique_ptr<CoreType> _core;
  };
  Guarded<GuardedData> _guardedData;
};

template<typename S>
UnconfiguredStateManager<S>::UnconfiguredStateManager(
    LoggerContext loggerContext, std::unique_ptr<CoreType> core) noexcept
    : _loggerContext(std::move(loggerContext)),
      _guardedData{GuardedData{._core = std::move(core)}} {}

template<typename S>
auto UnconfiguredStateManager<S>::resign() && noexcept
    -> std::pair<std::unique_ptr<CoreType>,
                 std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>> {
  auto guard = _guardedData.getLockedGuard();
  return {std::move(guard.get()).resign(), nullptr};
}

template<typename S>
auto UnconfiguredStateManager<S>::getInternalStatus() const
    -> Status::Unconfigured {
  return {};
}

template<typename S>
auto UnconfiguredStateManager<S>::GuardedData::resign() && noexcept
    -> std::unique_ptr<CoreType> {
  return std::move(_core);
}

}  // namespace arangodb::replication2::replicated_state
