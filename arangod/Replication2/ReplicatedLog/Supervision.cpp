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

using namespace arangodb::replication2::agency;

namespace arangodb::replication2::replicated_log {

auto checkLogAdded(const Log& log) -> std::unique_ptr<Action> {
  if (!log.plan) {
    // action that creates Plan entry for log
    return nullptr;
  }
  return nullptr;
}

auto checkLeaderHealth(LogPlanSpecification const& plan,
                       ParticipantsHealth const& health)
    -> std::unique_ptr<Action> {
  if (health.isHealthy(plan.currentTerm->leader->serverId) &&
      health.validRebootId(plan.currentTerm->leader->serverId,
                           plan.currentTerm->leader->rebootId)) {
    // Current leader is all healthy so nothing to do.
    return nullptr;
  } else {
    // Leader is not healthy; start a new term
    auto newTerm = *plan.currentTerm;

    newTerm.leader.reset();
    newTerm.term = LogTerm{plan.currentTerm->term.value + 1};

    return std::make_unique<UpdateTermAction>(newTerm);
  }
}

using LogCurrentLocalStates =
    std::unordered_map<ParticipantId, LogCurrentLocalState>;
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
                                         .rebootId = newLeaderRebootId},
        plan.currentTerm->participants);
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
        return nullptr;
      };
    }
  }
  // nothing changed, nothing to do
  return nullptr;
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
      return nullptr;
    }
  }
  return nullptr;
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
      return nullptr;
    }
  }
  return nullptr;
}

// Check whether the Target configuration differs from Plan
// and do a validity check on the new config
auto checkLogTargetConfig(LogTarget const& target,
                          LogPlanSpecification const& plan)
    -> std::unique_ptr<Action> {
  if (target.config != plan.targetConfig) {
    // Validity check on config?
    return nullptr;
  }
  return nullptr;
}

// The main function
auto checkReplicatedLog(Log const& log, ParticipantsHealth const& health)
    -> std::unique_ptr<Action> {
  return nullptr;
}

}  // namespace arangodb::replication2::replicated_log
