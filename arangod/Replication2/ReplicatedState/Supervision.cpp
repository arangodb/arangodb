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

#include "Inspection/VPack.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"
#include "Replication2/ReplicatedState/SupervisionAction.h"
#include <memory>
#include "Agency/AgencyPaths.h"

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::replication2::agency;

/*
 * This is the flow graph of the replicated state supervision. Operations that
 * are on the same level are allowed to be executed in parallel. The first entry
 * in a chain that produces an action terminates the rest of the chain. Actions
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
 * 2. check if the log has converged
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
namespace paths = arangodb::cluster::paths::aliases;

namespace arangodb::replication2::replicated_state {

auto isParticipantSnapshotCompleted(ParticipantId const& participant,
                                    StateGeneration expectedGeneration,
                                    RSA::Current const& current,
                                    RSA::Plan const& plan) -> bool {
  ADB_PROD_ASSERT(plan.participants.contains(participant))
      << "plan did not contain participant " << participant;
  ADB_PROD_ASSERT(plan.participants.at(participant).generation ==
                  expectedGeneration)
      << "expected = " << expectedGeneration
      << " planned = " << plan.participants.at(participant).generation;
  if (expectedGeneration == StateGeneration{1}) {
    return true;
  }
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
  ADB_PROD_ASSERT(state.current.has_value());
  ADB_PROD_ASSERT(state.plan.has_value());
  ADB_PROD_ASSERT(log.plan.has_value());

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
    return false;
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

auto checkStateAdded(SupervisionContext& ctx, RSA::State const& state) {
  auto id = state.target.id;

  if (!state.plan) {
    auto statePlan =
        RSA::Plan{.id = state.target.id,
                  // use generation 2 here, because the initial
                  // participants are written with generation 1 and
                  // AddParticipant uses the value written here for new
                  // participants and *then* increments the generation
                  .generation = StateGeneration{2},
                  .properties = state.target.properties,
                  .owner = "target",
                  .participants = {}};

    auto logTarget =
        replication2::agency::LogTarget(id, {}, state.target.config);
    logTarget.owner = "replicated-state";
    logTarget.leader = state.target.leader;
    logTarget.version = 1;

    for (auto const& [participantId, _] : state.target.participants) {
      logTarget.participants.emplace(participantId, ParticipantFlags{});
      statePlan.participants.emplace(
          participantId,
          agency::Plan::Participant{.generation = StateGeneration{1}});
    }

    ctx.createAction<AddStateToPlanAction>(std::move(logTarget),
                                           std::move(statePlan));
  }
}

auto checkLeaderSet(SupervisionContext& ctx, RLA::Log const& log,
                    RSA::State const& state) {
  auto const& targetLeader = state.target.leader;
  auto const& planLeader = log.target.leader;

  if (targetLeader != planLeader) {
    ctx.createAction<SetLeaderAction>(targetLeader);
  }
}

auto checkConfigSet(SupervisionContext& ctx, RLA::Log const& log,
                    RSA::State const& state) {
  auto const& stateConfig = state.target.config;
  auto const& logConfig = log.target.config;

  if (stateConfig != logConfig) {
    ctx.createAction<SetLogConfigAction>(stateConfig);
  }
}

auto checkParticipantAdded(SupervisionContext& ctx, RLA::Log const& log,
                           RSA::State const& state) {
  ADB_PROD_ASSERT(state.plan.has_value());

  auto const& targetParticipants = state.target.participants;
  auto const& planParticipants = state.plan->participants;

  for (auto const& [participant, flags] : targetParticipants) {
    // participant might be new to target or readded
    // i.e. it is still in State/Plan but not in Log/Target
    if (!planParticipants.contains(participant) ||
        !log.target.participants.contains(participant)) {
      if (ctx.numberServersInTarget + 1 >= ctx.numberServersOk) {
        ctx.createAction<AddParticipantAction>(participant);
      } else {
        ctx.reportStatus(RSA::StatusCode::kInsufficientSnapshotCoverage,
                         participant);
      }
    }
  }
}

void checkTargetParticipantRemoved(SupervisionContext& ctx, RLA::Log const& log,
                                   RSA::State const& state) {
  ADB_PROD_ASSERT(state.plan.has_value());

  auto const& stateTargetParticipants = state.target.participants;
  auto const& logTargetParticipants = log.target.participants;

  for (auto const& [participant, flags] : logTargetParticipants) {
    if (!stateTargetParticipants.contains(participant)) {
      // check if it is ok for that participant to be dropped
      bool isOk = isParticipantOk(participant, log, state);
      auto newNumberOfOkServer = ctx.numberServersOk - (isOk ? 1 : 0);

      if (newNumberOfOkServer >= ctx.numberServersInTarget) {
        ctx.createAction<RemoveParticipantFromLogTargetAction>(participant);
      } else {
        ctx.reportStatus(RSA::StatusCode::kInsufficientSnapshotCoverage,
                         participant);
      }
    }
  }
}

auto checkLogParticipantRemoved(SupervisionContext& ctx, RLA::Log const& log,
                                RSA::State const& state) {
  ADB_PROD_ASSERT(state.plan.has_value());

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
    auto const inTarget = logTargetParticipants.contains(participant);
    if (!inTarget && participantGone(participant)) {
      ctx.createAction<RemoveParticipantFromStatePlanAction>(participant);
    } else if (!inTarget) {
      ctx.reportStatus(RSA::StatusCode::kLogParticipantNotYetGone, participant);
    }
  }
}

/* Check whether there is a participant that is excluded but reported snapshot
 * complete */
auto checkSnapshotComplete(SupervisionContext& ctx, RLA::Log const& log,
                           RSA::State const& state) {
  if (state.current and log.plan) {
    for (auto const& [participant, flags] : log.target.participants) {
      if (!flags.allowedAsLeader || !flags.allowedInQuorum) {
        ADB_PROD_ASSERT(state.plan->participants.contains(participant))
            << "if a participant is in Log/Target is has to be in State/Plan";
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
            ctx.createAction<UpdateParticipantFlagsAction>(participant,
                                                           newFlags);
            continue;
          }
        }
        // otherwise, report error
        ctx.reportStatus(RSA::StatusCode::kServerSnapshotMissing, participant);
      } else {
        ADB_PROD_ASSERT(isParticipantSnapshotCompleted(
            participant, *state.current, *state.plan))
            << "If a participant is allowed as leader and in a quorum, its "
               "snapshot must be available";
      }
    }
  }
}

auto hasConverged(RSA::State const& state, RLA::Log const& log) -> bool {
  if (!state.plan) {
    return false;
  }
  if (!state.current) {
    return false;
  }

  if (state.target.leader != log.target.leader) {
    return false;
  }

  if (!log.current || !log.current->supervision ||
      log.current->supervision->targetVersion != log.target.version) {
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

auto checkConverged(SupervisionContext& ctx, RLA::Log const& log,
                    RSA::State const& state) {
  if (!state.target.version.has_value()) {
    return;
  }

  if (!state.current or !state.current->supervision) {
    return ctx.createAction<CurrentConvergedAction>(std::uint64_t{0});
  }

  // check that we are actually waiting for this version
  if (state.current->supervision->version == state.target.version) {
    return;
  }

  // now check if we actually have converged
  if (hasConverged(state, log)) {
    ctx.createAction<CurrentConvergedAction>(*state.target.version);
  }
}

auto isEmptyAction(Action const& action) {
  return std::holds_alternative<EmptyAction>(action);
}

auto checkReplicatedStateParticipants(SupervisionContext& ctx,
                                      RLA::Log const& log,
                                      RSA::State const& state) {
  if (!state.current.has_value()) {
    return ctx.reportStatus(RSA::StatusCode::kLogCurrentNotAvailable,
                            "State/Current not yet populated");
  }
  if (!log.plan.has_value()) {
    return ctx.reportStatus(RSA::StatusCode::kLogPlanNotAvailable,
                            "Log/Plan not yet populated");
  }

  auto const serversInTarget = state.target.participants.size();
  auto const serversOk = countOkServers(log, state);
  ctx.numberServersInTarget = serversInTarget;
  ctx.numberServersOk = serversOk;

  checkParticipantAdded(ctx, log, state);
  checkTargetParticipantRemoved(ctx, log, state);
  checkLogParticipantRemoved(ctx, log, state);
  checkSnapshotComplete(ctx, log, state);
}

auto checkForwardSettings(SupervisionContext& ctx, RLA::Log const& log,
                          RSA::State const& state) {
  checkLeaderSet(ctx, log, state);
  checkConfigSet(ctx, log, state);
}

void checkReplicatedState(SupervisionContext& ctx,
                          std::optional<RLA::Log> const& log,
                          RSA::State const& state) {
  // First check if the replicated log is already there, if not create it.
  // Everything else requires the replicated log to exist.
  checkStateAdded(ctx, state);

  // It will need to be observable in future that we are doing nothing because
  // we're waiting for the log to appear.
  if (!log.has_value()) {
    // if state/plan is visible, log/target should be visible as well
    ADB_PROD_ASSERT(state.plan == std::nullopt);
    ctx.reportStatus(RSA::StatusCode::kLogNotCreated,
                     "replicated log has not yet been created");
    return;
  }

  ADB_PROD_ASSERT(state.plan.has_value());
  checkReplicatedStateParticipants(ctx, *log, state);
  checkForwardSettings(ctx, *log, state);
  checkConverged(ctx, *log, state);
}

// TODO: sctx is unused
auto buildAgencyTransaction(DatabaseID const& database, LogId id,
                            SupervisionContext& sctx, ActionContext& actx,
                            arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto logTargetPath =
      paths::target()->replicatedLogs()->database(database)->log(id)->str();
  auto statePlanPath =
      paths::plan()->replicatedStates()->database(database)->state(id)->str();
  auto currentSupervisionPath = paths::current()
                                    ->replicatedStates()
                                    ->database(database)
                                    ->state(id)
                                    ->supervision()
                                    ->str();

  return envelope.write()
      .cond(actx.hasModificationFor<replication2::agency::LogTarget>(),
            [&](arangodb::agency::envelope::write_trx&& trx) {
              return std::move(trx)
                  .emplace_object(
                      logTargetPath,
                      [&](VPackBuilder& builder) {
                        velocypack::serialize(
                            builder,
                            actx.getValue<replication2::agency::LogTarget>());
                      })
                  .inc(paths::target()->version()->str());
            })
      .cond(actx.hasModificationFor<agency::Plan>(),
            [&](arangodb::agency::envelope::write_trx&& trx) {
              return std::move(trx)
                  .inc(paths::plan()->version()->str())
                  .emplace_object(statePlanPath, [&](VPackBuilder& builder) {
                    velocypack::serialize(builder,
                                          actx.getValue<agency::Plan>());
                  });
            })
      .cond(actx.hasModificationFor<agency::Current::Supervision>(),
            [&](arangodb::agency::envelope::write_trx&& trx) {
              return std::move(trx)
                  .emplace_object(
                      currentSupervisionPath,
                      [&](VPackBuilder& builder) {
                        velocypack::serialize(
                            builder,
                            actx.getValue<agency::Current::Supervision>());
                      })
                  .inc(paths::plan()->version()->str());
            })
      .end();
}

auto executeCheckReplicatedState(DatabaseID const& database, RSA::State state,
                                 std::optional<RLA::Log> log,
                                 arangodb::agency::envelope env) noexcept
    -> arangodb::agency::envelope {
  auto const now = std::chrono::system_clock::now();
  auto const id = state.target.id;
  auto const hasStatusReport = state.current && state.current->supervision &&
                               state.current->supervision->statusReport;

  // prepare the context
  SupervisionContext ctx;
  // check if error reporting is enabled
  if (state.current && state.current->supervision &&
      state.current->supervision->lastTimeModified.has_value()) {
    auto const lastMod = *state.current->supervision->lastTimeModified;
    if ((now - lastMod) > std::chrono::seconds{15}) {
      ctx.enableErrorReporting();
    }
  }

  // now run checkReplicatedState
  checkReplicatedState(ctx, log, state);

  // now check if there is status update
  if (std::holds_alternative<EmptyAction>(ctx.getAction())) {
    // there is only a status update
    if (ctx.isErrorReportingEnabled()) {
      // now compare the new status with the old status
      if (state.current && state.current->supervision) {
        if (state.current->supervision->statusReport == ctx.getReport()) {
          // report did not change, do not create a transaction
          return env;
        }
      }
    }
  }

  // and now compose the agency transaction
  auto actionCtx =
      executeAction(std::move(state), std::move(log), ctx.getAction());

  // update status report
  if (ctx.isErrorReportingEnabled()) {
    if (ctx.getReport().empty()) {
      if (hasStatusReport) {
        actionCtx.modify<RSA::Current::Supervision>(
            [&](auto& supervision) { supervision.statusReport.reset(); });
      }
    } else {
      actionCtx.modify<RSA::Current::Supervision>([&](auto& supervision) {
        supervision.statusReport = std::move(ctx.getReport());
      });
    }
  } else if (std::holds_alternative<CurrentConvergedAction>(ctx.getAction())) {
    actionCtx.modify<RSA::Current::Supervision>(
        [&](auto& supervision) { supervision.statusReport.reset(); });
  }

  // update last time modified
  if (!std::holds_alternative<EmptyAction>(ctx.getAction())) {
    actionCtx.modify<RSA::Current::Supervision>(
        [&](auto& supervision) { supervision.lastTimeModified = now; });
  }

  if (!actionCtx.hasModification()) {
    return env;
  }
  return buildAgencyTransaction(database, id, ctx, actionCtx, std::move(env));
}

}  // namespace arangodb::replication2::replicated_state
