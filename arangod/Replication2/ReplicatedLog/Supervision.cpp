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

/*
 * If the currentleader is not present in target, this means
 * that the user removed that leader (rather forcefully)
 *
 * This in turn means we have to gracefully remove the leader
 * from its position;
 *
 * To not end up in a state where we have a) no leader and b)
 * not even a way to elect a new one we want to replace the leader
 * with a new one (gracefully); this is as opposed to just
 * rip out the old leader and waiting for failover to occur
 *
 * */
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

  // A participant is acceptable if it is neither excluded nor
  // already the leader
  auto acceptableLeaderSet = std::vector<ParticipantId>{};
  for (auto const& [participant, flags] :
       current.leader->committedParticipantsConfig->participants) {
    if (participant != current.leader->serverId and (not flags.excluded)) {
      acceptableLeaderSet.emplace_back(participant);
    }
  }

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
      auto const term = LogTerm{plan.currentTerm->term.value + 1};
      auto const termSpec = LogPlanTermSpecification(
          term, plan.currentTerm->config,
          LogPlanTermSpecification::Leader{participant, rebootId});

      return DictateLeaderAction(termSpec);
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

    return UpdateParticipantFlagsAction(chosenOne, flags,
                                        plan.participantsConfig.generation);
  }

  // TODO: Better error message
  return DictateLeaderFailedAction{"Failed to find a suitable leader"};
}

/*
 * Check whether Target contains an entry for a leader;
 * This means that leadership is supposed to be forced
 *
 */
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

    if (planLeaderConfig.forced != true || planLeaderConfig.excluded == true) {
      return ErrorAction(LogCurrentSupervisionError::TARGET_LEADER_EXCLUDED);
    }

    if (!health.notIsFailed(targetLeader)) {
      return EmptyAction();
      // TODO: we need to be able to trace why actions were not taken
      //       distinguishing between errors and conditions not being met (?)
    };

    auto const rebootId = health._health.at(targetLeader).rebootId;
    auto const term = LogTerm{plan.currentTerm->term.value + 1};
    auto const termSpec = LogPlanTermSpecification(
        term, plan.currentTerm->config,
        LogPlanTermSpecification::Leader{targetLeader, rebootId});

    return DictateLeaderAction(termSpec);
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
        participantsConfig.participants.at(participant).excluded;
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

auto doLeadershipElection(LogPlanSpecification const& plan,
                          LogCurrent const& current,
                          ParticipantsHealth const& health) -> Action {
  // Check whether there are enough participants to reach a quorum

  if (plan.participantsConfig.participants.size() + 1 <=
      plan.currentTerm->config.writeConcern) {
    auto election = LogCurrentSupervisionElection();
    election.term = plan.currentTerm->term;
    election.outcome = LogCurrentSupervisionElection::Outcome::IMPOSSIBLE;
    return LeaderElectionAction(election);
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
    election.outcome = LogCurrentSupervisionElection::Outcome::IMPOSSIBLE;
    return LeaderElectionAction(election);
  }

  if (election.participantsAvailable >= requiredNumberOfOKParticipants) {
    // We randomly elect on of the electible leaders
    auto const maxIdx = static_cast<uint16_t>(numElectible - 1);
    auto const& newLeader =
        election.electibleLeaderSet.at(RandomGenerator::interval(maxIdx));
    auto const& newLeaderRebootId = health._health.at(newLeader).rebootId;

    election.outcome = LogCurrentSupervisionElection::Outcome::SUCCESS;
    return LeaderElectionAction(
        election,
        LogPlanTermSpecification(
            LogTerm{plan.currentTerm->term.value + 1}, plan.currentTerm->config,
            LogPlanTermSpecification::Leader{.serverId = newLeader,
                                             .rebootId = newLeaderRebootId}));
  } else {
    // Not enough participants were available to form a quorum, so
    // we can't elect a leader
    election.outcome = LogCurrentSupervisionElection::Outcome::FAILED;
    return LeaderElectionAction(election);
  }
}

auto desiredParticipantFlags(LogTarget const& target,
                             LogPlanSpecification const& plan,
                             ParticipantId const& participant)
    -> ParticipantFlags {
  if (participant == target.leader and
      participant != plan.currentTerm->leader->serverId) {
    auto flags = target.participants.at(participant);
    if (!flags.excluded) {
      flags.forced = true;
    }
    return flags;
  }
  return target.participants.at(participant);
}

auto getParticipantWithUpdatedFlags(LogTarget const& target,
                                    LogPlanSpecification const& plan)
    -> std::optional<std::pair<ParticipantId, ParticipantFlags>> {
  auto const& tps = target.participants;
  auto const& pps = plan.participantsConfig.participants;

  for (auto const& [targetParticipant, targetFlags] : tps) {
    if (auto const& planParticipant = pps.find(targetParticipant);
        planParticipant != pps.end()) {
      // participant is in plan, check whether flags are the same
      auto const df = desiredParticipantFlags(target, plan, targetParticipant);
      if (df != planParticipant->second) {
        // Flags changed, so we need to commit new flags for this participant
        return std::make_pair(targetParticipant, df);
      }
    }
  }

  // nothing changed, nothing to do
  return std::nullopt;
}

auto getAddedParticipant(LogTarget const& target,
                         LogPlanSpecification const& plan)
    -> std::optional<std::pair<ParticipantId, ParticipantFlags>> {
  auto tps = target.participants;
  auto pps = plan.participantsConfig.participants;

  // is adding a participant or updating flags somehow the same action?
  for (auto const& [targetParticipant, targetFlags] : tps) {
    if (auto const& planParticipant = pps.find(targetParticipant);
        planParticipant == pps.end()) {
      // Here's a participant that is not in plan yet; we add it
      return std::make_pair(targetParticipant, targetFlags);
    }
  }
  return std::nullopt;
}

auto getRemovedParticipant(LogTarget const& target,
                           LogPlanSpecification const& plan)
    -> std::optional<std::pair<ParticipantId, ParticipantFlags>> {
  for (auto const& [planParticipant, flags] :
       plan.participantsConfig.participants) {
    if (!target.participants.contains(planParticipant)) {
      return std::make_pair(planParticipant, flags);
    }
  }

  return std::nullopt;
}

// The main function
auto checkReplicatedLog(Log const& log, ParticipantsHealth const& health)
    -> Action {
  auto const& target = log.target;

  if (!log.plan) {
    // The log is not planned right now, so we create it
    return AddLogToPlanAction(log.target.participants);
  }

  // plan now exists
  auto const& plan = *log.plan;

  if (!plan.currentTerm) {
    return CreateInitialTermAction{._config = target.config};
  }

  auto const& currentTerm = *plan.currentTerm;

  if (!log.current) {
    // As long as we don't  have current, we cannot progress with
    // establishing leadership
    return CurrentNotAvailableAction{};
  }
  auto const& current = *log.current;

  if (!plan.currentTerm->leader) {
    return doLeadershipElection(plan, current, health);
  }

  auto const& leader = *plan.currentTerm->leader;

  // If the leader is unhealthy, we need to create a new term that
  // does not have a leader; in the next round we should be electing
  // a new leader above
  if (isLeaderFailed(leader, health)) {
    return WriteEmptyTermAction{._term = currentTerm};
  }

  // Check whether the participant entry for the current
  // leader has been removed from target; this means we have
  // to gracefully remove this leader
  if (!target.participants.contains(currentTerm.leader->serverId)) {
    return dictateLeader(target, plan, *log.current, health);
  }

  // If flags for a participant differ between target
  // and plan, apply that change to plan.
  if (auto participantFlags = getParticipantWithUpdatedFlags(target, plan)) {
    return UpdateParticipantFlagsAction(participantFlags->first,
                                        participantFlags->second,
                                        plan.participantsConfig.generation);
  }

  // If a participant exists in Target, but not in Plan,
  // add that participant to Plan.
  if (auto participant = getAddedParticipant(log.target, plan)) {
    return AddParticipantToPlanAction(participant->first, participant->second,
                                      plan.participantsConfig.generation);
  }

  // A specific participant is configured in Target to become the
  // leader.
  if (target.leader) {
    if (auto action = leaderInTarget(*target.leader, plan, current, health)) {
      return *action;
    } else {
      // TODO!
    }
  }

  // If a participant is in Plan but not in Target, gracefully
  // remove them
  if (auto maybeParticipant = getRemovedParticipant(target, plan)) {
    auto const& [participantId, flags] = *maybeParticipant;
    // The removed participant is currently the leader
    if (participantId == leader.serverId) {
      return EvictLeaderAction(participantId, flags, currentTerm,
                               plan.participantsConfig.generation);
    } else {
      return RemoveParticipantFromPlanAction(
          participantId, plan.participantsConfig.generation);
    }
  }

  // If the configuration differs between Target and Plan,
  // apply the new configuration.
  //
  // TODO: This has not been implemented yet!
  if (target.config != currentTerm.config) {
    return UpdateLogConfigAction(target.config);
  }

  // Here we are converged and can hence signal so
  return ConvergedToTargetAction{};
}

}  // namespace arangodb::replication2::replicated_log
