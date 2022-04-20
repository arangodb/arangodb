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

#include "StateStatus.h"

#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"
#include "Basics/TimeString.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;

namespace {
inline constexpr std::string_view StringWaitingForLeadershipEstablished =
    "WaitingForLeadershipEstablished";
inline constexpr std::string_view StringIngestingExistingLog =
    "IngestingExistingLog";
inline constexpr std::string_view StringRecoveryInProgress =
    "RecoveryInProgress";
inline constexpr std::string_view StringServiceAvailable = "ServiceAvailable";

inline constexpr std::string_view StringWaitForLeaderConfirmation =
    "WaitForLeaderConfirmation";
inline constexpr std::string_view StringTransferSnapshot = "TransferSnapshot";
inline constexpr std::string_view StringNothingToApply = "NothingToApply";
inline constexpr std::string_view StringApplyRecentEntries =
    "ApplyRecentEntries";
inline constexpr std::string_view StringUninitializedState =
    "UninitializedState";
inline constexpr std::string_view StringSnapshotTransferFailed =
    "SnapshotTransferFailed";

inline constexpr auto StringRole = std::string_view{"role"};
inline constexpr auto StringUnconfigured = std::string_view{"unconfigured"};
inline constexpr auto StringDetail = std::string_view{"detail"};
inline constexpr auto StringManagerState = std::string_view{"managerState"};
inline constexpr auto StringManager = std::string_view{"manager"};
inline constexpr auto StringLog = std::string_view{"log"};
inline constexpr auto StringLastChange = std::string_view{"lastChange"};
inline constexpr auto StringSnapshot = std::string_view{"snapshot"};
inline constexpr auto StringGeneration = std::string_view{"generation"};

auto followerStateFromString(std::string_view str) -> FollowerInternalState {
  if (str == StringUninitializedState) {
    return FollowerInternalState::kUninitializedState;
  } else if (str == StringWaitForLeaderConfirmation) {
    return FollowerInternalState::kWaitForLeaderConfirmation;
  } else if (str == StringTransferSnapshot) {
    return FollowerInternalState::kTransferSnapshot;
  } else if (str == StringNothingToApply) {
    return FollowerInternalState::kNothingToApply;
  } else if (str == StringApplyRecentEntries) {
    return FollowerInternalState::kApplyRecentEntries;
  } else if (str == StringSnapshotTransferFailed) {
    return FollowerInternalState::kSnapshotTransferFailed;
  } else {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER,
                                  "unknown follower internal state %*s",
                                  str.size(), str.data());
  }
}

auto leaderStateFromString(std::string_view str) -> LeaderInternalState {
  if (str == StringUninitializedState) {
    return LeaderInternalState::kUninitializedState;
  } else if (str == StringIngestingExistingLog) {
    return LeaderInternalState::kIngestingExistingLog;
  } else if (str == StringRecoveryInProgress) {
    return LeaderInternalState::kRecoveryInProgress;
  } else if (str == StringServiceAvailable) {
    return LeaderInternalState::kServiceAvailable;
  } else if (str == StringWaitingForLeadershipEstablished) {
    return LeaderInternalState::kWaitingForLeadershipEstablished;
  } else {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER,
                                  "unknown leader internal state %*s",
                                  str.size(), str.data());
  }
}
}  // namespace

auto replicated_state::to_string(LeaderInternalState state) noexcept
    -> std::string_view {
  switch (state) {
    case LeaderInternalState::kWaitingForLeadershipEstablished:
      return StringWaitingForLeadershipEstablished;
    case LeaderInternalState::kIngestingExistingLog:
      return StringIngestingExistingLog;
    case LeaderInternalState::kRecoveryInProgress:
      return StringRecoveryInProgress;
    case LeaderInternalState::kServiceAvailable:
      return StringServiceAvailable;
    case LeaderInternalState::kUninitializedState:
      return StringUninitializedState;
  }
  TRI_ASSERT(false) << "invalid state value " << int(state);
  return "(unknown-internal-leader-state)";
}

auto replicated_state::to_string(FollowerInternalState state) noexcept
    -> std::string_view {
  switch (state) {
    case FollowerInternalState::kWaitForLeaderConfirmation:
      return StringWaitForLeaderConfirmation;
    case FollowerInternalState::kTransferSnapshot:
      return StringTransferSnapshot;
    case FollowerInternalState::kNothingToApply:
      return StringNothingToApply;
    case FollowerInternalState::kApplyRecentEntries:
      return StringApplyRecentEntries;
    case FollowerInternalState::kUninitializedState:
      return StringUninitializedState;
    case FollowerInternalState::kSnapshotTransferFailed:
      return StringSnapshotTransferFailed;
  }
  TRI_ASSERT(false) << "invalid state value " << int(state);
  return "(unknown-internal-follower-state)";
}

void StateStatus::toVelocyPack(velocypack::Builder& builder) const {
  std::visit([&](auto const& s) { s.toVelocyPack(builder); }, variant);
}

auto StateStatus::fromVelocyPack(velocypack::Slice slice) -> StateStatus {
  auto role = slice.get(StringRole).stringView();
  if (role == StaticStrings::Leader) {
    return StateStatus{LeaderStatus::fromVelocyPack(slice)};
  } else if (role == StaticStrings::Follower) {
    return StateStatus{FollowerStatus::fromVelocyPack(slice)};
  } else if (role == StringUnconfigured) {
    return StateStatus{UnconfiguredStatus::fromVelocyPack(slice)};
  } else {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER, "unknown role %*s",
                                  role.size(), role.data());
  }
}

void FollowerStatus::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
  builder.add(StringRole, velocypack::Value(StaticStrings::Follower));
  builder.add(velocypack::Value(StringManager));
  managerState.toVelocyPack(builder);
  builder.add(velocypack::Value(StringSnapshot));
  snapshot.toVelocyPack(builder);
  builder.add(StringGeneration, velocypack::Value(generation.value));
}

void LeaderStatus::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
  builder.add(StringRole, velocypack::Value(StaticStrings::Leader));
  builder.add(velocypack::Value(StringManager));
  managerState.toVelocyPack(builder);
  builder.add(velocypack::Value(StringSnapshot));
  snapshot.toVelocyPack(builder);
  builder.add(StringGeneration, velocypack::Value(generation.value));
}

void UnconfiguredStatus::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
  builder.add(StringRole, velocypack::Value(StringUnconfigured));
  builder.add(velocypack::Value(StringSnapshot));
  snapshot.toVelocyPack(builder);
  builder.add(StringGeneration, velocypack::Value(generation.value));
}

auto UnconfiguredStatus::fromVelocyPack(velocypack::Slice s)
    -> UnconfiguredStatus {
  TRI_ASSERT(s.get(StringRole).stringView() == StaticStrings::Follower);
  auto generation = s.get(StringLog).extract<StateGeneration>();
  auto snapshot = SnapshotInfo::fromVelocyPack(s.get(StringSnapshot));
  return UnconfiguredStatus{
      .generation = generation,
      .snapshot = snapshot,
  };
}

auto FollowerStatus::fromVelocyPack(velocypack::Slice s) -> FollowerStatus {
  TRI_ASSERT(s.get(StringRole).stringView() == StaticStrings::Follower);
  auto state = ManagerState::fromVelocyPack(s.get(StringManager));
  auto generation = s.get(StringLog).extract<StateGeneration>();
  auto snapshot = SnapshotInfo::fromVelocyPack(s.get(StringSnapshot));
  return FollowerStatus{.managerState = std::move(state),
                        .generation = generation,
                        .snapshot = std::move(snapshot)};
}

auto LeaderStatus::fromVelocyPack(velocypack::Slice s) -> LeaderStatus {
  TRI_ASSERT(s.get(StringRole).stringView() == StaticStrings::Leader);
  auto state = ManagerState::fromVelocyPack(s.get(StringManager));
  auto generation = s.get(StringLog).extract<StateGeneration>();
  auto snapshot = SnapshotInfo::fromVelocyPack(s.get(StringSnapshot));
  return LeaderStatus{.managerState = std::move(state),
                      .generation = generation,
                      .snapshot = std::move(snapshot)};
}

void FollowerStatus::ManagerState::toVelocyPack(
    velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
  builder.add(StringLastChange,
              velocypack::Value(timepointToString(lastChange)));
  builder.add(StringManagerState, velocypack::Value(to_string(state)));
  if (detail) {
    builder.add(StringDetail, velocypack::Value(*detail));
  }
}

auto FollowerStatus::ManagerState::fromVelocyPack(velocypack::Slice s)
    -> FollowerStatus::ManagerState {
  auto state = followerStateFromString(s.get(StringManagerState).stringView());
  auto detail = std::optional<std::string>{};
  if (auto detailSlice = s.get(StringDetail); !detailSlice.isNone()) {
    detail = detailSlice.copyString();
  }
  auto tp = stringToTimepoint(s.get(StringLastChange).stringView());
  return ManagerState{
      .state = state, .lastChange = tp, .detail = std::move(detail)};
}

void LeaderStatus::ManagerState::toVelocyPack(
    velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
  builder.add(StringLastChange,
              velocypack::Value(timepointToString(lastChange)));
  builder.add(StringManagerState, velocypack::Value(to_string(state)));
  if (detail) {
    builder.add(StringDetail, velocypack::Value(*detail));
  }
}

auto LeaderStatus::ManagerState::fromVelocyPack(velocypack::Slice s)
    -> LeaderStatus::ManagerState {
  auto state = leaderStateFromString(s.get(StringManagerState).stringView());
  auto detail = std::optional<std::string>{};
  if (auto detailSlice = s.get(StringDetail); !detailSlice.isNone()) {
    detail = detailSlice.copyString();
  }
  auto tp = stringToTimepoint(s.get(StringLastChange).stringView());
  return ManagerState{
      .state = state, .lastChange = tp, .detail = std::move(detail)};
}

auto arangodb::replication2::replicated_state::operator<<(
    std::ostream& out, StateStatus const& stateStatus) -> std::ostream& {
  VPackBuilder builder;
  stateStatus.toVelocyPack(builder);
  return out << builder.slice().toJson();
}
