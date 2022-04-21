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
#include "Replication2/Supervision/ModifyContext.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "Replication2/ReplicatedState/StateCommon.h"

namespace arangodb::replication2::replicated_state {

using ActionContext = ModifyContext<replication2::agency::LogTarget,
                                    agency::Plan, agency::Current::Supervision>;

struct EmptyAction {
  void execute(ActionContext&) {}
};

struct AddParticipantAction {
  ParticipantId participant;

  void execute(ActionContext& ctx) {
    ctx.modify<agency::Plan, replication2::agency::LogTarget>(
        [&](auto& plan, auto& logTarget) {
          logTarget.participants[participant] = ParticipantFlags{
              .allowedInQuorum = false, .allowedAsLeader = false};

          plan.participants[participant].generation = plan.generation;
          plan.generation.value += 1;
        });
  }
};

struct RemoveParticipantFromLogTargetAction {
  ParticipantId participant;

  void execute(ActionContext& ctx) {
    ctx.modify<agency::Plan, replication2::agency::LogTarget>(
        [&](auto& plan, auto& logTarget) {
          logTarget.participants.erase(participant);
        });
  }
};

struct RemoveParticipantFromStatePlanAction {
  ParticipantId participant;

  void execute(ActionContext& ctx) {
    ctx.modify<agency::Plan, replication2::agency::LogTarget>(
        [&](auto& plan, auto& logTarget) {
          plan.participants.erase(participant);
        });
  }
};

struct AddStateToPlanAction {
  replication2::agency::LogTarget logTarget;
  agency::Plan statePlan;

  void execute(ActionContext& ctx) {
    ctx.setValue<agency::Plan>(std::move(statePlan));
    ctx.setValue<replication2::agency::LogTarget>(std::move(logTarget));
  }
};

struct UpdateParticipantFlagsAction {
  ParticipantId participant;
  ParticipantFlags flags;

  void execute(ActionContext& ctx) {
    ctx.modify<replication2::agency::LogTarget>(
        [&](auto& target) { target.participants.at(participant) = flags; });
  }
};

struct CurrentConvergedAction {
  std::uint64_t version;

  void execute(ActionContext& ctx) {
    ctx.modifyOrCreate<replicated_state::agency::Current::Supervision>(
        [&](auto& current) { current.version = version; });
  }
};

struct SetLeaderAction {
  std::optional<ParticipantId> leader;

  void execute(ActionContext& ctx) {
    ctx.modify<replication2::agency::LogTarget>(
        [&](auto& target) { target.leader = leader; });
  }
};

using Action = std::variant<
    EmptyAction, AddParticipantAction, RemoveParticipantFromLogTargetAction,
    RemoveParticipantFromStatePlanAction, AddStateToPlanAction,
    UpdateParticipantFlagsAction, CurrentConvergedAction, SetLeaderAction>;

auto executeAction(
    arangodb::replication2::replicated_state::agency::State state,
    std::optional<replication2::agency::Log> log, Action& action)
    -> ActionContext;

}  // namespace arangodb::replication2::replicated_state
