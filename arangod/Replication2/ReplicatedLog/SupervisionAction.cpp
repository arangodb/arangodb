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

#include "velocypack/Builder.h"
#include "velocypack/velocypack-common.h"
#include "velocypack/velocypack-aliases.h"

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
    case Action::ActionType::AddLogToPlanAction: {
      return "AddLogToPlan";
    } break;
    case Action::ActionType::FailedLeaderElectionAction: {
      return "FailedLeaderElection";
    } break;
    case Action::ActionType::SuccessfulLeaderElectionAction: {
      return "SuccessfulLeaderElection";
    } break;
    case Action::ActionType::CreateInitialTermAction: {
      return "CreateInitialTermAction";
    } break;
    case Action::ActionType::UpdateTermAction: {
      return "UpdateTermAction";
    } break;
    case Action::ActionType::ImpossibleCampaignAction: {
      return "ImpossibleCampaignAction";
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
  }
  return "this-value-is-here-to-shut-up-the-compiler-if-this-is-reached-that-"
         "is-a-bug";
}

auto operator<<(std::ostream& os, Action::ActionType const& action)
    -> std::ostream& {
  return os << to_string(action);
}

void EmptyAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}

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

void CreateInitialTermAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}

void UpdateTermAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));

  builder.add(VPackValue("newTerm"));
  _newTerm.toVelocyPack(builder);
}

auto to_string(Action const& action) -> std::string {
  VPackBuilder bb;
  action.toVelocyPack(bb);
  return bb.toString();
}

auto operator<<(std::ostream& os, Action const& action) -> std::ostream& {
  return os << to_string(action);
}

void ImpossibleCampaignAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}

void FailedLeaderElectionAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));

  builder.add(VPackValue("campaign"));
  _campaign.toVelocyPack(builder);
}

void SuccessfulLeaderElectionAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));

  builder.add(VPackValue("campaign"));
  _campaign.toVelocyPack(builder);

  builder.add(VPackValue("newLeader"));
  builder.add(VPackValue(_newLeader));

  builder.add(VPackValue("newTerm"));
  _newTerm.toVelocyPack(builder);
}

void UpdateParticipantFlagsAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}

void AddParticipantToPlanAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}

void RemoveParticipantFromPlanAction::toVelocyPack(
    VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}

void UpdateLogConfigAction::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}

}  // namespace arangodb::replication2::replicated_log
