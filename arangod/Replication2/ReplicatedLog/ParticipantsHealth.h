////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"

namespace arangodb::replication2::replicated_log {

struct ParticipantHealth {
  RebootId rebootId;
  bool notIsFailed;
  friend auto operator==(ParticipantHealth const& s,
                         ParticipantHealth const& s2) noexcept
      -> bool = default;
};

struct ParticipantsHealth {
  auto notIsFailed(ParticipantId const& participant) const -> bool {
    if (auto it = _health.find(participant); it != std::end(_health)) {
      return it->second.notIsFailed;
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
  auto getRebootId(ParticipantId const& participant) const
      -> std::optional<RebootId> {
    if (auto it = _health.find(participant); it != std::end(_health)) {
      return it->second.rebootId;
    }
    return std::nullopt;
  }
  auto contains(ParticipantId const& participant) const -> bool {
    return _health.contains(participant);
  }
  auto numberNotIsFailedOf(
      agency::ParticipantsFlagsMap const& participants) const -> size_t {
    auto n = size_t{0};

    for (auto const& [participant, _] : participants) {
      if (notIsFailed(participant)) {
        ++n;
      }
    }
    return n;
  }

  auto update(ParticipantId const& p, RebootId id, bool live) {
    _health.emplace(p, ParticipantHealth{id, live});
  }

  friend auto operator==(ParticipantsHealth const& s,
                         ParticipantsHealth const& s2) noexcept
      -> bool = default;
  std::unordered_map<ParticipantId, ParticipantHealth> _health;
};

}  // namespace arangodb::replication2::replicated_log
