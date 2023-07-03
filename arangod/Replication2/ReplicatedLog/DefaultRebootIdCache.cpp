////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "DefaultRebootIdCache.h"

#include "Cluster/ClusterInfo.h"

#include <fmt/core.h>
#include <fmt/ranges.h>

template<>
struct fmt::formatter<::arangodb::ServerHealthState>
    : fmt::formatter<string_view> {
  template<class FormatContext>
  auto format(::arangodb::ServerHealthState state, FormatContext& fc) const {
    std::stringstream ss;
    ss << state;
    return formatter<string_view>::format(ss.str(), fc);
  }
};

namespace arangodb::replication2::replicated_log {

DefaultRebootIdCache::DefaultRebootIdCache(ClusterInfo& clusterInfo)
    : clusterInfo(clusterInfo) {}

auto DefaultRebootIdCache::getRebootIdsFor(
    std::vector<ParticipantId> const& participants) const
    -> std::unordered_map<ParticipantId, RebootId> {
  auto result = std::unordered_map<ParticipantId, RebootId>{};
  auto rebootIds = clusterInfo.rebootIds();
  for (auto const& participant : participants) {
    auto serverStateIt = rebootIds.find(participant);
    if (serverStateIt != rebootIds.end()) {
      result.try_emplace(participant, serverStateIt->second.rebootId);
    } else {
      // We need to report a RebootId for each participant. Reporting 0 is
      // always safe, as it is the most pessimistic assumption.
      result.try_emplace(participant, RebootId(0));
      // All participants should always be available in the lists of known
      // servers (I think).
      TRI_ASSERT(false) << fmt::format(
          "Participant {} not found in ServersKnown. LogLeader asked for these "
          "participants: {} while the ClusterInfo provided these servers: {}",
          participant, participants, rebootIds);
    }
  }

  return result;
}

auto DefaultRebootIdCache::registerCallbackOnChange(
    PeerState peer, IRebootIdCache::Callback callback, std::string description)
    -> cluster::CallbackGuard {
  return clusterInfo.rebootTracker().callMeOnChange(
      std::move(peer), std::move(callback), std::move(description));
}

}  // namespace arangodb::replication2::replicated_log
