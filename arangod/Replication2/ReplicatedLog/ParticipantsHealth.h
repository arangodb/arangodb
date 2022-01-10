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

#include "Cluster/ClusterTypes.h"
#include "Replication2/ReplicatedLog/LogCommon.h"

namespace arangodb::replication2::replicated_log {

struct ParticipantHealth {
  RebootId rebootId;
  bool isHealthy;
};

struct ParticipantsHealth {
  auto isHealthy(ParticipantId const& participant) const -> bool {
    if (auto it = _health.find(participant); it != std::end(_health)) {
      return it->second.isHealthy;
    }
    return false;
  };
  auto validRebootId(ParticipantId const& participant, RebootId rebootId) const
      -> bool {
    if (auto it = _health.find(participant); it != std::end(_health)) {
      return it->second.rebootId == rebootId;
    }
    return false;
  };

  std::unordered_map<ParticipantId, ParticipantHealth> _health;
};

}  // namespace arangodb::replication2::replicated_log
