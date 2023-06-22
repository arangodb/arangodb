////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "Supervision.h"

#include <algorithm>
#include <cstddef>
#include <memory>

#include "Agency/AgencyPaths.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/TimeString.h"
#include "Cluster/ClusterTypes.h"
#include "Inspection/VPack.h"
#include "Random/RandomGenerator.h"
#include "Replication2/AgencyCollectionSpecification.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"
#include "Replication2/ReplicatedLog/SupervisionContext.h"

#include "Logger/LogMacros.h"

namespace paths = arangodb::cluster::paths::aliases;
using namespace arangodb::replication2::agency;

namespace arangodb::replication2::replicated_log {

namespace {

/// The snapshot is valid if it is available and the term matches. We could have
/// also conditioned this on the state being operational, but that cannot happen
/// unless the follower gets an append-entries request.
bool isSnapshotValidInTerm(LogCurrentLocalState const& state, LogTerm term) {
  return state.snapshotAvailable && state.term == term;
}

/// Conditions
/// - server is healthy
/// - server has a valid snapshot in the current term
bool isParticipantUsable(LogCurrent const& current,
                         std::optional<LogPlanTermSpecification> currentTerm,
                         ParticipantsHealth const& health,
                         ParticipantId const& participantId) {
  if (not health.notIsFailed(participantId)) {
    // server is not healthy
    return false;
  }

  auto local = current.localState.find(participantId);
  if (local == current.localState.end()) {
    // server is not in current
    return false;
  }

  if (not currentTerm.has_value()) {
    // no term in plan, just check if a snapshot is available
    return local->second.snapshotAvailable;
  }

  return isSnapshotValidInTerm(local->second, currentTerm->term);
}
}  // namespace

/// Computes the number of usable participants, i.e. those which are not failed
/// and have a snapshot.
/// \return Number of usable participants.
auto computeNumUsableParticipants(
    LogCurrent const& current,
    std::optional<LogPlanTermSpecification> currentTerm,
    std::vector<ParticipantId> const& participants,
    ParticipantsHealth const& health) -> std::size_t {
  auto count = std::count_if(
      participants.cbegin(), participants.cend(), [&](auto const& pid) {
        return isParticipantUsable(current, currentTerm, health, pid);
      });
  TRI_ASSERT(count >= 0);
  return count;
}

/// We rely only on health information, as current is not available.
/// You may want to use this function when the log is in the process of being
/// created.
auto computeEffectiveWriteConcern(LogTargetConfig const& config,
                                  ParticipantsFlagsMap const& participants,
                                  ParticipantsHealth const& health) -> size_t {
  auto const numberNotFailedParticipants =
      health.numberNotIsFailedOf(participants);
  return std::max(config.writeConcern, std::min(numberNotFailedParticipants,
                                                config.softWriteConcern));
}

/// After a log has been created, we have to take into account the number of
/// usable participants, as it is no longer sufficient for a participant to be
/// healthy.
auto computeEffectiveWriteConcern(LogTargetConfig const& config,
                                  LogCurrent const& current,
                                  LogPlanSpecification const& plan,
                                  ParticipantsHealth const& health) -> size_t {
  std::vector<ParticipantId> participants;
  participants.reserve(plan.participantsConfig.participants.size());
  for (auto const& [p, _] : plan.participantsConfig.participants) {
    participants.emplace_back(p);
  }

  auto const numUsableParticipants = computeNumUsableParticipants(
      current, plan.currentTerm, participants, health);

  return std::max(config.writeConcern,
                  std::min(numUsableParticipants, config.softWriteConcern));
}

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

// Leader has failed if it is marked as failed or its rebootId is
// different from what is expected
auto isLeaderFailed(ServerInstanceReference const& leader,
                    ParticipantsHealth const& health) -> bool {
  // TODO: less obscure with fewer negations
  // TODO: write test first
  if (health.notIsFailed(leader.serverId) and
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
//
// TODO: should this have some kind of preference?
//       consider the case where all participants are replaced; ideally
//       leadership should be handed to a participant that is in target
//       Yet, is there a case where it is necessary to hand leadership to
//       an otherwise healthy participant that is not in target anymore?
auto getParticipantsAcceptableAsLeaders(
    ParticipantId const& currentLeader, LogTerm term,
    ParticipantsFlagsMap const& participants,
    std::unordered_map<ParticipantId, LogCurrentLocalState> const& localStates)
    -> std::vector<ParticipantId> {
  // A participant is acceptable if it is neither excluded nor
  // already the leader
  auto acceptableLeaderSet = std::vector<ParticipantId>{};
  for (auto const& [participant, flags] : participants) {
    if (participant == currentLeader or not flags.allowedAsLeader) {
      continue;
    }
    // The participant should be operation and have a snapshot valid in the
    // current term.
    auto iter = localStates.find(participant);
    if (iter == localStates.end() or
        not isSnapshotValidInTerm(iter->second, term)) {
      continue;
    }
    acceptableLeaderSet.emplace_back(participant);
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
  } else if (not maybeStatus->snapshotAvailable) {
    return LogCurrentSupervisionElection::ErrorCode::SNAPSHOT_MISSING;
  } else {
    return LogCurrentSupervisionElection::ErrorCode::OK;
  }
}

// Report whether a server is clean, meaning it hasn't lost any data since the
// last commit. This is allowed to have false negatives (i.e. not report a
// server as clean, which actually is clean), but not to have false positives
// (i.e. all servers reported as clean really must be).
// For waitForSync=true, all servers are always clean.
// False negatives may inhibit leader election and thus stall the log, until
// either all servers report back or the unclean server(s) are replaced.
auto ICleanOracle::serverIsClean(ServerInstanceReference const& participant,
                                 bool const assumedWaitForSync) -> bool {
  if (assumedWaitForSync) {
    return true;
  } else {
    return serverIsCleanWfsFalse(participant);
  }
}

CleanOracle::CleanOracle(
    std::unordered_map<ParticipantId, RebootId> const* const safeRebootIds)
    : _safeRebootIds(*safeRebootIds) {}

auto CleanOracle::serverIsCleanWfsFalse(
    ServerInstanceReference const& serverInstance) -> bool {
  // Trivial implementation. It is safe, but maximally pessimistic.
  // To be improved later: see
  // https://arangodb.atlassian.net/wiki/spaces/CInfra/pages/2061369370/Concept+Conservative+leader+election
  // for details.
  if (auto it = _safeRebootIds.find(serverInstance.serverId);
      it != _safeRebootIds.end()) {
    return it->second == serverInstance.rebootId;
  }
  return false;
}

auto runElectionCampaign(LogCurrentLocalStates const& states,
                         ParticipantsConfig const& participantsConfig,
                         ParticipantsHealth const& health, LogTerm const term,
                         bool const assumedWaitForSync, ICleanOracle& mrProper)
    -> LogCurrentSupervisionElection {
  auto election = LogCurrentSupervisionElection();
  election.term = term;
  auto const getMaybeStatus =
      [&states](ParticipantId const& p) -> std::optional<LogCurrentLocalState> {
    auto status = states.find(p);
    if (status != states.end()) {
      return status->second;
    } else {
      return std::nullopt;
    }
  };

  auto const participantsAttending = std::size_t(
      std::count_if(participantsConfig.participants.begin(),
                    participantsConfig.participants.end(), [&](auto const& it) {
                      auto const& participantId = it.first;
                      if (auto status = getMaybeStatus(participantId); status) {
                        return status->term == term;
                      } else {
                        return false;
                      }
                    }));

  bool const allParticipantsAttendingElection =
      std::all_of(participantsConfig.participants.begin(),
                  participantsConfig.participants.end(), [&](auto const& it) {
                    auto const& participantId = it.first;
                    if (auto status = getMaybeStatus(participantId); status) {
                      return status->term == term;
                    } else {
                      return false;
                    }
                  });
  TRI_ASSERT(
      (participantsAttending == participantsConfig.participants.size()) ==
      allParticipantsAttendingElection);
  election.allParticipantsAttending = allParticipantsAttendingElection;
  election.participantsAttending = participantsAttending;

  for (auto const& [participant, flags] : participantsConfig.participants) {
    auto const excluded = not flags.allowedAsLeader;
    auto const healthy = health.notIsFailed(participant);

    auto maybeStatus = getMaybeStatus(participant);

    auto reason = computeReason(maybeStatus, healthy, excluded, term);
    election.detail.emplace(participant, reason);

    if (reason == LogCurrentSupervisionElection::ErrorCode::OK) {
      TRI_ASSERT(maybeStatus.has_value());
      auto const isClean = mrProper.serverIsClean(
          {participant, maybeStatus->rebootId}, assumedWaitForSync);
      // Servers that aren't clean can still be electible, but don't count
      // against the quorum size when voting for a leader.
      // With waitForSync=true, servers are always clean.
      // If all participants are attending the election, the election can take
      // place as if all servers were clean. Note that (only) in this situation
      // data might have been lost with waitForSync=false.
      // TODO It might be nice to log a warning in case an election can _only_
      //      take place because all participants are attending, indicating
      //      possible data loss due to waitForSync=false.
      //      But currently, we don't have all necessary information in one
      //      place.
      if (isClean || allParticipantsAttendingElection) {
        election.participantsVoting += 1;
      }

      if (maybeStatus->spearhead >= election.bestTermIndex) {
        if (maybeStatus->spearhead != election.bestTermIndex) {
          election.electibleLeaderSet.clear();
        }
        election.electibleLeaderSet.emplace_back(participant,
                                                 maybeStatus->rebootId);
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
//  * snapshot available
//
auto checkLeaderPresent(SupervisionContext& ctx, Log const& log,
                        ParticipantsHealth const& health) -> void {
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

  if (!log.current.has_value()) {
    return;
  }
  TRI_ASSERT(log.current.has_value());
  auto const& current = *log.current;

  if (!current.supervision.has_value()) {
    return;
  }
  ADB_PROD_ASSERT(current.supervision.has_value());

  if (currentTerm.leader) {
    return;
  }

  // Check whether there are enough participants to reach a quorum
  if (plan.participantsConfig.participants.size() + 1 <=
      current.supervision->assumedWriteConcern) {
    ctx.reportStatus<LogCurrentSupervision::LeaderElectionImpossible>();
    ctx.createAction<NoActionPossibleAction>();
    return;
  }

  ADB_PROD_ASSERT(plan.participantsConfig.participants.size() + 1 >
                  current.supervision->assumedWriteConcern);

  auto const requiredNumberOfOKParticipants =
      plan.participantsConfig.participants.size() + 1 -
      current.supervision->assumedWriteConcern;

  // Find the participants that are healthy and that have the best LogTerm
  auto cleanOracle = CleanOracle(&current.safeRebootIds);
  auto election = runElectionCampaign(
      current.localState, plan.participantsConfig, health, currentTerm.term,
      current.supervision->assumedWaitForSync, cleanOracle);
  election.participantsRequired = requiredNumberOfOKParticipants;

  auto const numElectible = election.electibleLeaderSet.size();

  if (numElectible == 0 ||
      numElectible > std::numeric_limits<uint16_t>::max()) {
    // TODO: enter some detail about numElectible
    ctx.reportStatus<LogCurrentSupervision::LeaderElectionOutOfBounds>();
    ctx.createAction<NoActionPossibleAction>();
    return;
  }

  if (election.participantsVoting >= requiredNumberOfOKParticipants) {
    // We randomly elect one of the electible leaders
    auto const maxIdx = static_cast<uint16_t>(numElectible - 1);
    auto const& newLeader =
        election.electibleLeaderSet.at(RandomGenerator::interval(maxIdx));

    TRI_ASSERT(current.supervision->assumedWriteConcern <=
               plan.participantsConfig.config.effectiveWriteConcern);

    auto effectiveWriteConcern =
        computeEffectiveWriteConcern(log.target.config, current, plan, health);
    auto assumedWriteConcern = std::min(
        current.supervision->assumedWriteConcern, effectiveWriteConcern);
    TRI_ASSERT(assumedWriteConcern <= effectiveWriteConcern);

    ctx.reportStatus<LogCurrentSupervision::LeaderElectionSuccess>(election);
    ctx.createAction<LeaderElectionAction>(newLeader, effectiveWriteConcern,
                                           assumedWriteConcern, election);
    return;

  } else {
    // Not enough participants were available to form a quorum, so
    // we can't elect a leader
    ctx.reportStatus<LogCurrentSupervision::LeaderElectionQuorumNotReached>(
        election);
    ctx.createAction<NoActionPossibleAction>();
    return;
  }
}

auto checkLeaderHealthy(SupervisionContext& ctx, Log const& log,
                        ParticipantsHealth const& health) -> void {
  if (!log.plan.has_value()) {
    return;
  }
  TRI_ASSERT(log.plan.has_value());
  auto const& plan = *log.plan;

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

  // If the leader is unhealthy, write a new term that
  // does not have a leader.
  if (isLeaderFailed(*currentTerm.leader, health)) {
    // Make sure the new term is bigger than any
    // term seen by participants in current
    auto minTerm = currentTerm.term;
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

  if (!log.current->leader.has_value()) {
    return;
  }

  auto const& committedParticipants =
      current.leader->committedParticipantsConfig->participants;

  if (!target.participants.contains(leader.serverId)) {
    if (!isConfigurationCommitted(log)) {
      ctx.reportStatus<LogCurrentSupervision::WaitingForConfigCommitted>();
      ctx.createAction<NoActionPossibleAction>();
      return;
    }

    auto const acceptableLeaderSet = getParticipantsAcceptableAsLeaders(
        leader.serverId, currentTerm.term, committedParticipants,
        current.localState);

    // If there's a new target, we don't want to switch to another server than
    // that to avoid switching the leader too often. Note that this doesn't
    // affect the situation where the current leader is unhealthy, which is
    // handled in checkLeaderHealthy().
    if (target.leader) {
      // Unless the target leader is not permissible as a leader for some
      // reason, we return and wait for checkLeaderSetInTarget() to do its work.
      // Otherwise, we still continue as usual to possibly select some random
      // participant as a follower, in order to make progress.
      auto const& planParticipants = plan.participantsConfig.participants;
      auto const targetLeaderConfigIt = planParticipants.find(*target.leader);
      if (targetLeaderConfigIt != planParticipants.cend() &&
          health.notIsFailed(*target.leader) &&
          targetLeaderConfigIt->second.allowedAsLeader) {
        // Let checkLeaderSetInTarget() do the work instead
        return;
      }
    }

    //  Check whether we already have a participant that is
    //  acceptable and forced
    //
    //  if so, make them leader
    for (auto const& participant : acceptableLeaderSet) {
      // both assertions are guaranteed by getParticipantsAcceptableAsLeaders()
      TRI_ASSERT(committedParticipants.contains(participant));
      TRI_ASSERT(participant != leader.serverId);
      auto const& flags = committedParticipants.at(participant);

      if (flags.forced) {
        auto const& rebootId = health.getRebootId(participant);
        if (rebootId.has_value()) {
          ctx.createAction<SwitchLeaderAction>(
              ServerInstanceReference{participant, *rebootId});

          return;
        } else {
          // TODO: this should get the participant
          ctx.reportStatus<LogCurrentSupervision::SwitchLeaderFailed>();
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

      TRI_ASSERT(committedParticipants.contains(chosenOne));
      auto flags = committedParticipants.at(chosenOne);

      flags.forced = true;

      ctx.createAction<UpdateParticipantFlagsAction>(chosenOne, flags);
    } else {
      // We did not have a selectable leader
      ctx.reportStatus<LogCurrentSupervision::SwitchLeaderFailed>();
    }
  }
}

auto checkLeaderSetInTarget(SupervisionContext& ctx, Log const& log,
                            ParticipantsHealth const& health) -> void {
  auto const& target = log.target;

  if (!log.plan.has_value()) {
    return;
  }
  if (!log.current.has_value()) {
    return;
  }
  TRI_ASSERT(log.plan.has_value());
  TRI_ASSERT(log.current.has_value());
  auto const& plan = *log.plan;

  if (target.leader.has_value()) {
    // The leader set in target is not valid
    if (!plan.participantsConfig.participants.contains(*target.leader)) {
      // TODO: Add detail which leader we find invalid (or even rename this
      // status code to leader not a participant)
      ctx.reportStatus<LogCurrentSupervision::TargetLeaderInvalid>();
      return;
    }

    if (!health.notIsFailed(*target.leader)) {
      ctx.reportStatus<LogCurrentSupervision::TargetLeaderFailed>();
      return;
    }

    if (hasCurrentTermWithLeader(log) and
        target.leader != plan.currentTerm->leader->serverId) {
      TRI_ASSERT(plan.participantsConfig.participants.contains(*target.leader));
      auto const& planLeaderConfig =
          plan.participantsConfig.participants.at(*target.leader);

      {
        auto const& localStates = log.current->localState;
        auto iter = localStates.find(*target.leader);
        if (iter == localStates.end() or
            not isSnapshotValidInTerm(iter->second, plan.currentTerm->term)) {
          ctx.reportStatus<
              LogCurrentSupervision::TargetLeaderSnapshotMissing>();
          return;
        }
      }

      if (!planLeaderConfig.allowedAsLeader) {
        ctx.reportStatus<LogCurrentSupervision::TargetLeaderExcluded>();
        return;
      }

      if (planLeaderConfig.forced != true) {
        auto desiredFlags = planLeaderConfig;
        desiredFlags.forced = true;
        ctx.createAction<UpdateParticipantFlagsAction>(*target.leader,
                                                       desiredFlags);
        return;
      }

      if (!isConfigurationCommitted(log)) {
        ctx.reportStatus<LogCurrentSupervision::WaitingForConfigCommitted>();
        ctx.createAction<NoActionPossibleAction>();
        return;
      }

      auto const& rebootId = health.getRebootId(*target.leader);
      if (rebootId.has_value()) {
        ctx.createAction<SwitchLeaderAction>(
            ServerInstanceReference{*target.leader, *rebootId});
      } else {
        ctx.reportStatus<LogCurrentSupervision::TargetLeaderInvalid>();
      }
    }
  }
}

// Pick leader at random from participants
auto pickRandomParticipantToBeLeader(ParticipantsFlagsMap const& participants,
                                     ParticipantsHealth const& health,
                                     uint64_t logId)
    -> std::optional<ParticipantId> {
  auto acceptableParticipants = std::vector<ParticipantId>{};

  for (auto [part, flags] : participants) {
    if (flags.allowedAsLeader && health.contains(part)) {
      acceptableParticipants.emplace_back(part);
    }
  }

  if (!acceptableParticipants.empty()) {
    auto maxIdx = static_cast<uint16_t>(acceptableParticipants.size());
    auto p = acceptableParticipants.begin();

    std::advance(p, logId % maxIdx);

    return *p;
  }

  return std::nullopt;
}

auto pickLeader(std::optional<ParticipantId> targetLeader,
                ParticipantsFlagsMap const& participants,
                ParticipantsHealth const& health, uint64_t logId)
    -> std::optional<ServerInstanceReference> {
  auto& leaderId = targetLeader;

  if (!leaderId) {
    leaderId = pickRandomParticipantToBeLeader(participants, health, logId);
  }

  if (leaderId.has_value()) {
    auto const& rebootId = health.getRebootId(*leaderId);
    if (rebootId.has_value()) {
      return ServerInstanceReference{*leaderId, *rebootId};
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
      ctx.reportStatus<LogCurrentSupervision::TargetNotEnoughParticipants>();
      ctx.createAction<NoActionPossibleAction>();
    } else {
      auto leader = pickLeader(target.leader, target.participants, health,
                               log.target.id.id());
      auto effectiveWriteConcern = computeEffectiveWriteConcern(
          target.config, target.participants, health);
      auto config =
          LogPlanConfig(effectiveWriteConcern, target.config.waitForSync);
      ctx.createAction<AddLogToPlanAction>(target.id, target.participants,
                                           config, target.properties, leader);
    }
  }
}

auto checkParticipantToAdd(SupervisionContext& ctx, Log const& log,
                           ParticipantsHealth const& health) -> void {
  auto const& target = log.target;

  if (!log.plan.has_value()) {
    return;
  }
  TRI_ASSERT(log.plan.has_value());
  auto const& plan = *log.plan;

  for (auto const& [targetParticipant, targetFlags] : target.participants) {
    if (auto const& planParticipant =
            plan.participantsConfig.participants.find(targetParticipant);
        planParticipant == plan.participantsConfig.participants.end()) {
      ctx.createAction<AddParticipantToPlanAction>(targetParticipant,
                                                   targetFlags);
    }
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

  auto const& targetParticipants = target.participants;
  auto const& planParticipants = plan.participantsConfig.participants;

  if (planParticipants.size() == targetParticipants.size()) {
    return;  // Nothing to do here, because checkParticipantToAdd() runs before.
  }

  auto participantsToRemain = std::vector<ParticipantId>();
  auto participantsToRemove = std::vector<ParticipantId>();
  for (auto const& kv : planParticipants) {
    auto const& participantId = kv.first;
    if (targetParticipants.contains(participantId)) {
      participantsToRemain.emplace_back(participantId);
    } else {
      participantsToRemove.emplace_back(participantId);
    }
  }
  auto const& current = *log.current;

  // check if, after a remove, enough servers are available to form a quorum
  auto const needed = plan.participantsConfig.config.effectiveWriteConcern;
  auto numUsableRemaining = computeNumUsableParticipants(
      current, plan.currentTerm, participantsToRemain, health);

  // If we haven't enough servers in plan that are usable, choose some of the
  // usable ones in the "to remove" set to remain (for now).
  for (auto it = participantsToRemove.begin();
       numUsableRemaining < needed && it != participantsToRemove.end();) {
    auto const& participantId = *it;
    auto const& planFlags = planParticipants.find(participantId)->second;
    // To compensate for `numUsableRemaining < needed`, we select some usable
    // participants to remain, even though they're no longer in target.
    if (planFlags.allowedInQuorum &&
        isParticipantUsable(current, plan.currentTerm, health, participantId)) {
      // we choose to let this participant remain
      participantsToRemain.emplace_back(std::move(*it));
      std::iter_swap(it, participantsToRemove.end() - 1);
      participantsToRemove.pop_back();
      ++numUsableRemaining;
    } else {
      ++it;
    }
  }

  TRI_ASSERT(numUsableRemaining ==
             computeNumUsableParticipants(current, plan.currentTerm,
                                          participantsToRemain, health));

  // if there are not enough participants, make sure we can still commit
  if (needed > numUsableRemaining) {
    // remove all allowedInQuorum=false, when possible
    for (auto const& [pid, flags] : planParticipants) {
      auto shouldBeAllowedInQuorum = [&, &maybeRemovedParticipant = pid] {
        if (auto iter = targetParticipants.find(maybeRemovedParticipant);
            iter != targetParticipants.end()) {
          if (not iter->second.allowedInQuorum) {
            return false;
          }
        }

        return true;
      };

      if (not flags.allowedInQuorum and shouldBeAllowedInQuorum()) {
        // unset the flag for now
        auto newFlags = flags;
        newFlags.allowedInQuorum = true;
        ctx.createAction<UpdateParticipantFlagsAction>(pid, newFlags);
      }
    }

    ctx.reportStatus<LogCurrentSupervision::TargetNotEnoughParticipants>();
    return;
  }

  if (committedParticipantsConfig.generation !=
      plan.participantsConfig.generation) {
    // still waiting
    ctx.reportStatus<LogCurrentSupervision::WaitingForConfigCommitted>();
    ctx.createAction<NoActionPossibleAction>();
    return;
  }

  for (auto const& participantToRemove : participantsToRemove) {
    auto const& [maybeRemovedParticipant, maybeRemovedParticipantFlags] =
        *planParticipants.find(participantToRemove);
    // never remove a leader
    if (!targetParticipants.contains(maybeRemovedParticipant) and
        maybeRemovedParticipant != leader.serverId) {
      // If the participant is not allowed in Quorum it is safe to remove it
      if (not maybeRemovedParticipantFlags.allowedInQuorum) {
        ctx.createAction<RemoveParticipantFromPlanAction>(
            maybeRemovedParticipant);
      } else {
        // A participant can only be removed without risk,
        // if it is not member of any quorum
        auto newFlags = maybeRemovedParticipantFlags;
        newFlags.allowedInQuorum = false;
        ctx.createAction<UpdateParticipantFlagsAction>(maybeRemovedParticipant,
                                                       newFlags);
      }
    }
  }
}

auto checkParticipantWithFlagsToUpdate(SupervisionContext& ctx, Log const& log,
                                       ParticipantsHealth const& health)
    -> void {
  auto const& target = log.target;

  if (!log.plan.has_value()) {
    return;
  }
  TRI_ASSERT(log.plan.has_value());
  auto const& plan = *log.plan;

  for (auto const& [targetParticipant, targetFlags] : target.participants) {
    if (auto const& planParticipant =
            plan.participantsConfig.participants.find(targetParticipant);
        planParticipant != std::end(plan.participantsConfig.participants)) {
      // participant is in plan, check whether flags are the same
      if (targetFlags != planParticipant->second) {
        // Flags changed, so we need to commit new flags for this participant
        ctx.createAction<UpdateParticipantFlagsAction>(targetParticipant,
                                                       targetFlags);
      }
    }
  }
}

auto checkConfigChanged(SupervisionContext& ctx, Log const& log,
                        ParticipantsHealth const& health) -> void {
  if (!log.plan.has_value() || !log.current.has_value()) {
    return;
  }
  ADB_PROD_ASSERT(log.plan.has_value());
  ADB_PROD_ASSERT(log.current.has_value());

  auto const& target = log.target;
  auto const& plan = *log.plan;
  auto const& current = *log.current;

  ADB_PROD_ASSERT(current.supervision.has_value());

  // Check write concern
  auto effectiveWriteConcern =
      computeEffectiveWriteConcern(target.config, current, plan, health);

  if (effectiveWriteConcern !=
      plan.participantsConfig.config.effectiveWriteConcern) {
    ctx.createAction<UpdateEffectiveAndAssumedWriteConcernAction>(
        effectiveWriteConcern,
        std::min(effectiveWriteConcern,
                 current.supervision->assumedWriteConcern));

    return;
  }

  // Wait for sync
  if (target.config.waitForSync != plan.participantsConfig.config.waitForSync) {
    ctx.createAction<UpdateWaitForSyncAction>(
        target.config.waitForSync,
        std::min(target.config.waitForSync,
                 current.supervision->assumedWaitForSync));
    return;
  }
}

auto checkConfigCommitted(SupervisionContext& ctx, Log const& log) -> void {
  if (!log.plan.has_value() || !log.current.has_value()) {
    return;
  }
  ADB_PROD_ASSERT(log.plan.has_value());
  ADB_PROD_ASSERT(log.current.has_value());

  auto const& plan = *log.plan;
  auto const& current = *log.current;

  ADB_PROD_ASSERT(current.supervision.has_value());

  if (!current.leader.has_value() ||
      !current.leader->committedParticipantsConfig.has_value()) {
    return;
  }

  if (plan.participantsConfig.generation ==
      current.leader->committedParticipantsConfig->generation) {
    if (plan.participantsConfig.config.effectiveWriteConcern !=
        current.supervision->assumedWriteConcern) {
      // update assumedWriteConcern
      ctx.createAction<SetAssumedWriteConcernAction>(
          plan.participantsConfig.config.effectiveWriteConcern);
    }

    if (plan.participantsConfig.config.waitForSync !=
        current.supervision->assumedWaitForSync) {
      ctx.createAction<SetAssumedWaitForSyncAction>(
          plan.participantsConfig.config.waitForSync);
    }
  }
}

auto checkConverged(SupervisionContext& ctx, Log const& log) {
  auto const& target = log.target;
  // TODO add status report for each exit point
  if (!log.current.has_value()) {
    return;
  }
  TRI_ASSERT(log.current.has_value());
  auto const& current = *log.current;

  if (!current.leader.has_value()) {
    return;
  }

  ADB_PROD_ASSERT(log.plan.has_value());
  auto const& plan = *log.plan;
  if (not plan.currentTerm or not plan.currentTerm->leader) {
    return;
  }

  if (plan.participantsConfig.generation !=
      current.leader->committedParticipantsConfig->generation) {
    return;
  }

  if (current.leader->term != plan.currentTerm->term and
      not current.leader->leadershipEstablished) {
    return;
  }

  auto allStatesReady = std::all_of(
      log.current->localState.begin(), log.current->localState.end(),
      [&](auto const& pair) -> bool {
        // Current can contain stale entries, i.e. participants that were once
        // part of the replicated log, but no longer are. The supervision should
        // only ever consider those entries in Current that belong to a
        // participant in Plan.

        if (not plan.participantsConfig.participants.contains(pair.first)) {
          return true;
        }

        // Check if the follower has acked the current term. We are not
        // interested in information from an old term.
        if (pair.second.term != plan.currentTerm->term) {
          return false;
        }

        return pair.second.state ==
               replicated_log::LocalStateMachineStatus::kOperational;
      });
  if (!allStatesReady) {
    return;
  }

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

  // If currentTerm's leader entry does not have a value,
  // make sure a leader is elected.
  checkLeaderPresent(ctx, log, health);

  // If the leader is unhealthy, write a new term that
  // does not have a leader.
  // In the next round this will lead to a leadership election.
  checkLeaderHealthy(ctx, log, health);

  checkConfigChanged(ctx, log, health);
  checkConfigCommitted(ctx, log);

  // Check whether a participant was added in Target that is not in Plan.
  // If so, add it to Plan.
  //
  // This has to happen before checkLeaderRemovedFromTargetParticipants,
  // because we don't want to make anyone leader who is not in participants
  // anymore
  checkParticipantToAdd(ctx, log, health);

  // If a participant is in Plan but not in Target, gracefully
  // remove it
  checkParticipantToRemove(ctx, log, health);

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

  // If the user has updated flags for a participant, which is detected by
  // comparing Target to Plan, write that change to Plan.
  // If the configuration differs between Target and Plan,
  // apply the new configuration.
  checkParticipantWithFlagsToUpdate(ctx, log, health);

  // Check whether we have converged, and if so, report and set version
  // to target version
  checkConverged(ctx, log);
}

}  // namespace arangodb::replication2::replicated_log
