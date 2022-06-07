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
  std::visit([&](auto const& s) { velocypack::serialize(builder, s); },
             variant);
}

auto StateStatus::fromVelocyPack(velocypack::Slice slice) -> StateStatus {
  auto role = slice.get(static_strings::StringRole).stringView();
  if (role == StaticStrings::Leader) {
    return StateStatus{velocypack::deserialize<LeaderStatus>(slice)};
  } else if (role == StaticStrings::Follower) {
    return StateStatus{velocypack::deserialize<FollowerStatus>(slice)};
  } else if (role == static_strings::StringUnconfigured) {
    return StateStatus{velocypack::deserialize<UnconfiguredStatus>(slice)};
  } else {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER, "unknown role %*s",
                                  role.size(), role.data());
  }
}

auto arangodb::replication2::replicated_state::operator<<(
    std::ostream& out, StateStatus const& stateStatus) -> std::ostream& {
  VPackBuilder builder;
  stateStatus.toVelocyPack(builder);
  return out << builder.slice().toJson();
}

auto FollowerInternalStateStringTransformer::toSerialized(
    FollowerInternalState source, std::string& target) const
    -> inspection::Status {
  target = to_string(source);
  return {};
}

auto FollowerInternalStateStringTransformer::fromSerialized(
    std::string const& source, FollowerInternalState& target) const
    -> inspection::Status {
  if (source == StringUninitializedState) {
    target = FollowerInternalState::kUninitializedState;
  } else if (source == StringWaitForLeaderConfirmation) {
    target = FollowerInternalState::kWaitForLeaderConfirmation;
  } else if (source == StringTransferSnapshot) {
    target = FollowerInternalState::kTransferSnapshot;
  } else if (source == StringNothingToApply) {
    target = FollowerInternalState::kNothingToApply;
  } else if (source == StringApplyRecentEntries) {
    target = FollowerInternalState::kApplyRecentEntries;
  } else if (source == StringSnapshotTransferFailed) {
    target = FollowerInternalState::kSnapshotTransferFailed;
  } else {
    return inspection::Status{"unknown follower internal state " + source};
  }
  return {};
}

auto LeaderInternalStateStringTransformer::toSerialized(
    LeaderInternalState source, std::string& target) const
    -> inspection::Status {
  target = to_string(source);
  return {};
}

auto LeaderInternalStateStringTransformer::fromSerialized(
    std::string const& source, LeaderInternalState& target) const
    -> inspection::Status {
  if (source == StringUninitializedState) {
    target = LeaderInternalState::kUninitializedState;
  } else if (source == StringIngestingExistingLog) {
    target = LeaderInternalState::kIngestingExistingLog;
  } else if (source == StringRecoveryInProgress) {
    target = LeaderInternalState::kRecoveryInProgress;
  } else if (source == StringServiceAvailable) {
    target = LeaderInternalState::kServiceAvailable;
  } else if (source == StringWaitingForLeadershipEstablished) {
    target = LeaderInternalState::kWaitingForLeadershipEstablished;
  } else {
    return inspection::Status{"unknown leader internal state " + source};
  }
  return {};
}
