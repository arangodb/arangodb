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

/*
 * Helper function to create a list of participants that can be used
 * for a log when the list of participants is currently empty in Target.
 *
 * This should only happen in testing with older supervision tests
 *
 * */
auto getEnoughHealthyParticipants(size_t replicationFactor,
                                  ParticipantsHealth const &health)
    -> LogTarget::Participants {

  LogTarget::Participants participants;

  for (auto const &[pid, health] : health._health) {
    if (health.isHealthy) {
      participants.emplace(pid, ParticipantFlags{});
    }
    if (participants.size() == replicationFactor) {
      break;
    }
  }
  return participants;
}

// The main function
auto checkReplicatedLog(Log const &log, ParticipantsHealth const &health)
    -> Action {
  auto const &target = log.target;
  auto const &plan = log.plan;
  //  auto const &current = log.current;

  if (!plan) {
    // TODO: this is a temporary hack to make older tests work
    if (target.participants.empty()) {
      return AddParticipantsToTargetAction{getEnoughHealthyParticipants(
          target.config.replicationFactor, health)};
    } else {
      return AddLogToPlanAction{log.target.participants};
    }
  } else {
#if 0
    if (!plan.currentTerm) {
      return CreateInitialTermAction(config);
    } else {
      if (!current) {
        // Once we log that current isn't available, current will become
        // available ;)
        return CurrentNotAvailableAction{};
      } else {
        if (!plan.currentTerm->leader) {
          // TODO: this would have to work correctly
          return leadershipElection(plan, *current, health);
        } else {
          if (isLeaderHealthy(*plan, health)) {

            // The current leader has been removed from target
            if (!target.participants.contains(
                    plan.currentTerm->leader->serverId)) {

            } else if (auto flags = getParticipantWithUpdatedFlags()) {
              return UpdateParticipantFlagsAction{flags};
            } else if (auto participant = getAddedParticipant()) {
              return AddParticipantAction{participant};
            } else if (target.leader &&
                       target.leader != plan.currentTerm->leader->serverId) {

            } else if (auto const &part =
                           getRemovedParticipant(target, *plan)) {
              if (part->first == plan.currentTerm->leader->serverId) {
                return MakeEvictLeaderAction();
              } else {
                return MakeRemoveParticipantAction(part);
              }
            } else if (target.config != plan.currentTerm->config) {
              return UpdateConfigAction{target.config};
            } else {
              // Nothing to do}
              // Is this where we converged?
              return ConvergedToTargetAction{};
            }
          } else {
            // Leader is not healthy; start a new term
            return MakeUpdateTermAction(*plan->currentTerm);
          }
        }
      }
    }
#endif
    return EmptyAction{};
  }

  // TODO: if we reach here, our if above isn't covering, and
  // that is a problem; signal this problem
  TRI_ASSERT(false);
  return EmptyAction{};
}

#if 0
auto checkLogTargetParticipantRemoved(LogTarget const &target,
                                      LogPlanSpecification const &plan)
    -> std::unique_ptr<Action> {
  auto tps = target.participants;
  auto pps = plan.participantsConfig.participants;

  // Check whether a participant has been removed
  for (auto const &[planParticipant, flags] : pps) {
    if (!tps.contains(planParticipant)) {
      if (plan.currentTerm && plan.currentTerm->leader &&
          plan.currentTerm->leader->serverId == planParticipant) {
        auto desiredFlags = flags;
        desiredFlags.excluded = true;
        auto newTerm = *plan.currentTerm;
        newTerm.term = LogTerm{newTerm.term.value + 1};
        newTerm.leader.reset();
        return std::make_unique<EvictLeaderAction>(
            plan.id, planParticipant, desiredFlags, newTerm,
            plan.participantsConfig.generation);
      } else {
        return std::make_unique<RemoveParticipantFromPlanAction>(
            plan.id, planParticipant, plan.participantsConfig.generation);
      }
    }
  }
  return std::make_unique<EmptyAction>();
}


  auto checkLogTargetParticipantFlags(LogTarget const &target,
                                    LogPlanSpecification const &plan)
    -> std::unique_ptr<Action> {
  auto const &tps = target.participants;
  auto const &pps = plan.participantsConfig.participants;

  for (auto const &[targetParticipant, targetFlags] : tps) {
    if (auto const &planParticipant = pps.find(targetParticipant);
        planParticipant != pps.end()) {
      // participant is in plan, check whether flags are the same
      auto const df = desiredParticipantFlags(target, plan, targetParticipant);
      if (df != planParticipant->second) {
        // Flags changed, so we need to commit new flags for this participant
        return std::make_unique<UpdateParticipantFlagsAction>(
            target.id, targetParticipant, df,
            plan.participantsConfig.generation);
      }
    }
  }

  // nothing changed, nothing to do
  return std::make_unique<EmptyAction>();
}


auto healthyParticipants(LogTarget target, ParticipantsHealth const &health)
    -> LogTarget {
  for (auto const &[pid, health] : health._health) {
    if (health.isHealthy) {
      target.participants.emplace(pid, ParticipantFlags{});
    }
    if (target.participants.size() == target.config.replicationFactor) {
      break;
    }
  }
  return target;
}

auto isLeaderHealthy(LogPlanSpecification const &plan,
                     ParticipantsHealth const &health) -> bool {
  return (health.isHealthy(plan.currentTerm->leader->serverId) &&
          health.validRebootId(plan.currentTerm->leader->serverId,
                               plan.currentTerm->leader->rebootId));
}

auto checkLeaderRemovedFromTarget(LogTarget const &target,
                                  LogPlanSpecification const &plan,
                                  LogCurrent const &current,
                                  ParticipantsHealth const &health)
    -> std::unique_ptr<Action> {
  // TODO: integrate

  if (current.leader) {
    if (current.leader->committedParticipantsConfig) {
      if (current.leader->committedParticipantsConfig->generation ==
          plan.participantsConfig.generation) {

      } else {
        // Committed Config generation not equal to config
      }
    } else {
      // hasn't committed config
    }
  } else {
    // no leader in current
  }

  // Why is this here?
  if (!current.leader || !current.leader->committedParticipantsConfig ||
      current.leader->committedParticipantsConfig->generation !=
          plan.participantsConfig.generation) {
    return std::make_unique<EmptyAction>();
  }

  // If we enter this funciton we know that there is a leader in current term,
  // so we do not need this bit
  if (
      // plan.currentTerm && plan.currentTerm->leader &&
      !target.participants.contains(plan.currentTerm->leader->serverId)) {
    // A participant is acceptable if it is neither excluded nor
    // already the leader
    auto acceptableLeaderSet = std::vector<ParticipantId>{};
    for (auto const &[participant, flags] :
         current.leader->committedParticipantsConfig->participants) {

      if (participant != current.leader->serverId and (not flags.excluded)) {
        acceptableLeaderSet.emplace_back(participant);
      }
    }

    //  Check whether we already have a participant that is
    //  acceptable and forced
    //
    //  if so, make them leader
    for (auto const &participant : acceptableLeaderSet) {
      auto const &flags =
          current.leader->committedParticipantsConfig->participants.at(
              participant);

      if (participant != current.leader->serverId and flags.forced) {
        auto const rebootId = health._health.at(participant).rebootId;
        auto const term = LogTerm{plan.currentTerm->term.value + 1};
        auto const termSpec = LogPlanTermSpecification(
            term, plan.currentTerm->config,
            LogPlanTermSpecification::Leader{participant, rebootId});

        return std::make_unique<DictateLeaderAction>(target.id, termSpec);
      }
    }

    // Did not find a  participant above, so pick one at random
    // and force them.
    auto const numElectible = acceptableLeaderSet.size();
    if (numElectible > 0) {
      auto const maxIdx = static_cast<uint16_t>(numElectible - 1);
      auto const &chosenOne =
          acceptableLeaderSet.at(RandomGenerator::interval(maxIdx));

      auto flags = current.leader->committedParticipantsConfig->participants.at(
          chosenOne);

      flags.forced = true;

      return std::make_unique<UpdateParticipantFlagsAction>(
          target.id, chosenOne, flags, plan.participantsConfig.generation);
    } else {
      // We should be signaling that we could not determine a leader
      // because noone suitable was available
    }
  }

  // This should not happen anymore (or we need to signal an informational trace
  // message)
  return std::make_unique<EmptyAction>();
}

/*
 * Check whether Target contains an entry for a leader;
 * This means that leadership is supposed to be forced
 *
 */
auto checkLeaderInTarget(LogTarget const &target,
                         LogPlanSpecification const &plan,
                         LogCurrent const &current,
                         ParticipantsHealth const &health)
    -> std::unique_ptr<Action> {
  // Someone wishes there to be a particular leader

  if (target.leader && plan.currentTerm && plan.currentTerm->leader &&
      target.leader != plan.currentTerm->leader->serverId) {
    // move to new leader

    // Check that current generation is equal to planned generation
    // Check that target.leader is forced
    // Check that target.leader is not excluded
    // Check that target.leader is healthy
    if (!current.leader || !current.leader->committedParticipantsConfig ||
        current.leader->committedParticipantsConfig->generation !=
            plan.participantsConfig.generation) {
      return std::make_unique<EmptyAction>();
    }

    if (!plan.participantsConfig.participants.contains(*target.leader)) {
      if (current.supervision && current.supervision->error &&
          current.supervision->error ==
              LogCurrentSupervisionError::TARGET_LEADER_INVALID) {
        // Error has already been reported; don't re-report
        return std::make_unique<EmptyAction>();
      } else {
        return std::make_unique<ErrorAction>(
            plan.id, LogCurrentSupervisionError::TARGET_LEADER_INVALID);
      }
    }
    auto const &planLeaderConfig =
        plan.participantsConfig.participants.at(*target.leader);

    if (planLeaderConfig.forced != true || planLeaderConfig.excluded == true) {
      return std::make_unique<ErrorAction>(
          plan.id, LogCurrentSupervisionError::TARGET_LEADER_EXCLUDED);
    }

    if (!health.isHealthy(*target.leader)) {
      return std::make_unique<EmptyAction>();
      // TODO: we need to be able to trace why actions were not taken
      //       distinguishing between errors and conditions not being met (?)
    };

    auto const rebootId = health._health.at(*target.leader).rebootId;
    auto const term = LogTerm{plan.currentTerm->term.value + 1};
    auto const termSpec = LogPlanTermSpecification(
        term, plan.currentTerm->config,
        LogPlanTermSpecification::Leader{*target.leader, rebootId});

    return std::make_unique<DictateLeaderAction>(target.id, termSpec);
  }
  return std::make_unique<EmptyAction>();
}

auto computeReason(LogCurrentLocalState const &status, bool healthy,
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

auto runElectionCampaign(LogCurrentLocalStates const &states,
                         ParticipantsConfig const &participantsConfig,
                         ParticipantsHealth const &health, LogTerm term)
    -> LogCurrentSupervisionElection {
  auto election = LogCurrentSupervisionElection();

  for (auto const &[participant, status] : states) {
    auto const excluded =
        participantsConfig.participants.contains(participant) and
        participantsConfig.participants.at(participant).excluded;
    auto const healthy = health.isHealthy(participant);
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

auto leadershipElection(LogTerm const &currentTerm,
                        LogParticipantsConfig const &participantsConfig,
                        LogCurrentLocalState const &currentLocalState,
                        ParticipantsHealth const &health) -> Action {

  auto const configuredParticipants = participantsConfig.participants.size();
  auto const writeConcern = currentTerm.config.writeConcern;

  // Check whether there are enough participants to reach a quorum
  if (configuredParticipants + 1 <= writeConcern) {
    return LeaderElectionImpossibleAction{};
  } else {
    auto const requiredNumberOfOKParticipants =
        configuredParticipants + 1 - writeConcern;

    // Find the participants that are healthy and that have the best LogTerm
    auto election = runElectionCampaign(currentLocalState, participantsConfig,
                                        health, currentTerm);

    auto const numElectible = election.electibleLeaderSet.size();

    if (numElectible == 0 ||
        numElectible > std::numeric_limits<uint16_t>::max()) {
      return LeaderElectionImpossibleAction(election);
    } else {

      if (election.participantsAvailable < requiredNumberOfOKParticipants) {
        // Not enough participants
        return LeaderElectionFailedAction{election};
      } else {

        // We randomly elect on of the electible leaders
        auto const maxIdx = static_cast<uint16_t>(numElectible - 1);
        auto const &newLeader =
            election.electibleLeaderSet.at(RandomGenerator::interval(maxIdx));
        auto const &newLeaderRebootId = health._health.at(newLeader).rebootId;

        return MakeLeaderElectionSuccessAction(election, currentTerm, newLeader,
                                               newLeaderRebootId);
      }
    }
  }
}

auto desiredParticipantFlags(LogTarget const &target,
                             LogPlanSpecification const &plan,
                             ParticipantId const &participant)
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

auto getAddedParticipant(LogTarget const &target,
                         LogPlanSpecification const &plan)
    -> std::optional<std::pair<ParticipantId, ParticipantFlags>> {
  auto pps = plan.participantsConfig.participants;

  // is adding a participant or updating flags somehow the same action?
  for (auto const &[targetParticipant, targetFlags] : target.participants) {
    pl if (auto const &planParticipant =
               plan.participantsConfig.find(targetParticipant);
           planParticipant == std::end(plan.participantsConfig)) {
      // Here's a participant that is not in plan yet; we add it
      return std::make_pair(targetParticipant, targetFlags);
    }
  }
  return std::nullopt;
}

auto getRemovedParticipant(LogTarget const &target,
                           LogPlanSpecification const &plan)
    -> std::optional<std::pair<ParticipantId, ParticipantFlags>> {
  for (auto const &[planParticipant, flags] :
       plan.participantsConfig.participants) {
    if (!target.participant.contains(planParticipant)) {
      return std::make_pair(planParticipant, flags);
    }
  }
  return std::nullopt;
}

#endif

} // namespace arangodb::replication2::replicated_log
