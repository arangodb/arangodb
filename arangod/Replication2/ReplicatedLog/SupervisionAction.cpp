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

#include "Agency/AgencyPaths.h"
#include "Agency/TransactionBuilder.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "velocypack/Builder.h"
#include "velocypack/velocypack-common.h"

#include "Agency/AgencyPaths.h"

#include "Logger/LogMacros.h"

namespace paths = arangodb::cluster::paths::aliases;

namespace arangodb::replication2::replicated_log {

// makes sure plan version is incremented.
void Executor::insertNewPlanEntry(std::shared_ptr<Root::Arango::Plan> path,
                                  VPackSlice entry) {
  planUpdated = true;
}

void Executor::updatePlanEntry(std::shared_ptr<Root::Arango::Plan> path,
                               VPackSlice entry) {
  planUpdated = true;
}

auto to_string(Action const &action) -> std::string_view {
  return std::visit([](auto &&arg) { return arg.name; }, action);
}

void toVelocyPack(Action const &action, VPackBuilder &builder) {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(action)));
}

auto execute(Action const &action, DatabaseID const &dbName, LogId const &log,
             arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto exec = Executor(dbName, log, std::move(envelope));

  std::visit(exec, action);

  return std::move(exec.envelope);
}

void Executor::operator()(EmptyAction const &action) {}

void Executor::operator()(ErrorAction const &action) {}

void Executor::operator()(AddLogToPlanAction const &action) {
  // TODO: maybe move this into the constructor of AddLogToPlanAction?
  auto spec = LogPlanSpecification(
      log, std::nullopt,
      ParticipantsConfig{.generation = 1,
                         .participants = action._participants});

  /*
   * writeNewEntry(planPath, spec); // can we implicitly increase plan version?
   * should only do this once per transaction?
   */

  envelope = envelope.write()
                 .emplace_object(
                     planPath->str(),
                     [&](VPackBuilder &builder) { spec.toVelocyPack(builder); })
                 .inc(paths::plan()->version()->str())
                 .precs()
                 .isEmpty(planPath->str())
                 .end();
};

void Executor::operator()(AddParticipantsToTargetAction const &action) {
  envelope =
      envelope.write()
          .emplace_object(targetPath->str(), [&](VPackBuilder &builder) {})
          .inc(paths::plan()->version()->str())
          .precs()
          //      .isEmpty(path)
          .end();
};

void Executor::operator()(CreateInitialTermAction const &action) {
  auto const path = planPath->currentTerm()->str();

  auto term = LogPlanTermSpecification(LogTerm{1}, action.config, std::nullopt);

  /*
  writeNewEntry(planPath->currentTerm(), term);
  */

  envelope =
      envelope.write()
          .emplace_object(
              path, [&](VPackBuilder &builder) { term.toVelocyPack(builder); })
          .inc(paths::plan()->version()->str())
          .precs()
          .isEmpty(path)
          .end();
}

void Executor::operator()(LeaderElectionImpossibleAction const &action) {
  auto const &supervision = currentPath->supervision()->election()->str();

  /*
  writeNewEntry(currentPath->supervision(), term);
  */

  envelope =
      envelope.write()
          .emplace_object(
              supervision,
              [&](VPackBuilder &builder) {
                auto ob = VPackObjectBuilder(&builder);

                builder.add("configuredParticipants",
                            VPackValue(action.configuredParticipants));
                builder.add("writeConcern", VPackValue(action.writeConcern));
              })
          .inc(paths::current()->version()->str())
          .precs()
          .end();
}

void Executor::operator()(
    LeaderElectionNumElectibleOutOfRangeAction const &action) {
  auto const &supervision = currentPath->supervision()->election()->str();

  /*
  writeNewEntry(currentPath->supervision(), term);
  writeEntry(currentPath->supervision(), term);
  */

  envelope = envelope.write()
                 .emplace_object(supervision,
                                 [&](VPackBuilder &builder) {
                                   action.election.toVelocyPack(builder);
                                 })
                 .inc(paths::current()->version()->str())
                 .end();
}

void Executor::operator()(
    LeaderElectionNotEnoughParticipantsAction const &action) {
  /*
  writeNewEntry(currentPath->supervision()->election(), action.election);
  writeEntry(currentPath->supervision()->election(), action.election);
  */

  auto const &supervision = currentPath->supervision()->election()->str();
  envelope = envelope.write()
                 .emplace_object(supervision,
                                 [&](VPackBuilder &builder) {
                                   action.election.toVelocyPack(builder);
                                 })
                 .inc(paths::current()->version()->str())
                 .precs()
                 .end();
};

void Executor::operator()(LeaderElectionSuccessAction const &action) {
  auto const &term = planPath->currentTerm()->str();
  auto const &supervision = currentPath->supervision()->election()->str();

  /*
    fundamentally what one wants is
    planPath->currentTerm()->write(action.newTerm);
    (which also makes sure that plan version is incremented)

    write(planPath->currentTerm(), action.newTerm);
    write(currentPath->supervision()->election(), action.election);
  */

  envelope = envelope.write()
                 .emplace_object(term,
                                 [&](VPackBuilder &builder) {
                                   action.newTerm.toVelocyPack(builder);
                                 })
                 .inc(paths::plan()->version()->str())
                 .emplace_object(supervision,
                                 [&](VPackBuilder &builder) {
                                   action.election.toVelocyPack(builder);
                                 })
                 .inc(paths::current()->version()->str())
                 .precs()
                 .end();
}

void Executor::operator()(CurrentNotAvailableAction const &action) {
  auto const path = currentPath->supervision()->error()->str();

  envelope = envelope.write()
                 .emplace_object(path,
                                 [&](VPackBuilder &builder) {
                                   // TODO: proper error message/struct
                                   builder.add(VPackValue("error"));
                                 })
                 .inc(paths::plan()->version()->str())
                 .precs()
                 .isEmpty(path)
                 .end();
}

void Executor::operator()(UpdateTermAction const &action) {
  /*
   * write(Plan)Entry(plan->currentTerm(), action._newTerm);
   */
  auto const path = planPath->currentTerm()->str();

  envelope = envelope.write()
                 .emplace_object(path,
                                 [&](VPackBuilder &builder) {
                                   action._newTerm.toVelocyPack(builder);
                                 })
                 .inc(paths::plan()->version()->str())
                 .end();
}

void Executor::operator()(DictateLeaderAction const &action) {
  auto path = planPath->currentTerm()->str();

  /*
   *
   * updateEntry
   *
   */

  envelope = envelope.write()
                 .emplace_object(path,
                                 [&](VPackBuilder &builder) {
                                   action._term.toVelocyPack(builder);
                                 })
                 .inc(paths::plan()->version()->str())
                 /* TODO: previous term should still be there
                       .precs()
                       .isEmpty(path)
                  */
                 .end();
}

void Executor::operator()(EvictLeaderAction const &action) {
  auto leader = planPath->participantsConfig()
                    ->participants()
                    ->server(action._leader)
                    ->str();
  auto currentTerm = planPath->currentTerm()->str();
  auto generation = planPath->participantsConfig()->str();

  // TODO: it's a bit of a shame that our "actions" do not compose,
  //       as we are
  //       - updating the current term
  //       - updating the current leader's config
  //       and we have "actions" for both, and we might want to be able to
  //       compose them from smaller operations
  envelope = envelope.write()
                 .emplace_object(leader,
                                 [&](VPackBuilder &builder) {
                                   action._flags.toVelocyPack(builder);
                                 })
                 .emplace_object(currentTerm,
                                 [&](VPackBuilder &builder) {
                                   action._term.toVelocyPack(builder);
                                 })
                 .inc(generation)
                 .inc(paths::plan()->version()->str())
                 .precs()
                 .isEqual(generation, action._generation)
                 .end();
}

void Executor::operator()(UpdateParticipantFlagsAction const &action) {
  auto path = planPath->participantsConfig();
  auto generation = planPath->participantsConfig()->str();

  envelope = envelope.write()
                 .emplace_object(
                     path->participants()->server(action._participant)->str(),
                     [&](VPackBuilder &builder) {
                       action._flags.toVelocyPack(builder);
                     })
                 .inc(generation)
                 .inc(paths::plan()->version()->str())
                 .precs()
                 .isEqual(generation, action._generation)
                 .end();
}

void Executor::operator()(AddParticipantToPlanAction const &action) {
  auto const path = planPath->participantsConfig();
  auto const participant =
      path->participants()->server(action._participant)->str();
  auto const generation = planPath->participantsConfig()->generation()->str();

  envelope =
      envelope
          .write()
          // I think here I want to write .emplace_object(participant, _flags)
          .emplace_object(participant,
                          [&](VPackBuilder &builder) {
                            action._flags.toVelocyPack(builder);
                          })
          .inc(generation)
          .inc(paths::plan()->version()->str())
          .precs()
          .isEmpty(participant)
          .isEqual(generation, action._generation)
          .end();
}

void Executor::operator()(RemoveParticipantFromPlanAction const &action) {
  auto path = planPath->participantsConfig();
  auto participant = path->participants()->server(action._participant)->str();
  auto generation = planPath->participantsConfig()->generation()->str();

  envelope = envelope.write()
                 .remove(participant)
                 .inc(generation)
                 .inc(paths::plan()->version()->str())
                 .precs()
                 .isNotEmpty(participant)
                 .isEqual(generation, action._generation)
                 .end();
}

void Executor::operator()(UpdateLogConfigAction const &action) {
  // It is currently undefined what should happen if someone changes
  // the configuration
  TRI_ASSERT(false);
}

} // namespace arangodb::replication2::replicated_log
