////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include <iosfwd>
#include "Agency/AgencyPaths.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterTypes.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/types.h"

#include <fmt/core.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <optional>
#include <type_traits>
#include <utility>

namespace arangodb::replication2::agency {

using ParticipantsFlagsMap =
    std::unordered_map<ParticipantId, ParticipantFlags>;

struct LogPlanConfig {
  std::size_t effectiveWriteConcern = 1;
  // TODO: Move this to the term config, we won't allow changing this
  // intra-term.
  bool waitForSync = false;

  LogPlanConfig() noexcept = default;
  LogPlanConfig(std::size_t effectiveWriteConcern, bool waitForSync) noexcept;
  friend auto operator==(LogPlanConfig const& left,
                         LogPlanConfig const& right) noexcept -> bool = default;
};

template<class Inspector>
auto inspect(Inspector& f, LogPlanConfig& x) {
  return f.object(x).fields(
      f.field("effectiveWriteConcern", x.effectiveWriteConcern),
      f.field("waitForSync", x.waitForSync));
}

struct ParticipantsConfig {
  std::size_t generation = 0;
  ParticipantsFlagsMap participants;
  LogPlanConfig config{};

  friend auto operator==(ParticipantsConfig const& left,
                         ParticipantsConfig const& right) noexcept
      -> bool = default;
  friend auto operator<<(std::ostream& os, ParticipantsConfig const&)
      -> std::ostream&;
};

auto operator<<(std::ostream& os, ParticipantsConfig const&) -> std::ostream&;

template<class Inspector>
auto inspect(Inspector& f, ParticipantsConfig& x) {
  return f.object(x).fields(f.field("generation", x.generation),
                            f.field("config", x.config),
                            f.field("participants", x.participants));
}

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

struct ServerInstanceReference {
  ParticipantId serverId;
  RebootId rebootId;

  ServerInstanceReference(ParticipantId participant, RebootId rebootId)
      : serverId{std::move(participant)}, rebootId{rebootId} {}
  ServerInstanceReference() : rebootId{RebootId{0}} {};
  friend auto operator==(ServerInstanceReference const&,
                         ServerInstanceReference const&) noexcept
      -> bool = default;
};

auto operator<<(std::ostream&, ServerInstanceReference const&) noexcept
    -> std::ostream&;

struct LogPlanTermSpecification {
  LogTerm term;
  std::optional<ServerInstanceReference> leader;

  LogPlanTermSpecification() = default;

  LogPlanTermSpecification(LogTerm term,
                           std::optional<ServerInstanceReference>);

  friend auto operator==(LogPlanTermSpecification const&,
                         LogPlanTermSpecification const&) noexcept
      -> bool = default;
  friend auto operator<<(std::ostream& os, LogPlanTermSpecification const&)
      -> std::ostream&;
};

auto operator<<(std::ostream& os, LogPlanTermSpecification const&)
    -> std::ostream&;

struct LogPlanSpecification {
  LogId id;
  std::optional<LogPlanTermSpecification> currentTerm;

  ParticipantsConfig participantsConfig;
  Properties properties;

  std::optional<std::string> owner;

  LogPlanSpecification() = default;

  LogPlanSpecification(LogId id, std::optional<LogPlanTermSpecification> term);
  LogPlanSpecification(LogId id, std::optional<LogPlanTermSpecification> term,
                       ParticipantsConfig participantsConfig);

  friend auto operator==(LogPlanSpecification const&,
                         LogPlanSpecification const&) noexcept
      -> bool = default;
  friend auto operator<<(std::ostream& os, LogPlanSpecification const&)
      -> std::ostream&;
};

auto operator<<(std::ostream& os, LogPlanSpecification const&) -> std::ostream&;

struct LogCurrentLocalState {
  LogTerm term{};
  TermIndexPair spearhead{};
  bool snapshotAvailable{false};
  replicated_log::LocalStateMachineStatus state;
  RebootId rebootId = RebootId(0);

  LogCurrentLocalState() = default;
  LogCurrentLocalState(LogTerm, TermIndexPair, bool,
                       RebootId rebootId) noexcept;
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
    SERVER_EXCLUDED = 3,
    SNAPSHOT_MISSING = 4,
  };

  LogTerm term;

  TermIndexPair bestTermIndex;

  // minimum quorum size of voters
  std::size_t participantsRequired{};
  // number of participants that are attending (i.e. reported back during this
  // election)
  std::size_t participantsAttending{};
  // number of participants that are attending and also eligible to vote
  std::size_t participantsVoting{};
  // whether all participants attend this election.
  bool allParticipantsAttending{};
  std::unordered_map<ParticipantId, ErrorCode> detail;
  // set of participants which are attending, eligible, and have the maximum
  // spearhead amongst all attending and eligible participants.
  std::vector<ServerInstanceReference> electibleLeaderSet;

  friend auto operator==(LogCurrentSupervisionElection const&,
                         LogCurrentSupervisionElection const&) noexcept -> bool;
  friend auto operator!=(LogCurrentSupervisionElection const& left,
                         LogCurrentSupervisionElection const& right) noexcept
      -> bool {
    return !(left == right);
  }

  LogCurrentSupervisionElection() = default;
};

auto operator<<(std::ostream&, LogCurrentSupervisionElection const&)
    -> std::ostream&;

auto operator==(LogCurrentSupervisionElection const&,
                LogCurrentSupervisionElection const&) noexcept -> bool;

auto to_string(LogCurrentSupervisionElection::ErrorCode) noexcept
    -> std::string_view;

struct LogCurrentSupervision {
  using clock = std::chrono::system_clock;

  struct TargetLeaderInvalid {
    static constexpr std::string_view code = "TargetLeaderInvalid";
    friend auto operator==(TargetLeaderInvalid const& s,
                           TargetLeaderInvalid const& s2) noexcept
        -> bool = default;
  };
  struct TargetLeaderExcluded {
    static constexpr std::string_view code = "TargetLeaderExcluded";
    friend auto operator==(TargetLeaderExcluded const& s,
                           TargetLeaderExcluded const& s2) noexcept
        -> bool = default;
  };
  struct TargetLeaderSnapshotMissing {
    static constexpr std::string_view code = "TargetLeaderSnapshotMissing";
    friend auto operator==(TargetLeaderSnapshotMissing const& s,
                           TargetLeaderSnapshotMissing const& s2) noexcept
        -> bool = default;
  };
  struct TargetLeaderFailed {
    static constexpr std::string_view code = "TargetLeaderFailed";
    friend auto operator==(TargetLeaderFailed const& s,
                           TargetLeaderFailed const& s2) noexcept
        -> bool = default;
  };
  struct TargetNotEnoughParticipants {
    static constexpr std::string_view code = "TargetNotEnoughParticipants";
    friend auto operator==(TargetNotEnoughParticipants const& s,
                           TargetNotEnoughParticipants const& s2) noexcept
        -> bool = default;
  };
  struct WaitingForConfigCommitted {
    static constexpr std::string_view code = "WaitingForConfigCommitted";
    friend auto operator==(WaitingForConfigCommitted const& s,
                           WaitingForConfigCommitted const& s2) noexcept
        -> bool = default;
  };
  struct LeaderElectionImpossible {
    static constexpr std::string_view code = "LeaderElectionImpossible";
    friend auto operator==(LeaderElectionImpossible const& s,
                           LeaderElectionImpossible const& s2) noexcept
        -> bool = default;
  };
  struct LeaderElectionOutOfBounds {
    static constexpr std::string_view code = "LeaderElectionOutOfBounds";
    friend auto operator==(LeaderElectionOutOfBounds const& s,
                           LeaderElectionOutOfBounds const& s2) noexcept
        -> bool = default;
  };
  struct LeaderElectionQuorumNotReached {
    static constexpr std::string_view code = "LeaderElectionQuorumNotReached";
    LogCurrentSupervisionElection election;
    friend auto operator==(LeaderElectionQuorumNotReached const& s,
                           LeaderElectionQuorumNotReached const& s2) noexcept
        -> bool = default;
  };
  struct LeaderElectionSuccess {
    static constexpr std::string_view code = "LeaderElectionSuccess";
    LogCurrentSupervisionElection election;
    friend auto operator==(LeaderElectionSuccess const& s,
                           LeaderElectionSuccess const& s2) noexcept
        -> bool = default;
  };
  struct SwitchLeaderFailed {
    static constexpr std::string_view code = "SwitchLeaderFailed";
    friend auto operator==(SwitchLeaderFailed const& s,
                           SwitchLeaderFailed const& s2) noexcept
        -> bool = default;
  };
  struct PlanNotAvailable {
    static constexpr std::string_view code = "PlanNotAvailable";
    friend auto operator==(PlanNotAvailable const& s,
                           PlanNotAvailable const& s2) noexcept
        -> bool = default;
  };
  struct CurrentNotAvailable {
    static constexpr std::string_view code = "CurrentNotAvailable";
    friend auto operator==(CurrentNotAvailable const& s,
                           CurrentNotAvailable const& s2) noexcept
        -> bool = default;
  };

  using StatusMessage =
      std::variant<TargetLeaderInvalid, TargetLeaderExcluded,
                   TargetLeaderSnapshotMissing, TargetLeaderFailed,
                   TargetNotEnoughParticipants, WaitingForConfigCommitted,
                   LeaderElectionImpossible, LeaderElectionOutOfBounds,
                   LeaderElectionQuorumNotReached, LeaderElectionSuccess,
                   SwitchLeaderFailed, PlanNotAvailable, CurrentNotAvailable>;

  using StatusReport = std::vector<StatusMessage>;

  // TODO / FIXME: This prevents assumedWriteConcern from
  // being initialised to 0, which would prevent any progress
  // at all and a broken log.
  // Under normal operation assumedWriteConcern is set to the
  // first effectiveWriteConcern that is calculated on creation
  // of the log.
  size_t assumedWriteConcern{1};
  bool assumedWaitForSync{false};
  std::optional<uint64_t> targetVersion;
  std::optional<StatusReport> statusReport;
  std::optional<clock::time_point> lastTimeModified;

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

    friend auto operator==(Leader const& s, Leader const& s2) noexcept
        -> bool = default;
  };

  // Will be nullopt until a leader has assumed leadership
  std::optional<Leader> leader;

  // Lower bounds of RebootIds used in the last committed log entry.
  std::unordered_map<ParticipantId, RebootId> safeRebootIds;

  // Temporary hack until Actions are de-serializable.
  struct ActionDummy {
    std::string timestamp;
    friend auto operator==(ActionDummy const& s, ActionDummy const& s2) noexcept
        -> bool = default;
  };
  std::vector<ActionDummy> actions;

  LogCurrent() = default;
  friend auto operator==(LogCurrent const& s, LogCurrent const& s2) noexcept
      -> bool = default;
};

struct LogTargetConfig {
  std::size_t writeConcern = 1;
  std::size_t softWriteConcern = 1;
  bool waitForSync = false;

  LogTargetConfig() noexcept = default;
  LogTargetConfig(std::size_t writeConcern, std::size_t softWriteConcern,
                  bool waitForSync) noexcept;

  friend auto operator==(LogTargetConfig const& left,
                         LogTargetConfig const& right) noexcept
      -> bool = default;
};

template<class Inspector>
auto inspect(Inspector& f, LogTargetConfig& x) {
  return f.object(x).fields(f.field("writeConcern", x.writeConcern),
                            f.field("softWriteConcern", x.softWriteConcern)
                                .fallback(std::ref(x.writeConcern)),
                            f.field("waitForSync", x.waitForSync));
}

struct LogTarget {
  LogId id;
  ParticipantsFlagsMap participants;
  LogTargetConfig config;
  Properties properties;

  std::optional<ParticipantId> leader;
  std::optional<uint64_t> version;

  struct Supervision {
    std::size_t maxActionsTraceLength{0};
    friend auto operator==(Supervision const&, Supervision const&) noexcept
        -> bool = default;
  };

  std::optional<Supervision> supervision;
  std::optional<std::string> owner;

  LogTarget() = default;

  LogTarget(LogId id, ParticipantsFlagsMap participants,
            LogTargetConfig const& config);

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

template<>
struct fmt::formatter<arangodb::replication2::agency::ServerInstanceReference>
    : formatter<string_view> {
  // parse is inherited from formatter<string_view>.
  template<typename FormatContext>
  auto format(
      arangodb::replication2::agency::ServerInstanceReference const& inst,
      FormatContext& ctx) const {
    return fmt::format_to(ctx.out(), "{}@{}", inst.serverId,
                          inst.rebootId.value());
  }
};

template<>
struct fmt::formatter<
    arangodb::replication2::agency::LogCurrentSupervisionElection::ErrorCode>
    : formatter<string_view> {
  // parse is inherited from formatter<string_view>.
  template<typename FormatContext>
  auto format(
      arangodb::replication2::agency::LogCurrentSupervisionElection::ErrorCode
          errorCode,
      FormatContext& ctx) const {
    return fmt::format_to(ctx.out(), "{}", to_string(errorCode));
  }
};
