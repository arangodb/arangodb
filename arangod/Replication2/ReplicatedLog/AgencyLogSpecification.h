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
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterTypes.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <optional>
#include <type_traits>

namespace arangodb::replication2::agency {

struct LogPlanTermSpecification {
  LogTerm term;
  LogConfig config;
  struct Leader {
    ParticipantId serverId;
    RebootId rebootId;

    Leader(ParticipantId const& participant, RebootId const& rebootId)
        : serverId{participant}, rebootId{rebootId} {}
    Leader() : rebootId{RebootId{0}} {};
    auto toVelocyPack(VPackBuilder&) const -> void;
    friend auto operator==(Leader const&, Leader const&) noexcept
        -> bool = default;
  };
  std::optional<Leader> leader;

  auto toVelocyPack(VPackBuilder&) const -> void;
  LogPlanTermSpecification() = default;

  LogPlanTermSpecification(LogTerm term, LogConfig config,
                           std::optional<Leader>);

  friend auto operator==(LogPlanTermSpecification const&,
                         LogPlanTermSpecification const&) noexcept
      -> bool = default;
};

struct LogPlanSpecification {
  LogId id;
  std::optional<LogPlanTermSpecification> currentTerm;

  ParticipantsConfig participantsConfig;

  auto toVelocyPack(velocypack::Builder&) const -> void;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice)
      -> LogPlanSpecification;
  LogPlanSpecification();

  LogPlanSpecification(LogId id, std::optional<LogPlanTermSpecification> term);
  LogPlanSpecification(LogId id, std::optional<LogPlanTermSpecification> term,
                       ParticipantsConfig participantsConfig);

  friend auto operator==(LogPlanSpecification const&,
                         LogPlanSpecification const&) noexcept
      -> bool = default;
};

struct LogCurrentLocalState {
  LogTerm term{};
  TermIndexPair spearhead{};

  auto toVelocyPack(VPackBuilder&) const -> void;
  LogCurrentLocalState() = default;
  LogCurrentLocalState(LogTerm, TermIndexPair) noexcept;
  friend auto operator==(LogCurrentLocalState const& s,
                         LogCurrentLocalState const& s2) noexcept
      -> bool = default;
};

struct LogCurrentSupervisionElection {
  // This error code applies to participants, not to
  // the election itself
  enum class ErrorCode {
    OK = 0,
    SERVER_NOT_GOOD = 1,
    TERM_NOT_CONFIRMED = 2,
    SERVER_EXCLUDED = 3
  };

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
};

auto operator==(LogCurrentSupervisionElection const&,
                LogCurrentSupervisionElection const&) noexcept -> bool;

auto to_string(LogCurrentSupervisionElection::ErrorCode) noexcept
    -> std::string_view;
auto toVelocyPack(LogCurrentSupervisionElection::ErrorCode, VPackBuilder&)
    -> void;

enum class LogCurrentSupervisionError {
  TARGET_LEADER_INVALID,
  TARGET_LEADER_EXCLUDED,
  TARGET_NOT_ENOUGH_PARTICIPANTS,
  GENERAL_ERROR  // TODO: Using this whilw refactoring
                 // other code; needs to be improved
};

auto to_string(LogCurrentSupervisionError) noexcept -> std::string_view;
auto toVelocyPack(LogCurrentSupervisionError, VPackBuilder&) -> void;

struct LogCurrentSupervision {
  std::optional<LogCurrentSupervisionElection> election;
  std::optional<LogCurrentSupervisionError> error;
  std::optional<std::string> statusMessage;
  std::optional<uint64_t> targetVersion;

  auto toVelocyPack(VPackBuilder&) const -> void;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice)
      -> LogCurrentSupervision;

  LogCurrentSupervision() = default;
  friend auto operator==(LogCurrentSupervision const& s,
                         LogCurrentSupervision const& s2) noexcept
      -> bool = default;
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
    [[nodiscard]] static auto fromVelocyPack(VPackSlice) -> Leader;
    friend auto operator==(Leader const& s, Leader const& s2) noexcept
        -> bool = default;
  };

  // Will be nullopt until a leader has been assumed leadership
  std::optional<Leader> leader;
  std::optional<std::uint64_t> targetVersion;

  // Temporary hack until Actions are de-serializable.
  struct ActionDummy {
    std::string timestamp;
    friend auto operator==(ActionDummy const& s, ActionDummy const& s2) noexcept
        -> bool = default;
  };
  std::vector<ActionDummy> actions;

  auto toVelocyPack(VPackBuilder&) const -> void;
  [[nodiscard]] static auto fromVelocyPack(VPackSlice) -> LogCurrent;
  LogCurrent() = default;
  friend auto operator==(LogCurrent const& s, LogCurrent const& s2) noexcept
      -> bool = default;
};

struct LogTarget {
  LogId id;
  ParticipantsFlagsMap participants;
  LogConfig config;

  std::optional<ParticipantId> leader;
  std::optional<uint64_t> version;

  struct Supervision {
    std::size_t maxActionsTraceLength{0};
    friend auto operator==(Supervision const&, Supervision const&) noexcept
        -> bool = default;
  };

  std::optional<Supervision> supervision;

  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> LogTarget;
  void toVelocyPack(velocypack::Builder&) const;

  LogTarget() = default;

  LogTarget(LogId id, ParticipantsFlagsMap const& participants,
            LogConfig const& config);

  friend auto operator==(LogTarget const&, LogTarget const&) noexcept
      -> bool = default;
};

/* Convenience Wrapper */
struct Log {
  LogTarget target;

  // These two do not necessarily exist in the Agency
  // so when we're called for a Log these might not
  // exist
  std::optional<LogPlanSpecification> plan;
  std::optional<LogCurrent> current;
  friend auto operator==(Log const& s, Log const& s2) noexcept
      -> bool = default;
};

}  // namespace arangodb::replication2::agency
