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

/*
 * This is flow graph of the replicated state supervision. Operations that are
 * on the same level are allowed to be executed in parallel. The first entry in
 * a chain, that produces an action terminates the rest of the chain. Actions
 * of a lower level are only executed if their parent is ok.
 *
 * 1. ReplicatedLog/Target and ReplicatedState/Plan exists
 *  -> AddReplicatedLogAction
 *    1.1. Forward config and target leader to the replicated log
 *      -> UpdateLeaderAction
 *      -> UpdateConfigAction
 *    1.2. Check Participant Snapshot completion
 *      -> UpdateTargetParticipantFlagsAction
 *    1.3. Check if a participant is in State/Target but not in State/Plan
 *      -> AddParticipantAction
 *        1.3.1. Check if the participant is State/Plan but not in Log/Target
 *          -> AddLogParticipantAction
 *    1.4. Check if participants can be removed from Log/Target
 *    1.5. Check if participants can be dropped from State/Plan
 * 2. check if the log as converged
 *  -> ConvergedAction
 *
 *
 * The supervision has to make sure that the following invariants are always
 * satisfied:
 * 1. the number of OK servers is always bigger or equal to the number of
 *    servers in target.
 * 2. If a server is listed in Log/Target, it is also listed in State/Plan.
 */

namespace RLA = arangodb::replication2::agency;
namespace RSA = arangodb::replication2::replicated_state::agency;

namespace arangodb::replication2::replicated_state {

auto isParticipantSnapshotCompleted(ParticipantId const& participant,
                                    StateGeneration expectedGeneration,
                                    RSA::Current const& current,
                                    RSA::Plan const& plan) -> bool {
  TRI_ASSERT(plan.participants.contains(participant));
  TRI_ASSERT(plan.participants.at(participant).generation == expectedGeneration)
      << "expected = " << expectedGeneration
      << " planned = " << plan.participants.at(participant).generation;
  if (auto iter = current.participants.find(participant);
      iter != current.participants.end()) {
    auto const& state = iter->second;
    return state.generation == expectedGeneration &&
           state.snapshot.status == SnapshotStatus::kCompleted;
  }

  return false;
}

auto isParticipantSnapshotCompleted(ParticipantId const& participant,
                                    RSA::Current const& current,
                                    RSA::Plan const& plan) -> bool {
  if (auto iter = plan.participants.find(participant);
      iter != plan.participants.end()) {
    auto expectedGeneration = iter->second.generation;
    return isParticipantSnapshotCompleted(participant, expectedGeneration,
                                          current, plan);
  }

  return false;
}

/**
 * A server is considered OK if
 * - its snapshot is complete
 * - and is allowedAsLeader && allowedInQuorum in Log/Target and Log/Plan
 * @param participant
 * @param log
 * @param state
 * @return
 */
auto isParticipantOk(ParticipantId const& participant, RLA::Log const& log,
                     RSA::State const& state) {
  TRI_ASSERT(state.current.has_value());
  TRI_ASSERT(state.plan.has_value());
  TRI_ASSERT(log.plan.has_value());

  // check if the participant has an up-to-date snapshot
  auto snapshotOk =
      isParticipantSnapshotCompleted(participant, *state.current, *state.plan);
  if (!snapshotOk) {
    return false;
  }

  auto const flagsAreCorrect = [&](ParticipantsFlagsMap const& flagsMap) {
    if (auto iter = flagsMap.find(participant); iter != flagsMap.end()) {
      auto const& flags = iter->second;
      return flags.allowedAsLeader and flags.allowedInQuorum;
    }
    return true;
  };

  // check if the flags for that participant are set correctly
  auto const& config = log.plan->participantsConfig.participants;
  return flagsAreCorrect(config) and flagsAreCorrect(log.target.participants);
}

/**
 * Counts the number of OK participants.
 * @param log
 * @param state
 * @return
 */
auto countOkServers(RLA::Log const& log, RSA::State const& state)
    -> std::size_t {
  return std::count_if(
      state.plan->participants.begin(), state.plan->participants.end(),
      [&](auto const& p) { return isParticipantOk(p.first, log, state); });
}

struct SupervisionContext {
  RLA::Log const& log;
  RSA::State const& state;

  SupervisionContext(RLA::Log const& log, RSA::State const& state)
      : log(log), state(state) {}

  template<typename ActionType, typename... Args>
  void createAction(Args&&... args) {
    if (std::holds_alternative<std::monostate>(_action)) {
      _action.emplace(std::forward<Args>(args)...);
    }
  }

  void reportStatus(RSA::StatusCode code,
                    std::optional<ParticipantId> participant) {
    _reports.emplace_back(code, std::move(participant));
  }

 private:
  Action _action;
  std::vector<RSA::StatusMessage> _reports;
};

auto checkStatePlanLogTargetCreate(RSA::State const& state,
                                   RLA::Log const& log) {}

void checkReplicatedState(SupervisionContext& ctx) {}

#if 0
auto checkStateAdded(RSA::State const& state) -> Action {
  auto id = state.target.id;

  if (!state.plan) {
    auto statePlan = RSA::Plan{.id = state.target.id,
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

auto checkLeaderSet(RLA::Log const& log, RSA::State const& state) -> Action {
  auto const& targetLeader = state.target.leader;
  auto const& planLeader = log.target.leader;

  if (targetLeader != planLeader) {
    return SetLeaderAction{targetLeader};
  }

  return EmptyAction();
}

auto checkParticipantAdded(SupervisionContext const& ctx, RLA::Log const& log,
                           RSA::State const& state) -> Action {
  TRI_ASSERT(state.plan.has_value());

  if (ctx.numberServersInTarget + 1 < ctx.numberServersOk) {
    return EmptyAction();
  }

  auto const& targetParticipants = state.target.participants;
  auto const& planParticipants = state.plan->participants;

  for (auto const& [participant, flags] : targetParticipants) {
    if (!planParticipants.contains(participant)) {
      return AddParticipantAction{participant, state.plan->generation};
    }
  }
  return EmptyAction();
}

auto checkTargetParticipantRemoved(SupervisionContext const& ctx,
                                   RLA::Log const& log, RSA::State const& state)
    -> Action {
  TRI_ASSERT(state.plan.has_value());

  auto const& stateTargetParticipants = state.target.participants;
  auto const& logTargetParticipants = log.target.participants;

  for (auto const& [participant, flags] : logTargetParticipants) {
    if (!stateTargetParticipants.contains(participant)) {
      // check if it is ok for that participant to be dropped
      bool isOk = isParticipantOk(participant, log, state);
      auto newNumberOfOkServer = ctx.numberServersOk - (isOk ? 1 : 0);

      if (newNumberOfOkServer >= ctx.numberServersInTarget) {
        return RemoveParticipantFromLogTargetAction{participant};
      }
    }
  }
  return EmptyAction();
}

auto checkLogParticipantRemoved(RLA::Log const& log, RSA::State const& state)
    -> Action {
  TRI_ASSERT(state.plan.has_value());

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
auto checkSnapshotComplete(RLA::Log const& log, RSA::State const& state)
    -> Action {
  if (state.current and log.plan) {
    // TODO generation?
    for (auto const& [participant, flags] :
         log.plan->participantsConfig.participants) {
      if (!log.target.participants.contains(participant)) {
        continue;
      }
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
          } else {
          }
        }
      }
    }
  }
  return EmptyAction();
}

auto hasConverged(RSA::State const& state) -> bool {
  if (!state.plan) {
    return false;
  }
  if (!state.current) {
    return false;
  }

  // TODO check that the log has converged
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

auto checkConverged(RLA::Log const& log, RSA::State const& state) -> Action {
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

auto checkReplicatedStateParticipants(RLA::Log const& log,
                                      RSA::State const& state) -> Action {
  if (!state.current.has_value()) {
    return WaitForAction{"waiting for current"};
  }

  auto const serversInTarget = state.target.participants.size();
  auto const serversOk = countOkServers(log, state);
  auto ctx = SupervisionContext{serversInTarget, serversOk, true};

  if (auto action = checkParticipantAdded(ctx, log, state);
      !isEmptyAction(action)) {
    return action;
  }

  if (auto action = checkTargetParticipantRemoved(ctx, log, state);
      !isEmptyAction(action)) {
    return action;
  }

  if (auto action = checkLogParticipantRemoved(log, state);
      !isEmptyAction(action)) {
    return action;
  }

  if (auto action = checkSnapshotComplete(log, state); !isEmptyAction(action)) {
    return action;
  }

  return EmptyAction();
}

auto checkForwardSettings(RLA::Log const& log, RSA::State const& state)
    -> Action {
  if (auto action = checkLeaderSet(log, state); !isEmptyAction(action)) {
    return action;
  }

  return EmptyAction();
}

auto checkReplicatedState(std::optional<RLA::Log> const& log,
                          RSA::State const& state) -> Action {
  // First check if the replicated log is already there, if not create it.
  // Everything else requires the replicated log to exist.
  if (auto action = checkStateAdded(state); !isEmptyAction(action)) {
    return action;
  }

  // It will need to be observable in future that we are doing nothing because
  // we're waiting for the log to appear.
  if (!log.has_value()) {
    // if state/plan is visible, log/target should be visible as well
    TRI_ASSERT(state.plan == std::nullopt);
    return WaitForAction{"replicated log not yet visible"};
  }

  TRI_ASSERT(state.plan.has_value());
  if (auto action = checkReplicatedStateParticipants(*log, state);
      !isEmptyAction(action)) {
    return action;
  }

  if (auto action = checkForwardSettings(*log, state); !isEmptyAction(action)) {
    return action;
  }

  if (auto action = checkConverged(*log, state); !isEmptyAction(action)) {
    return action;
  }

  return EmptyAction{};
}
#endif

}  // namespace arangodb::replication2::replicated_state
