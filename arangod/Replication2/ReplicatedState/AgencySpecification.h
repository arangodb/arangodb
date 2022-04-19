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

#include "Basics/ErrorCode.h"
#include "Inspection/VPack.h"
#include "Inspection/Transformers.h"
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

struct ImplementationSpec {
  std::string type;

  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice)
      -> ImplementationSpec;
};

struct Properties {
  ImplementationSpec implementation;

  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Properties;
};

struct Plan {
  LogId id;
  StateGeneration generation;
  Properties properties;

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
  struct ParticipantStatus {
    StateGeneration generation;
    SnapshotInfo snapshot;  // TODO might become an array later?

    friend auto operator==(ParticipantStatus const&,
                           ParticipantStatus const&) noexcept -> bool = default;
    void toVelocyPack(velocypack::Builder& builder) const;
    [[nodiscard]] static auto fromVelocyPack(velocypack::Slice)
        -> ParticipantStatus;
  };

  std::unordered_map<ParticipantId, ParticipantStatus> participants;

  struct Supervision {
    using clock = std::chrono::system_clock;

    std::optional<std::uint64_t> version;

    enum StatusCode {
      kLogNotCreated,
      kLogPlanNotAvailable,
      kLogCurrentNotAvailable,
      kServerSnapshotMissing,
      kInsufficientSnapshotCoverage,
      kLogParticipantNotYetGone,
    };

    struct StatusMessage {
      std::optional<std::string> message;
      StatusCode code;
      std::optional<ParticipantId> participant;

      StatusMessage() = default;
      StatusMessage(StatusCode code, std::optional<ParticipantId> participant)
          : code(code), participant(std::move(participant)) {}

      friend auto operator==(StatusMessage const&,
                             StatusMessage const&) noexcept -> bool = default;
    };

    using StatusReport = std::vector<StatusMessage>;

    std::optional<StatusReport> statusReport;
    std::optional<clock::time_point> lastTimeModified;

    void toVelocyPack(velocypack::Builder& builder) const;
    [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Supervision;
    friend auto operator==(Supervision const&, Supervision const&) noexcept
        -> bool = default;
  };

  std::optional<Supervision> supervision;

  friend auto operator==(Current const&, Current const&) noexcept
      -> bool = default;
  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Current;
};

using StatusCode = Current::Supervision::StatusCode;
using StatusMessage = Current::Supervision::StatusMessage;
using StatusReport = Current::Supervision::StatusReport;

auto to_string(StatusCode) noexcept -> std::string_view;

struct StatusCodeStringTransformer {
  using SerializedType = std::string;
  auto toSerialized(StatusCode source, std::string& target) const
      -> inspection::Status;
  auto fromSerialized(std::string const& source, StatusCode& target) const
      -> inspection::Status;
};

struct Target {
  LogId id;

  Properties properties;

  std::optional<ParticipantId> leader;

  struct Participant {
    void toVelocyPack(velocypack::Builder& builder) const;
    [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Participant;
  };

  std::unordered_map<ParticipantId, Participant> participants;
  LogConfig config;
  std::optional<std::uint64_t> version;

  struct Supervision {};

  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Target;
};

struct State {
  Target target;
  std::optional<Plan> plan;
  std::optional<Current> current;
};

template<class Inspector>
auto inspect(Inspector& f, Current::Supervision& x) {
  return f.object(x).fields(
      f.field("version", x.version),
      f.field("lastTimeModified", x.lastTimeModified)
          .transformWith(inspection::TimeStampTransformer{}),
      f.field("statusReport", x.statusReport));
}

template<class Inspector>
auto inspect(Inspector& f, Current::Supervision::StatusMessage& x) {
  return f.object(x).fields(
      f.field("message", x.message),
      f.field("code", x.code).transformWith(StatusCodeStringTransformer{}),
      f.field("participant", x.participant));
}

}  // namespace arangodb::replication2::replicated_state::agency
