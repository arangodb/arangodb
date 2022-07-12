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
#pragma once
#include "Replication2/Helper/ModelChecker/AgencyState.h"
#include "Replication2/Helper/ModelChecker/HashValues.h"
#include "Replication2/ModelChecker/ModelChecker.h"
#include "Replication2/ModelChecker/ActorModel.h"
#include "Replication2/ModelChecker/Predicates.h"

namespace arangodb::test::mcpreds {
static inline auto isLeaderHealth() {
  return MC_BOOL_PRED(global, {
    AgencyState const& state = global.state;
    if (state.replicatedLog && state.replicatedLog->plan &&
        state.replicatedLog->plan->currentTerm) {
      auto const& term = *state.replicatedLog->plan->currentTerm;
      if (term.leader) {
        auto const& leader = *term.leader;
        auto const& health = global.state.health;
        return health.validRebootId(leader.serverId, leader.rebootId) &&
               health.notIsFailed(leader.serverId);
      }
    }
    return false;
  });
}

static inline auto isParticipantPlanned(
    replication2::ParticipantId participant) {
  return MC_BOOL_PRED(global, {
    AgencyState const& state = global.state;
    if (state.replicatedLog && state.replicatedLog->plan) {
      return state.replicatedLog->plan->participantsConfig.participants
          .contains(participant);
    }
    return false;
  });
}

static inline auto isParticipantNotPlanned(
    replication2::ParticipantId participant) {
  return MC_BOOL_PRED(global, {
    AgencyState const& state = global.state;
    if (state.replicatedLog && state.replicatedLog->plan) {
      return !state.replicatedLog->plan->participantsConfig.participants
                  .contains(participant);
    }
    return true;
  });
}

static inline auto isParticipantCurrent(
    replication2::ParticipantId participant) {
  return MC_BOOL_PRED(global, {
    AgencyState const& state = global.state;
    if (state.replicatedLog && state.replicatedLog->current &&
        state.replicatedLog->current->leader &&
        state.replicatedLog->current->leader->committedParticipantsConfig) {
      return state.replicatedLog->current->leader->committedParticipantsConfig
          ->participants.contains(participant);
    }
    return false;
  });
}

static inline auto serverIsLeader(std::string_view id) {
  return MC_BOOL_PRED(global, {
    AgencyState const& state = global.state;
    if (state.replicatedLog && state.replicatedLog->plan &&
        state.replicatedLog->plan->currentTerm) {
      auto const& term = *state.replicatedLog->plan->currentTerm;
      if (term.leader) {
        auto const& leader = *term.leader;
        return leader.serverId == id;
      }
    }
    return false;
  });
}

static inline auto nonExcludedServerHasSnapshot() {
  return MC_BOOL_PRED2([](auto const& global) {
    auto const& agency = global.state;
    if (!agency.replicatedLog || !agency.replicatedState) {
      return true;
    }
    auto const& log = *agency.replicatedLog;
    auto const& state = *agency.replicatedState;
    if (!log.plan) {
      return true;
    }

    for (auto const& [pid, flags] : log.plan->participantsConfig.participants) {
      if (flags.allowedAsLeader || flags.allowedInQuorum) {
        // check is the snapshot is available
        if (!state.plan) {
          return false;
        }

        auto planIter = state.plan->participants.find(pid);
        if (planIter == state.plan->participants.end()) {
          return false;
        }

        auto wantedGeneration = planIter->second.generation;
        if (wantedGeneration ==
            replication2::replicated_state::StateGeneration{1}) {
          continue;
        }

        if (!state.current) {
          return false;
        }

        if (auto iter = state.current->participants.find(pid);
            iter == state.current->participants.end()) {
          return false;
        } else if (iter->second.generation != wantedGeneration ||
                   iter->second.snapshot.status !=
                       replication2::replicated_state::SnapshotStatus::
                           kCompleted) {
          std::cout << wantedGeneration << " " << iter->second.generation;
          return false;
        }
      }
    }

    return true;
  });
}

static inline auto
isAssumedWriteConcernLessThanOrEqualToEffectiveWriteConcern() {
  return MC_BOOL_PRED(global, {
    AgencyState const& state = global.state;

    // If the log has not been planned yet, we don't want to
    // break off.
    if (state.replicatedLog and not state.replicatedLog->plan.has_value()) {
      return true;
    }

    if (state.replicatedLog and state.replicatedLog->plan and
        state.replicatedLog->current and
        state.replicatedLog->current->supervision) {
      auto const& planConfig =
          state.replicatedLog->plan->participantsConfig.config;
      auto const& currentSupervision =
          *state.replicatedLog->current->supervision;

      return currentSupervision.assumedWriteConcern <=
             planConfig.effectiveWriteConcern;
    }
    return false;
  });
}

static inline auto isAssumedWriteConcernLessThanWriteConcernUsedForCommit() {
  return MC_BOOL_PRED(global, {
    AgencyState const& state = global.state;

    if (not state.logLeaderWriteConcern) {
      return true;
    }

    if (state.replicatedLog and state.replicatedLog->plan and
        state.replicatedLog->current and
        state.replicatedLog->current->supervision) {
      return state.replicatedLog->current->supervision->assumedWriteConcern <=
             *state.logLeaderWriteConcern;
    }
    return false;
  });
}

static inline auto isPlannedWriteConcern(bool concern) {
  return MC_BOOL_PRED(global, {
    AgencyState const& state = global.state;
    if (state.replicatedLog && state.replicatedLog->plan) {
      return state.replicatedLog->plan->participantsConfig.config.waitForSync ==
             concern;
    }
    return false;
  });
}

}  // namespace arangodb::test::mcpreds
