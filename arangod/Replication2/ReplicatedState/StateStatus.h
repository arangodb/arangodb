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

namespace static_strings {
inline constexpr auto StringDetail = std::string_view{"detail"};
inline constexpr auto StringManager = std::string_view{"manager"};
inline constexpr auto StringLastAppliedIndex =
    std::string_view{"lastAppliedIndex"};
inline constexpr auto StringLastChange = std::string_view{"lastChange"};
inline constexpr auto StringManagerState = std::string_view{"managerState"};
inline constexpr auto StringSnapshot = std::string_view{"snapshot"};
inline constexpr auto StringGeneration = std::string_view{"generation"};
inline constexpr auto StringRole = std::string_view{"role"};
inline constexpr auto StringUnconfigured = std::string_view{"unconfigured"};
inline constexpr auto StringLeader = std::string_view{"leader"};
inline constexpr auto StringFollower = std::string_view{"follower"};
}  // namespace static_strings

enum class LeaderInternalState {
  kUninitializedState,
  kWaitingForLeadershipEstablished,
  kRecoveryInProgress,
  kServiceStarting,
  kServiceAvailable,
};

auto to_string(LeaderInternalState) noexcept -> std::string_view;
struct LeaderInternalStateStringTransformer {
  using SerializedType = std::string;
  auto toSerialized(LeaderInternalState source, std::string& target) const
      -> inspection::Status;
  auto fromSerialized(std::string const& source,
                      LeaderInternalState& target) const -> inspection::Status;
};

struct LeaderStatus {
  using clock = std::chrono::system_clock;

  struct ManagerState {
    LeaderInternalState state{};
    clock::time_point lastChange{};
    std::optional<std::string> detail;
  };

  ManagerState managerState;
  StateGeneration generation;
  SnapshotInfo snapshot;
};

template<class Inspector>
auto inspect(Inspector& f, LeaderStatus& x) {
  auto role = std::string{static_strings::StringLeader};
  return f.object(x).fields(
      f.field(static_strings::StringGeneration, x.generation),
      f.field(static_strings::StringSnapshot, x.snapshot),
      f.field(static_strings::StringManager, x.managerState),
      f.field(static_strings::StringRole, role));
}

template<class Inspector>
auto inspect(Inspector& f, LeaderStatus::ManagerState& x) {
  return f.object(x).fields(
      f.field(static_strings::StringManagerState, x.state)
          .transformWith(LeaderInternalStateStringTransformer{}),
      f.field(static_strings::StringLastChange, x.lastChange)
          .transformWith(inspection::TimeStampTransformer{}),
      f.field(static_strings::StringDetail, x.detail));
}

enum class FollowerInternalState {
  kUninitializedState,
  kWaitForLeaderConfirmation,
  kTransferSnapshot,
  kWaitForNewEntries,
  kApplyRecentEntries,
  kSnapshotTransferFailed,
};

auto to_string(FollowerInternalState) noexcept -> std::string_view;
struct FollowerInternalStateStringTransformer {
  using SerializedType = std::string;
  auto toSerialized(FollowerInternalState source, std::string& target) const
      -> inspection::Status;
  auto fromSerialized(std::string const& source,
                      FollowerInternalState& target) const
      -> inspection::Status;
};

struct FollowerStatus {
  using clock = std::chrono::system_clock;

  struct ManagerState {
    FollowerInternalState state{};
    clock::time_point lastChange{};
    std::optional<std::string> detail;
  };

  ManagerState managerState;
  StateGeneration generation;
  SnapshotInfo snapshot;
  LogIndex lastAppliedIndex;
};

template<class Inspector>
auto inspect(Inspector& f, FollowerStatus& x) {
  auto role = std::string{static_strings::StringFollower};
  return f.object(x).fields(
      f.field(static_strings::StringGeneration, x.generation),
      f.field(static_strings::StringSnapshot, x.snapshot),
      f.field(static_strings::StringManager, x.managerState),
      f.field(static_strings::StringLastAppliedIndex, x.lastAppliedIndex),
      f.field(static_strings::StringRole, role));
}

template<class Inspector>
auto inspect(Inspector& f, FollowerStatus::ManagerState& x) {
  return f.object(x).fields(
      f.field(static_strings::StringManagerState, x.state)
          .transformWith(FollowerInternalStateStringTransformer{}),
      f.field(static_strings::StringLastChange, x.lastChange)
          .transformWith(inspection::TimeStampTransformer{}),
      f.field(static_strings::StringDetail, x.detail));
}

struct UnconfiguredStatus {
  StateGeneration generation;
  SnapshotInfo snapshot;
  auto operator==(UnconfiguredStatus const&) const noexcept -> bool = default;
};

template<class Inspector>
auto inspect(Inspector& f, UnconfiguredStatus& x) {
  auto role = std::string{static_strings::StringUnconfigured};
  return f.object(x).fields(
      f.field(static_strings::StringGeneration, x.generation),
      f.field(static_strings::StringSnapshot, x.snapshot),
      f.field(static_strings::StringRole, role));
}

struct StateStatus {
  std::variant<LeaderStatus, FollowerStatus, UnconfiguredStatus> variant;

  [[nodiscard]] auto asFollowerStatus() const noexcept
      -> FollowerStatus const* {
    return std::get_if<FollowerStatus>(&variant);
  }

  [[nodiscard]] auto asLeaderStatus() const noexcept -> LeaderStatus const* {
    return std::get_if<LeaderStatus>(&variant);
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
