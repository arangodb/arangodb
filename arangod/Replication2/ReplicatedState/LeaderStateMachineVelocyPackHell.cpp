#include "LeaderStateMachine.h"

#include "velocypack/Builder.h"
#include "velocypack/velocypack-common.h"
#include "velocypack/velocypack-aliases.h"

namespace arangodb::replication2::replicated_state {

void Log::Plan::TermSpecification::Leader::toVelocyPack(
    VPackBuilder& builder) const {
  auto lb = VPackObjectBuilder(&builder);
  builder.add("serverId", VPackValue(serverId));
  builder.add("rebootId", VPackValue(rebootId.value()));
}

void Log::Plan::TermSpecification::Config::toVelocyPack(
    VPackBuilder& builder) const {
  auto lb = VPackObjectBuilder(&builder);
  builder.add("waitForSync", VPackValue(waitForSync));
  builder.add("writeConcern", VPackValue(writeConcern));
  builder.add("softWriteConcern", VPackValue(softWriteConcern));
}

void Log::Plan::TermSpecification::toVelocyPack(VPackBuilder& builder) const {
  auto tb = VPackObjectBuilder(&builder);

  builder.add("term", VPackValue(term));

  builder.add(VPackValue("leader"));
  if (leader) {
    leader->toVelocyPack(builder);
  } else {
    builder.add(VPackValue(velocypack::ValueType::Null));
  }

  builder.add(VPackValue("config"));
  config.toVelocyPack(builder);
}

void Log::Plan::Participants::Participant::toVelocyPack(
    VPackBuilder& builder) const {
  auto pb = VPackObjectBuilder(&builder);
  builder.add("forced", VPackValue(forced));
  builder.add("excluded", VPackValue(excluded));
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

}  // namespace arangodb::replication2::replicated_state
