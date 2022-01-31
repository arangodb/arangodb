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

  auto toVelocyPack(VPackBuilder&) const -> void;
  LogPlanTermSpecification(from_velocypack_t, VPackSlice);
  LogPlanTermSpecification() = default;

  LogPlanTermSpecification(LogTerm term, LogConfig config,
                           std::optional<Leader>);
};

struct LogPlanSpecification {
  LogId id;
  std::optional<LogPlanTermSpecification> currentTerm;

  ParticipantsConfig participantsConfig;

  auto toVelocyPack(velocypack::Builder&) const -> void;
  static auto fromVelocyPack(velocypack::Slice) -> LogPlanSpecification;
  LogPlanSpecification(from_velocypack_t, VPackSlice);
  LogPlanSpecification();

  LogPlanSpecification(LogId id, std::optional<LogPlanTermSpecification> term);
  LogPlanSpecification(LogId id, std::optional<LogPlanTermSpecification> term,
                       ParticipantsConfig participantsConfig);
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
  // TODO: I would really like the Election type to be
  //       a tagged union, but it is such a pain to implement
  //       so I currently hacked the possible outcomes of an
  //       election in here.
  enum class Outcome {
    SUCCESS = 0,     // A new leader has been selected
    IMPOSSIBLE = 1,  // Election is impossible because the number of
                     // participants + 1 is less than writeConcern
    FAILED = 2,      // The election failed
  };
  // This error code applies to participants, not to
  // the election itself
  enum class ErrorCode {
    OK = 0,
    SERVER_NOT_GOOD = 1,
    TERM_NOT_CONFIRMED = 2,
    SERVER_EXCLUDED = 3
  };

  std::optional<Outcome> outcome{std::nullopt};
  LogTerm term;

  TermIndexPair bestTermIndex;

  std::size_t participantsRequired{};
  std::size_t participantsAvailable{};
  std::unordered_map<ParticipantId, ErrorCode> detail;
  std::vector<ParticipantId> electibleLeaderSet;

  auto toVelocyPack(VPackBuilder&) const -> void;

  friend auto operator==(LogCurrentSupervisionElection const&,
                         LogCurrentSupervisionElection const&) noexcept -> bool;
  friend auto operator!=(LogCurrentSupervisionElection const& left,
                         LogCurrentSupervisionElection const& right) noexcept
      -> bool {
    return !(left == right);
  }

  LogCurrentSupervisionElection() = default;
  LogCurrentSupervisionElection(from_velocypack_t, VPackSlice slice);
};

auto operator==(LogCurrentSupervisionElection const&,
                LogCurrentSupervisionElection const&) noexcept -> bool;

auto to_string(LogCurrentSupervisionElection::ErrorCode) noexcept
    -> std::string_view;
auto toVelocyPack(LogCurrentSupervisionElection::ErrorCode, VPackBuilder&)
    -> void;

auto to_string(LogCurrentSupervisionElection::Outcome) noexcept
    -> std::string_view;
auto toVelocyPack(LogCurrentSupervisionElection::Outcome, VPackBuilder&)
    -> void;

enum class LogCurrentSupervisionError { TARGET_LEADER_INVALID };

auto to_string(LogCurrentSupervisionError) noexcept -> std::string_view;
auto toVelocyPack(LogCurrentSupervisionError, VPackBuilder&) -> void;

struct LogCurrentSupervision {
  std::optional<LogCurrentSupervisionElection> election;
  std::optional<LogCurrentSupervisionError> error;

  auto toVelocyPack(VPackBuilder&) const -> void;

  LogCurrentSupervision() = default;
  LogCurrentSupervision(from_velocypack_t, VPackSlice slice);
};

struct LogCurrent {
  std::unordered_map<ParticipantId, LogCurrentLocalState> localState;
  std::optional<LogCurrentSupervision> supervision;

  struct Leader {
    ParticipantId serverId;
    LogTerm term;
    // optional because the leader might not have committed anything
    std::optional<ParticipantsConfig> committedParticipantsConfig;
    bool leadershipEstablished;
    // will be set after 5s if leader is unable to establish leadership
    std::optional<replicated_log::CommitFailReason> commitStatus;

    auto toVelocyPack(VPackBuilder&) const -> void;
    static auto fromVelocyPack(VPackSlice) -> Leader;
  };

  // Will be nullopt until a leader has been assumed leadership
  std::optional<Leader> leader;

  auto toVelocyPack(VPackBuilder&) const -> void;
  static auto fromVelocyPack(VPackSlice) -> LogCurrent;
  LogCurrent(from_velocypack_t, VPackSlice);
  LogCurrent() = default;
};

struct LogTarget {
  using Participants = std::unordered_map<ParticipantId, ParticipantFlags>;

  LogId id;
  Participants participants;
  LogConfig config;

  std::optional<ParticipantId> leader;

  struct Properties {
    void toVelocyPack(velocypack::Builder&) const;
    static auto fromVelocyPack(velocypack::Slice) -> Properties;
  };
  Properties properties;

  struct Supervision {
    std::size_t maxActionsTraceLength{0};
    auto toVelocyPack(velocypack::Builder&) const -> void;
    static auto fromVelocyPack(velocypack::Slice) -> Supervision;
  };

  std::optional<Supervision> supervision;

  static auto fromVelocyPack(velocypack::Slice) -> LogTarget;
  void toVelocyPack(velocypack::Builder&) const;

  LogTarget(from_velocypack_t, VPackSlice);
  LogTarget() = default;

  LogTarget(LogId id, Participants const& participants,
            LogConfig const& config);
};

/* Convenience Wrapper */
struct Log {
  LogTarget target;

  // These two do not necessarily exist in the Agency
  // so when we're called for a Log these might not
  // exist
  std::optional<LogPlanSpecification> plan;
  std::optional<LogCurrent> current;
};

}  // namespace arangodb::replication2::agency
