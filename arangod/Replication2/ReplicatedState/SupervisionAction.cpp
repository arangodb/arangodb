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
auto to_string(Action::ActionType action) -> std::string_view {
  switch (action) {
    case Action::ActionType::EmptyAction: {
      return "Empty";
    } break;
    case Action::ActionType::AddStateToPlanAction: {
      return "AddStateToPlan";
    }
    case Action::ActionType::AddParticipantAction: {
      return "AddParticipantAction";
    }
    case Action::ActionType::UnExcludeParticipantAction: {
      return "UnExcludeParticipantAction";
    }
  }
  return "this-value-is-here-to-shut-up-the-compiler-if-this-is-reached-that-"
         "is-a-bug";
}

auto operator<<(std::ostream& os, Action::ActionType const& action)
    -> std::ostream& {
  return os << to_string(action);
}
auto to_string(Action const& action) -> std::string {
  VPackBuilder bb;
  action.toVelocyPack(bb);
  return bb.toString();
}

auto operator<<(std::ostream& os, Action const& action) -> std::ostream& {
  return os << to_string(action);
}

/*
 * EmptyAction
 */
void EmptyAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}

auto EmptyAction::execute(std::string dbName,
                          arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  return envelope;
}

void AddStateToPlanAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
  builder.add(VPackValue("logTarget"));
  logTarget.toVelocyPack(builder);
  builder.add(VPackValue("statePlan"));
  statePlan.toVelocyPack(builder);
}

auto AddStateToPlanAction::execute(std::string dbName,
                                   arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto logTargetPath = paths::target()
                           ->replicatedLogs()
                           ->database(dbName)
                           ->log(logTarget.id)
                           ->str();
  auto statePlanPath = paths::plan()
                           ->replicatedStates()
                           ->database(dbName)
                           ->state(statePlan.id)
                           ->str();

  return envelope.write()
      .emplace_object(
          logTargetPath,
          [&](VPackBuilder& builder) { logTarget.toVelocyPack(builder); })
      .emplace_object(
          statePlanPath,
          [&](VPackBuilder& builder) { statePlan.toVelocyPack(builder); })
      .inc(paths::target()->version()->str())
      .inc(paths::plan()->version()->str())
      .precs()
      .isEmpty(logTargetPath)
      .isEmpty(statePlanPath)
      .end();
}

void AddParticipantAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
  builder.add("participant", VPackValue(participant));
}

auto AddParticipantAction::execute(std::string dbName,
                                   arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  // TODO: I got fed up with AgencyPaths. shame on me
  auto logTargetParticipantPath = basics::StringUtils::concatT(
      paths::target()->replicatedLogs()->database(dbName)->log(log)->str(),
      "/participants/", participant);

  auto statePlanPath =
      paths::plan()->replicatedStates()->database(dbName)->state(log)->str();
  auto statePlanParticipantPath = basics::StringUtils::concatT(
      statePlanPath, "/participants/", participant);

  return envelope.write()
      .emplace_object(
          logTargetParticipantPath,
          [&](VPackBuilder& builder) {
            ParticipantFlags{.excluded = true}.toVelocyPack(builder);
          })
      .emplace_object(
          statePlanParticipantPath,
          [&](VPackBuilder& builder) {
            agency::Plan::Participant{.generation = generation}.toVelocyPack(
                builder);
          })
      .inc(paths::target()->version()->str())
      .inc(paths::plan()->version()->str())
      .inc(basics::StringUtils::concatT(statePlanPath, "/generation"))
      .precs()
      .isEmpty(logTargetParticipantPath)
      .isEmpty(statePlanParticipantPath)
      .isEqual(basics::StringUtils::concatT(statePlanPath, "/generation"),
               generation)
      .end();
}

void UnExcludeParticipantAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
  builder.add("participant", VPackValue(participant));
}

auto UnExcludeParticipantAction::execute(std::string dbName,
                                         arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  // TODO: I got fed up with AgencyPaths. shame on me
  auto logTargetPath = basics::StringUtils::concatT(
      paths::target()->replicatedLogs()->database(dbName)->log(log)->str(),
      "/participants/", participant, "/excluded");

  return envelope.write()
      .emplace_object(
          logTargetPath,
          [&](VPackBuilder& builder) { builder.add(VPackValue(false)); })
      .inc(paths::target()->version()->str())
      .end();
}

}  // namespace arangodb::replication2::replicated_state
