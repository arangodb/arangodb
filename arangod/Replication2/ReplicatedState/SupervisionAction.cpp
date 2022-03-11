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

#include "SupervisionAction.h"
#include "Agency/AgencyPaths.h"
#include "Agency/TransactionBuilder.h"
#include "Basics/StringUtils.h"
#include "velocypack/Builder.h"

namespace paths = arangodb::cluster::paths::aliases;

namespace arangodb::replication2::replicated_state {

void Executor::operator()(EmptyAction const&) {}

void Executor::operator()(AddParticipantAction const& action) {
  auto logTargetParticipantPath = paths::target()
                                      ->replicatedLogs()
                                      ->database(database)
                                      ->log(id)
                                      ->participants()
                                      ->server(action.participant);

  auto statePlanPath =
      paths::plan()->replicatedStates()->database(database)->state(id);
  auto statePlanParticipantPath =
      statePlanPath->participants()->participant(action.participant);

  envelope = envelope.write()
                 .emplace_object(logTargetParticipantPath->str(),
                                 [&](VPackBuilder& builder) {
                                   ParticipantFlags{.allowedInQuorum = false,
                                                    .allowedAsLeader = false}
                                       .toVelocyPack(builder);
                                 })
                 .emplace_object(statePlanParticipantPath->str(),
                                 [&](VPackBuilder& builder) {
                                   agency::Plan::Participant{
                                       .generation = action.generation}
                                       .toVelocyPack(builder);
                                 })
                 .inc(paths::target()->version()->str())
                 .inc(paths::plan()->version()->str())
                 .inc(statePlanPath->generation()->str())
                 .precs()
                 .isEmpty(logTargetParticipantPath->str())
                 .isEmpty(statePlanParticipantPath->str())
                 .isEqual(statePlanPath->generation()->str(), action.generation)
                 .end();
}

void Executor::operator()(AddStateToPlanAction const& action) {
  auto logTargetPath =
      paths::target()->replicatedLogs()->database(database)->log(id)->str();
  auto statePlanPath =
      paths::plan()->replicatedStates()->database(database)->state(id)->str();

  envelope = envelope.write()
                 .emplace_object(logTargetPath,
                                 [&](VPackBuilder& builder) {
                                   action.logTarget.toVelocyPack(builder);
                                 })
                 .emplace_object(statePlanPath,
                                 [&](VPackBuilder& builder) {
                                   action.statePlan.toVelocyPack(builder);
                                 })
                 .inc(paths::target()->version()->str())
                 .inc(paths::plan()->version()->str())
                 .precs()
                 .isEmpty(logTargetPath)
                 .isEmpty(statePlanPath)
                 .end();
}

void Executor::operator()(ModifyParticipantFlagsAction const& action) {
  auto logTargetPath = paths::target()
                           ->replicatedLogs()
                           ->database(database)
                           ->log(id)
                           ->participants()
                           ->server(action.participant);

  envelope = envelope.write()
                 .emplace_object(logTargetPath->str(),
                                 [&](VPackBuilder& builder) {
                                   action.flags.toVelocyPack(builder);
                                 })
                 .inc(paths::target()->version()->str())
                 .end();
}

auto execute(LogId id, DatabaseID const& database, Action const& action,
             arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto executor = Executor{id, database, std::move(envelope)};
  std::visit(executor, action);
  return std::move(executor.envelope);
}

}  // namespace arangodb::replication2::replicated_state
