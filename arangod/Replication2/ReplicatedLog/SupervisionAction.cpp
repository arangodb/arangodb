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
  envelope.write()
      .emplace_object(currentPath->supervision()->error()->str(),
                      [&](VPackBuilder& builder) {
                        ::toVelocyPack(action._error, builder);
                      })
      .inc(paths::current()->version()->str())
      .precs()
      .end();
}

void Executor::operator()(AddLogToPlanAction const& action) {
  envelope.write()
      .emplace_object(
          planPath->str(),
          [&](VPackBuilder& builder) { action._spec.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())
      .precs()
      .isEmpty(planPath->str())
      .end();
}

void Executor::operator()(AddParticipantsToTargetAction const& action) {
  envelope.write()
      .emplace_object(
          targetPath->str(),
          [&](VPackBuilder& builder) { action._spec.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())
      .precs()
      .end();
}

void Executor::operator()(CreateInitialTermAction const& action) {
  envelope.write()
      .emplace_object(
          planPath->currentTerm()->str(),
          [&](VPackBuilder& builder) { action._term.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())
      .precs()
      .isEmpty(planPath->currentTerm()->str())
      .end();
}

void Executor::operator()(DictateLeaderAction const& action) {
  envelope.write()
      .emplace_object(
          planPath->currentTerm()->str(),
          [&](VPackBuilder& builder) { action._term.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())

      /* TODO: previous term should still be there
            .precs()
            .isEmpty(path)
       */
      .end();
}

void Executor::operator()(EvictLeaderAction const& action) {
  envelope.write()
      .emplace_object(
          planPath->participantsConfig()
              ->participants()
              ->server(action._leader)
              ->str(),
          [&](VPackBuilder& builder) { action._flags.toVelocyPack(builder); })
      .emplace_object(
          planPath->currentTerm()->str(),
          [&](VPackBuilder& builder) { action._newTerm.toVelocyPack(builder); })
      .inc(planPath->participantsConfig()->generation()->str())
      .inc(paths::plan()->version()->str())
      .precs()
      .isEqual(planPath->participantsConfig()->generation()->str(),
               action._generation)
      .end();
}

void Executor::operator()(UpdateTermAction const& action) {
  envelope.write()
      .emplace_object(
          planPath->currentTerm()->str(),
          [&](VPackBuilder& builder) { action._newTerm.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())
      .end();
}

void Executor::operator()(LeaderElectionAction const& action) {
  if (action._election.outcome ==
      LogCurrentSupervisionElection::Outcome::SUCCESS) {
    TRI_ASSERT(action._newTerm);
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
  envelope.write()
      .emplace_object(
          planPath->participantsConfig()
              ->participants()
              ->server(action._participant)
              ->str(),
          [&](VPackBuilder& builder) { action._flags.toVelocyPack(builder); })
      .inc(planPath->participantsConfig()->generation()->str())
      .inc(paths::plan()->version()->str())
      .precs()
      .isEqual(planPath->participantsConfig()->generation()->str(),
               action._generation)
      .end();
}

void Executor::operator()(AddParticipantToPlanAction const& action) {
  envelope.write()
      .emplace_object(
          planPath->participantsConfig()
              ->participants()
              ->server(action._participant)
              ->str(),
          [&](VPackBuilder& builder) { action._flags.toVelocyPack(builder); })
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
  envelope.write()
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

auto to_string(Action const& action) -> std::string_view {
  return std::visit([](auto&& arg) { return arg.name; }, action);
}

void toVelocyPack(Action const& action, VPackBuilder& builder) {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(action)));
}

auto execute(Action const& action, DatabaseID const& dbName, LogId const& log,
             arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto exec = Executor(dbName, log, std::move(envelope));

  std::visit(exec, action);

  return std::move(exec.envelope);
}

}  // namespace arangodb::replication2::replicated_log
