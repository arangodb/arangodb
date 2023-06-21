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

#include "Replication2/Helper/ModelChecker/Actors.h"

#include <utility>
#include "Replication2/ModelChecker/ModelChecker.h"
#include "Replication2/ModelChecker/Predicates.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/Supervision.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"

#include "Replication2/Helper/ModelChecker/AgencyState.h"
#include "Replication2/Helper/ModelChecker/AgencyTransitions.h"
#include "Replication2/Helper/ModelChecker/HashValues.h"
#include "Replication2/Helper/ModelChecker/Predicates.h"

using namespace arangodb;
using namespace arangodb::test;
using namespace arangodb::replication2;
namespace RLA = arangodb::replication2::agency;

namespace arangodb::test {

auto SupervisionActor::step(AgencyState const& agency) const
    -> std::vector<AgencyTransition> {
  std::vector<AgencyTransition> v;
  if (auto action = stepReplicatedLog(agency); action) {
    v.emplace_back(std::move(*action));
  }
  return v;
}

auto SupervisionActor::stepReplicatedLog(AgencyState const& agency)
    -> std::optional<AgencyTransition> {
  if (!agency.replicatedLog.has_value()) {
    return std::nullopt;
  }
  replicated_log::SupervisionContext ctx;
  replicated_log::checkReplicatedLog(ctx, *agency.replicatedLog, agency.health);

  if (ctx.hasAction()) {
    auto action = ctx.getAction();
    if (!std::holds_alternative<replicated_log::NoActionPossibleAction>(
            action)) {
      return SupervisionLogAction{std::move(action)};
    }
  }
  return std::nullopt;
}

auto DBServerActor::step(AgencyState const& agency) const
    -> std::vector<AgencyTransition> {
  std::vector<AgencyTransition> v;
  if (auto action = stepReplicatedState(agency); action) {
    v.emplace_back(std::move(*action));
  }
  if (auto action = stepReplicatedLogReportTerm(agency); action) {
    v.emplace_back(std::move(*action));
  }
  if (auto action = stepReplicatedLogLeaderCommit(agency); action) {
    v.emplace_back(std::move(*action));
  }
  return v;
}

auto DBServerActor::stepReplicatedLogLeaderCommit(
    AgencyState const& agency) const -> std::optional<AgencyTransition> {
  if (!agency.replicatedLog || !agency.replicatedLog->plan) {
    return std::nullopt;
  }
  auto committedGeneration = std::invoke([&]() -> std::size_t {
    if (agency.replicatedLog->current) {
      auto const& current = *agency.replicatedLog->current;
      if (current.leader) {
        auto const& leader = *current.leader;
        if (leader.serverId == name) {
          if (leader.leadershipEstablished &&
              leader.committedParticipantsConfig) {
            return leader.committedParticipantsConfig->generation;
          }
        }
      }
    }
    return 0;
  });

  auto const& plan = *agency.replicatedLog->plan;
  auto const isCommitPossible = [&] {
    std::size_t numberOfAvailableServers = 0;

    for (auto [pid, flags] : plan.participantsConfig.participants) {
      if (not flags.allowedInQuorum) {
        continue;
      }
      if (not agency.health.notIsFailed(pid)) {
        continue;
      }
      numberOfAvailableServers += 1;
    }

    return numberOfAvailableServers >=
           plan.participantsConfig.config.effectiveWriteConcern;
  };

  if (plan.currentTerm) {
    auto const& term = *plan.currentTerm;
    if (term.leader && term.leader->serverId == name) {
      if (plan.participantsConfig.generation != committedGeneration) {
        bool const isPossible = isCommitPossible();
        if (isPossible) {
          return DBServerCommitConfigAction{
              name, plan.participantsConfig.generation, term.term};
        }
      }
    }
  }
  return std::nullopt;
}

auto DBServerActor::stepReplicatedLogReportTerm(AgencyState const& agency) const
    -> std::optional<AgencyTransition> {
  if (!agency.replicatedLog || !agency.replicatedLog->plan) {
    return std::nullopt;
  }
  auto const& plan = *agency.replicatedLog->plan;
  auto reportedTerm = std::invoke([&] {
    if (agency.replicatedLog->current) {
      auto const& current = *agency.replicatedLog->current;
      auto iter = current.localState.find(name);
      if (iter != current.localState.end()) {
        return iter->second.term;
      }
    }
    return LogTerm{0};
  });

  // check if we have to report a new term in current
  if (plan.currentTerm) {
    auto const& term = *plan.currentTerm;
    if (term.term != reportedTerm) {
      return DBServerReportTermAction{name, term.term};
    }
  }
  return std::nullopt;
}

auto DBServerActor::stepReplicatedState(AgencyState const& agency) const
    -> std::optional<AgencyTransition> {
  if (!agency.replicatedLog) {
    return std::nullopt;
  }
  if (auto& plan = agency.replicatedLog->plan; plan.has_value()) {
    if (auto iter = plan->participantsConfig.participants.find(name);
        iter != plan->participantsConfig.participants.end()) {
      if (auto current = agency.replicatedLog->current; current.has_value()) {
        if (auto iter2 = current->localState.find(name);
            iter2 != current->localState.end()) {
          auto& status = iter2->second;
          if (status.snapshotAvailable) {
            return std::nullopt;
          }
        }
      }

      return DBServerSnapshotCompleteAction{name};
    }
  }

  return std::nullopt;
}

DBServerActor::DBServerActor(ParticipantId name) : name(std::move(name)) {}

auto KillLeaderActor::step(AgencyState const& agency) const
    -> std::vector<AgencyTransition> {
  std::vector<AgencyTransition> v;

  if (agency.replicatedLog && agency.replicatedLog->plan) {
    if (agency.replicatedLog->plan->currentTerm) {
      auto const& term = *agency.replicatedLog->plan->currentTerm;
      if (term.term == LogTerm{1} && term.leader) {
        auto const& health = agency.health;
        auto const& leader = *term.leader;
        auto isHealth =
            health.validRebootId(leader.serverId, leader.rebootId) &&
            health.notIsFailed(leader.serverId);
        if (isHealth) {
          v.emplace_back(KillServerAction{leader.serverId});
        }
      }
    }
  }

  return v;
}

auto KillServerActor::step(AgencyState const& agency) const
    -> std::vector<AgencyTransition> {
  std::vector<AgencyTransition> v;
  auto const& health = agency.health;
  auto isHealth = health.notIsFailed(name);
  if (isHealth) {
    v.emplace_back(KillServerAction{name});
  }

  return v;
}

KillServerActor::KillServerActor(ParticipantId name) : name(std::move(name)) {}

auto KillAnyServerActor::expand(AgencyState const& s,
                                KillAnyServerActor::InternalState const& i)
    -> std::vector<std::tuple<AgencyTransition, AgencyState, InternalState>> {
  if (i.wasKilled) {
    return {};
  }
  std::vector<std::tuple<AgencyTransition, AgencyState, InternalState>> result;
  for (auto const& [pid, health] : s.health._health) {
    auto action = KillServerAction{pid};
    auto newState = s;
    action.apply(newState);
    result.emplace_back(std::move(action), std::move(newState),
                        InternalState{.wasKilled = true});
  }
  return result;
}

AddServerActor::AddServerActor(ParticipantId newServer)
    : newServer(std::move(newServer)) {}

auto AddServerActor::step(AgencyState const& agency) const
    -> std::vector<AgencyTransition> {
  if (!agency.replicatedLog) {
    return {};
  }

  auto const& target = agency.replicatedLog->target;
  TRI_ASSERT(!target.participants.contains(newServer));
  auto result = std::vector<AgencyTransition>{};

  result.emplace_back(AddLogParticipantAction{newServer});

  return result;
}

RemoveServerActor::RemoveServerActor(ParticipantId server)
    : server(std::move(server)) {}

auto RemoveServerActor::step(AgencyState const& agency) const
    -> std::vector<AgencyTransition> {
  if (!agency.replicatedLog) {
    return {};
  }

  auto const& target = agency.replicatedLog->target;
  TRI_ASSERT(target.participants.contains(server));
  auto result = std::vector<AgencyTransition>{};

  result.emplace_back(RemoveLogParticipantAction{server});

  return result;
}

ReplaceAnyServerActor::ReplaceAnyServerActor(ParticipantId newServer)
    : newServer(std::move(newServer)) {}

auto ReplaceAnyServerActor::step(AgencyState const& agency) const
    -> std::vector<AgencyTransition> {
  if (!agency.replicatedLog) {
    return {};
  }

  auto const& target = agency.replicatedLog->target;
  TRI_ASSERT(!target.participants.contains(newServer));
  auto result = std::vector<AgencyTransition>{};
  result.reserve(target.participants.size());
  for (auto const& [p, stat] : target.participants) {
    result.emplace_back(ReplaceServerTargetState{p, newServer});
  }

  return result;
}

ReplaceSpecificServerActor::ReplaceSpecificServerActor(ParticipantId oldServer,
                                                       ParticipantId newServer)
    : oldServer(std::move(oldServer)), newServer(std::move(newServer)) {}

auto ReplaceSpecificServerActor::step(AgencyState const& agency) const
    -> std::vector<AgencyTransition> {
  if (!agency.replicatedLog) {
    return {};
  }

  auto const& target = agency.replicatedLog->target;
  TRI_ASSERT(!target.participants.contains(newServer));
  TRI_ASSERT(target.participants.contains(oldServer));

  return {ReplaceServerTargetState{oldServer, newServer}};
}

ReplaceSpecificLogServerActor::ReplaceSpecificLogServerActor(
    ParticipantId oldServer, ParticipantId newServer)
    : oldServer(std::move(oldServer)), newServer(std::move(newServer)) {}

auto ReplaceSpecificLogServerActor::step(AgencyState const& agency) const
    -> std::vector<AgencyTransition> {
  if (!agency.replicatedLog) {
    return {};
  }

  auto const& target = agency.replicatedLog->target;
  TRI_ASSERT(!target.participants.contains(newServer));
  TRI_ASSERT(target.participants.contains(oldServer));

  return {ReplaceServerTargetLog{oldServer, newServer}};
}

SetLeaderActor::SetLeaderActor(ParticipantId leader)
    : newLeader(std::move(leader)) {}

auto SetLeaderActor::step(AgencyState const& agency) const
    -> std::vector<AgencyTransition> {
  return {SetLeaderInTargetAction(newLeader)};
}

SetWriteConcernActor::SetWriteConcernActor(size_t newWriteConcern)
    : newWriteConcern(newWriteConcern) {}

auto SetWriteConcernActor::step(AgencyState const& agency) const
    -> std::vector<AgencyTransition> {
  return {SetWriteConcernAction(newWriteConcern)};
}

SetSoftWriteConcernActor::SetSoftWriteConcernActor(size_t newSoftWriteConcern)
    : newSoftWriteConcern(newSoftWriteConcern) {}

auto SetSoftWriteConcernActor::step(AgencyState const& agency) const
    -> std::vector<AgencyTransition> {
  return {SetSoftWriteConcernAction(newSoftWriteConcern)};
}

SetBothWriteConcernActor::SetBothWriteConcernActor(size_t newWriteConcern,
                                                   size_t newSoftWriteConcern)
    : newWriteConcern(newWriteConcern),
      newSoftWriteConcern(newSoftWriteConcern) {}

auto SetBothWriteConcernActor::step(AgencyState const& agency) const
    -> std::vector<AgencyTransition> {
  return {SetBothWriteConcernAction(newWriteConcern, newSoftWriteConcern)};
}

auto ModifySoftWCMultipleStepsActor::expand(AgencyState const& s,
                                            InternalState const& i)
    -> std::vector<std::tuple<AgencyTransition, AgencyState, InternalState>> {
  if (!s.replicatedLog) {
    return {};
  }
  if (i.state == State::RESET) {
    return {};
  }

  std::vector<AgencyTransition> actions;
  State nextState{State::INIT};
  if (i.state == State::INIT) {
    actions = {SetSoftWriteConcernAction(setInvalidWC)};
    nextState = State::SET_TO_INVALID;
  } else if (i.state == State::SET_TO_INVALID) {
    actions = {SetSoftWriteConcernAction(resetValidWC)};
    nextState = State::RESET;
  }

  auto result =
      std::vector<std::tuple<AgencyTransition, AgencyState, InternalState>>{};

  for (auto& action : actions) {
    auto newState = s;
    std::visit([&](auto& action) { action.apply(newState); }, action);
    result.emplace_back(std::move(action), std::move(newState),
                        InternalState{.state = nextState});
  }

  return result;
}
ModifySoftWCMultipleStepsActor::ModifySoftWCMultipleStepsActor(
    size_t setInvalidWc, size_t resetValidWc)
    : setInvalidWC(setInvalidWc), resetValidWC(resetValidWc) {}
}  // namespace arangodb::test

SetWaitForSyncActor::SetWaitForSyncActor(bool newWaitForSync)
    : newWaitForSync(newWaitForSync) {}

auto SetWaitForSyncActor::step(AgencyState const& agency) const
    -> std::vector<AgencyTransition> {
  return {SetWaitForSyncAction(newWaitForSync)};
}
