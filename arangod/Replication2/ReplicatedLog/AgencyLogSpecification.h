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

#include "Agency/AgencyPaths.h"
#include "Cluster/ClusterTypes.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <optional>

namespace arangodb::replication2::agency {

struct from_velocypack_t {};
inline constexpr auto from_velocypack = from_velocypack_t{};

struct LogPlanTermSpecification {
  LogTerm term;
  LogConfig config;
  struct Leader {
    ParticipantId serverId;
    RebootId rebootId;
  };
  std::optional<Leader> leader;

  struct Participant {};
  std::unordered_map<ParticipantId, Participant> participants;

  auto toVelocyPack(VPackBuilder&) const -> void;
  LogPlanTermSpecification(from_velocypack_t, VPackSlice);
  LogPlanTermSpecification() = default;

  LogPlanTermSpecification(LogTerm term, LogConfig config, std::optional<Leader>, std::unordered_map<ParticipantId, Participant> participants);
};

struct LogPlanSpecification {
  LogId id;
  std::optional<LogPlanTermSpecification> currentTerm;

  LogConfig targetConfig;

  auto toVelocyPack(VPackBuilder&) const -> void;
  LogPlanSpecification(from_velocypack_t, VPackSlice);
  LogPlanSpecification() = default;

  LogPlanSpecification(LogId id, std::optional<LogPlanTermSpecification> term, LogConfig config);
};

struct LogCurrentLocalState {
  LogTerm term{};
  TermIndexPair spearhead{};

  auto toVelocyPack(VPackBuilder&) const -> void;
  LogCurrentLocalState() = default;
  LogCurrentLocalState(from_velocypack_t, VPackSlice);
  LogCurrentLocalState(LogTerm, TermIndexPair) noexcept;
};


struct LogCurrentSupervisionElection {
  enum class ErrorCode {
    OK = 0,
    SERVER_NOT_GOOD = 1,
    TERM_NOT_CONFIRMED = 2,
  };

  LogTerm term;
  std::size_t participantsRequired{};
  std::size_t participantsAvailable{};
  std::unordered_map<ParticipantId, ErrorCode> detail;

  auto toVelocyPack(VPackBuilder&) const -> void;

  friend auto operator==(LogCurrentSupervisionElection const&,
                         LogCurrentSupervisionElection const&) noexcept -> bool;
  friend auto operator!=(LogCurrentSupervisionElection const& left,
                         LogCurrentSupervisionElection const& right) noexcept -> bool {
    return !(left == right);
  }

  LogCurrentSupervisionElection() = default;
  LogCurrentSupervisionElection(from_velocypack_t, VPackSlice slice);
};

auto operator==(LogCurrentSupervisionElection const&,
                LogCurrentSupervisionElection const&) noexcept -> bool;

auto to_string(LogCurrentSupervisionElection::ErrorCode) noexcept -> std::string_view;
auto toVelocyPack(LogCurrentSupervisionElection::ErrorCode, VPackBuilder&) -> void;

struct LogCurrentSupervision {
  std::optional<LogCurrentSupervisionElection> election;

  auto toVelocyPack(VPackBuilder&) const -> void;

  LogCurrentSupervision() = default;
  LogCurrentSupervision(from_velocypack_t, VPackSlice slice);
};

struct LogCurrent {
  std::unordered_map<ParticipantId, LogCurrentLocalState> localState;
  std::optional<LogCurrentSupervision> supervision;

  auto toVelocyPack(VPackBuilder&) const -> void;
  LogCurrent(from_velocypack_t, VPackSlice);
  LogCurrent() = default;
};

}
