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

#include "Agency/TransactionBuilder.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

using namespace arangodb::replication2::agency;

namespace arangodb::replication2::replicated_log {

struct ActionContext {
  ActionContext(std::optional<LogPlanSpecification> plan,
                std::optional<LogCurrent> current)
      : plan(std::move(plan)), current(std::move(current)) {}

  template<typename F>
  auto modifyPlan(F&& fn) {
    static_assert(std::is_invocable_r_v<void, F, LogPlanSpecification&>);
    TRI_ASSERT(plan.has_value())
        << "modifying action expects plan to be present";
    modifiedPlan = true;
    return std::invoke(std::forward<F>(fn), *plan);
  }

  template<typename F>
  auto modifyCurrent(F&& fn) {
    static_assert(std::is_invocable_r_v<void, F, LogCurrent&>);
    TRI_ASSERT(current.has_value())
        << "modifying action expects current to be present";
    modifiedCurrent = true;
    return std::invoke(std::forward<F>(fn), *current);
  }

  template<typename F>
  auto modifyBoth(F&& fn) {
    static_assert(
        std::is_invocable_r_v<void, F, LogPlanSpecification&, LogCurrent&>);
    TRI_ASSERT(plan.has_value())
        << "modifying action expects log plan to be present";
    TRI_ASSERT(current.has_value())
        << "modifying action expects current to be present";
    modifiedPlan = true;
    modifiedCurrent = true;
    return std::invoke(std::forward<F>(fn), *plan, *current);
  }

  void setPlan(LogPlanSpecification newPlan) {
    plan.emplace(std::move(newPlan));
    modifiedPlan = true;
  }

  void setCurrent(LogCurrent newCurrent) {
    current.emplace(std::move(newCurrent));
    modifiedCurrent = true;
  }

  auto hasModification() const noexcept -> bool {
    return modifiedPlan || modifiedCurrent;
  }

  auto hasPlanModification() const noexcept -> bool { return modifiedPlan; }

  auto hasCurrentModification() const noexcept -> bool {
    return modifiedCurrent;
  }

  auto getPlan() const noexcept -> LogPlanSpecification const& {
    return plan.value();
  }

  auto getCurrent() const noexcept -> LogCurrent const& {
    return current.value();
  }

 private:
  std::optional<LogPlanSpecification> plan;
  bool modifiedPlan = false;
  std::optional<LogCurrent> current;
  bool modifiedCurrent = false;
};

struct EmptyAction {
  static constexpr std::string_view name = "EmptyAction";

  EmptyAction() : _message(""){};
  explicit EmptyAction(std::string message) : _message(std::move(message)) {}
  std::string _message;

  auto updateCurrent(LogCurrent current) -> LogCurrent {
    if (!current.supervision) {
      current.supervision = LogCurrentSupervision{};
    }

    if (!current.supervision->statusMessage or
        current.supervision->statusMessage != _message) {
      current.supervision->statusMessage = _message;
    }

    return current;
  }

  auto execute(ActionContext& ctx) const -> void {}
};

struct ErrorAction {
  static constexpr std::string_view name = "ErrorAction";

  ErrorAction(LogCurrentSupervisionError const& error) : _error{error} {};

  LogCurrentSupervisionError _error;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyCurrent([&](LogCurrent& current) {
      if (!current.supervision) {
        current.supervision = LogCurrentSupervision{};
      }

      if (!current.supervision->error || current.supervision->error != _error) {
        current.supervision->error = _error;
      }
    });
  }
};

struct AddLogToPlanAction {
  static constexpr std::string_view name = "AddLogToPlanAction";

  AddLogToPlanAction(LogId const id, ParticipantsFlagsMap const& participants)
      : _id(id), _participants(participants){};
  LogId const _id;
  ParticipantsFlagsMap const _participants;

  auto execute(ActionContext& ctx) const -> void {
    ctx.setPlan(LogPlanSpecification(
        _id, std::nullopt,
        ParticipantsConfig{.generation = 1, .participants = _participants}));
  }
};

struct CreateInitialTermAction {
  static constexpr std::string_view name = "CreateIntialTermAction";

  LogConfig const _config;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyPlan([&](LogPlanSpecification& plan) {
      // Precondition: currentTerm is std::nullopt
      plan.currentTerm =
          LogPlanTermSpecification(LogTerm{1}, _config, std::nullopt);
    });
  }
};

struct CurrentNotAvailableAction {
  static constexpr std::string_view name = "CurrentNotAvailableAction";

  auto execute(ActionContext& ctx) const -> void {
    auto current = LogCurrent{};
    current.supervision = LogCurrentSupervision{};
    current.supervision->statusMessage =
        "Current was not available yet";  // It is now.

    ctx.setCurrent(current);
  }
};

struct DictateLeaderAction {
  static constexpr std::string_view name = "DictateLeaderAction";

  DictateLeaderAction(LogPlanTermSpecification::Leader const& leader)
      : _leader{leader} {};

  LogPlanTermSpecification::Leader _leader;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyPlan([&](LogPlanSpecification& plan) {
      plan.currentTerm->term = LogTerm{plan.currentTerm->term.value + 1};
      plan.currentTerm->leader = _leader;
    });
  }
};

struct DictateLeaderFailedAction {
  static constexpr std::string_view name = "DictateLeaderFailedAction";

  DictateLeaderFailedAction(std::string const& message) : _message{message} {};

  std::string _message;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyCurrent([&](LogCurrent& current) {
      if (!current.supervision) {
        current.supervision = LogCurrentSupervision{};
      }

      current.supervision->statusMessage = _message;
    });
  }
};

struct EvictLeaderAction {
  static constexpr std::string_view name = "EvictLeaderAction";

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyPlan([&](LogPlanSpecification& plan) {
      plan.participantsConfig.participants
          .at(plan.currentTerm->leader->serverId)
          .allowedAsLeader = false;
      plan.participantsConfig.generation += 1;
      plan.currentTerm->term = LogTerm{plan.currentTerm->term.value + 1};
      plan.currentTerm->leader.reset();
    });
  }
};

struct WriteEmptyTermAction {
  static constexpr std::string_view name = "WriteEmptyTermAction";

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyPlan([&](LogPlanSpecification& plan) {
      plan.currentTerm->term = LogTerm{plan.currentTerm->term.value + 1};
      plan.currentTerm->leader.reset();
    });
  }
};

struct LeaderElectionImpossibleAction {
  static constexpr std::string_view name = "LeaderElectionImpossibleAction";

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyCurrent([&](LogCurrent& current) {
      if (!current.supervision) {
        current.supervision = LogCurrentSupervision{};
      }
      current.supervision->statusMessage = "Leader election impossible";
    });
  }
};

struct LeaderElectionOutOfBoundsAction {
  static constexpr std::string_view name = "LeaderElectionOutOfBoundsAction";

  LogCurrentSupervisionElection _election;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyCurrent([&](LogCurrent& current) {
      if (!current.supervision) {
        current.supervision = LogCurrentSupervision{};
      }

      current.supervision->statusMessage =
          "Number of electible participants out of bounds";
      current.supervision->election = _election;
    });
  }
};

struct LeaderElectionQuorumNotReachedAction {
  static constexpr std::string_view name =
      "LeaderElectionQuorumNotReachedAction";

  LogCurrentSupervisionElection _election;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyCurrent([&](LogCurrent& current) {
      if (!current.supervision) {
        current.supervision = LogCurrentSupervision{};
      }

      current.supervision->statusMessage = "Quorum not reached";
      current.supervision->election = _election;
    });
  }
};

struct LeaderElectionAction {
  static constexpr std::string_view name = "LeaderElectionAction";

  LeaderElectionAction(LogPlanTermSpecification::Leader electedLeader,
                       LogCurrentSupervisionElection const& electionReport)
      : _electedLeader{electedLeader}, _electionReport(electionReport){};

  LogPlanTermSpecification::Leader _electedLeader;
  LogCurrentSupervisionElection _electionReport;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyPlan([&](LogPlanSpecification& plan) {
      plan.currentTerm->term = LogTerm{plan.currentTerm->term.value + 1};
      plan.currentTerm->leader = _electedLeader;
    });
    ctx.modifyCurrent([&](LogCurrent& current) {
      if (!current.supervision) {
        current.supervision = LogCurrentSupervision{};
      }
      current.supervision->election = _electionReport;
    });
  }
};

struct UpdateParticipantFlagsAction {
  static constexpr std::string_view name = "UpdateParticipantFlagsAction";

  UpdateParticipantFlagsAction(ParticipantId const& participant,
                               ParticipantFlags const& flags)
      : _participant(participant), _flags(flags){};

  ParticipantId _participant;
  ParticipantFlags _flags;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyPlan([&](LogPlanSpecification& plan) {
      plan.participantsConfig.participants.at(_participant) = _flags;
      plan.participantsConfig.generation += 1;
    });
  }
};

struct AddParticipantToPlanAction {
  static constexpr std::string_view name = "AddParticipantToPlanAction";

  AddParticipantToPlanAction(ParticipantId const& participant,
                             ParticipantFlags const& flags)
      : _participant(participant), _flags(flags) {}

  ParticipantId _participant;
  ParticipantFlags _flags;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyPlan([&](LogPlanSpecification& plan) {
      plan.participantsConfig.generation += 1;
      plan.participantsConfig.participants.emplace(_participant, _flags);
    });
  }
};

struct RemoveParticipantFromPlanAction {
  static constexpr std::string_view name = "RemoveParticipantFromPlanAction";

  RemoveParticipantFromPlanAction(ParticipantId const& participant)
      : _participant(participant){};

  ParticipantId _participant;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyPlan([&](LogPlanSpecification& plan) {
      plan.participantsConfig.participants.erase(_participant);
      plan.participantsConfig.generation += 1;
    });
  }
};

struct UpdateLogConfigAction {
  static constexpr std::string_view name = "UpdateLogConfigAction";

  UpdateLogConfigAction(LogConfig const& config) : _config(config){};

  LogConfig _config;

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyCurrent([&](LogCurrent& current) {
      if (!current.supervision) {
        current.supervision = LogCurrentSupervision{};
      }

      current.supervision->statusMessage =
          "UpdatingLogConfig is not implemented yet";
    });
  }
};

struct ConvergedToTargetAction {
  static constexpr std::string_view name = "ConvergedToTargetAction";

  auto execute(ActionContext& ctx) const -> void {
    ctx.modifyCurrent([&](LogCurrent& current) {
      if (!current.supervision) {
        current.supervision = LogCurrentSupervision{};
      }

      current.supervision->statusMessage = "Converged to target";
    });
  }
};

using Action = std::variant<
    EmptyAction, ErrorAction, AddLogToPlanAction, CreateInitialTermAction,
    CurrentNotAvailableAction, DictateLeaderAction, DictateLeaderFailedAction,
    EvictLeaderAction, WriteEmptyTermAction, LeaderElectionAction,
    LeaderElectionImpossibleAction, LeaderElectionOutOfBoundsAction,
    LeaderElectionQuorumNotReachedAction, UpdateParticipantFlagsAction,
    AddParticipantToPlanAction, RemoveParticipantFromPlanAction,
    UpdateLogConfigAction, ConvergedToTargetAction>;

struct VelocyPacker {
  VelocyPacker(VPackBuilder& builder) : builder(builder), ob(&builder){};
  VelocyPacker(VelocyPacker&) = delete;
  VPackBuilder& builder;
  VPackObjectBuilder ob;

  void operator()(EmptyAction const& action);
  void operator()(ErrorAction const& action);
  void operator()(AddLogToPlanAction const& action);
  void operator()(CreateInitialTermAction const& action);
  void operator()(CurrentNotAvailableAction const& action);
  void operator()(DictateLeaderAction const& action);
  void operator()(DictateLeaderFailedAction const& action);
  void operator()(EvictLeaderAction const& action);
  void operator()(WriteEmptyTermAction const& action);
  void operator()(LeaderElectionImpossibleAction const& action);
  void operator()(LeaderElectionOutOfBoundsAction const& action);
  void operator()(LeaderElectionQuorumNotReachedAction const& action);
  void operator()(LeaderElectionAction const& action);
  void operator()(UpdateParticipantFlagsAction const& action);
  void operator()(AddParticipantToPlanAction const& action);
  void operator()(RemoveParticipantFromPlanAction const& action);
  void operator()(UpdateLogConfigAction const& action);
  void operator()(ConvergedToTargetAction const& action);
};

auto execute(Action const& action, DatabaseID const& dbName, LogId const& log,
             std::optional<LogPlanSpecification> plan,
             std::optional<LogCurrent> current,
             arangodb::agency::envelope envelope) -> arangodb::agency::envelope;

auto to_string(Action const& action) -> std::string_view;
void toVelocyPack(Action const& action, VPackBuilder& builder);

}  // namespace arangodb::replication2::replicated_log
