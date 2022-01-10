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

#pragma once

#include "velocypack/Builder.h"
#include "velocypack/velocypack-common.h"
#include "velocypack/velocypack-aliases.h"

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"

using namespace arangodb::replication2::agency;

namespace arangodb::replication2::replicated_log {

struct LeaderElectionCampaign {
  enum class Reason { ServerIll, TermNotConfirmed, OK };

  std::unordered_map<ParticipantId, Reason> reasons;
  size_t numberOKParticipants{0};
  replication2::TermIndexPair bestTermIndex;
  std::vector<ParticipantId> electibleLeaderSet;

  void toVelocyPack(VPackBuilder& builder) const;
};
auto to_string(LeaderElectionCampaign::Reason reason) -> std::string_view;
auto operator<<(std::ostream& os, LeaderElectionCampaign::Reason reason)
    -> std::ostream&;

auto to_string(LeaderElectionCampaign const& campaign) -> std::string;
auto operator<<(std::ostream& os, LeaderElectionCampaign const& action)
    -> std::ostream&;

auto computeReason(LogCurrentLocalState const& status, bool healthy,
                   LogTerm term) -> LeaderElectionCampaign::Reason;

}  // namespace arangodb::replication2::replicated_log
