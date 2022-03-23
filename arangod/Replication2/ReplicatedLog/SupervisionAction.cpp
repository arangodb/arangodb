////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
#include "SupervisionAction.h"

#include "Agency/TransactionBuilder.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "velocypack/Builder.h"
#include "velocypack/velocypack-common.h"

#include "Agency/AgencyPaths.h"

#include "Logger/LogMacros.h"

using namespace arangodb::replication2::agency;
namespace paths = arangodb::cluster::paths::aliases;

namespace arangodb::replication2::replicated_log {

auto to_string(Action const& action) -> std::string_view {
  return std::visit([](auto&& arg) { return arg.name; }, action);
}

void VelocyPacker::operator()(EmptyAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));
}

void VelocyPacker::operator()(ErrorAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));

  builder.add(VPackValue("error"));
  ::toVelocyPack(action._error, builder);
}

void VelocyPacker::operator()(AddLogToPlanAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));
}

void VelocyPacker::operator()(CreateInitialTermAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));
}

void VelocyPacker::operator()(CurrentNotAvailableAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));
}

void VelocyPacker::operator()(DictateLeaderAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));

  builder.add(VPackValue("newLeader"));
  action._leader.toVelocyPack(builder);
}

void VelocyPacker::operator()(DictateLeaderFailedAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));

  builder.add(VPackValue("message"));
  builder.add(VPackValue(action._message));
}

void VelocyPacker::operator()(EvictLeaderAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));
}

void VelocyPacker::operator()(WriteEmptyTermAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));
}

void VelocyPacker::operator()(LeaderElectionImpossibleAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));
}

void VelocyPacker::operator()(LeaderElectionOutOfBoundsAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));
}

void VelocyPacker::operator()(
    LeaderElectionQuorumNotReachedAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));
};

void VelocyPacker::operator()(LeaderElectionAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));

  builder.add(VPackValue("campaign"));
  action._electionReport.toVelocyPack(builder);

  builder.add(VPackValue("newLeader"));
  action._electedLeader.toVelocyPack(builder);
}

void VelocyPacker::operator()(UpdateParticipantFlagsAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));

  builder.add("participant", VPackValue(action._participant));
  builder.add(VPackValue("flags"));
  action._flags.toVelocyPack(builder);
}

void VelocyPacker::operator()(AddParticipantToPlanAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));
}

void VelocyPacker::operator()(RemoveParticipantFromPlanAction const& action) {
  builder.add("type", action.name);
  builder.add("participant", action._participant);
}

void VelocyPacker::operator()(UpdateLogConfigAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));
}

void VelocyPacker::operator()(ConvergedToTargetAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));
}

void toVelocyPack(Action const& action, VPackBuilder& builder) {
  auto packer = VelocyPacker(builder);
  std::visit(packer, action);
}

auto execute(Action const& action, DatabaseID const& dbName, LogId const& log,
             std::optional<LogPlanSpecification> const plan,
             std::optional<LogCurrent> const current,
             arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto planPath =
      paths::plan()->replicatedLogs()->database(dbName)->log(log)->str();

  auto currentPath = paths::current()
                         ->replicatedLogs()
                         ->database(dbName)
                         ->log(log)
                         ->supervision()
                         ->str();

  if (std::holds_alternative<EmptyAction>(action)) {
    return envelope;
  }

  auto ctx = ActionContext{std::move(plan), std::move(current)};

  std::visit([&](auto& action) { action.execute(ctx); }, action);

  if (!ctx.hasModification()) {
    return envelope;
  }

  if (ctx.hasPlanModification()) {
    VPackBuilder b;
    ctx.getPlan().toVelocyPack(b);
  }
  if (ctx.hasCurrentModification()) {
    VPackBuilder b;
    ctx.getCurrent().toVelocyPack(b);
  }

  return envelope.write()
      .cond(ctx.hasPlanModification(),
            [&](arangodb::agency::envelope::write_trx&& trx) {
              return std::move(trx)
                  .emplace_object(planPath,
                                  [&](VPackBuilder& builder) {
                                    ctx.getPlan().toVelocyPack(builder);
                                  })
                  .inc(paths::plan()->version()->str());
            })
      .cond(ctx.hasCurrentModification(),
            [&](arangodb::agency::envelope::write_trx&& trx) {
              return std::move(trx)
                  .emplace_object(
                      currentPath,
                      [&](VPackBuilder& builder) {
                        ctx.getCurrent().supervision->toVelocyPack(builder);
                      })
                  .inc(paths::current()->version()->str());
            })
      .end();
}

}  // namespace arangodb::replication2::replicated_log
