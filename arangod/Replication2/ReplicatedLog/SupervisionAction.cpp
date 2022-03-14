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

void Executor::operator()(EmptyAction const& action) {}

void Executor::operator()(ErrorAction const& action) {
  envelope = envelope.write()
                 .emplace_object(currentPath->supervision()->error()->str(),
                                 [&](VPackBuilder& builder) {
                                   ::toVelocyPack(action._error, builder);
                                 })
                 .inc(paths::current()->version()->str())
                 .precs()
                 .end();
}

void Executor::operator()(AddLogToPlanAction const& action) {
  auto spec = LogPlanSpecification(
      log, std::nullopt,
      ParticipantsConfig{.generation = 1,
                         .participants = action._participants});

  envelope = envelope.write()
                 .emplace_object(
                     planPath->str(),
                     [&](VPackBuilder& builder) { spec.toVelocyPack(builder); })
                 .inc(paths::plan()->version()->str())
                 .precs()
                 .isEmpty(planPath->str())
                 .end();
}

void Executor::operator()(CreateInitialTermAction const& action) {
  auto term =
      LogPlanTermSpecification(LogTerm{1}, action._config, std::nullopt);

  envelope = envelope.write()
                 .emplace_object(
                     planPath->currentTerm()->str(),
                     [&](VPackBuilder& builder) { term.toVelocyPack(builder); })
                 .inc(paths::plan()->version()->str())
                 .precs()
                 .isEmpty(planPath->currentTerm()->str())
                 .end();
}

void Executor::operator()(DictateLeaderAction const& action) {
  envelope = envelope.write()
                 .emplace_object(planPath->currentTerm()->str(),
                                 [&](VPackBuilder& builder) {
                                   action._term.toVelocyPack(builder);
                                 })
                 .inc(paths::plan()->version()->str())

                 /* TODO: previous term should still be there
                       .precs()
                       .isEmpty(path)
                  */
                 .end();
}

void Executor::operator()(DictateLeaderFailedAction const& action) {
  envelope = envelope.write()
                 .emplace_object(currentPath->supervision()->error()->str(),
                                 [&](VPackBuilder& builder) {
                                   builder.add(VPackValue(action._message));
                                 })
                 .inc(paths::current()->version()->str())
                 .end();
}

void Executor::operator()(CurrentNotAvailableAction const& action) {
  envelope =
      envelope.write()
          .emplace_object(
              currentPath->supervision()->str(),
              [&](VPackBuilder& builder) {
                auto ob = VPackObjectBuilder(&builder);

                builder.add("message", VPackValue("Current not available yet"));
              })
          .inc(paths::plan()->version()->str())
          .precs()
          .isEmpty(currentPath->supervision()->str())
          .end();
}

void Executor::operator()(EvictLeaderAction const& action) {
  auto newFlags = action._flags;
  newFlags.allowedAsLeader = false;
  auto newTerm = action._currentTerm;
  newTerm.term = LogTerm{newTerm.term.value + 1};
  newTerm.leader.reset();

  envelope =
      envelope.write()
          .emplace_object(
              planPath->participantsConfig()
                  ->participants()
                  ->server(action._leader)
                  ->str(),
              [&](VPackBuilder& builder) { newFlags.toVelocyPack(builder); })
          .emplace_object(
              planPath->currentTerm()->str(),
              [&](VPackBuilder& builder) { newTerm.toVelocyPack(builder); })
          .inc(planPath->participantsConfig()->generation()->str())
          .inc(paths::plan()->version()->str())
          .precs()
          .isEqual(planPath->participantsConfig()->generation()->str(),
                   action._generation)
          .end();
}

void Executor::operator()(UpdateTermAction const& action) {
  envelope = envelope.write()
                 .emplace_object(planPath->currentTerm()->str(),
                                 [&](VPackBuilder& builder) {
                                   action._newTerm.toVelocyPack(builder);
                                 })
                 .inc(paths::plan()->version()->str())
                 .end();
}

void Executor::operator()(WriteEmptyTermAction const& action) {
  auto newTerm = action._term;
  newTerm.term = LogTerm{action._term.term.value + 1};
  newTerm.leader.reset();

  envelope = envelope.write()
                 .emplace_object(planPath->currentTerm()->str(),
                                 [&](VPackBuilder& builder) {
                                   newTerm.toVelocyPack(builder);
                                 })
                 .inc(paths::plan()->version()->str())
                 .end();
}

void Executor::operator()(LeaderElectionAction const& action) {
  if (action._election.outcome ==
      LogCurrentSupervisionElection::Outcome::SUCCESS) {
    TRI_ASSERT(action._newTerm);
    envelope =
        envelope.write()
            .emplace_object(planPath->currentTerm()->str(),
                            [&](VPackBuilder& builder) {
                              action._newTerm->toVelocyPack(builder);
                            })
            .inc(paths::plan()->version()->str())
            .emplace_object(currentPath->supervision()->election()->str(),
                            [&](VPackBuilder& builder) {
                              action._election.toVelocyPack(builder);
                            })
            .inc(paths::current()->version()->str())
            .precs()
            .end();
  } else {
    envelope =
        envelope.write()
            .emplace_object(currentPath->supervision()->election()->str(),
                            [&](VPackBuilder& builder) {
                              action._election.toVelocyPack(builder);
                            })
            .inc(paths::current()->version()->str())
            .precs()
            .end();
  }
}

void Executor::operator()(UpdateParticipantFlagsAction const& action) {
  envelope = envelope.write()
                 .emplace_object(planPath->participantsConfig()
                                     ->participants()
                                     ->server(action._participant)
                                     ->str(),
                                 [&](VPackBuilder& builder) {
                                   action._flags.toVelocyPack(builder);
                                 })
                 .inc(planPath->participantsConfig()->generation()->str())
                 .inc(paths::plan()->version()->str())
                 .precs()
                 .isEqual(planPath->participantsConfig()->generation()->str(),
                          action._generation)
                 .end();
}

void Executor::operator()(AddParticipantToPlanAction const& action) {
  envelope = envelope.write()
                 .emplace_object(planPath->participantsConfig()
                                     ->participants()
                                     ->server(action._participant)
                                     ->str(),
                                 [&](VPackBuilder& builder) {
                                   action._flags.toVelocyPack(builder);
                                 })
                 .inc(planPath->participantsConfig()->generation()->str())
                 .inc(paths::plan()->version()->str())
                 .precs()
                 .isEmpty(planPath->participantsConfig()
                              ->participants()
                              ->server(action._participant)
                              ->str())
                 .isEqual(planPath->participantsConfig()->generation()->str(),
                          action._generation)
                 .end();
}

void Executor::operator()(RemoveParticipantFromPlanAction const& action) {
  envelope = envelope.write()
                 .remove(planPath->participantsConfig()
                             ->participants()
                             ->server(action._participant)
                             ->str())
                 .inc(planPath->participantsConfig()->generation()->str())
                 .inc(paths::plan()->version()->str())
                 .precs()
                 .isNotEmpty(planPath->participantsConfig()
                                 ->participants()
                                 ->server(action._participant)
                                 ->str())
                 .isEqual(planPath->participantsConfig()->generation()->str(),
                          action._generation)
                 .end();
}

void Executor::operator()(UpdateLogConfigAction const& action) {
  // It is currently undefined what should happen if someone changes
  // the configuration
  TRI_ASSERT(false);
}

void Executor::operator()(ConvergedToTargetAction const& action) {}

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

  builder.add(VPackValue("newTerm"));
  action._term.toVelocyPack(builder);
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

void VelocyPacker::operator()(UpdateTermAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));

  builder.add(VPackValue("newTerm"));
  action._newTerm.toVelocyPack(builder);
}

void VelocyPacker::operator()(WriteEmptyTermAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));

  builder.add(VPackValue("previousTerm"));
  action._term.toVelocyPack(builder);
}

void VelocyPacker::operator()(LeaderElectionAction const& action) {
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));

  builder.add(VPackValue("campaign"));
  action._election.toVelocyPack(builder);

  if (action._newTerm) {
    builder.add(VPackValue("newTerm"));
    action._newTerm->toVelocyPack(builder);
  }
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
  builder.add(VPackValue("type"));
  builder.add(VPackValue(action.name));
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
             arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto exec = Executor(dbName, log, std::move(envelope));

  std::visit(exec, action);

  return std::move(exec.envelope);
}

}  // namespace arangodb::replication2::replicated_log
