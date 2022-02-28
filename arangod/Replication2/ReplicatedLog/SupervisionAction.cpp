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
auto to_string(Action::ActionType action) -> std::string_view {
  switch (action) {
    case Action::ActionType::EmptyAction: {
      return "Empty";
    } break;
    case Action::ActionType::ErrorAction: {
      return "Error";
    } break;
    case Action::ActionType::AddLogToPlanAction: {
      return "AddLogToPlan";
    } break;
    case Action::ActionType::AddParticipantsToTargetAction: {
      return "AddLogToPlan";
    } break;
    case Action::ActionType::LeaderElectionAction: {
      return "LeaderElection";
    } break;
    case Action::ActionType::CreateInitialTermAction: {
      return "CreateInitialTermAction";
    } break;
    case Action::ActionType::DictateLeaderAction: {
      return "DictateLeaderAction";
    } break;
    case Action::ActionType::UpdateTermAction: {
      return "UpdateTermAction";
    } break;
    case Action::ActionType::UpdateParticipantFlagsAction: {
      return "UpdateParticipantFlags";
    } break;
    case Action::ActionType::AddParticipantToPlanAction: {
      return "AddParticipantToPlanAction";
    } break;
    case Action::ActionType::RemoveParticipantFromPlanAction: {
      return "RemoveParticipantFromPlan";
    } break;
    case Action::ActionType::UpdateLogConfigAction: {
      return "UpdateLogConfig";
    } break;
    case Action::ActionType::EvictLeaderAction: {
      return "EvictLeaderAction";
    } break;
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

  builder.add(VPackValue("message"));
  builder.add(VPackValue(_message));
}

void ErrorAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));

  builder.add(VPackValue("error"));
  ::toVelocyPack(_error, builder);
}

auto ErrorAction::execute(std::string dbName,
                          arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto current_path = paths::current()
                          ->replicatedLogs()
                          ->database(dbName)
                          ->log(_id)
                          ->supervision()
                          ->error()
                          ->str();
  return envelope.write()
      .emplace_object(
          current_path,
          [&](VPackBuilder& builder) { ::toVelocyPack(_error, builder); })
      .inc(paths::current()->version()->str())
      .precs()
      .end();
}

/*
 * AddLogToPlanAction
 */
void AddLogToPlanAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}

auto AddLogToPlanAction::execute(std::string dbName,
                                 arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path =
      paths::plan()->replicatedLogs()->database(dbName)->log(_spec.id)->str();

  return envelope.write()
      .emplace_object(
          path, [&](VPackBuilder& builder) { _spec.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())
      .precs()
      .isEmpty(path)
      .end();
};

/*
 * AddLogToPlanAction
 */
void AddParticipantsToTargetAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}

auto AddParticipantsToTargetAction::execute(std::string dbName,
                                            arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path =
      paths::target()->replicatedLogs()->database(dbName)->log(_spec.id)->str();

  return envelope.write()
      .emplace_object(
          path, [&](VPackBuilder& builder) { _spec.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())
      .precs()
      //      .isEmpty(path)
      .end();
};

/*
 * CreateInitialTermAction
 */
void CreateInitialTermAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}

auto CreateInitialTermAction::execute(std::string dbName,
                                      arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path = paths::plan()
                  ->replicatedLogs()
                  ->database(dbName)
                  ->log(_id)
                  ->currentTerm()
                  ->str();

  return envelope.write()
      .emplace_object(
          path, [&](VPackBuilder& builder) { _term.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())
      .precs()
      .isEmpty(path)
      .end();
}

void DictateLeaderAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
  builder.add(VPackValue("newTerm"));
  _term.toVelocyPack(builder);
}

auto DictateLeaderAction::execute(std::string dbName,
                                  arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path = paths::plan()
                  ->replicatedLogs()
                  ->database(dbName)
                  ->log(_id)
                  ->currentTerm()
                  ->str();

  return envelope.write()
      .emplace_object(
          path, [&](VPackBuilder& builder) { _term.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())

      /* TODO: previous term should still be there
            .precs()
            .isEmpty(path)
       */
      .end();

  return envelope;
}

void EvictLeaderAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}

auto EvictLeaderAction::execute(std::string dbName,
                                arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path = paths::plan()->replicatedLogs()->database(dbName)->log(_id);

  return envelope.write()
      .emplace_object(
          path->participantsConfig()->participants()->server(_leader)->str(),
          [&](VPackBuilder& builder) { _flags.toVelocyPack(builder); })
      .emplace_object(
          path->currentTerm()->str(),
          [&](VPackBuilder& builder) { _newTerm.toVelocyPack(builder); })
      .inc(path->participantsConfig()->generation()->str())
      .inc(paths::plan()->version()->str())
      .precs()
      .isEqual(path->participantsConfig()->generation()->str(), _generation)
      .end();
}

/*
 * UpdateTermAction
 */
void UpdateTermAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));

  builder.add(VPackValue("newTerm"));
  _newTerm.toVelocyPack(builder);
}

auto UpdateTermAction::execute(std::string dbName,
                               arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path = paths::plan()
                  ->replicatedLogs()
                  ->database(dbName)
                  ->log(_id)
                  ->currentTerm()
                  ->str();

  return envelope.write()
      .emplace_object(
          path, [&](VPackBuilder& builder) { _newTerm.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())
      .end();
}

/*
 * LeaderElectionAction
 */
void LeaderElectionAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));

  builder.add(VPackValue("campaign"));
  _election.toVelocyPack(builder);

  if (_newTerm) {
    builder.add(VPackValue("newTerm"));
    _newTerm->toVelocyPack(builder);
  }
}

auto LeaderElectionAction::execute(std::string dbName,
                                   arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto plan_path = paths::plan()
                       ->replicatedLogs()
                       ->database(dbName)
                       ->log(_id)
                       ->currentTerm()
                       ->str();
  auto current_path = paths::current()
                          ->replicatedLogs()
                          ->database(dbName)
                          ->log(_id)
                          ->supervision()
                          ->election()
                          ->str();

  if (_election.outcome == LogCurrentSupervisionElection::Outcome::SUCCESS) {
    TRI_ASSERT(_newTerm);
    return envelope.write()
        .emplace_object(
            plan_path,
            [&](VPackBuilder& builder) { _newTerm->toVelocyPack(builder); })
        .inc(paths::plan()->version()->str())
        .emplace_object(
            current_path,
            [&](VPackBuilder& builder) { _election.toVelocyPack(builder); })
        .inc(paths::current()->version()->str())
        .precs()
        .end();
  } else {
    return envelope.write()
        .emplace_object(
            current_path,
            [&](VPackBuilder& builder) { _election.toVelocyPack(builder); })
        .inc(paths::current()->version()->str())
        .precs()
        .end();
  }
}

/*
 * UpdateParticipantFlagsAction
 */
void UpdateParticipantFlagsAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));

  builder.add("participant", VPackValue(_participant));
  builder.add(VPackValue("flags"));
  _flags.toVelocyPack(builder);
}
auto UpdateParticipantFlagsAction::execute(std::string dbName,
                                           arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path = paths::plan()
                  ->replicatedLogs()
                  ->database(dbName)
                  ->log(_id)
                  ->participantsConfig();

  return envelope.write()
      .emplace_object(
          path->participants()->server(_participant)->str(),
          [&](VPackBuilder& builder) { _flags.toVelocyPack(builder); })
      .inc(path->generation()->str())
      .inc(paths::plan()->version()->str())
      .precs()
      .isEqual(path->generation()->str(), _generation)
      .end();
}

/*
 * AddParticipantToPlanAction
 */
void AddParticipantToPlanAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}
auto AddParticipantToPlanAction::execute(std::string dbName,
                                         arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path = paths::plan()
                  ->replicatedLogs()
                  ->database(dbName)
                  ->log(_id)
                  ->participantsConfig();

  return envelope.write()
      .emplace_object(
          path->participants()->server(_participant)->str(),
          [&](VPackBuilder& builder) { _flags.toVelocyPack(builder); })
      .inc(path->generation()->str())
      .inc(paths::plan()->version()->str())
      .precs()
      .isEmpty(path->participants()->server(_participant)->str())
      .isEqual(path->generation()->str(), _generation)
      .end();
}

/*
 * RemoveParticipantFromPlanAction
 */
void RemoveParticipantFromPlanAction::toVelocyPack(
    VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}

auto RemoveParticipantFromPlanAction::execute(
    std::string dbName, arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path = paths::plan()
                  ->replicatedLogs()
                  ->database(dbName)
                  ->log(_id)
                  ->participantsConfig();

  return envelope.write()
      .remove(path->participants()->server(_participant)->str())
      .inc(path->generation()->str())
      .inc(paths::plan()->version()->str())
      .precs()
      .isNotEmpty(path->participants()->server(_participant)->str())
      .isEqual(path->generation()->str(), _generation)
      .end();
}

/*
 * UpdateLogConfigAction
 */
void UpdateLogConfigAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}
auto UpdateLogConfigAction::execute(std::string dbName,
                                    arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  // It is currently undefined what should happen if someone changes
  // the configuration
  TRI_ASSERT(false);
  return envelope;
}

}  // namespace arangodb::replication2::replicated_log
