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
#include "AgencySpecificationLog.h"

#include "velocypack/Builder.h"
#include "velocypack/velocypack-common.h"
#include "velocypack/velocypack-aliases.h"

namespace arangodb::replication2::replicated_state::agency {

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

}  // namespace arangodb::replication2::replicated_state::agency
