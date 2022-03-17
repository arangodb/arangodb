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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <memory>
#include <utility>

#include "Agency/TransactionBuilder.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "Replication2/ReplicatedState/StateCommon.h"

namespace arangodb::replication2::replicated_state {

struct ActionContext {
  using LogTarget = replication2::agency::LogTarget;
  using StatePlan = agency::Plan;

  ActionContext(std::optional<LogTarget> logTarget,
                std::optional<StatePlan> statePlan)
      : logTarget(std::move(logTarget)), statePlan(std::move(statePlan)) {}

  template<typename F>
  auto modifyLogTarget(F&& fn) {
    static_assert(std::is_invocable_r_v<void, F, LogTarget&>);
    TRI_ASSERT(logTarget.has_value())
        << "modifying action expects target to be present";
    modifiedTarget = true;
    return std::invoke(std::forward<F>(fn), *logTarget);
  }

  template<typename F>
  auto modifyStatePlan(F&& fn) {
    static_assert(std::is_invocable_r_v<void, F, StatePlan&>);
    TRI_ASSERT(statePlan.has_value())
        << "modifying action expects target to be present";
    modifiedPlan = true;
    return std::invoke(std::forward<F>(fn), *statePlan);
  }

  template<typename F>
  auto modifyBoth(F&& fn) {
    static_assert(std::is_invocable_r_v<void, F, StatePlan&, LogTarget&>);
    TRI_ASSERT(statePlan.has_value())
        << "modifying action expects state plan to be present";
    TRI_ASSERT(logTarget.has_value())
        << "modifying action expects target to be present";
    modifiedPlan = true;
    modifiedTarget = true;
    return std::invoke(std::forward<F>(fn), *statePlan, *logTarget);
  }

  void setLogTarget(LogTarget newTarget) {
    logTarget.emplace(std::move(newTarget));
    modifiedTarget = true;
  }

  void setStatePlan(StatePlan newPlan) {
    statePlan.emplace(std::move(newPlan));
    modifiedPlan = true;
  }

  auto hasModification() const noexcept -> bool {
    return modifiedTarget || modifiedPlan;
  }

  auto hasLogTargetModification() const noexcept -> bool {
    return modifiedTarget;
  }

  auto hasStatePlanModification() const noexcept -> bool {
    return modifiedPlan;
  }

  auto getLogTarget() const noexcept -> LogTarget const& {
    return logTarget.value();
  }

  auto getStatePlan() const noexcept -> StatePlan const& {
    return statePlan.value();
  }

 private:
  std::optional<LogTarget> logTarget;
  bool modifiedTarget = false;
  std::optional<StatePlan> statePlan;
  bool modifiedPlan = false;
};

struct EmptyAction {
  void execute(ActionContext&) {}
};

struct AddParticipantAction {
  ParticipantId participant;
  StateGeneration generation;

  void execute(ActionContext& ctx) {
    ctx.modifyBoth([&](agency::Plan& plan,
                       replication2::agency::LogTarget& logTarget) {
      logTarget.participants[participant] =
          ParticipantFlags{.allowedInQuorum = false, .allowedAsLeader = false};

      plan.participants[participant].generation = plan.generation;
      plan.generation.value += 1;
    });
  }
};

struct AddStateToPlanAction {
  replication2::agency::LogTarget logTarget;
  agency::Plan statePlan;

  void execute(ActionContext& ctx) {
    ctx.setLogTarget(std::move(logTarget));
    ctx.setStatePlan(std::move(statePlan));
  }
};

struct UpdateParticipantFlagsAction {
  ParticipantId participant;
  ParticipantFlags flags;

  void execute(ActionContext& ctx) {
    ctx.modifyLogTarget([&](replication2::agency::LogTarget& target) {
      target.participants.at(participant) = flags;
    });
  }
};

using Action = std::variant<EmptyAction, AddParticipantAction,
                            AddStateToPlanAction, UpdateParticipantFlagsAction>;

auto execute(LogId id, DatabaseID const& database, Action action,
             std::optional<agency::Plan> state,
             std::optional<replication2::agency::LogTarget> log,
             arangodb::agency::envelope envelope) -> arangodb::agency::envelope;

}  // namespace arangodb::replication2::replicated_state
