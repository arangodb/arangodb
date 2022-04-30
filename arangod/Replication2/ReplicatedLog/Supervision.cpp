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

#include "Agency/AgencyPaths.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/TimeString.h"
#include "Random/RandomGenerator.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"
#include "Replication2/ReplicatedLog/SupervisionContext.h"

#include "Logger/LogMacros.h"

using namespace arangodb::replication2::agency;

namespace arangodb::replication2::replicated_log {

auto isConfigurationCommitted(Log const& log) -> bool {
  if (!log.plan) {
    return false;
  }
  auto const& plan = *log.plan;

  if (!log.current) {
    return false;
  }
  auto const& current = *log.current;

  return current.leader && current.leader->committedParticipantsConfig &&
         current.leader->committedParticipantsConfig->generation ==
             plan.participantsConfig.generation;
}

auto hasCurrentTermWithLeader(Log const& log) -> bool {
  if (!log.plan) {
    return false;
  }
  auto const& plan = *log.plan;

  return plan.currentTerm and plan.currentTerm->leader;
}

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

// Check whether Target contains an entry for a leader, which means
// that the user would like a particular participant to be leader;

auto computeReason(std::optional<LogCurrentLocalState> const& maybeStatus,
                   bool healthy, bool excluded, LogTerm term)
    -> LogCurrentSupervisionElection::ErrorCode {
  if (!healthy) {
    return LogCurrentSupervisionElection::ErrorCode::SERVER_NOT_GOOD;
  } else if (excluded) {
    return LogCurrentSupervisionElection::ErrorCode::SERVER_EXCLUDED;
  } else if (!maybeStatus or term != maybeStatus->term) {
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

  for (auto const& [participant, flags] : participantsConfig.participants) {
    auto const excluded = not flags.allowedAsLeader;
    auto const healthy = health.notIsFailed(participant);

    auto maybeStatus = std::invoke(
        [&states](ParticipantId const& participant)
            -> std::optional<LogCurrentLocalState> {
          auto status = states.find(participant);
          if (status != states.end()) {
            return status->second;
          } else {
            return std::nullopt;
          }
        },
        participant);

    auto reason = computeReason(maybeStatus, healthy, excluded, term);
    election.detail.emplace(participant, reason);

    if (reason == LogCurrentSupervisionElection::ErrorCode::OK) {
      election.participantsAvailable += 1;

      if (maybeStatus->spearhead >= election.bestTermIndex) {
        if (maybeStatus->spearhead != election.bestTermIndex) {
          election.electibleLeaderSet.clear();
        }
        election.electibleLeaderSet.push_back(participant);
        election.bestTermIndex = maybeStatus->spearhead;
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
auto checkLeaderPresent(SupervisionContext& ctx, Log const& log,
                        ParticipantsHealth const& health) -> void {
  TRI_ASSERT(log.plan.has_value());
  auto const& plan = *log.plan;

  TRI_ASSERT(plan.currentTerm.has_value());
  auto const& currentTerm = *plan.currentTerm;

  TRI_ASSERT(log.current.has_value());
  auto const& current = *log.current;

  if (currentTerm.leader) {
    return;
  }

  // Check whether there are enough participants to reach a quorum
  if (plan.participantsConfig.participants.size() + 1 <=
      plan.currentTerm->config.writeConcern) {
    ctx.reportStatus(
        LogCurrentSupervision::StatusCode::kLeaderElectionImpossible,
        std::nullopt);
    return;
  }

  TRI_ASSERT(plan.participantsConfig.participants.size() + 1 >
             plan.currentTerm->config.writeConcern);

  auto const requiredNumberOfOKParticipants =
      plan.participantsConfig.participants.size() + 1 -
      plan.currentTerm->config.writeConcern;

  // Find the participants that are healthy and that have the best LogTerm
  auto election = runElectionCampaign(
      current.localState, plan.participantsConfig, health, currentTerm.term);
  election.participantsRequired = requiredNumberOfOKParticipants;

  auto const numElectible = election.electibleLeaderSet.size();

  if (numElectible == 0 ||
      numElectible > std::numeric_limits<uint16_t>::max()) {
    // TODO: should this be a report?
    ctx.createAction<LeaderElectionOutOfBoundsAction>(election);
    return;
  }

  if (election.participantsAvailable >= requiredNumberOfOKParticipants) {
    // We randomly elect on of the electible leaders
    auto const maxIdx = static_cast<uint16_t>(numElectible - 1);
    auto const& newLeader =
        election.electibleLeaderSet.at(RandomGenerator::interval(maxIdx));
    auto const& newLeaderRebootId = health.getRebootId(newLeader);

    if (newLeaderRebootId.has_value()) {
      ctx.createAction<LeaderElectionAction>(
          LogPlanTermSpecification::Leader(newLeader, *newLeaderRebootId),
          election);
      return;
    } else {
      // TODO: better error
      //       return LeaderElectionImpossibleAction();
      return;
    }
  } else {
    // Not enough participants were available to form a quorum, so
    // we can't elect a leader
    // return LeaderElectionQuorumNotReachedAction{election};
    return;
  }
}

auto checkLeaderHealthy(SupervisionContext& ctx, Log const& log,
                        ParticipantsHealth const& health) -> void {
  if (!log.plan.has_value()) {
    return;
  }
  TRI_ASSERT(log.plan.has_value());
  auto const plan = *log.plan;

  if (!log.current.has_value()) {
    return;
  }
  TRI_ASSERT(log.current.has_value());
  auto const current = *log.current;

  if (!plan.currentTerm.has_value()) {
    return;
  }
  TRI_ASSERT(plan.currentTerm.has_value());
  auto const& currentTerm = *plan.currentTerm;

  if (!currentTerm.leader.has_value()) {
    return;
  }
  TRI_ASSERT(currentTerm.leader.has_value());
  auto const& leader = *currentTerm.leader;

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
    ctx.createAction<WriteEmptyTermAction>(minTerm);
  }
}

auto checkLeaderRemovedFromTargetParticipants(SupervisionContext& ctx,
                                              Log const& log,
                                              ParticipantsHealth const& health)
    -> void {
  auto const& target = log.target;

  if (!log.plan.has_value()) {
    return;
  }
  TRI_ASSERT(log.plan.has_value());
  auto const& plan = *log.plan;

  if (!plan.currentTerm.has_value()) {
    return;
  }
  TRI_ASSERT(plan.currentTerm.has_value());
  auto const& currentTerm = *plan.currentTerm;

  if (!currentTerm.leader.has_value()) {
    return;
  }
  TRI_ASSERT(currentTerm.leader.has_value());
  auto const& leader = *currentTerm.leader;

  if (!log.current.has_value()) {
    return;
  }
  TRI_ASSERT(log.current.has_value());
  auto const& current = *log.current;

  if (!isConfigurationCommitted(log)) {
    ctx.reportStatus(
        LogCurrentSupervision::StatusCode::kWaitingForConfigCommitted,
        std::nullopt);
    ctx.createAction<NoActionPossibleAction>();
  }
  auto const& committedParticipants =
      current.leader->committedParticipantsConfig->participants;

  if (!target.participants.contains(leader.serverId)) {
    auto const acceptableLeaderSet = getParticipantsAcceptableAsLeaders(
        current.leader->serverId,
        current.leader->committedParticipantsConfig->participants);

    //  Check whether we already have a participant that is
    //  acceptable and forced
    //
    //  if so, make them leader
    for (auto const& participant : acceptableLeaderSet) {
      auto const& flags = committedParticipants.at(participant);

      if (participant != current.leader->serverId and flags.forced) {
        auto const& rebootId = health.getRebootId(participant);
        if (rebootId.has_value()) {
          ctx.createAction<SwitchLeaderAction>(
              LogPlanTermSpecification::Leader{participant, *rebootId});
          return;
        } else {
          ctx.reportStatus(
              LogCurrentSupervision::StatusCode::kDictateLeaderFailed,
              participant);
        }
      }
    }

    // Did not find a participant above, so pick one at random
    // and force it.
    auto const numElectible = acceptableLeaderSet.size();
    if (numElectible > 0) {
      auto const maxIdx = static_cast<uint16_t>(numElectible - 1);
      auto const& chosenOne =
          acceptableLeaderSet.at(RandomGenerator::interval(maxIdx));

      auto flags = committedParticipants.at(chosenOne);

      flags.forced = true;

      ctx.createAction<UpdateParticipantFlagsAction>(chosenOne, flags);
    } else {
      // We did not have a selectable leader
      ctx.reportStatus(LogCurrentSupervision::StatusCode::kDictateLeaderFailed,
                       std::nullopt);
    }
  }
}

auto checkLeaderSetInTarget(SupervisionContext& ctx, Log const& log,
                            ParticipantsHealth const& health) -> void {
  auto const& target = log.target;

  TRI_ASSERT(log.plan.has_value());
  auto const& plan = *log.plan;

  if (target.leader.has_value()) {
    // The leader set in target is not valid
    if (!plan.participantsConfig.participants.contains(*target.leader)) {
      ctx.reportStatus(LogCurrentSupervision::StatusCode::kTargetLeaderInvalid,
                       *target.leader);
    }

    if (!health.notIsFailed(*target.leader)) {
      ctx.reportStatus(LogCurrentSupervision::StatusCode::kTargetLeaderFailed,
                       *target.leader);
    };

    if (!isConfigurationCommitted(log)) {
      ctx.createAction<NoActionPossibleAction>();
      ctx.reportStatus(
          LogCurrentSupervision::StatusCode::kWaitingForConfigCommitted,
          std::nullopt);
      return;
    }

    if (hasCurrentTermWithLeader(log) and
        target.leader != plan.currentTerm->leader->serverId) {
      auto const& planLeaderConfig =
          plan.participantsConfig.participants.at(*target.leader);

      if (planLeaderConfig.forced != true ||
          !planLeaderConfig.allowedAsLeader) {
        ctx.reportStatus(
            LogCurrentSupervision::StatusCode::kTargetLeaderExcluded,
            *target.leader);
      }

      auto const& rebootId = health.getRebootId(*target.leader);
      if (rebootId.has_value()) {
        ctx.createAction<SwitchLeaderAction>(
            LogPlanTermSpecification::Leader{*target.leader, *rebootId});
      } else {
        ctx.reportStatus(
            LogCurrentSupervision::StatusCode::kTargetLeaderInvalid,
            *target.leader);
      }
    }
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

// If there is a participant such that the flags between Target and Plan
// differ, returns at a pair consisting of the ParticipantId and the desired
// flags.
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
    if (flags.allowedAsLeader && health.contains(part)) {
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
    auto const& rebootId = health.getRebootId(*leaderId);
    if (rebootId.has_value()) {
      return LogPlanTermSpecification::Leader{*leaderId, *rebootId};
    }
  };

  return std::nullopt;
}

auto checkLogExists(SupervisionContext& ctx, Log const& log,
                    ParticipantsHealth const& health) -> void {
  auto const& target = log.target;
  if (!log.plan) {
    // The log is not planned right now, so we create it
    // provided we have enough participants
    if (target.participants.size() + 1 < target.config.writeConcern) {
      ctx.reportStatus(
          LogCurrentSupervision::StatusCode::kTargetNotEnoughParticipants,
          std::nullopt);
    } else {
      auto leader = pickLeader(target.leader, target.participants, health);
      ctx.createAction<AddLogToPlanAction>(target.id, target.participants,
                                           target.config, leader);
    }
  }
}

auto checkCurrentExists(SupervisionContext& ctx, Log const& log) -> void {
  // If the Current subtree does not exist yet, create it by writing
  // a message into it.
  if (!log.current) {
    ctx.createAction<CurrentNotAvailableAction>();
  }
}

auto checkLeader(SupervisionContext& ctx, Log const& log,
                 ParticipantsHealth const& health) -> void {
  if (!log.plan.has_value() or !log.current.has_value()) {
    return;
  }
  TRI_ASSERT(log.plan.has_value());
  TRI_ASSERT(log.current.has_value());

  // If currentTerm's leader entry does not have a value,
  // run a leadership election. The doLeadershipElection can
  // return different Actions, dependking on whether a leadership
  // election is possible, and if so, whether there is enough
  // eligible participants for leadership.
  checkLeaderPresent(ctx, log, health);

  // If the leader is unhealthy, write a new term that
  // does not have a leader.
  // In the next round this will lead to a leadership election.
  checkLeaderHealthy(ctx, log, health);

  // If the participant who is leader has been removed from target,
  // gracefully remove it by selecting a different eligible participant
  // as leader
  //
  // At this point there should only ever be precisely one participant to
  // remove (the current leader); Once it is not the leader anymore it will be
  // disallowed from any quorum above.
  checkLeaderRemovedFromTargetParticipants(ctx, log, health);

  // Check whether a specific participant is configured in Target to become
  // the leader. This requires that participant to be flagged to always be
  // part of a quorum; once that change is committed, the leader can be
  // switched if the target.leader participant is healty.
  //
  // This operation can fail and
  // TODO: Report if leaderInTarget fails.
  checkLeaderSetInTarget(ctx, log, health);
}

auto checkParticipantToAdd(SupervisionContext& ctx, Log const& log,
                           ParticipantsHealth const& health) -> void {
  auto const& target = log.target;

  if (!log.plan.has_value()) {
    return;
  }
  TRI_ASSERT(log.plan.has_value());
  auto const& plan = *log.plan;
  if (auto participant = getAddedParticipant(
          target.participants, plan.participantsConfig.participants)) {
    ctx.createAction<AddParticipantToPlanAction>(participant->first,
                                                 participant->second);
  }
}

auto checkParticipantToRemove(SupervisionContext& ctx, Log const& log,
                              ParticipantsHealth const& health) -> void {
  auto const& target = log.target;

  if (!log.plan.has_value()) {
    return;
  }
  TRI_ASSERT(log.plan.has_value());
  auto const& plan = *log.plan;

  if (!log.current.has_value() || !log.current->leader.has_value()) {
    return;
  };
  TRI_ASSERT(log.current.has_value());
  TRI_ASSERT(log.current->leader.has_value());
  auto const& leader = *log.current->leader;

  if (!leader.committedParticipantsConfig.has_value()) {
    return;
  }
  TRI_ASSERT(leader.committedParticipantsConfig.has_value());
  auto const& committedParticipantsConfig = *leader.committedParticipantsConfig;

  if (auto maybeParticipant = getRemovedParticipant(
          target.participants, plan.participantsConfig.participants)) {
    auto const& [participantId, planFlags] = *maybeParticipant;

    // We do not ever remove a leader
    if (participantId != leader.serverId) {
      // If the participant is not allowed in Quorum it is safe to remove it
      if (not planFlags.allowedInQuorum and
          committedParticipantsConfig.generation ==
              plan.participantsConfig.generation) {
        ctx.createAction<RemoveParticipantFromPlanAction>(participantId);
      } else if (planFlags.allowedInQuorum) {
        // A participant can only be removed without risk,
        // if it is not member of any quorum
        auto newFlags = planFlags;
        newFlags.allowedInQuorum = false;
        ctx.createAction<UpdateParticipantFlagsAction>(participantId, newFlags);
      } else {
        // still waiting
        ctx.reportStatus(
            LogCurrentSupervision::StatusCode::kWaitingForConfigCommitted,
            participantId);
      }
    }
  }
}

auto checkParticipantFlagsUpdated(SupervisionContext& ctx, Log const& log,
                                  ParticipantsHealth const& health) -> void {
  auto const& target = log.target;

  if (!log.plan.has_value()) {
    return;
  }
  TRI_ASSERT(log.plan.has_value());
  auto const& plan = *log.plan;

  if (!log.current.has_value() || !log.current->leader.has_value()) {
    return;
  }
  TRI_ASSERT(log.current.has_value());
  TRI_ASSERT(log.current->leader.has_value());
  auto const& leader = *log.current->leader;

  //       This function currently forces a participant if it is set
  //       as desired leader in Target, and this isn't obvious at all.
  //       This should be moved to a separate action that overrides that
  //       particular field for the desired leader.
  if (auto participantFlags = getParticipantWithUpdatedFlags(
          target.participants, plan.participantsConfig.participants,
          target.leader, leader.serverId)) {
    ctx.createAction<UpdateParticipantFlagsAction>(participantFlags->first,
                                                   participantFlags->second);
  }
}

auto checkConfigUpdated(SupervisionContext& ctx, Log const& log,
                        ParticipantsHealth const& health) -> void {
  auto const& target = log.target;

  if (!log.plan.has_value()) {
    return;
  }
  TRI_ASSERT(log.plan.has_value());
  auto const& plan = *log.plan;

  if (!plan.currentTerm.has_value()) {
    return;
  }
  TRI_ASSERT(plan.currentTerm.has_value());
  auto const& currentTerm = *plan.currentTerm;

  if (target.config != currentTerm.config) {
    ctx.reportStatus(
        LogCurrentSupervision::StatusCode::kConfigChangeNotImplemented,
        std::nullopt);
  }
}

auto checkConverged(SupervisionContext& ctx, Log const& log) {
  auto const& target = log.target;

  if (!log.current.has_value()) {
    return;
  }
  TRI_ASSERT(log.current.has_value());
  auto const& current = *log.current;

  if (target.version.has_value() &&
      (!current.supervision.has_value() ||
       target.version != current.supervision->targetVersion)) {
    ctx.createAction<ConvergedToTargetAction>(target.version);
  }
}
//
// This function is called from Agency/Supervision.cpp every k seconds for
// every replicated log in every database.
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
auto checkReplicatedLog(SupervisionContext& ctx, Log const& log,
                        ParticipantsHealth const& health) -> void {
  // Check whether the log (that exists in target by virtue of
  // checkReplicatedLog being called here) is planned. If not, then create it,
  // provided we have enough participants If there are not enough participants
  // we can only report back that this log cannot be created.
  checkLogExists(ctx, log, health);

  checkCurrentExists(ctx, log);

  // Check whether the leader is healthy and all
  checkLeader(ctx, log, health);

  // Check whether a participant was added in Target that is not in Plan.
  // If so, add it to Plan.
  checkParticipantToAdd(ctx, log, health);

  // If a participant is in Plan but not in Target, gracefully
  // remove it
  checkParticipantToRemove(ctx, log, health);

  // If the user has updated flags for a participant, which is detected by
  // comparing Target to Plan, write that change to Plan.
  // If the configuration differs between Target and Plan,
  // apply the new configuration.
  checkParticipantFlagsUpdated(ctx, log, health);

  // TODO: This has not been implemented yet!
  checkConfigUpdated(ctx, log, health);

  // Check whether we have converged, and if so, report and set version
  // to target version
  checkConverged(ctx, log);
}

// TODO: remove once refactored
using namespace arangodb::cluster::paths;
auto executeCheckReplicatedLog(DatabaseID const& dbName,
                               std::string const& idString, Log log,
                               ParticipantsHealth const& health,
                               arangodb::agency::envelope envelope) noexcept
    -> arangodb::agency::envelope {
  SupervisionContext ctx;

  checkReplicatedLog(ctx, log, health);

  if (ctx.hasAction()) {
    envelope = arangodb::replication2::replicated_log::execute(
        ctx.getAction(), dbName, log.target.id, log.plan, log.current,
        std::move(envelope));
  }
  return envelope;
}

}  // namespace arangodb::replication2::replicated_log
