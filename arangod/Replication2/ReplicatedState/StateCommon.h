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
template<typename, typename>
struct Extractor;
class Builder;
class Slice;
}  // namespace arangodb::velocypack
namespace arangodb {
namespace replication2::replicated_state {

struct StateGeneration {
  constexpr StateGeneration() noexcept : value{0} {}
  constexpr explicit StateGeneration(std::uint64_t value) noexcept
      : value{value} {}
  std::uint64_t value;

  [[nodiscard]] auto saturatedDecrement(uint64_t delta = 1) const noexcept
      -> StateGeneration;

  friend auto operator<=>(StateGeneration const&,
                          StateGeneration const&) = default;

  [[nodiscard]] auto operator+(std::uint64_t delta) const -> StateGeneration;
  auto operator++() noexcept -> StateGeneration&;
  auto operator++(int) noexcept -> StateGeneration;

  friend auto operator<<(std::ostream&, StateGeneration) -> std::ostream&;

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
};

auto to_string(StateGeneration) -> std::string;

template<class Inspector>
auto inspect(Inspector& f, StateGeneration& x) {
  if constexpr (Inspector::isLoading) {
    auto v = uint64_t{0};
    auto res = f.apply(v);
    if (res.ok()) {
      x = StateGeneration(v);
    }
    return res;
  } else {
    return f.apply(x.value);
  }
}

enum class SnapshotStatus {
  kUninitialized,
  kInProgress,
  kCompleted,
  kFailed,
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
auto operator<<(std::ostream&, StateGeneration) -> std::ostream&;

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

template<>
struct arangodb::velocypack::Extractor<
    replication2::replicated_state::StateGeneration> {
  static auto extract(velocypack::Slice slice)
      -> replication2::replicated_state::StateGeneration {
    return replication2::replicated_state::StateGeneration{
        slice.getNumericValue<std::uint64_t>()};
  }
};

}  // namespace arangodb
