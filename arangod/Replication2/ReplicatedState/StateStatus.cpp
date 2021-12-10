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

#include "StateStatus.h"

#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;

namespace {
inline constexpr std::string_view String_WaitingForLeadershipEstablished =
    "WaitingForLeadershipEstablished";
inline constexpr std::string_view String_IngestingExistingLog =
    "IngestingExistingLog";
inline constexpr std::string_view String_RecoveryInProgress =
    "RecoveryInProgress";
inline constexpr std::string_view String_ServiceAvailable = "ServiceAvailable";

inline constexpr std::string_view String_WaitForLeaderConfirmation =
    "WaitForLeaderConfirmation";
inline constexpr std::string_view String_TransferSnapshot = "TransferSnapshot";
inline constexpr std::string_view String_NothingToApply = "NothingToApply";
inline constexpr std::string_view String_ApplyRecentEntries =
    "ApplyRecentEntries";
inline constexpr std::string_view String_UninitializedState =
    "UninitializedState";

inline constexpr auto String_Role = velocypack::StringRef{"role"};
inline constexpr auto String_Detail = velocypack::StringRef{"detail"};
inline constexpr auto String_State = velocypack::StringRef{"state"};
inline constexpr auto String_Log = velocypack::StringRef{"log"};
inline constexpr auto String_Generation = velocypack::StringRef{"generation"};

auto followerStateFromString(std::string_view str) -> FollowerInternalState {
  if (str == String_UninitializedState) {
    return FollowerInternalState::kUninitializedState;
  } else if (str == String_WaitForLeaderConfirmation) {
    return FollowerInternalState::kWaitForLeaderConfirmation;
  } else if (str == String_TransferSnapshot) {
    return FollowerInternalState::kTransferSnapshot;
  } else if (str == String_NothingToApply) {
    return FollowerInternalState::kNothingToApply;
  } else if (str == String_ApplyRecentEntries) {
    return FollowerInternalState::kApplyRecentEntries;
  } else {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER,
                                  "unknown follower internal state %*s",
                                  str.size(), str.data());
  }
}

auto leaderStateFromString(std::string_view str) -> LeaderInternalState {
  if (str == String_UninitializedState) {
    return LeaderInternalState::kUninitializedState;
  } else if (str == String_IngestingExistingLog) {
    return LeaderInternalState::kIngestingExistingLog;
  } else if (str == String_RecoveryInProgress) {
    return LeaderInternalState::kRecoveryInProgress;
  } else if (str == String_ServiceAvailable) {
    return LeaderInternalState::kServiceAvailable;
  } else if (str == String_WaitingForLeadershipEstablished) {
    return LeaderInternalState::kWaitingForLeadershipEstablished;
  } else {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER,
                                  "unknown leader internal state %*s",
                                  str.size(), str.data());
  }
}
}  // namespace

auto replicated_state::to_string(LeaderInternalState state) noexcept -> std::string_view {
  switch (state) {
    case LeaderInternalState::kWaitingForLeadershipEstablished:
      return String_WaitingForLeadershipEstablished;
    case LeaderInternalState::kIngestingExistingLog:
      return String_IngestingExistingLog;
    case LeaderInternalState::kRecoveryInProgress:
      return String_RecoveryInProgress;
    case LeaderInternalState::kServiceAvailable:
      return String_ServiceAvailable;
    case LeaderInternalState::kUninitializedState:
      return String_UninitializedState;
  }
  TRI_ASSERT(false) << "invalid state value " << int(state);
  return "(unknown-internal-leader-state)";
}

auto replicated_state::to_string(FollowerInternalState state) noexcept -> std::string_view {
  switch (state) {
    case FollowerInternalState::kWaitForLeaderConfirmation:
      return String_WaitForLeaderConfirmation;
    case FollowerInternalState::kTransferSnapshot:
      return String_TransferSnapshot;
    case FollowerInternalState::kNothingToApply:
      return String_NothingToApply;
    case FollowerInternalState::kApplyRecentEntries:
      return String_ApplyRecentEntries;
    case FollowerInternalState::kUninitializedState:
      return String_UninitializedState;
  }
  TRI_ASSERT(false) << "invalid state value " << int(state);
  return "(unknown-internal-follower-state)";
}

void StateStatus::toVelocyPack(velocypack::Builder& builder) const {
  std::visit([&](auto const& s) { s.toVelocyPack(builder); }, variant);
}

auto StateStatus::fromVelocyPack(velocypack::Slice slice) -> StateStatus {
  auto role = slice.get(String_Role).stringView();
  if (role == StaticStrings::Leader) {
    return StateStatus{LeaderStatus::fromVelocyPack(slice)};
  } else if (role == StaticStrings::Follower) {
    return StateStatus{FollowerStatus::fromVelocyPack(slice)};
  } else {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER, "unknown role %*s",
                                  role.size(), role.data());
  }
}

void FollowerStatus::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
  builder.add(String_Role, velocypack::Value(StaticStrings::Follower));
  builder.add(String_Generation, velocypack::Value(generation));
  builder.add(velocypack::Value(String_State));
  state.toVelocyPack(builder);
  builder.add(velocypack::Value(String_Log));
  log.toVelocyPack(builder);
}

void LeaderStatus::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
  builder.add(String_Role, velocypack::Value(StaticStrings::Leader));
  builder.add(String_Generation, velocypack::Value(generation));
  builder.add(velocypack::Value(String_State));
  state.toVelocyPack(builder);
  builder.add(velocypack::Value(String_Log));
  log.toVelocyPack(builder);
}

auto FollowerStatus::fromVelocyPack(velocypack::Slice s) -> FollowerStatus {
  TRI_ASSERT(s.get(String_Role).stringView() == StaticStrings::Follower);
  auto state = State::fromVelocyPack(s.get(String_State));
  auto log = replicated_log::FollowerStatus::fromVelocyPack(s.get(String_Log));
  auto generation = s.get(String_Generation).extract<StateGeneration>();
  return FollowerStatus{.generation = generation,
                        .state = std::move(state),
                        .log = std::move(log)};
}

auto LeaderStatus::fromVelocyPack(velocypack::Slice s) -> LeaderStatus {
  TRI_ASSERT(s.get(String_Role).stringView() == StaticStrings::Leader);
  auto state = State::fromVelocyPack(s.get(String_State));
  auto log = replicated_log::LeaderStatus::fromVelocyPack(s.get(String_Log));
  auto generation = s.get(String_Generation).extract<StateGeneration>();
  return LeaderStatus{.generation = generation,
                      .state = std::move(state),
                      .log = std::move(log)};
}

void FollowerStatus::State::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
  // TODO add lastChange timepoint
  builder.add(String_State, velocypack::Value(to_string(state)));
  if (detail) {
    builder.add(String_Detail, velocypack::Value(*detail));
  }
}

auto FollowerStatus::State::fromVelocyPack(velocypack::Slice s) -> FollowerStatus::State {
  // TODO read lastChange timepoint
  auto state = followerStateFromString(s.get(String_State).stringView());
  auto detail = std::optional<std::string>{};
  if (auto detailSlice = s.get(String_Detail); !detailSlice.isNone()) {
    detail = detailSlice.copyString();
  }
  return State{.state = state, .detail = std::move(detail)};
}

void LeaderStatus::State::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
  // TODO add lastChange timepoint
  builder.add(String_State, velocypack::Value(to_string(state)));
  if (detail) {
    builder.add(String_Detail, velocypack::Value(*detail));
  }
}

auto LeaderStatus::State::fromVelocyPack(velocypack::Slice s) -> LeaderStatus::State {
  // TODO read lastChange timepoint
  auto state = leaderStateFromString(s.get(String_State).stringView());
  auto detail = std::optional<std::string>{};
  if (auto detailSlice = s.get(String_Detail); !detailSlice.isNone()) {
    detail = detailSlice.copyString();
  }
  return State{.state = state, .detail = std::move(detail)};
}
