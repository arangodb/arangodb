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

#include "StateCommon.h"

#include <velocypack/Value.h>
#include <velocypack/Builder.h>

#include "Basics/debugging.h"
#include "Basics/StaticStrings.h"
#include "Basics/TimeString.h"

using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;

namespace {
auto const String_InProgress = std::string_view{"InProgress"};
auto const String_Completed = std::string_view{"Completed"};
auto const String_Failed = std::string_view{"Failed"};
auto const String_Uninitialized = std::string_view{"Uninitialized"};
auto const String_Status = std::string_view{"status"};
auto const String_Timestamp = std::string_view{"timestamp"};
}  // namespace

StateGeneration::operator arangodb::velocypack::Value() const noexcept {
  return arangodb::velocypack::Value(value);
}

auto StateGeneration::operator+(std::uint64_t delta) const -> StateGeneration {
  return StateGeneration{value + delta};
}

auto StateGeneration::saturatedDecrement(uint64_t delta) const noexcept
    -> StateGeneration {
  if (value > delta) {
    return StateGeneration{value - delta};
  }
  return StateGeneration{0};
}

auto replicated_state::operator<<(std::ostream& os, StateGeneration g)
    -> std::ostream& {
  return os << g.value;
}

auto StateGeneration::operator++() noexcept -> StateGeneration& {
  ++value;
  return *this;
}

auto StateGeneration::operator++(int) noexcept -> StateGeneration {
  return StateGeneration{value++};
}

auto replicated_state::operator<<(std::ostream& os, SnapshotStatus const& ss)
    -> std::ostream& {
  return os << to_string(ss);
}

void SnapshotInfo::updateStatus(SnapshotStatus s) noexcept {
  if (status != s) {
    status = s;
    timestamp = clock::now();
  }
}

auto replicated_state::to_string(SnapshotStatus s) noexcept
    -> std::string_view {
  switch (s) {
    case SnapshotStatus::kUninitialized:
      return String_Uninitialized;
    case SnapshotStatus::kInProgress:
      return String_InProgress;
    case SnapshotStatus::kCompleted:
      return String_Completed;
    case SnapshotStatus::kFailed:
      return String_Failed;
    default:
      return "(unknown snapshot status)";
  }
}

auto replicated_state::snapshotStatusFromString(
    std::string_view string) noexcept -> SnapshotStatus {
  if (string == String_InProgress) {
    return SnapshotStatus::kInProgress;
  } else if (string == String_Completed) {
    return SnapshotStatus::kCompleted;
  } else if (string == String_Failed) {
    return SnapshotStatus::kFailed;
  } else {
    return SnapshotStatus::kUninitialized;
  }
}

void SnapshotInfo::toVelocyPack(velocypack::Builder& builder) const {
  velocypack::ObjectBuilder ob(&builder);
  TRI_ASSERT(!error.has_value());  // error not yet implemented
  builder.add(String_Timestamp,
              velocypack::Value(timepointToString(timestamp)));
  builder.add(String_Status, velocypack::Value(to_string(status)));
}

auto SnapshotInfo::fromVelocyPack(velocypack::Slice slice) -> SnapshotInfo {
  SnapshotInfo info;
  info.status = snapshotStatusFromString(slice.get(String_Status).stringView());
  info.timestamp = stringToTimepoint(slice.get(String_Timestamp).stringView());
  return info;
}
