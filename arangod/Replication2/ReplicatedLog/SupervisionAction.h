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

#include "Basics/debugging.h"
#include "velocypack/Builder.h"
#include "velocypack/velocypack-common.h"
#include <memory>
#include <utility>

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>

#include "Agency/TransactionBuilder.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/Supervision/ModifyContext.h"

using namespace arangodb::replication2::agency;

namespace arangodb::replication2::replicated_log {

using ActionContext =
    ModifyContext<LogPlanSpecification, LogCurrentSupervision>;

/* The empty action signifies that no action has been put
 * into an action context yet; we use a seprarte action
 * instead of a std::optional<Action>, because it is less
 * prone to crashes and undefined behaviour
 */
struct EmptyAction {
  static constexpr std::string_view name = "EmptyAction";

  explicit EmptyAction(){};

  auto execute(ActionContext& ctx) const -> void {}
};
template<typename Inspector>
auto inspect(Inspector& f, EmptyAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack));
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

struct AddLogToPlanAction {
  static constexpr std::string_view name = "AddLogToPlanAction";

  AddLogToPlanAction(LogId const id, ParticipantsFlagsMap participants,
                     LogPlanConfig config,
                     std::optional<LogPlanTermSpecification::Leader> leader)
      : _id(id),
        _participants(std::move(participants)),
        _config(std::move(config)),
        _leader(std::move(leader)){};
  LogId _id;
  ParticipantsFlagsMap _participants;
  LogPlanConfig _config;
  std::optional<LogPlanTermSpecification::Leader> _leader;

  auto execute(ActionContext& ctx) const -> void {
    auto newPlan = LogPlanSpecification(
        _id, LogPlanTermSpecification(LogTerm{1}, _leader),
        ParticipantsConfig{
            .generation = 1, .participants = _participants, .config = _config});
    newPlan.owner = "target";
    ctx.setValue<LogPlanSpecification>(std::move(newPlan));

    ctx.modifyOrCreate<LogCurrentSupervision>(
        [&](LogCurrentSupervision& currentSupervision) {
          currentSupervision.assumedWriteConcern =
              _config.effectiveWriteConcern;
        });
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
      TRI_ASSERT(plan.participantsConfig.participants.contains(_participant));
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

  UpdateLogConfigAction(LogPlanConfig const& config) : _config(config){};

  LogPlanConfig _config;

  auto execute(ActionContext& ctx) const -> void {
    // TODO: updating log config is not implemented yet
  }
};
template<typename Inspector>
auto inspect(Inspector& f, UpdateLogConfigAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(f.field("type", hack));
}

struct UpdateEffectiveAndAssumedWriteConcernAction {
  static constexpr std::string_view name =
      "UpdateEffectiveAndAssumedWriteConcernAction";
  size_t newEffectiveWriteConcern;
  size_t newAssumedWriteConcern;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modify<LogPlanSpecification>([&](LogPlanSpecification& plan) {
      plan.participantsConfig.config.effectiveWriteConcern =
          newEffectiveWriteConcern;
      plan.participantsConfig.generation += 1;
    });
    ctx.modify<LogCurrentSupervision>(
        [&](LogCurrentSupervision& currentSupervision) {
          currentSupervision.assumedWriteConcern = newAssumedWriteConcern;
        });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, UpdateEffectiveAndAssumedWriteConcernAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(
      f.field("type", hack),
      f.field("newEffectiveWriteConcern", x.newEffectiveWriteConcern),
      f.field("newAssumedWriteConcern", x.newAssumedWriteConcern));
}

struct UpdateWaitForSyncAction {
  static constexpr std::string_view name = "UpdateWaitForSyncAction";
  bool newWaitForSync;
  bool newAssumedWaitForSync;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modify<LogPlanSpecification>([&](LogPlanSpecification& plan) {
      plan.participantsConfig.config.waitForSync = newWaitForSync;
      plan.participantsConfig.generation += 1;
    });
    ctx.modify<LogCurrentSupervision>(
        [&](LogCurrentSupervision& currentSupervision) {
          currentSupervision.assumedWaitForSync = newAssumedWaitForSync;
        });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, UpdateWaitForSyncAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(
      f.field("type", hack), f.field("newWaitForSync", x.newWaitForSync),
      f.field("newAssumedWaitForSync", x.newAssumedWaitForSync));
}

struct SetAssumedWriteConcernAction {
  static constexpr std::string_view name = "SetAssumedWriteConcernAction";
  size_t newAssumedWriteConcern;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyOrCreate<LogCurrentSupervision>(
        [&](LogCurrentSupervision& currentSupervision) {
          currentSupervision.assumedWriteConcern = newAssumedWriteConcern;
        });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, SetAssumedWriteConcernAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(
      f.field("type", hack),
      f.field("newAssumedWriteConcern", x.newAssumedWriteConcern));
}

struct SetAssumedWaitForSyncAction {
  static constexpr std::string_view name = "SetAssumedWaitForSyncAction";
  bool newAssumedWaitForSync;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyOrCreate<LogCurrentSupervision>(
        [&](LogCurrentSupervision& currentSupervision) {
          currentSupervision.assumedWaitForSync = newAssumedWaitForSync;
        });
  }
};
template<typename Inspector>
auto inspect(Inspector& f, SetAssumedWaitForSyncAction& x) {
  auto hack = std::string{x.name};
  return f.object(x).fields(
      f.field("type", hack),
      f.field("newAssumedWriteConcern", x.newAssumedWaitForSync));
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

/* NOTE!
 *
 * EmptyAction *has* *to* *be* first, as the default constructor
 * of a variant constructs the variant with index 0
 *
 */
using Action =
    std::variant<EmptyAction, NoActionPossibleAction, AddLogToPlanAction,
                 SwitchLeaderAction, WriteEmptyTermAction, LeaderElectionAction,
                 UpdateParticipantFlagsAction, AddParticipantToPlanAction,
                 RemoveParticipantFromPlanAction, UpdateLogConfigAction,
                 UpdateEffectiveAndAssumedWriteConcernAction,
                 SetAssumedWriteConcernAction, UpdateWaitForSyncAction,
                 SetAssumedWaitForSyncAction, ConvergedToTargetAction>;

auto executeAction(Log log, Action& action) -> ActionContext;
}  // namespace arangodb::replication2::replicated_log

#include "Inspection/VPack.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"

template<>
struct fmt::formatter<arangodb::replication2::replicated_log::Action>
    : formatter<string_view> {
  // parse is inherited from formatter<string_view>.
  template<typename FormatContext>
  auto format(arangodb::replication2::replicated_log::Action a,
              FormatContext& ctx) const {
    VPackBuilder builder;

    std::visit(
        [&builder](auto&& arg) {
          arangodb::velocypack::serialize(builder, arg);
        },
        a);
    return formatter<string_view>::format(builder.slice().toJson(), ctx);
  }
};
