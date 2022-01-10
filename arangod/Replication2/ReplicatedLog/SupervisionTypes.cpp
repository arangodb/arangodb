////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2022 ArangoDB GmbH, Cologne, Germany
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

#include "SupervisionTypes.h"

namespace arangodb::replication2::replicated_log {

auto to_string(LeaderElectionCampaign::Reason reason) -> std::string_view {
  switch (reason) {
    case LeaderElectionCampaign::Reason::OK: {
      return "OK";
    } break;
    case LeaderElectionCampaign::Reason::ServerIll: {
      return "ServerIll";
    } break;
    case LeaderElectionCampaign::Reason::TermNotConfirmed: {
      return "TermNotConfirmed";
    } break;
  }
  return "this-value-is-here-to-shut-up-the-compiler-if-this-is-reached-that-"
         "is-a-bug";
}

auto operator<<(std::ostream& os, LeaderElectionCampaign::Reason reason)
    -> std::ostream& {
  return os << to_string(reason);
}

void LeaderElectionCampaign::toVelocyPack(VPackBuilder& builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add("numberOKParticipants", VPackValue(numberOKParticipants));

  builder.add(VPackValue("bestTermIndex"));
  bestTermIndex.toVelocyPack(builder);

  {
    auto rb = VPackObjectBuilder(&builder, "reasons");
    for (auto const& [participant, reason] : reasons) {
      builder.add(VPackValue(participant));
      builder.add(VPackValue(to_string(reason)));
    }
  }

  {
    auto eb = VPackArrayBuilder(&builder, "electibleLeaderSet");
    for (auto const& participant : electibleLeaderSet) {
      builder.add(VPackValue(participant));
    }
  }
}

auto to_string(LeaderElectionCampaign const& campaign) -> std::string {
  auto bb = VPackBuilder{};
  campaign.toVelocyPack(bb);
  return bb.toString();
}

auto computeReason(LogCurrentLocalState const& status, bool healthy,
                   LogTerm term) -> LeaderElectionCampaign::Reason {
  if (!healthy) {
    return LeaderElectionCampaign::Reason::ServerIll;
  } else if (term != status.term) {
    return LeaderElectionCampaign::Reason::TermNotConfirmed;
  } else {
    return LeaderElectionCampaign::Reason::OK;
  }
}

}  // namespace arangodb::replication2::replicated_log
