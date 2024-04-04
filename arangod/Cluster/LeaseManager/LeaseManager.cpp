////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "LeaseManager.h"

#include "Cluster/RebootTracker.h"
#include "Inspection/VPack.h"

#include "Logger/LogMacros.h"

using namespace arangodb::cluster;

LeaseManager::LeaseManager(RebootTracker& rebootTracker)
    : _rebootTracker(rebootTracker), _leases{} {}

auto LeaseManager::requireLease(PeerState const& peerState, AbortMethod abortMethod) -> LeaseIdGuard {
  // NOTE: In theory _nextLeaseId can overflow here, but that should never be a problem.
  // if we ever reach that point without restarting the server, it is highly unlikely that
  // we still have handed out low number leases.

  auto id = LeaseId{_lastUsedLeaseId++};
  auto leaseEntry = LeaseEntry{};
  // TODO ADD Locking
  if (auto it = _leases.find(peerState); it == _leases.end()) {
    auto trackerGuard = _rebootTracker.callMeOnChange(peerState, [abortMethod]() {
          LOG_DEVEL << "ULF ULF ULF";
          abortMethod();
        }, "Wir moegen den Cluster nicht so sehr.");
    _leases.emplace(peerState, LeaseListOfPeer{._serverAbortCallback{std::move(trackerGuard)}, ._mapping{{id, std::move(leaseEntry)}}});
  } else {
    it->second._mapping.emplace(id, std::move(leaseEntry));
  }
  return LeaseIdGuard{std::move(id)};
}

auto LeaseManager::leasesToVPack() const -> arangodb::velocypack::Builder {
  VPackBuilder builder;
  VPackObjectBuilder guard(&builder);
  for (auto const& [peerState, leaseEntry] : _leases) {
    builder.add(VPackValue(
        fmt::format("{}:{}", peerState.serverId,
                    basics::StringUtils::itoa(peerState.rebootId.value()))));
    VPackObjectBuilder leaseMappingGuard(&builder);
    for (auto const& [id, entry] : leaseEntry._mapping) {
      builder.add(VPackValue(basics::StringUtils::itoa(id.id())));
      velocypack::serialize(builder, entry);
    }
  }
  return builder;
}
