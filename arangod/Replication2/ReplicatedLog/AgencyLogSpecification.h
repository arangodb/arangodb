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

namespace static_strings {
auto constexpr CommittedParticipantsConfig =
    std::string_view{"committedParticipantsConfig"};
auto constexpr ParticipantsConfig = std::string_view{"participantsConfig"};
auto constexpr BestTermIndex = std::string_view{"bestTermIndex"};
auto constexpr ParticipantsRequired = std::string_view{"participantsRequired"};
auto constexpr ParticipantsAvailable =
    std::string_view{"participantsAvailable"};
auto constexpr Details = std::string_view{"details"};
auto constexpr ElectibleLeaderSet = std::string_view{"electibleLeaderSet"};
auto constexpr Election = std::string_view{"election"};
auto constexpr Error = std::string_view{"error"};
auto constexpr StatusMessage = std::string_view{"StatusMessage"};
auto constexpr LeadershipEstablished =
    std::string_view{"leadershipEstablished"};
auto constexpr CommitStatus = std::string_view{"commitStatus"};
auto constexpr Supervision = std::string_view{"supervision"};
auto constexpr Leader = std::string_view{"leader"};
auto constexpr TargetVersion = std::string_view{"targetVersion"};
auto constexpr Version = std::string_view{"version"};
auto constexpr Actions = std::string_view{"actions"};
auto constexpr MaxActionsTraceLength =
    std::string_view{"maxActionsTraceLength"};
auto constexpr Code = std::string_view{"code"};
auto constexpr Message = std::string_view{"message"};
}  // namespace static_strings

struct from_velocypack_t {};
inline constexpr auto from_velocypack = from_velocypack_t{};

using ParticipantsFlagsMap =
    std::unordered_map<ParticipantId, ParticipantFlags>;

struct LogPlanTermSpecification {
  LogTerm term;
  LogConfig config;
  struct Leader {
    ParticipantId serverId;
    RebootId rebootId;

    Leader(ParticipantId const& participant, RebootId const& rebootId)
        : serverId{participant}, rebootId{rebootId} {}
    Leader(from_velocypack_t, VPackSlice);
    Leader() : rebootId{RebootId{0}} {};
    auto toVelocyPack(VPackBuilder&) const -> void;
    friend auto operator==(Leader const&, Leader const&) noexcept
        -> bool = default;
  };
  std::optional<Leader> leader;

  auto toVelocyPack(VPackBuilder&) const -> void;
  LogPlanTermSpecification(from_velocypack_t, VPackSlice);
  LogPlanTermSpecification() = default;

  LogPlanTermSpecification(LogTerm term, LogConfig config,
                           std::optional<Leader>);

  friend auto operator==(LogPlanTermSpecification const&,
                         LogPlanTermSpecification const&) noexcept
      -> bool = default;
};

template<class Inspector>
auto inspect(Inspector& f, LogPlanTermSpecification::Leader& x) {
  return f.object(x).fields(f.field(StaticStrings::ServerId, x.serverId),
                            f.field(StaticStrings::RebootId, x.rebootId));
}

template<class Inspector>
auto inspect(Inspector& f, LogPlanTermSpecification& x) {
  return f.object(x).fields(f.field(StaticStrings::Term, x.term),
                            f.field(StaticStrings::Config, x.config),
                            f.field(StaticStrings::Leader, x.leader));
}

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

  friend auto operator==(LogPlanSpecification const&,
                         LogPlanSpecification const&) noexcept
      -> bool = default;
};

template<class Inspector>
auto inspect(Inspector& f, LogPlanSpecification& x) {
  return f.object(x).fields(
      f.field(StaticStrings::Id, x.id),
      f.field(StaticStrings::CurrentTerm, x.currentTerm),
      f.field(static_strings::ParticipantsConfig, x.participantsConfig));
};

struct LogCurrentLocalState {
  LogTerm term{};
  TermIndexPair spearhead{};

  auto toVelocyPack(VPackBuilder&) const -> void;
  LogCurrentLocalState() = default;
  LogCurrentLocalState(from_velocypack_t, VPackSlice);
  LogCurrentLocalState(LogTerm, TermIndexPair) noexcept;
};

template<class Inspector>
auto inspect(Inspector& f, LogCurrentLocalState& x) {
  return f.object(x).fields(f.field(StaticStrings::Term, x.term),
                            f.field(StaticStrings::Spearhead, x.spearhead));
}

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
  LogCurrentSupervisionElection(from_velocypack_t, VPackSlice slice);
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

template<typename Enum>
struct EnumStruct {
  static_assert(std::is_enum_v<Enum>);

  EnumStruct() : code{0}, message{} {};
  EnumStruct(Enum e)
      : code(static_cast<std::underlying_type_t<Enum>>(e)),
        message(to_string(e)){};

  [[nodiscard]] auto get() const -> Enum { return static_cast<Enum>(code); }

  std::underlying_type_t<Enum> code;
  std::string message;
};

template<class Inspector, typename Enum>
auto inspect(Inspector& f, EnumStruct<Enum>& x) {
  return f.object(x).fields(f.field(static_strings::Code, x.code),
                            f.field(static_strings::Message, x.message));
};

template<class Inspector>
auto inspect(Inspector& f, LogCurrentSupervisionError& x) {
  if constexpr (Inspector::isLoading) {
    auto v = EnumStruct<LogCurrentSupervisionError>();
    auto res = f.apply(v);
    if (res.ok()) {
      x = v.get();
    }
    return res;
  } else {
    auto v = EnumStruct<LogCurrentSupervisionError>(x);
    return f.apply(v);
  }
}

template<class Inspector>
auto inspect(Inspector& f, LogCurrentSupervisionElection::ErrorCode& x) {
  if constexpr (Inspector::isLoading) {
    auto v = EnumStruct<LogCurrentSupervisionElection::ErrorCode>();
    auto res = f.apply(v);
    if (res.ok()) {
      x = v.get();
    }
    return res;
  } else {
    auto v = EnumStruct<LogCurrentSupervisionElection::ErrorCode>(x);
    return f.apply(v);
  }
}

template<class Inspector>
auto inspect(Inspector& f, LogCurrentSupervisionElection& x) {
  return f.object(x).fields(
      f.field(StaticStrings::Term, x.term),
      f.field(static_strings::BestTermIndex, x.bestTermIndex),
      f.field(static_strings::ParticipantsRequired, x.participantsRequired),
      f.field(static_strings::ParticipantsAvailable, x.participantsAvailable),
      f.field(static_strings::Details, x.detail),
      f.field(static_strings::ElectibleLeaderSet, x.electibleLeaderSet));
}

struct LogCurrentSupervision {
  std::optional<LogCurrentSupervisionElection> election;
  std::optional<LogCurrentSupervisionError> error;
  std::optional<std::string> statusMessage;
  std::optional<uint64_t> targetVersion;

  auto toVelocyPack(VPackBuilder&) const -> void;
  static auto fromVelocyPack(velocypack::Slice) -> LogCurrentSupervision;

  LogCurrentSupervision() = default;
  LogCurrentSupervision(from_velocypack_t, VPackSlice slice);
};

template<class Inspector>
auto inspect(Inspector& f, LogCurrentSupervision& x) {
  return f.object(x).fields(
      f.field(static_strings::Election, x.election),
      f.field(static_strings::Error, x.error),
      f.field(static_strings::TargetVersion, x.targetVersion),
      f.field(static_strings::StatusMessage, x.statusMessage));
}

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
  std::optional<std::uint64_t> targetVersion;

  // Temporary hack until Actions are de-serializable.
  struct ActionDummy {
    std::string timestamp;
  };
  std::vector<ActionDummy> actions;

  auto toVelocyPack(VPackBuilder&) const -> void;
  static auto fromVelocyPack(VPackSlice) -> LogCurrent;
  LogCurrent(from_velocypack_t, VPackSlice);
  LogCurrent() = default;
};

template<class Inspector>
auto inspect(Inspector& f, LogCurrent::Leader& x) {
  return f.object(x).fields(
      f.field(StaticStrings::ServerId, x.serverId),
      f.field(StaticStrings::Term, x.term),
      f.field(static_strings::CommittedParticipantsConfig,
              x.committedParticipantsConfig),
      f.field(static_strings::LeadershipEstablished, x.leadershipEstablished)
          .fallback(false),
      f.field(static_strings::CommitStatus, x.commitStatus));
}

template<class Inspector>
auto inspect(Inspector& f, LogCurrent::ActionDummy& x) {
  if constexpr (Inspector::isLoading) {
    x = LogCurrent::ActionDummy{};
  } else {
  }
  return arangodb::inspection::Status::Success{};
}

template<class Inspector>
auto inspect(Inspector& f, LogCurrent& x) {
  return f.object(x).fields(
      f.field(StaticStrings::LocalStatus, x.localState)
          .fallback(std::unordered_map<ParticipantId, LogCurrentLocalState>{}),
      f.field(static_strings::Supervision, x.supervision),
      f.field(static_strings::Leader, x.leader),
      f.field(static_strings::Actions, x.actions)
          .fallback(std::vector<LogCurrent::ActionDummy>{}));
}

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

  static auto fromVelocyPack(velocypack::Slice) -> LogTarget;
  void toVelocyPack(velocypack::Builder&) const;

  LogTarget(from_velocypack_t, VPackSlice);
  LogTarget() = default;

  LogTarget(LogId id, ParticipantsFlagsMap const& participants,
            LogConfig const& config);

  friend auto operator==(LogTarget const&, LogTarget const&) noexcept
      -> bool = default;
};

template<class Inspector>
auto inspect(Inspector& f, LogTarget::Supervision& x) {
  return f.object(x).fields(
      f.field(static_strings::MaxActionsTraceLength, x.maxActionsTraceLength));
}

template<class Inspector>
auto inspect(Inspector& f, LogTarget& x) {
  return f.object(x).fields(
      f.field(StaticStrings::Id, x.id),
      f.field(StaticStrings::Participants, x.participants)
          .fallback(ParticipantsFlagsMap{}),
      f.field(StaticStrings::Config, x.config),
      f.field(StaticStrings::Leader, x.leader),
      f.field(static_strings::Version, x.version),
      f.field(static_strings::Supervision, x.supervision));
}

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
