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

#include "Agency/AgencyPaths.h"
#include "Agency/TransactionBuilder.h"
#include "SupervisionAction.h"
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
    case Action::ActionType::CreateLogForStateAction: {
      return "CreateLogForStateAction";
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

void CreateLogForStateAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
  builder.add(VPackValue("spec"));
  spec.toVelocyPack(builder);
}

auto CreateLogForStateAction::execute(std::string dbName,
                                      arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path =
      paths::target()->replicatedLogs()->database(dbName)->log(spec.id)->str();

  return envelope.write()
      .emplace_object(
          path, [&](VPackBuilder& builder) { spec.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())
      .precs()
      .isEmpty(path)
      .end();
}

}  // namespace arangodb::replication2::replicated_state
