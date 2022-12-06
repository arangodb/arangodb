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

#include <cstddef>
#include <memory>

#include "Agency/AgencyPaths.h"
#include "Agency/TransactionBuilder.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/TimeString.h"
#include "Cluster/ClusterTypes.h"
#include "Inspection/VPack.h"
#include "Random/RandomGenerator.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"
#include "Replication2/ReplicatedLog/SupervisionContext.h"

#include "Logger/LogMacros.h"

namespace paths = arangodb::cluster::paths::aliases;

namespace arangodb::replication2::replicated_log {

auto executeCheckReplicatedLog(DatabaseID const& dbName,
                               std::string const& logIdString, Log log,
                               ParticipantsHealth const& health,
                               arangodb::agency::envelope envelope) noexcept
    -> arangodb::agency::envelope {
  SupervisionContext sctx;
  auto const now = std::chrono::system_clock::now();
  auto const logId = log.target.id;
  auto const hasStatusReport = log.current && log.current->supervision &&
                               log.current->supervision->statusReport;

  // check if error reporting is enabled
  if (log.current && log.current->supervision &&
      log.current->supervision->lastTimeModified.has_value()) {
    auto const lastMod = *log.current->supervision->lastTimeModified;
    if ((now - lastMod) > std::chrono::seconds{15}) {
      sctx.enableErrorReporting();
    }
  }

  auto maxActionsTraceLength = std::invoke([&log]() {
    if (log.target.supervision.has_value()) {
      return log.target.supervision->maxActionsTraceLength;
    } else {
      return static_cast<size_t>(0);
    }
  });

  checkReplicatedLog(sctx, log, health);

  bool const hasNoExecutableAction =
      std::holds_alternative<EmptyAction>(sctx.getAction()) ||
      std::holds_alternative<NoActionPossibleAction>(sctx.getAction());
  // now check if there is status update
  if (hasNoExecutableAction) {
    // there is only a status update
    if (sctx.isErrorReportingEnabled()) {
      // now compare the new status with the old status
      if (log.current && log.current->supervision) {
        if (log.current->supervision->statusReport == sctx.getReport()) {
          // report did not change, do not create a transaction
          return envelope;
        }
      }
    }
  }

  auto actionCtx = arangodb::replication2::replicated_log::executeAction(
      std::move(log), sctx.getAction());

  if (sctx.isErrorReportingEnabled()) {
    if (sctx.getReport().empty()) {
      if (hasStatusReport) {
        actionCtx.modify<LogCurrentSupervision>(
            [&](auto& supervision) { supervision.statusReport.reset(); });
      }
    } else {
      actionCtx.modify<LogCurrentSupervision>([&](auto& supervision) {
        supervision.statusReport = std::move(sctx.getReport());
      });
    }
  } else if (std::holds_alternative<ConvergedToTargetAction>(
                 sctx.getAction())) {
    actionCtx.modify<LogCurrentSupervision>(
        [&](auto& supervision) { supervision.statusReport.reset(); });
  }

  // update last time modified
  if (!hasNoExecutableAction) {
    actionCtx.modify<LogCurrentSupervision>(
        [&](auto& supervision) { supervision.lastTimeModified = now; });
  }

  if (!actionCtx.hasModification()) {
    return envelope;
  }

  return buildAgencyTransaction(dbName, logId, sctx, actionCtx,
                                maxActionsTraceLength, std::move(envelope));
}

auto buildAgencyTransaction(DatabaseID const& dbName, LogId const& logId,
                            SupervisionContext& sctx, ActionContext& actx,
                            size_t maxActionsTraceLength,
                            arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto planPath =
      paths::plan()->replicatedLogs()->database(dbName)->log(logId)->str();

  auto currentSupervisionPath = paths::current()
                                    ->replicatedLogs()
                                    ->database(dbName)
                                    ->log(logId)
                                    ->supervision()
                                    ->str();

  // If we want to keep a trace of actions, then only record actions
  // that actually modify the data structure. This excludes the EmptyAction
  // and the NoActionPossibleAction.
  if (sctx.hasModifyingAction() && maxActionsTraceLength > 0) {
    envelope = envelope.write()
                   .push_queue_emplace(
                       arangodb::cluster::paths::aliases::current()
                           ->replicatedLogs()
                           ->database(dbName)
                           ->log(logId)
                           ->actions()
                           ->str(),
                       // TODO: struct + inspect + transformWith
                       [&](velocypack::Builder& b) {
                         VPackObjectBuilder ob(&b);
                         b.add("time", VPackValue(timepointToString(
                                           std::chrono::system_clock::now())));
                         b.add(VPackValue("desc"));
                         std::visit([&b](auto&& arg) { serialize(b, arg); },
                                    sctx.getAction());
                       },
                       maxActionsTraceLength)
                   .precs()
                   .isNotEmpty(paths::target()
                                   ->replicatedLogs()
                                   ->database(dbName)
                                   ->log(logId)
                                   ->str())
                   .end();
  }

  return envelope.write()
      .cond(actx.hasModificationFor<LogPlanSpecification>(),
            [&](arangodb::agency::envelope::write_trx&& trx) {
              return std::move(trx)
                  .inc(paths::plan()->version()->str())
                  .emplace_object(planPath, [&](VPackBuilder& builder) {
                    velocypack::serialize(
                        builder, actx.getValue<LogPlanSpecification>());
                  });
            })
      .cond(actx.hasModificationFor<LogCurrentSupervision>(),
            [&](arangodb::agency::envelope::write_trx&& trx) {
              return std::move(trx)
                  .emplace_object(currentSupervisionPath,
                                  [&](VPackBuilder& builder) {
                                    velocypack::serialize(
                                        builder,
                                        actx.getValue<LogCurrentSupervision>());
                                  })
                  .inc(paths::current()->version()->str());
            })
      .precs()
      .isNotEmpty(paths::target()
                      ->replicatedLogs()
                      ->database(dbName)
                      ->log(logId)
                      ->str())
      .end();
}

}  // namespace arangodb::replication2::replicated_log
