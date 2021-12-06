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

#include "Basics/ErrorCode.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedState/StateCommon.h"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>

namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack

namespace arangodb::replication2::replicated_state::agency {

struct Plan {
  LogId id;
  StateGeneration generation;

  struct Participant {
    StateGeneration generation;

    void toVelocyPack(velocypack::Builder& builder) const;
    [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Participant;
  };

  std::unordered_map<ParticipantId, Participant> participants;

  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Plan;
};



struct Current {
  LogId id;

  struct ParticipantStatus {
    StateGeneration generation;

    struct Snapshot {
      enum class Status {
        kInProgress,
        kCompleted,
        kFailed,
      };

      using clock = std::chrono::system_clock;

      struct Error {
        ErrorCode error;
        std::optional<std::string> message;
        clock::time_point retryAt;
        void toVelocyPack(velocypack::Builder& builder) const;
        [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Error;

        friend auto operator==(Error const&, Error const&) noexcept -> bool = default;
      };

      void toVelocyPack(velocypack::Builder& builder) const;
      [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Snapshot;

      friend auto operator==(Snapshot const&, Snapshot const&) noexcept -> bool = default;

      Status status{Status::kInProgress};
      clock::time_point timestamp;
      std::optional<Error> error;
    };

    // TODO might become an array later?
    Snapshot snapshot;

    friend auto operator==(ParticipantStatus const&, ParticipantStatus const&) noexcept
        -> bool = default;
    void toVelocyPack(velocypack::Builder& builder) const;
    [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> ParticipantStatus;
  };

  std::unordered_map<ParticipantId, ParticipantStatus> participants;

  friend auto operator==(Current const&, Current const&) noexcept -> bool = default;
  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Current;
};

auto to_string(Current::ParticipantStatus::Snapshot::Status status) -> std::string_view;
auto from_string(std::string_view string) -> Current::ParticipantStatus::Snapshot::Status;

}  // namespace arangodb::replication2::replicated_state::agency
