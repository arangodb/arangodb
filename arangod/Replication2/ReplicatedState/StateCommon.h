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
#include <compare>
#include <cstdint>
#include <iostream>
#include <optional>

#include <velocypack/Slice.h>
#include "Basics/Result.h"
#include "Inspection/VPackLoadInspector.h"
#include "Inspection/VPackSaveInspector.h"
#include "Inspection/Transformers.h"

namespace arangodb::velocypack {
class Value;
class Builder;
class Slice;
}  // namespace arangodb::velocypack

namespace arangodb {
namespace replication2::replicated_state {

enum class SnapshotStatus {
  kUninitialized,
  kInProgress,
  kCompleted,
  kFailed,
  kInvalidated,
};

struct SnapshotInfo {
  using clock = std::chrono::system_clock;

  struct Error {
    ErrorCode error{0};
    std::optional<std::string> message;
    clock::time_point retryAt;

    friend auto operator==(Error const&, Error const&) noexcept
        -> bool = default;
  };

  void updateStatus(SnapshotStatus status) noexcept;

  SnapshotStatus status{SnapshotStatus::kUninitialized};
  clock::time_point timestamp;
  std::optional<Error> error;

  friend auto operator==(SnapshotInfo const&, SnapshotInfo const&) noexcept
      -> bool = default;
};

auto to_string(SnapshotStatus) noexcept -> std::string_view;
auto snapshotStatusFromString(std::string_view) noexcept -> SnapshotStatus;
auto operator<<(std::ostream&, SnapshotStatus const&) -> std::ostream&;

struct SnapshotStatusStringTransformer {
  using SerializedType = std::string;
  auto toSerialized(SnapshotStatus source, std::string& target) const
      -> inspection::Status;
  auto fromSerialized(std::string const& source, SnapshotStatus& target) const
      -> inspection::Status;
};

template<class Inspector>
auto inspect(Inspector& f, SnapshotInfo& x) {
  return f.object(x).fields(
      f.field("timestamp", x.timestamp)
          .transformWith(inspection::TimeStampTransformer{}),
      f.field("error", x.error),
      f.field("status", x.status)
          .transformWith(SnapshotStatusStringTransformer{}));
}

template<class Inspector>
auto inspect(Inspector& f, SnapshotInfo::Error& x) {
  return f.object(x).fields(
      f.field("retryAt", x.retryAt)
          .transformWith(inspection::TimeStampTransformer{}),
      f.field("error", x.error)
          .transformWith(inspection::ErrorCodeTransformer{}),
      f.field("message", x.message));
}

}  // namespace replication2::replicated_state

}  // namespace arangodb
