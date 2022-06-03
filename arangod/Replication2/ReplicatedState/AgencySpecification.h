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
#include "Basics/StaticStrings.h"
#include "Inspection/VPack.h"
#include "Inspection/Transformers.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
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

namespace static_strings {
auto const String_Snapshot = velocypack::StringRef{"snapshot"};
auto const String_Generation = velocypack::StringRef{"generation"};
}  // namespace static_strings

struct ImplementationSpec {
  std::string type;
  std::optional<velocypack::SharedSlice> parameters;

  friend auto operator==(ImplementationSpec const& s,
                         ImplementationSpec const& s2) noexcept -> bool;
};

auto operator==(ImplementationSpec const& s,
                ImplementationSpec const& s2) noexcept -> bool;

template<class Inspector>
auto inspect(Inspector& f, ImplementationSpec& x) {
  return f.object(x).fields(
      f.field(StaticStrings::IndexType, x.type),
      f.field(StaticStrings::DataSourceParameters, x.parameters));
}

struct Properties {
  ImplementationSpec implementation;

  friend auto operator==(Properties const& s, Properties const& s2) noexcept
      -> bool = default;
};

template<class Inspector>
auto inspect(Inspector& f, Properties& x) {
  return f.object(x).fields(f.field("implementation", x.implementation));
}

struct Plan {
  LogId id;
  StateGeneration generation;
  Properties properties;
  std::optional<std::string> owner{};

  struct Participant {
    StateGeneration generation;

    friend auto operator==(Participant const& s, Participant const& s2) noexcept
        -> bool = default;
  };

  std::unordered_map<ParticipantId, Participant> participants;

  friend auto operator==(Plan const& s, Plan const& s2) noexcept
      -> bool = default;
};

template<class Inspector>
auto inspect(Inspector& f, Plan& x) {
  return f.object(x).fields(
      f.field(StaticStrings::Id, x.id),
      f.field(static_strings::String_Generation, x.generation),
      f.field(StaticStrings::Properties, x.properties),
      f.field("owner", x.owner),
      f.field(StaticStrings::Participants, x.participants)
          .fallback(std::unordered_map<ParticipantId, Plan::Participant>{}));
}

template<class Inspector>
auto inspect(Inspector& f, Plan::Participant& x) {
  return f.object(x).fields(
      f.field(static_strings::String_Generation, x.generation));
}

struct Current {
  struct ParticipantStatus {
    StateGeneration generation;
    SnapshotInfo snapshot;  // TODO might become an array later?

    friend auto operator==(ParticipantStatus const&,
                           ParticipantStatus const&) noexcept -> bool = default;
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

    friend auto operator==(Supervision const&, Supervision const&) noexcept
        -> bool = default;
  };

  std::optional<Supervision> supervision;

  friend auto operator==(Current const&, Current const&) noexcept
      -> bool = default;
};

template<class Inspector>
auto inspect(Inspector& f, Current& x) {
  return f.object(x).fields(
      f.field(StaticStrings::Participants, x.participants)
          .fallback(
              std::unordered_map<ParticipantId, Current::ParticipantStatus>{}),
      f.field("supervision", x.supervision));
}

template<class Inspector>
auto inspect(Inspector& f, Current::ParticipantStatus& x) {
  return f.object(x).fields(
      f.field(static_strings::String_Generation, x.generation),
      f.field(static_strings::String_Snapshot, x.snapshot));
}

template<class Inspector>
auto inspect(Inspector& f, Current::Supervision& x) {
  return f.object(x).fields(
      f.field(StaticStrings::Version, x.version),
      f.field("statusReport", x.statusReport),
      f.field("lastTimeModified", x.lastTimeModified)
          .transformWith(inspection::TimeStampTransformer{}));
}

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
    friend auto operator==(Participant const& s, Participant const& s2) noexcept
        -> bool = default;
  };

  std::unordered_map<ParticipantId, Participant> participants;
  arangodb::replication2::agency::LogTargetConfig config;
  std::optional<std::uint64_t> version;

  struct Supervision {};

  friend auto operator==(Target const& s, Target const& s2) noexcept
      -> bool = default;
};

template<class Inspector>
auto inspect(Inspector& f, Target& x) {
  return f.object(x).fields(
      f.field(StaticStrings::Id, x.id),
      f.field(StaticStrings::Properties, x.properties),
      f.field(StaticStrings::Leader, x.leader),
      f.field(StaticStrings::Participants, x.participants)
          .fallback(std::unordered_map<ParticipantId, Target::Participant>{}),
      f.field(StaticStrings::Config, x.config),
      f.field(StaticStrings::Version, x.version));
}

template<class Inspector>
auto inspect(Inspector& f, Target::Participant& x) {
  return f.object(x).fields();
}

struct State {
  Target target;
  std::optional<Plan> plan;
  std::optional<Current> current;
  friend auto operator==(State const& s, State const& s2) noexcept
      -> bool = default;
};

template<class Inspector>
auto inspect(Inspector& f, Current::Supervision::StatusMessage& x) {
  return f.object(x).fields(
      f.field("message", x.message),
      f.field("code", x.code).transformWith(StatusCodeStringTransformer{}),
      f.field("participant", x.participant));
}

}  // namespace arangodb::replication2::replicated_state::agency
