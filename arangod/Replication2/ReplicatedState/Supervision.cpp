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

#include "Supervision.h"

#include "Replication2/ReplicatedState/SupervisionAction.h"
#include <memory>

#include "Logger/LogMacros.h"

using namespace arangodb;

namespace arangodb::replication2::replicated_state {

auto checkStateAdded(replication2::replicated_state::agency::State const& state)
    -> Action {
  auto id = state.target.id;

  if (!state.plan) {
    auto statePlan = replication2::replicated_state::agency::Plan{
        .id = state.target.id,
        .generation = StateGeneration{1},
        .properties = state.target.properties,
        .participants = {}};

    auto logTarget =
        replication2::agency::LogTarget(id, {}, state.target.config);

    for (auto const& [participantId, _] : state.target.participants) {
      logTarget.participants.emplace(participantId, ParticipantFlags{});
      statePlan.participants.emplace(
          participantId,
          agency::Plan::Participant{.generation = StateGeneration{1}});
    }

    return AddStateToPlanAction{logTarget, statePlan};
  } else {
    return EmptyAction();
  }
}

auto checkLeaderSet(arangodb::replication2::agency::Log const& log,
                    replication2::replicated_state::agency::State const& state)
    -> Action {
  auto const& targetLeader = state.target.leader;

  auto const& planLeader = log.target.leader;

  if (targetLeader != planLeader) {
    return SetLeaderAction{targetLeader};
  }

  return EmptyAction();
}

auto checkParticipantAdded(
    arangodb::replication2::agency::Log const& log,
    replication2::replicated_state::agency::State const& state) -> Action {
  auto const& targetParticipants = state.target.participants;

  if (!state.plan) {
    return EmptyAction();
  }

  auto const& planParticipants = state.plan->participants;

  for (auto const& [participant, flags] : targetParticipants) {
    if (!planParticipants.contains(participant)) {
      return AddParticipantAction{
          participant, StateGeneration{state.plan->generation.value}};
    }
  }
  return EmptyAction();
}

auto checkTargetParticipantRemoved(
    arangodb::replication2::agency::Log const& log,
    replication2::replicated_state::agency::State const& state) -> Action {
  auto const& stateTargetParticipants = state.target.participants;

  if (!state.plan) {
    return EmptyAction();
  }

  auto const& logTargetParticipants = log.target.participants;

  for (auto const& [participant, flags] : logTargetParticipants) {
    if (!stateTargetParticipants.contains(participant)) {
      return RemoveParticipantFromLogTargetAction{participant};
    }
  }
  return EmptyAction();
}

auto checkLogParticipantRemoved(
    arangodb::replication2::agency::Log const& log,
    replication2::replicated_state::agency::State const& state) -> Action {
  if (!state.plan) {
    return EmptyAction();
  }

  auto const& stateTargetParticipants = state.target.participants;
  auto const& logTargetParticipants = log.target.participants;
  auto const& logPlanParticipants = log.plan->participantsConfig.participants;
  auto participantGone = [&](auto const& participant) {
    // Check both target and plan, so we don't drop too early (i.e. when the
    // target is already there, but the log plan hasn't been written yet).
    // Apart from that, as soon as the plan for the log is gone, we can safely
    // drop the state.
    return !stateTargetParticipants.contains(participant) &&
           !logTargetParticipants.contains(participant) &&
           !logPlanParticipants.contains(participant);
  };

  auto const& planParticipants = state.plan->participants;
  for (auto const& [participant, flags] : planParticipants) {
    if (participantGone(participant)) {
      return RemoveParticipantFromStatePlanAction{participant};
    }
  }
  return EmptyAction();
}

/* Check whether there is a participant that is excluded but reported snapshot
 * complete */
auto checkSnapshotComplete(
    arangodb::replication2::agency::Log const& log,
    replication2::replicated_state::agency::State const& state) -> Action {
  if (state.current and log.plan) {
    // TODO generation?
    for (auto const& [participant, flags] :
         log.plan->participantsConfig.participants) {
      if (!flags.allowedAsLeader || !flags.allowedInQuorum) {
        auto const& plannedGeneration =
            state.plan->participants.at(participant).generation;

        if (auto const& status = state.current->participants.find(participant);
            status != std::end(state.current->participants)) {
          auto const& participantStatus = status->second;

          if (participantStatus.snapshot.status ==
                  SnapshotStatus::kCompleted and
              participantStatus.generation == plannedGeneration) {
            auto newFlags = flags;
            newFlags.allowedInQuorum = true;
            newFlags.allowedAsLeader = true;
            return UpdateParticipantFlagsAction{participant, newFlags};
          }
        }
      }
    }
  }
  return EmptyAction();
}

auto hasConverged(replication2::replicated_state::agency::State const& state)
    -> bool {
  if (!state.plan) {
    return false;
  }
  if (!state.current) {
    return false;
  }

  for (auto const& [pid, flags] : state.target.participants) {
    if (!state.plan->participants.contains(pid)) {
      return false;
    }

    if (auto c = state.current->participants.find(pid);
        c == state.current->participants.end()) {
      return false;
    } else if (c->second.generation !=
               state.plan->participants.at(pid).generation) {
      return false;
    } else if (c->second.snapshot.status != SnapshotStatus::kCompleted) {
      return false;
    }
  }

  return true;
}

auto checkConverged(arangodb::replication2::agency::Log const& log,
                    replication2::replicated_state::agency::State const& state)
    -> Action {
  if (!state.target.version.has_value()) {
    return EmptyAction();
  }

  if (!state.current or !state.current->supervision) {
    return CurrentConvergedAction{0};
  }

  // check that we are actually waiting for this version
  if (state.current->supervision->version == state.target.version) {
    return EmptyAction{};
  }

  // now check if we actually have converged
  if (!hasConverged(state)) {
    return EmptyAction{};
  }
  return CurrentConvergedAction{*state.target.version};
}

auto isEmptyAction(Action const& action) {
  return std::holds_alternative<EmptyAction>(action);
}

auto checkReplicatedState(
    std::optional<arangodb::replication2::agency::Log> const& log,
    replication2::replicated_state::agency::State const& state) -> Action {
  if (auto action = checkStateAdded(state); !isEmptyAction(action)) {
    return action;
  }

  // TODO if the log isn't there yet we do nothing;
  // It will need to be observable in future that we are doing nothing because
  // we're waiting for the log to appear.
  if (!log) {
    return EmptyAction();
  }
  // TODO: check for healthy leader?
  // TODO: is there something to be checked here?

  if (auto action = checkLeaderSet(*log, state); !isEmptyAction(action)) {
    return action;
  }

  if (auto action = checkParticipantAdded(*log, state);
      !isEmptyAction(action)) {
    return action;
  }

  if (auto action = checkTargetParticipantRemoved(*log, state);
      !isEmptyAction(action)) {
    return action;
  }

  if (auto action = checkLogParticipantRemoved(*log, state);
      !isEmptyAction(action)) {
    return action;
  }

  if (auto action = checkSnapshotComplete(*log, state);
      !isEmptyAction(action)) {
    return action;
  }

  if (auto action = checkConverged(*log, state); !isEmptyAction(action)) {
    return action;
  }

  return EmptyAction();
}

}  // namespace arangodb::replication2::replicated_state
