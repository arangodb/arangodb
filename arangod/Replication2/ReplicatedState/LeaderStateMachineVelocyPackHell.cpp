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
#include "LeaderStateMachine.h"

#include "Replication2/ReplicatedState/AgencySpecificationLog.h"
#include "Replication2/ReplicatedState/AgencySpecificationState.h"

#include "velocypack/Builder.h"
#include "velocypack/velocypack-common.h"
#include "velocypack/velocypack-aliases.h"

using namespace arangodb::replication2::replicated_state::agency;

namespace arangodb::replication2::replicated_state {

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

}  // namespace arangodb::replication2::replicated_state
