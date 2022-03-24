////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

  struct ManagerState {
    LeaderInternalState state{};
    clock::time_point lastChange{};
    std::optional<std::string> detail;

    void toVelocyPack(velocypack::Builder&) const;
    static auto fromVelocyPack(velocypack::Slice) -> ManagerState;
  };

  ManagerState managerState;
  StateGeneration generation;
  SnapshotInfo snapshot;

  void toVelocyPack(velocypack::Builder&) const;
  static auto fromVelocyPack(velocypack::Slice) -> LeaderStatus;
};

enum class FollowerInternalState {
  kUninitializedState,
  kWaitForLeaderConfirmation,
  kTransferSnapshot,
  kNothingToApply,
  kApplyRecentEntries,
  kSnapshotTransferFailed,
};

auto to_string(FollowerInternalState) noexcept -> std::string_view;

struct FollowerStatus {
  using clock = std::chrono::system_clock;

  struct ManagerState {
    FollowerInternalState state{};
    clock::time_point lastChange{};
    std::optional<std::string> detail;

    void toVelocyPack(velocypack::Builder&) const;
    static auto fromVelocyPack(velocypack::Slice) -> ManagerState;
  };

  ManagerState managerState;
  StateGeneration generation;
  SnapshotInfo snapshot;

  void toVelocyPack(velocypack::Builder&) const;
  static auto fromVelocyPack(velocypack::Slice) -> FollowerStatus;
};

struct UnconfiguredStatus {
  StateGeneration generation;
  SnapshotInfo snapshot;

  void toVelocyPack(velocypack::Builder&) const;
  static auto fromVelocyPack(velocypack::Slice) -> UnconfiguredStatus;

  auto operator==(UnconfiguredStatus const&) const noexcept -> bool = default;
};

struct StateStatus {
  std::variant<LeaderStatus, FollowerStatus, UnconfiguredStatus> variant;

  [[nodiscard]] auto asFollowerStatus() const noexcept
      -> FollowerStatus const* {
    return std::get_if<FollowerStatus>(&variant);
  }

  [[nodiscard]] auto getSnapshotInfo() const noexcept -> SnapshotInfo const& {
    return std::visit(
        [](auto&& s) -> SnapshotInfo const& { return s.snapshot; }, variant);
  }

  [[nodiscard]] auto getGeneration() const noexcept -> StateGeneration {
    return std::visit([](auto&& s) -> StateGeneration { return s.generation; },
                      variant);
  }

  void toVelocyPack(velocypack::Builder&) const;
  static auto fromVelocyPack(velocypack::Slice) -> StateStatus;

  friend auto operator<<(std::ostream&, StateStatus const&) -> std::ostream&;
};

auto operator<<(std::ostream&, StateStatus const&) -> std::ostream&;

}  // namespace arangodb::replication2::replicated_state
