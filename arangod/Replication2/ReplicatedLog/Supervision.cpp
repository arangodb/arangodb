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

#include "Basics/StringUtils.h"
#include "Basics/Exceptions.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Random/RandomGenerator.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"

#include "Logger/LogMacros.h"

using namespace arangodb::replication2::agency;

namespace arangodb::replication2::replicated_log {

auto checkLogAdded(const Log& log) -> std::unique_ptr<Action> {
  if (!log.plan) {
    // action that creates Plan entry for log
    auto const spec = LogPlanSpecification(
        log.target.id, std::nullopt, log.target.config,
        ParticipantsConfig{.participants = log.target.participants});

    return std::make_unique<AddLogToPlanAction>(spec);
  }
  return std::make_unique<EmptyAction>();
}

auto checkTermPresent(LogPlanSpecification const& plan)
    -> std::unique_ptr<Action> {
  if (!plan.currentTerm) {
    return std::make_unique<CreateInitialTermAction>();
  }
  return std::make_unique<EmptyAction>();
}

auto checkLeaderHealth(LogPlanSpecification const& plan,
                       ParticipantsHealth const& health)
    -> std::unique_ptr<Action> {
  // TODO: if we assert this here, we assume we're always called
  //       with non std::nullopt currentTerm and leader
  TRI_ASSERT(plan.currentTerm != std::nullopt);
  TRI_ASSERT(plan.currentTerm->leader != std::nullopt);
  if (health.isHealthy(plan.currentTerm->leader->serverId) &&
      health.validRebootId(plan.currentTerm->leader->serverId,
                           plan.currentTerm->leader->rebootId)) {
    // Current leader is all healthy so nothing to do.
    return std::make_unique<EmptyAction>();
  } else {
    // Leader is not healthy; start a new term
    auto newTerm = *plan.currentTerm;

    newTerm.leader.reset();
    newTerm.term = LogTerm{plan.currentTerm->term.value + 1};

    return std::make_unique<UpdateTermAction>(newTerm);
  }
}

auto runElectionCampaign(LogCurrentLocalStates const& states,
                         ParticipantsHealth const& health, LogTerm term)
    -> LeaderElectionCampaign {
  auto campaign = LeaderElectionCampaign{};

  for (auto const& [participant, status] : states) {
    auto reason = computeReason(status, health.isHealthy(participant), term);
    campaign.reasons.emplace(participant, reason);

    if (reason == LeaderElectionCampaign::Reason::OK) {
      campaign.numberOKParticipants += 1;

      if (status.spearhead >= campaign.bestTermIndex) {
        if (status.spearhead != campaign.bestTermIndex) {
          campaign.electibleLeaderSet.clear();
        }
        campaign.electibleLeaderSet.push_back(participant);
        campaign.bestTermIndex = status.spearhead;
      }
    }
  }
  return campaign;
}

auto tryLeadershipElection(LogPlanSpecification const& plan,
                           LogCurrent const& current,
                           ParticipantsHealth const& health)
    -> std::unique_ptr<Action> {
  // Check whether there are enough participants to reach a quorum
  if (plan.participantsConfig.participants.size() + 1 <=
      plan.currentTerm->config.writeConcern) {
    return std::make_unique<ImpossibleCampaignAction>(
        /* TODO: should we have an error message? */);
  }

  TRI_ASSERT(plan.participantsConfig.participants.size() + 1 >
             plan.currentTerm->config.writeConcern);

  auto const requiredNumberOfOKParticipants =
      plan.participantsConfig.participants.size() + 1 -
      plan.currentTerm->config.writeConcern;

  // Find the participants that are healthy and that have the best LogTerm
  auto const campaign =
      runElectionCampaign(current.localState, health, plan.currentTerm->term);

  auto const numElectible = campaign.electibleLeaderSet.size();

  // Something went really wrong: we have enough ok participants, but none
  // of them is electible, or too many of them are (we only support
  // uint16_t::max participants at the moment)
  //
  // TODO: should this really be throwing or just erroring?
  if (ADB_UNLIKELY(numElectible == 0 ||
                   numElectible > std::numeric_limits<uint16_t>::max())) {
    abortOrThrow(TRI_ERROR_NUMERIC_OVERFLOW,
                 basics::StringUtils::concatT(
                     "Number of participants electible for leadership out "
                     "of range, should be between ",
                     1, " and ", std::numeric_limits<uint16_t>::max(),
                     ", but is ", numElectible),
                 ADB_HERE);
  }

  if (campaign.numberOKParticipants >= requiredNumberOfOKParticipants) {
    // We randomly elect on of the electible leaders
    auto const maxIdx = static_cast<uint16_t>(numElectible - 1);
    auto const& newLeader =
        campaign.electibleLeaderSet.at(RandomGenerator::interval(maxIdx));
    auto const& newLeaderRebootId = health._health.at(newLeader).rebootId;

    auto action = std::make_unique<SuccessfulLeaderElectionAction>();

    action->_campaign = campaign;
    action->_newTerm = LogPlanTermSpecification(
        LogTerm{plan.currentTerm->term.value + 1}, plan.currentTerm->config,
        LogPlanTermSpecification::Leader{.serverId = newLeader,
                                         .rebootId = newLeaderRebootId});
    action->_newLeader = newLeader;

    return action;
  } else {
    // Not enough participants were available to form a quorum, so
    // we can't elect a leader
    auto action = std::make_unique<FailedLeaderElectionAction>();
    action->_campaign = campaign;
    return action;
  }
}

auto checkLeaderPresent(LogPlanSpecification const& plan,
                        LogCurrent const& current,
                        ParticipantsHealth const& health)
    -> std::unique_ptr<Action> {
  // currentTerm has no leader
  if (!plan.currentTerm->leader.has_value()) {
    return tryLeadershipElection(plan, current, health);
  } else {
    return std::make_unique<EmptyAction>();
  }
}

auto checkLogTargetParticipantFlags(LogTarget const& target,
                                    LogPlanSpecification const& plan)
    -> std::unique_ptr<Action> {
  auto const& tps = target.participants;
  auto const& pps = plan.participantsConfig.participants;

  for (auto const& [targetParticipant, targetFlags] : tps) {
    if (auto const& planParticipant = pps.find(targetParticipant);
        planParticipant != pps.end()) {
      // participant is in plan, check whether flags are the same
      if (targetFlags != planParticipant->second) {
        // Flags changed, so we need to commit new flags for this participant
        return std::make_unique<UpdateParticipantFlagsAction>(targetParticipant,
                                                              targetFlags);
      };
    }
  }
  // nothing changed, nothing to do
  return std::make_unique<EmptyAction>();
}

auto checkLogTargetParticipantAdded(LogTarget const& target,
                                    LogPlanSpecification const& plan)
    -> std::unique_ptr<Action> {
  auto tps = target.participants;
  auto pps = plan.participantsConfig.participants;

  // is adding a participant or updating flags somehow the same action?
  for (auto const& [targetParticipant, targetFlags] : tps) {
    if (auto const& planParticipant = pps.find(targetParticipant);
        planParticipant == pps.end()) {
      // Here's a participant that is not in plan yet; we add it
      return std::make_unique<AddParticipantToPlanAction>(targetParticipant,
                                                          targetFlags);
    }
  }
  return std::make_unique<EmptyAction>();
}

auto checkLogTargetParticipantRemoved(LogTarget const& target,
                                      LogPlanSpecification const& plan)
    -> std::unique_ptr<Action> {
  auto tps = target.participants;
  auto pps = plan.participantsConfig.participants;
  //
  // Check whether a participant has been removed
  for (auto const& [planParticipant, _] : pps) {
    if (!tps.contains(planParticipant)) {
      return std::make_unique<RemoveParticipantFromPlanAction>(planParticipant);
    }
  }
  return std::make_unique<EmptyAction>();
}

// Check whether the Target configuration differs from Plan
// and do a validity check on the new config
auto checkLogTargetConfig(LogTarget const& target,
                          LogPlanSpecification const& plan)
    -> std::unique_ptr<Action> {
  if (target.config != plan.targetConfig) {
    // Validity check on config?
    return std::make_unique<UpdateLogConfigAction>(target.config);
  }
  return std::make_unique<EmptyAction>();
}

auto isEmptyAction(std::unique_ptr<Action>& action) {
  return (action == nullptr) or
         (action->type() == Action::ActionType::EmptyAction);
}

// The main function
auto checkReplicatedLog(Log const& log, ParticipantsHealth const& health)
    -> std::unique_ptr<Action> {
  // check whether this log exists in plan;
  // If it doesn't the action is to create the log

  if (auto action = checkLogAdded(log); !isEmptyAction(action)) {
    return action;
  }

  // TODO do we access current here? if so, check it must be present at this
  // point
  // TODO: maybe we should report an error here; we won't make any progress,
  // but also don't implode
  TRI_ASSERT(log.plan.has_value());

  /*
  if (!log.current.has_value()) {
    return std::make_unique<EmptyAction>();
  }*/

  if (auto action = checkTermPresent(*log.plan); !isEmptyAction(action)) {
    return action;
  }

  if (auto action = checkLeaderPresent(*log.plan, *log.current, health);
      !isEmptyAction(action)) {
    return action;
  }
  //
  // TODO: maybe we should report an error here; we won't make any progress,
  // but also don't implode
  TRI_ASSERT(log.plan->currentTerm->leader);

  // If the leader is unhealthy, we need to create a new term
  // in the next round we should be electing a new leader above
  if (auto action = checkLeaderHealth(*log.plan, health);
      !isEmptyAction(action)) {
    return action;
  }

  if (auto action = checkLogTargetParticipantFlags(log.target, *log.plan);
      !isEmptyAction(action)) {
    return action;
  }

  if (auto action = checkLogTargetParticipantAdded(log.target, *log.plan);
      !isEmptyAction(action)) {
    return action;
  }

  if (auto action = checkLogTargetParticipantRemoved(log.target, *log.plan);
      !isEmptyAction(action)) {
    return action;
  }

  if (auto action = checkLogTargetConfig(log.target, *log.plan);
      !isEmptyAction(action)) {
    return action;
  }

  // Nothing todo
  return std::make_unique<EmptyAction>();
}

}  // namespace arangodb::replication2::replicated_log
