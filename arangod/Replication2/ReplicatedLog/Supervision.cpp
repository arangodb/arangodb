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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "Supervision.h"

#include <memory>

#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Random/RandomGenerator.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"

#include "Logger/LogMacros.h"

using namespace arangodb::replication2::agency;

namespace arangodb::replication2::replicated_log {

// Leader has failed if it is marked as failed or it's rebootId is
// different from what is expected
auto isLeaderFailed(LogPlanTermSpecification::Leader const& leader,
                    ParticipantsHealth const& health) -> bool {
  // TODO: less obscure with fewer negations
  // TODO: write test first
  if (health.notIsFailed(leader.serverId) &&
      health.validRebootId(leader.serverId, leader.rebootId)) {
    return false;
  } else {
    return true;
  }
}

// If the currentleader is not present in target, this means
// that the user removed that leader (rather forcefully)
//
// This in turn means we have to gracefully remove the leader
// from its position;
//
// To not end up in a state where we have a) no leader and b)
// not even a way to elect a new one we want to replace the leader
// with a new one (gracefully); this is as opposed to just
// rip out the old leader and waiting for failover to occur

auto getParticipantsAcceptableAsLeaders(
    ParticipantId const& currentLeader,
    ParticipantsFlagsMap const& participants) -> std::vector<ParticipantId> {
  // A participant is acceptable if it is neither excluded nor
  // already the leader
  auto acceptableLeaderSet = std::vector<ParticipantId>{};
  for (auto const& [participant, flags] : participants) {
    if (participant != currentLeader and flags.allowedAsLeader) {
      acceptableLeaderSet.emplace_back(participant);
    }
  }

  return acceptableLeaderSet;
}

// Switch to a new leader gracefully by finding a participant that is
// functioning and handing leadership to them.
// This happens when the user uses Target to specify a leader.
auto dictateLeader(LogTarget const& target, LogPlanSpecification const& plan,
                   LogCurrent const& current, ParticipantsHealth const& health)
    -> Action {
  // TODO: integrate
  if (!current.leader || !current.leader->committedParticipantsConfig ||
      current.leader->committedParticipantsConfig->generation !=
          plan.participantsConfig.generation) {
    return DictateLeaderFailedAction{
        "No leader in current, current participants config not committed, or "
        "wrong generation"};
  }

  auto const acceptableLeaderSet = getParticipantsAcceptableAsLeaders(
      current.leader->serverId,
      current.leader->committedParticipantsConfig->participants);

  //  Check whether we already have a participant that is
  //  acceptable and forced
  //
  //  if so, make them leader
  for (auto const& participant : acceptableLeaderSet) {
    auto const& flags =
        current.leader->committedParticipantsConfig->participants.at(
            participant);

    if (participant != current.leader->serverId and flags.forced) {
      auto const rebootId = health._health.at(participant).rebootId;
      return DictateLeaderAction(
          LogPlanTermSpecification::Leader(participant, rebootId));
    }
  }

  // Did not find a  participant above, so pick one at random
  // and force them.
  auto const numElectible = acceptableLeaderSet.size();
  if (numElectible > 0) {
    auto const maxIdx = static_cast<uint16_t>(numElectible - 1);
    auto const& chosenOne =
        acceptableLeaderSet.at(RandomGenerator::interval(maxIdx));

    auto flags =
        current.leader->committedParticipantsConfig->participants.at(chosenOne);

    flags.forced = true;

    return UpdateParticipantFlagsAction(chosenOne, flags);
  }

  // TODO: Better error message
  return DictateLeaderFailedAction{"Failed to find a suitable leader"};
}

// Check whether Target contains an entry for a leader, which means
// that the user would like a particular participant to be leader;
auto leaderInTarget(ParticipantId const& targetLeader,
                    LogPlanSpecification const& plan, LogCurrent const& current,
                    ParticipantsHealth const& health) -> std::optional<Action> {
  // Someone wishes there to be a particular leader

  if (plan.currentTerm && plan.currentTerm->leader &&
      targetLeader != plan.currentTerm->leader->serverId) {
    // move to new leader

    // Check that current generation is equal to planned generation
    // Check that target.leader is forced
    // Check that target.leader is not excluded
    // Check that target.leader is healthy
    if (!current.leader || !current.leader->committedParticipantsConfig ||
        current.leader->committedParticipantsConfig->generation !=
            plan.participantsConfig.generation) {
      // The current leader has committed a configuration that is different
      // from the planned configuration

      return EmptyAction();
      // Really: return ConfigurationNotCommittedAction{};
    }

    if (!plan.participantsConfig.participants.contains(targetLeader)) {
      if (current.supervision && current.supervision->error &&
          current.supervision->error ==
              LogCurrentSupervisionError::TARGET_LEADER_INVALID) {
        // Error has already been reported; don't re-report
        return std::nullopt;
      } else {
        return ErrorAction(LogCurrentSupervisionError::TARGET_LEADER_INVALID);
      }
    }
    auto const& planLeaderConfig =
        plan.participantsConfig.participants.at(targetLeader);

    if (planLeaderConfig.forced != true || !planLeaderConfig.allowedAsLeader) {
      return ErrorAction(LogCurrentSupervisionError::TARGET_LEADER_EXCLUDED);
    }

    if (!health.notIsFailed(targetLeader)) {
      return EmptyAction();
      // TODO: we need to be able to trace why actions were not taken
      //       distinguishing between errors and conditions not being met (?)
    };

    auto const rebootId = health._health.at(targetLeader).rebootId;
    return DictateLeaderAction(
        LogPlanTermSpecification::Leader{targetLeader, rebootId});
  }
  return std::nullopt;
}

auto computeReason(LogCurrentLocalState const& status, bool healthy,
                   bool excluded, LogTerm term)
    -> LogCurrentSupervisionElection::ErrorCode {
  if (!healthy) {
    return LogCurrentSupervisionElection::ErrorCode::SERVER_NOT_GOOD;
  } else if (excluded) {
    return LogCurrentSupervisionElection::ErrorCode::SERVER_EXCLUDED;
  } else if (term != status.term) {
    return LogCurrentSupervisionElection::ErrorCode::TERM_NOT_CONFIRMED;
  } else {
    return LogCurrentSupervisionElection::ErrorCode::OK;
  }
}

auto runElectionCampaign(LogCurrentLocalStates const& states,
                         ParticipantsConfig const& participantsConfig,
                         ParticipantsHealth const& health, LogTerm term)
    -> LogCurrentSupervisionElection {
  auto election = LogCurrentSupervisionElection();
  election.term = term;

  for (auto const& [participant, status] : states) {
    auto const excluded =
        participantsConfig.participants.contains(participant) and
        not participantsConfig.participants.at(participant).allowedAsLeader;
    auto const healthy = health.notIsFailed(participant);
    auto reason = computeReason(status, healthy, excluded, term);
    election.detail.emplace(participant, reason);

    if (reason == LogCurrentSupervisionElection::ErrorCode::OK) {
      election.participantsAvailable += 1;

      if (status.spearhead >= election.bestTermIndex) {
        if (status.spearhead != election.bestTermIndex) {
          election.electibleLeaderSet.clear();
        }
        election.electibleLeaderSet.push_back(participant);
        election.bestTermIndex = status.spearhead;
      }
    }
  }
  return election;
}

// If the currentTerm does not have a leader, we have to select one
// participant to become the leader. For this we have to
//
//  * have enough participants (one participant more than
//    writeConcern)
//  * have to have enough participants that have not failed or
//    rebooted
//
// The subset of electable participants is determined. A participant is
// electable if it is
//  * allowedAsLeader
//  * not marked as failed
//  * amongst the participant with the most recent TermIndex.
//
auto doLeadershipElection(LogPlanSpecification const& plan,
                          LogCurrent const& current,
                          ParticipantsHealth const& health) -> Action {
  // Check whether there are enough participants to reach a quorum
  if (plan.participantsConfig.participants.size() + 1 <=
      plan.currentTerm->config.writeConcern) {
    return LeaderElectionImpossibleAction();
  }

  TRI_ASSERT(plan.participantsConfig.participants.size() + 1 >
             plan.currentTerm->config.writeConcern);

  auto const requiredNumberOfOKParticipants =
      plan.participantsConfig.participants.size() + 1 -
      plan.currentTerm->config.writeConcern;

  // Find the participants that are healthy and that have the best LogTerm
  auto election =
      runElectionCampaign(current.localState, plan.participantsConfig, health,
                          plan.currentTerm->term);
  election.participantsRequired = requiredNumberOfOKParticipants;

  auto const numElectible = election.electibleLeaderSet.size();

  if (numElectible == 0 ||
      numElectible > std::numeric_limits<uint16_t>::max()) {
    return LeaderElectionOutOfBoundsAction{._election = election};
  }

  if (election.participantsAvailable >= requiredNumberOfOKParticipants) {
    // We randomly elect on of the electible leaders
    auto const maxIdx = static_cast<uint16_t>(numElectible - 1);
    auto const& newLeader =
        election.electibleLeaderSet.at(RandomGenerator::interval(maxIdx));
    auto const& newLeaderRebootId = health._health.at(newLeader).rebootId;

    return LeaderElectionAction(
        LogPlanTermSpecification::Leader(newLeader, newLeaderRebootId),
        election);
  } else {
    // Not enough participants were available to form a quorum, so
    // we can't elect a leader
    return LeaderElectionQuorumNotReachedAction{election};
  }
}

auto desiredParticipantFlags(std::optional<ParticipantId> const& targetLeader,
                             ParticipantId const& currentTermLeader,
                             ParticipantId const& targetParticipant,
                             ParticipantFlags const& targetFlags)
    -> ParticipantFlags {
  if (targetParticipant == targetLeader and
      targetParticipant != currentTermLeader) {
    auto flags = targetFlags;
    if (flags.allowedAsLeader) {
      flags.forced = true;
    }
    return flags;
  }
  return targetFlags;
}

// If there is a participant such that the flags between Target and Plan differ,
// returns at a pair consisting of the ParticipantId and the desired flags.
//
// Note that the desired flags currently forces the flags for a configured,
// desired, leader to contain a forced flag.
auto getParticipantWithUpdatedFlags(
    ParticipantsFlagsMap const& targetParticipants,
    ParticipantsFlagsMap const& planParticipants,
    std::optional<ParticipantId> const& targetLeader,
    ParticipantId const& currentTermLeader)
    -> std::optional<std::pair<ParticipantId, ParticipantFlags>> {
  for (auto const& [targetParticipant, targetFlags] : targetParticipants) {
    if (auto const& planParticipant = planParticipants.find(targetParticipant);
        planParticipant != std::end(planParticipants)) {
      // participant is in plan, check whether flags are the same
      auto const desiredFlags = desiredParticipantFlags(
          targetLeader, currentTermLeader, targetParticipant, targetFlags);
      if (desiredFlags != planParticipant->second) {
        // Flags changed, so we need to commit new flags for this participant
        return std::make_pair(targetParticipant, desiredFlags);
      }
    }
  }

  // nothing changed, nothing to do
  return std::nullopt;
}

// If there is a participant that is present in Target, but not in Flags,
// returns a pair consisting of the ParticipantId and the ParticipantFlags.
// Otherwise returns std::nullopt
auto getAddedParticipant(ParticipantsFlagsMap const& targetParticipants,
                         ParticipantsFlagsMap const& planParticipants)
    -> std::optional<std::pair<ParticipantId, ParticipantFlags>> {
  for (auto const& [targetParticipant, targetFlags] : targetParticipants) {
    if (auto const& planParticipant = planParticipants.find(targetParticipant);
        planParticipant == planParticipants.end()) {
      return std::make_pair(targetParticipant, targetFlags);
    }
  }
  return std::nullopt;
}

// If there is a participant that is present in Plan, but not in Target,
// returns a pair consisting of the ParticipantId and the ParticipantFlags.
// Otherwise returns std::nullopt
auto getRemovedParticipant(ParticipantsFlagsMap const& targetParticipants,
                           ParticipantsFlagsMap const& planParticipants)
    -> std::optional<std::pair<ParticipantId, ParticipantFlags>> {
  for (auto const& [planParticipant, flags] : planParticipants) {
    if (!targetParticipants.contains(planParticipant)) {
      return std::make_pair(planParticipant, flags);
    }
  }
  return std::nullopt;
}

// Pick leader at random from participants
auto pickRandomParticipantToBeLeader(ParticipantsFlagsMap const& participants,
                                     ParticipantsHealth const& health)
    -> std::optional<ParticipantId> {
  auto acceptableParticipants = std::vector<ParticipantId>{};

  for (auto [part, flags] : participants) {
    if (flags.allowedAsLeader && health._health.contains(part)) {
      acceptableParticipants.emplace_back(part);
    }
  }

  if (!acceptableParticipants.empty()) {
    auto maxIdx = static_cast<uint16_t>(acceptableParticipants.size() - 1);
    auto p = acceptableParticipants.begin();

    std::advance(p, RandomGenerator::interval(maxIdx));

    return *p;
  }

  return std::nullopt;
}

auto pickLeader(std::optional<ParticipantId> targetLeader,
                ParticipantsFlagsMap const& participants,
                ParticipantsHealth const& health)
    -> std::optional<LogPlanTermSpecification::Leader> {
  auto leaderId = targetLeader;

  if (!leaderId) {
    leaderId = pickRandomParticipantToBeLeader(participants, health);
  }

  if (leaderId.has_value()) {
    auto rebootId = health.getRebootId(*leaderId);
    if (rebootId.has_value()) {
      return LogPlanTermSpecification::Leader{*leaderId, *rebootId};
    }
  };

  return std::nullopt;
}

//
// This function is called from Agency/Supervision.cpp every k seconds for every
// replicated log in every database.
//
// This means that this function is always going to deal with exactly *one*
// replicated log.
//
// A ReplicatedLog has a Target, a Plan, and a Current subtree in the agency,
// and these three subtrees are passed into checkReplicatedLog in the form
// of C++ structs.
//
// The return value of this function is an Action, where the type Action is a
// std::variant of all possible actions that we can perform as a result of
// checkReplicatedLog.
//
// These actions are executes by using std::visit via an Executor struct that
// contains the necessary context.

auto checkReplicatedLog(LogTarget const& target,
                        std::optional<LogPlanSpecification> const& maybePlan,
                        std::optional<LogCurrent> const& maybeCurrent,
                        ParticipantsHealth const& health) -> Action {
  if (!maybePlan) {
    // The log is not planned right now, so we create it
    // provided we have enough participants
    if (target.participants.size() + 1 < target.config.writeConcern) {
      return ErrorAction(
          LogCurrentSupervisionError::TARGET_NOT_ENOUGH_PARTICIPANTS);
    } else {
      auto leader = pickLeader(target.leader, target.participants, health);
      return AddLogToPlanAction(target.id, target.participants, target.config,
                                leader);
    }
  }

  // plan now exists
  auto const& plan = *maybePlan;

  // If the ReplicatedLog does not have a LogTerm yet, we create
  // the initial (empty) Term, which will kick off a leader election.
  if (!plan.currentTerm) {
    return CreateInitialTermAction{._config = target.config};
  }
  // currentTerm has a value now.
  auto const& currentTerm = *plan.currentTerm;

  // If the Current subtree does not exist yet, create it by writing
  // a message into it.
  if (!maybeCurrent) {
    return CurrentNotAvailableAction{};
  }
  auto const& current = *maybeCurrent;

  // If currentTerm's leader entry does not have a value,
  // run a leadership election. The doLeadershipElection can
  // return different Actions, depending on whether a leadership
  // election is possible, and if so, whether there is enough
  // eligible participants for leadership.
  if (!plan.currentTerm->leader) {
    return doLeadershipElection(plan, current, health);
  }
  auto const& leader = *plan.currentTerm->leader;

  // If the leader is unhealthy, write a new term that
  // does not have a leader.
  // In the next round this will lead to a leadership election.
  if (isLeaderFailed(leader, health)) {
    auto minTerm = plan.currentTerm->term;
    for (auto [participant, state] : current.localState) {
      if (state.spearhead.term > minTerm) {
        minTerm = state.spearhead.term;
      }
    }
    return WriteEmptyTermAction(minTerm);
  }

  // leader has been removed from target;
  // If so, try to gracefully remove this leader by
  // selecting a different eligible participant as leader
  // and switching leaders.
  if (!target.participants.contains(leader.serverId)) {
    return dictateLeader(target, plan, current, health);
  }

  // If the user has updated flags for a participant, which is detected by
  // comparing Target to Plan, write that change to Plan.
  // TODO: This function currently forces a participant if it is set
  //       as desired leader in Target, and this isn't obvious at all.
  //       This should be moved to a separate action that overrides that
  //       particular field for the desired leader.
  if (auto participantFlags = getParticipantWithUpdatedFlags(
          target.participants, plan.participantsConfig.participants,
          target.leader, leader.serverId)) {
    return UpdateParticipantFlagsAction(participantFlags->first,
                                        participantFlags->second);
  }

  // Check whether a participant was added in Target that is not in Plan.
  // If so, add it to Plan.
  if (auto participant = getAddedParticipant(
          target.participants, plan.participantsConfig.participants)) {
    return AddParticipantToPlanAction(participant->first, participant->second);
  }

  // Check whether a specific participant is configured in Target to become
  // the leader. This requires that participant to be flagged to always be
  // part of a quorum; once that change is committed, the leader can be
  // switched if the target.leader participant is healty.
  //
  // This operation can fail and
  // TODO: Report if leaderInTarget fails.
  if (target.leader) {
    if (auto action = leaderInTarget(*target.leader, plan, current, health)) {
      return *action;
    } else {
      // TODO!
    }
  }

  // If a participant is in Plan but not in Target, gracefully
  // remove them
  if (auto maybeParticipant = getRemovedParticipant(
          target.participants, plan.participantsConfig.participants)) {
    auto const& [participantId, planFlags] = *maybeParticipant;

    if (not planFlags.allowedInQuorum and
        current.leader->committedParticipantsConfig->generation ==
            plan.participantsConfig.generation) {
      return RemoveParticipantFromPlanAction(participantId);
    } else if (planFlags.allowedInQuorum) {
      // make this server not allowed in quorum. If the generation is
      // committed
      auto newFlags = planFlags;
      newFlags.allowedInQuorum = false;
      return UpdateParticipantFlagsAction(participantId, newFlags);
    } else {
      // still waiting
      return EmptyAction("Waiting for participants config to be committed");
    }
  }

  // If the configuration differs between Target and Plan,
  // apply the new configuration.
  //
  // TODO: This has not been implemented yet!
  if (target.config != currentTerm.config) {
    return UpdateLogConfigAction(target.config);
  }

  if (target.version != current.supervision->targetVersion) {
    return ConvergedToTargetAction{target.version};
  } else {
    // Note that if we converged and the version is the same this ends up
    // doing nothing. Maybe we should have a debug mode where this is still
    // reported?
    return EmptyAction("There was nothing to do.");
  }
}

}  // namespace arangodb::replication2::replicated_log
