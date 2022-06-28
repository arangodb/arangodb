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

#include <fmt/ostream.h>
#include <fmt/core.h>

#include "Replication2/Helper/AgencyLogBuilder.h"
#include "Replication2/Helper/AgencyStateBuilder.h"
#include "Replication2/ModelChecker/ActorModel.h"
#include "Replication2/ModelChecker/ModelChecker.h"
#include "Replication2/ModelChecker/Predicates.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/Supervision.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "Replication2/ReplicatedState/Supervision.h"

#include "Replication2/Helper/ModelChecker/AgencyState.h"
#include "Replication2/Helper/ModelChecker/AgencyTransitions.h"
#include "Replication2/Helper/ModelChecker/HashValues.h"
#include "Replication2/Helper/ModelChecker/Predicates.h"

#include "Logger/LogMacros.h"
#include <boost/core/demangle.hpp>

using namespace arangodb;
using namespace arangodb::test;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
namespace RLA = arangodb::replication2::agency;
namespace RSA = arangodb::replication2::replicated_state::agency;

namespace arangodb::test {

auto SupervisionStateAction::toString() const -> std::string {
  return std::string{"Supervision "} +
         std::visit(
             [&](auto const& x) {
               return boost::core::demangle(typeid(x).name());
             },
             _action);
}

SupervisionStateAction::SupervisionStateAction(replicated_state::Action action)
    : _action(std::move(action)) {}

void SupervisionStateAction::apply(AgencyState& agency) {
  auto actionCtx =
      executeAction(*agency.replicatedState, agency.replicatedLog, _action);
  if (actionCtx.hasModificationFor<RLA::LogTarget>()) {
    if (!agency.replicatedLog) {
      agency.replicatedLog.emplace();
    }
    agency.replicatedLog->target = actionCtx.getValue<RLA::LogTarget>();
  }
  if (actionCtx.hasModificationFor<RSA::Plan>()) {
    agency.replicatedState->plan = actionCtx.getValue<RSA::Plan>();
  }
  if (actionCtx.hasModificationFor<RSA::Current::Supervision>()) {
    if (!agency.replicatedState->current) {
      agency.replicatedState->current.emplace();
    }
    agency.replicatedState->current->supervision =
        actionCtx.getValue<RSA::Current::Supervision>();
  }
}

auto KillServerAction::toString() const -> std::string {
  return std::string{"kill "} + id;
}

KillServerAction::KillServerAction(ParticipantId id) : id(std::move(id)) {}

void KillServerAction::apply(AgencyState& agency) {
  agency.health._health.at(id).notIsFailed = false;
}

auto SupervisionLogAction::toString() const -> std::string {
  return std::string{"Supervision "} +
         std::visit(
             [&](auto const& x) {
               VPackBuilder builder;
               velocypack::serialize(builder, x);
               return boost::core::demangle(typeid(x).name()) + " " +
                      builder.toJson();
             },
             _action);
}

SupervisionLogAction::SupervisionLogAction(replicated_log::Action action)
    : _action(std::move(action)) {}

void SupervisionLogAction::apply(AgencyState& agency) {
  if (!agency.replicatedLog.has_value()) {
    // TODO: What now?
    TRI_ASSERT(false);
  }
  auto ctx = replicated_log::executeAction(*agency.replicatedLog, _action);
  if (ctx.hasModificationFor<LogCurrentSupervision>()) {
    if (!agency.replicatedLog->current.has_value()) {
      agency.replicatedLog->current.emplace(LogCurrent{});
    }
    agency.replicatedLog->current->supervision =
        ctx.getValue<LogCurrentSupervision>();
  }
  if (ctx.hasModificationFor<LogPlanSpecification>()) {
    agency.replicatedLog->plan = ctx.getValue<LogPlanSpecification>();
  }
}

auto DBServerSnapshotCompleteAction::toString() const -> std::string {
  return std::string{"Snapshot Complete for "} + name + "@" +
         to_string(generation);
}

void DBServerSnapshotCompleteAction::apply(AgencyState& agency) {
  if (!agency.replicatedState->current) {
    agency.replicatedState->current.emplace();
  }
  auto& status = agency.replicatedState->current->participants[name];
  status.generation = generation;
  status.snapshot.status = SnapshotStatus::kCompleted;
}

DBServerSnapshotCompleteAction::DBServerSnapshotCompleteAction(
    ParticipantId name, StateGeneration generation)
    : name(std::move(name)), generation(generation) {}

auto DBServerReportTermAction::toString() const -> std::string {
  return std::string{"Report Term for "} + name + ", term" + to_string(term);
}

void DBServerReportTermAction::apply(AgencyState& agency) const {
  if (!agency.replicatedLog->current) {
    agency.replicatedLog->current.emplace();
  }
  auto& status = agency.replicatedLog->current->localState[name];
  status.term = term;
}

DBServerReportTermAction::DBServerReportTermAction(ParticipantId name,
                                                   LogTerm term)
    : name(std::move(name)), term(term) {}

auto DBServerCommitConfigAction::toString() const -> std::string {
  return std::string{"Commit for"} + name + ", generation " +
         std::to_string(generation) + ", term " + to_string(term);
}

DBServerCommitConfigAction::DBServerCommitConfigAction(ParticipantId name,
                                                       std::size_t generation,
                                                       LogTerm term)
    : name(std::move(name)), generation(generation), term(term) {}

void DBServerCommitConfigAction::apply(AgencyState& agency) const {
  if (!agency.replicatedLog->current) {
    agency.replicatedLog->current.emplace();
  }
  if (!agency.replicatedLog->current->leader) {
    agency.replicatedLog->current->leader.emplace();
  }

  auto& leader = *agency.replicatedLog->current->leader;
  leader.leadershipEstablished = true;
  leader.serverId = name;
  leader.term = term;
  leader.committedParticipantsConfig =
      agency.replicatedLog->plan->participantsConfig;
  leader.committedParticipantsConfig->generation = generation;

  agency.logLeaderWriteConcern = agency.replicatedLog->plan->participantsConfig
                                     .config.effectiveWriteConcern;
}

auto operator<<(std::ostream& os, AgencyTransition const& a) -> std::ostream& {
  return os << std::visit([](auto const& action) { return action.toString(); },
                          a);
}

ReplaceServerTargetState::ReplaceServerTargetState(ParticipantId oldServer,
                                                   ParticipantId newServer)
    : oldServer(std::move(oldServer)), newServer(std::move(newServer)) {}

auto ReplaceServerTargetState::toString() const -> std::string {
  return fmt::format("replacing {} with {}", oldServer, newServer);
}

void ReplaceServerTargetState::apply(AgencyState& agency) const {
  TRI_ASSERT(agency.replicatedState.has_value());
  auto& target = agency.replicatedState->target;
  target.participants.erase(oldServer);
  target.participants[newServer];
  target.version.emplace(target.version.value_or(0) + 1);
}

ReplaceServerTargetLog::ReplaceServerTargetLog(ParticipantId oldServer,
                                               ParticipantId newServer)
    : oldServer(std::move(oldServer)), newServer(std::move(newServer)) {}

auto ReplaceServerTargetLog::toString() const -> std::string {
  return fmt::format("replacing {} with {}", oldServer, newServer);
}

void ReplaceServerTargetLog::apply(AgencyState& agency) const {
  TRI_ASSERT(agency.replicatedLog.has_value());
  auto& target = agency.replicatedLog->target;
  target.participants.erase(oldServer);
  target.participants[newServer];
  target.version.emplace(target.version.value_or(0) + 1);
}

auto SetLeaderInTargetAction::toString() const -> std::string {
  return fmt::format("setting `{}` as leader in target", newLeader);
}

SetLeaderInTargetAction::SetLeaderInTargetAction(ParticipantId newLeader)
    : newLeader(newLeader) {}
void SetLeaderInTargetAction::apply(AgencyState& agency) const {
  TRI_ASSERT(agency.replicatedLog.has_value());
  auto& target = agency.replicatedLog->target;
  target.leader.emplace(newLeader);
}

AddLogParticipantAction::AddLogParticipantAction(
    replication2::ParticipantId server)
    : server(std::move(server)) {}

void AddLogParticipantAction::apply(AgencyState& agency) const {
  TRI_ASSERT(agency.replicatedLog.has_value());
  auto& target = agency.replicatedLog->target;
  target.participants[server];
  target.version.emplace(target.version.value_or(0) + 1);
}

auto AddLogParticipantAction::toString() const -> std::string {
  return fmt::format("adding participant {}", server);
}

RemoveLogParticipantAction::RemoveLogParticipantAction(
    replication2::ParticipantId server)
    : server(std::move(server)) {}

void RemoveLogParticipantAction::apply(AgencyState& agency) const {
  TRI_ASSERT(agency.replicatedLog.has_value());
  auto& target = agency.replicatedLog->target;
  target.participants.erase(server);
  target.version.emplace(target.version.value_or(0) + 1);
}

auto RemoveLogParticipantAction::toString() const -> std::string {
  return fmt::format("removing participant {}", server);
}

SetWriteConcernAction::SetWriteConcernAction(size_t newWriteConcern)
    : newWriteConcern(newWriteConcern) {}

void SetWriteConcernAction::apply(AgencyState& agency) const {
  TRI_ASSERT(agency.replicatedLog.has_value());
  auto& target = agency.replicatedLog->target;
  target.config.writeConcern = newWriteConcern;
  target.version.emplace(target.version.value_or(0) + 1);
}

auto SetWriteConcernAction::toString() const -> std::string {
  return fmt::format("setting writeConcern to {}", newWriteConcern);
}

SetSoftWriteConcernAction::SetSoftWriteConcernAction(size_t newSoftWriteConcern)
    : newSoftWriteConcern(newSoftWriteConcern) {}

void SetSoftWriteConcernAction::apply(AgencyState& agency) const {
  TRI_ASSERT(agency.replicatedLog.has_value());
  auto& target = agency.replicatedLog->target;
  target.config.softWriteConcern = newSoftWriteConcern;
  target.version.emplace(target.version.value_or(0) + 1);
}

auto SetSoftWriteConcernAction::toString() const -> std::string {
  return fmt::format("setting softWriteConcern to {}", newSoftWriteConcern);
}

SetBothWriteConcernAction::SetBothWriteConcernAction(size_t newWriteConcern,
                                                     size_t newSoftWriteConcern)
    : newWriteConcern(newWriteConcern),
      newSoftWriteConcern(newSoftWriteConcern) {}

void SetBothWriteConcernAction::apply(AgencyState& agency) const {
  TRI_ASSERT(agency.replicatedLog.has_value());
  auto& target = agency.replicatedLog->target;
  target.config.writeConcern = newWriteConcern;
  target.config.softWriteConcern = newSoftWriteConcern;
  target.version.emplace(target.version.value_or(0) + 1);
}

auto SetBothWriteConcernAction::toString() const -> std::string {
  return fmt::format("setting writeConcern to {} and softWriteConcern to {}",
                     newWriteConcern, newSoftWriteConcern);
}

}  // namespace arangodb::test
