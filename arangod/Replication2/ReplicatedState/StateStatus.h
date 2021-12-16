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
#include <chrono>
#include <optional>
#include <string>

#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedState/StateCommon.h"

namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack

namespace arangodb::replication2::replicated_state {

enum class LeaderInternalState {
  kUninitializedState,
  kWaitingForLeadershipEstablished,
  kIngestingExistingLog,
  kRecoveryInProgress,
  kServiceAvailable,
};

auto to_string(LeaderInternalState) noexcept -> std::string_view;

struct LeaderStatus {
  using clock = std::chrono::system_clock;

  struct State {
    LeaderInternalState state{};
    clock::time_point lastChange{};
    std::optional<std::string> detail;

    void toVelocyPack(velocypack::Builder&) const;
    static auto fromVelocyPack(velocypack::Slice) -> State;
  };

  StateGeneration generation;
  State state;
  replicated_log::LeaderStatus log;

  void toVelocyPack(velocypack::Builder&) const;
  static auto fromVelocyPack(velocypack::Slice) -> LeaderStatus;
};

enum class FollowerInternalState {
  kUninitializedState,
  kWaitForLeaderConfirmation,
  kTransferSnapshot,
  kNothingToApply,
  kApplyRecentEntries,
};

auto to_string(FollowerInternalState) noexcept -> std::string_view;

struct FollowerStatus {
  using clock = std::chrono::system_clock;

  struct State {
    FollowerInternalState state{};
    clock::time_point lastChange{};
    std::optional<std::string> detail;

    void toVelocyPack(velocypack::Builder&) const;
    static auto fromVelocyPack(velocypack::Slice) -> State;
  };

  StateGeneration generation;
  State state;
  replicated_log::FollowerStatus log;

  void toVelocyPack(velocypack::Builder&) const;
  static auto fromVelocyPack(velocypack::Slice) -> FollowerStatus;
};

struct StateStatus {
  std::variant<LeaderStatus, FollowerStatus> variant;

  auto asFollowerStatus() const noexcept -> FollowerStatus const * {
    return std::get_if<FollowerStatus>(&variant);
  }

  void toVelocyPack(velocypack::Builder&) const;
  static auto fromVelocyPack(velocypack::Slice) -> StateStatus;
};

}  // namespace arangodb::replication2::replicated_state
