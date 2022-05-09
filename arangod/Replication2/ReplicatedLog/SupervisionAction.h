////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "velocypack/Builder.h"
#include "velocypack/velocypack-common.h"
#include <memory>
#include <utility>

#include <fmt/core.h>
#include <fmt/format.h>

#include "Agency/TransactionBuilder.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/Supervision/ModifyContext.h"

using namespace arangodb::replication2::agency;

namespace arangodb::replication2::replicated_log {

using ActionContext =
    ModifyContext<LogPlanSpecification, LogCurrentSupervision>;

// TODO: this action is redundant needs to be removed
struct EmptyAction {
  static constexpr std::string_view name = "EmptyAction";

  EmptyAction() : message(std::nullopt){};
  explicit EmptyAction(std::string message) : message(std::move(message)) {}

  std::optional<std::string> message;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyOrCreate<LogCurrentSupervision>(
        [&](LogCurrentSupervision& currentSupervision) {
          if (!currentSupervision.statusMessage or
              currentSupervision.statusMessage != message) {
            currentSupervision.statusMessage = message;
          }
        });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, EmptyAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack),
                            f.field("message", x.message));
}

/*
 * This action is placed into the supervision action to prevent
 * any other action from taking place.
 *
 * This is *different* from no action having been put into
 * the context as sometimes we will report a problem through
 * the reporting but do not want to continue;
 *
 * This action does not modify the agency state.
 */
struct NoActionPossibleAction {
  static constexpr std::string_view name = "NoActionPossibleAction";

  explicit NoActionPossibleAction(){};

  auto execute(ActionContext& ctx) const -> void {}
};
template<typename Inspector>
auto inspect(Inspector& f, NoActionPossibleAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack));
}

//  TODO: this action is redundant, needs to be removed
struct ErrorAction {
  static constexpr std::string_view name = "ErrorAction";

  ErrorAction(LogCurrentSupervisionError const& error) : _error{error} {};

  LogCurrentSupervisionError _error;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyOrCreate<LogCurrentSupervision>(
        [&](LogCurrentSupervision& currentSupervision) {
          if (!currentSupervision.error || currentSupervision.error != _error) {
            currentSupervision.error = _error;
          }
        });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, ErrorAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack),
                            f.field("message", x._error));
}

struct AddLogToPlanAction {
  static constexpr std::string_view name = "AddLogToPlanAction";

  AddLogToPlanAction(LogId const id, ParticipantsFlagsMap participants,
                     LogConfig config,
                     std::optional<LogPlanTermSpecification::Leader> leader)
      : _id(id),
        _participants(std::move(participants)),
        _config(std::move(config)),
        _leader(std::move(leader)){};
  LogId _id;
  ParticipantsFlagsMap _participants;
  LogConfig _config;
  std::optional<LogPlanTermSpecification::Leader> _leader;

  auto execute(ActionContext& ctx) const -> void {
    ctx.setValue<LogPlanSpecification>(
        _id, LogPlanTermSpecification(LogTerm{1}, _config, _leader),
        ParticipantsConfig{.generation = 1, .participants = _participants});
  }
};
template<typename Inspector>
auto inspect(Inspector& f, AddLogToPlanAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack), f.field("id", x._id),
                            f.field("participants", x._participants),
                            f.field("leader", x._leader),
                            f.field("config", x._config));
}

struct CurrentNotAvailableAction {
  static constexpr std::string_view name = "CurrentNotAvailableAction";

  auto execute(ActionContext& ctx) const -> void {
    ctx.setValue<LogCurrentSupervision>();
  }
};
template<typename Inspector>
auto inspect(Inspector& f, CurrentNotAvailableAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack));
}

struct SwitchLeaderAction {
  static constexpr std::string_view name = "SwitchLeaderAction";

  SwitchLeaderAction(LogPlanTermSpecification::Leader const& leader)
      : _leader{leader} {};

  LogPlanTermSpecification::Leader _leader;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modify<LogPlanSpecification>([&](LogPlanSpecification& plan) {
      plan.currentTerm->term = LogTerm{plan.currentTerm->term.value + 1};
      plan.currentTerm->leader = _leader;
    });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, SwitchLeaderAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack),
                            f.field("leader", x._leader));
}

// TODO: this should really be a report + NoActionPossible
struct DictateLeaderFailedAction {
  static constexpr std::string_view name = "DictateLeaderFailedAction";

  DictateLeaderFailedAction(std::string const& message) : _message{message} {};

  std::string _message;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyOrCreate<LogCurrentSupervision>(
        [&](LogCurrentSupervision& currentSupervision) {
          currentSupervision.statusMessage = _message;
        });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, DictateLeaderFailedAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack),
                            f.field("message", x._message));
}

struct WriteEmptyTermAction {
  static constexpr std::string_view name = "WriteEmptyTermAction";
  LogTerm minTerm;

  explicit WriteEmptyTermAction(LogTerm minTerm) : minTerm{minTerm} {};

  auto execute(ActionContext& ctx) const -> void {
    ctx.modify<LogPlanSpecification>([&](LogPlanSpecification& plan) {
      // TODO: what to do if currentTerm does not have a value?
      //       this shouldn't happen, but what if it does?
      plan.currentTerm->term = LogTerm{minTerm.value + 1};
      plan.currentTerm->leader.reset();
    });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, WriteEmptyTermAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack),
                            f.field("minTerm", x.minTerm));
}

// TODO: this should be a report and no action possible action
struct LeaderElectionImpossibleAction {
  static constexpr std::string_view name = "LeaderElectionImpossibleAction";

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyOrCreate<LogCurrentSupervision>(
        [&](LogCurrentSupervision& currentSupervision) {
          currentSupervision.statusMessage = "Leader election impossible";
        });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, LeaderElectionImpossibleAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack));
}

struct LeaderElectionOutOfBoundsAction {
  static constexpr std::string_view name = "LeaderElectionOutOfBoundsAction";

  LogCurrentSupervisionElection _election;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyOrCreate<LogCurrentSupervision>(
        [&](LogCurrentSupervision& currentSupervision) {
          currentSupervision.statusMessage =
              "Number of electible participants out of bounds";
          currentSupervision.election = _election;
        });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, LeaderElectionOutOfBoundsAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack),
                            f.field("election", x._election));
}

struct LeaderElectionQuorumNotReachedAction {
  static constexpr std::string_view name =
      "LeaderElectionQuorumNotReachedAction";

  LogCurrentSupervisionElection _election;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyOrCreate<LogCurrentSupervision>(
        [&](LogCurrentSupervision& currentSupervision) {
          currentSupervision.statusMessage = "Quorum not reached";
          currentSupervision.election = _election;
        });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, LeaderElectionQuorumNotReachedAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack),
                            f.field("election", x._election));
}

struct LeaderElectionAction {
  static constexpr std::string_view name = "LeaderElectionAction";

  LeaderElectionAction(LogPlanTermSpecification::Leader electedLeader,
                       LogCurrentSupervisionElection const& electionReport)
      : _electedLeader{electedLeader}, _electionReport(electionReport){};

  LogPlanTermSpecification::Leader _electedLeader;
  LogCurrentSupervisionElection _electionReport;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modify<LogPlanSpecification>([&](LogPlanSpecification& plan) {
      plan.currentTerm->term = LogTerm{plan.currentTerm->term.value + 1};
      plan.currentTerm->leader = _electedLeader;
    });
    ctx.modifyOrCreate<LogCurrentSupervision>(
        [&](LogCurrentSupervision& currentSupervision) {
          currentSupervision.election = _electionReport;
        });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, LeaderElectionAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack),
                            f.field("election", x._electionReport),
                            f.field("electedLeader", x._electedLeader));
}

struct UpdateParticipantFlagsAction {
  static constexpr std::string_view name = "UpdateParticipantFlagsAction";

  UpdateParticipantFlagsAction(ParticipantId const& participant,
                               ParticipantFlags const& flags)
      : _participant(participant), _flags(flags){};

  ParticipantId _participant;
  ParticipantFlags _flags;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modify<LogPlanSpecification>([&](LogPlanSpecification& plan) {
      plan.participantsConfig.participants.at(_participant) = _flags;
      plan.participantsConfig.generation += 1;
    });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, UpdateParticipantFlagsAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack),
                            f.field("participant", x._participant),
                            f.field("flags", x._flags));
}

struct AddParticipantToPlanAction {
  static constexpr std::string_view name = "AddParticipantToPlanAction";

  AddParticipantToPlanAction(ParticipantId const& participant,
                             ParticipantFlags const& flags)
      : _participant(participant), _flags(flags) {}

  ParticipantId _participant;
  ParticipantFlags _flags;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modify<LogPlanSpecification>([&](LogPlanSpecification& plan) {
      plan.participantsConfig.generation += 1;
      plan.participantsConfig.participants.emplace(_participant, _flags);
    });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, AddParticipantToPlanAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack),
                            f.field("participant", x._participant),
                            f.field("flags", x._flags));
}

struct RemoveParticipantFromPlanAction {
  static constexpr std::string_view name = "RemoveParticipantFromPlanAction";

  RemoveParticipantFromPlanAction(ParticipantId const& participant)
      : _participant(participant){};

  ParticipantId _participant;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modify<LogPlanSpecification>([&](LogPlanSpecification& plan) {
      plan.participantsConfig.participants.erase(_participant);
      plan.participantsConfig.generation += 1;
    });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, RemoveParticipantFromPlanAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack),
                            f.field("participant", x._participant));
}

struct UpdateLogConfigAction {
  static constexpr std::string_view name = "UpdateLogConfigAction";

  UpdateLogConfigAction(LogConfig const& config) : _config(config){};

  LogConfig _config;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyOrCreate<LogCurrentSupervision>(
        [&](LogCurrentSupervision& currentSupervision) {
          currentSupervision.statusMessage =
              "UpdatingLogConfig is not implemented yet";
        });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, UpdateLogConfigAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack));
}

struct ConvergedToTargetAction {
  static constexpr std::string_view name = "ConvergedToTargetAction";
  std::optional<std::uint64_t> version{std::nullopt};

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyOrCreate<LogCurrentSupervision>(
        [&](LogCurrentSupervision& currentSupervision) {
          currentSupervision.targetVersion = version;
        });
  }
};

template<typename Inspector>
auto inspect(Inspector& f, ConvergedToTargetAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack),
                            f.field("version", x.version));
}

using Action = std::variant<
    NoActionPossibleAction, EmptyAction, ErrorAction, AddLogToPlanAction,
    CurrentNotAvailableAction, SwitchLeaderAction, DictateLeaderFailedAction,
    WriteEmptyTermAction, LeaderElectionAction, LeaderElectionImpossibleAction,
    LeaderElectionOutOfBoundsAction, LeaderElectionQuorumNotReachedAction,
    UpdateParticipantFlagsAction, AddParticipantToPlanAction,
    RemoveParticipantFromPlanAction, UpdateLogConfigAction,
    ConvergedToTargetAction>;

auto executeAction(Log log, Action& action) -> ActionContext;

}  // namespace arangodb::replication2::replicated_log

template<>
struct fmt::formatter<arangodb::replication2::replicated_log::Action>
    : formatter<string_view> {
  // parse is inherited from formatter<string_view>.
  template<typename FormatContext>
  auto format(arangodb::replication2::replicated_log::Action a,
              FormatContext& ctx) const {
    auto const sv = std::visit([](auto&& arg) { return arg.name; }, a);
    return formatter<string_view>::format(sv, ctx);
  }
};
