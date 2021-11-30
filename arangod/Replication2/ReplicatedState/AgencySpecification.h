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

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Basics/ErrorCode.h"

#include <optional>
#include <chrono>
#include <string>

namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack

namespace arangodb::replication2::replicated_state::agency {

struct Current {
  LogId id;

  struct ParticipantStatus {
    std::size_t generation;

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
      };


      void toVelocyPack(velocypack::Builder& builder) const;
      [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Snapshot;

      Status status{Status::kInProgress};
      clock::time_point timestamp;
      std::optional<Error> error;
    };
  };

  std::unordered_map<ParticipantId, ParticipantStatus> participants;
};

static auto to_string(Current::ParticipantStatus::Snapshot::Status status) -> std::string_view;
static auto from_string(std::string_view string) -> Current::ParticipantStatus::Snapshot::Status;

}  // namespace arangodb::replication2::replicated_state::agency
