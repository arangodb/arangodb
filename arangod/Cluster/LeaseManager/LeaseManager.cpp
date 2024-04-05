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

LeaseManager::LeaseIdGuard::~LeaseIdGuard() {
  _manager.returnLease(_peerState, _id);
}

auto LeaseManager::LeaseIdGuard::cancel() const noexcept -> void {
  _manager.cancelLease(_peerState, _id);
}

LeaseManager::LeaseManager(RebootTracker& rebootTracker)
    : _rebootTracker(rebootTracker), _leases{} {}



auto LeaseManager::requireLeaseInternal(PeerState const& peerState, std::unique_ptr<LeaseEntry> leaseEntry) -> LeaseIdGuard {
  // NOTE: In theory _nextLeaseId can overflow here, but that should never be a problem.
  // if we ever reach that point without restarting the server, it is highly unlikely that
  // we still have handed out low number leases.

  auto id = LeaseId{_lastUsedLeaseId++};
  // TODO ADD Locking
  if (auto it = _leases.find(peerState); it == _leases.end()) {
    auto trackerGuard = _rebootTracker.callMeOnChange(peerState, [&]() {
          // The server has rebooted make sure we erase all it's entries
          // This needs to call abort methods of all leases.
          _leases.erase(peerState);
        }, "Wir moegen den Cluster nicht so sehr.");
    auto it2 = _leases.emplace(
        peerState,
        LeaseListOfPeer{._serverAbortCallback{std::move(trackerGuard)},
                        ._mapping{}});
    TRI_ASSERT(it2.second) << "Failed to register new peer state";
    it2.first->second._mapping.emplace(id, std::move(leaseEntry));
  } else {
    it->second._mapping.emplace(id, std::move(leaseEntry));
  }
  return LeaseIdGuard{peerState, std::move(id), *this};
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

auto LeaseManager::returnLease(PeerState const& peerState, LeaseId const& leaseId) noexcept -> void {
  if (auto it = _leases.find(peerState); it != _leases.end()) {
    // The lease may already be removed, e.g. by RebootTracker.
    // So we do not really care if it is removed from this list here or not.
    it->second._mapping.erase(leaseId);
  }
  // else nothing to do, lease already gone.
}

auto LeaseManager::cancelLease(PeerState const& peerState, LeaseId const& leaseId) noexcept -> void {
  if (auto it = _leases.find(peerState); it != _leases.end()) {
    // The lease may already be removed, e.g. by RebootTracker.
    // So we do not really care if it is removed from this list here or not.
    if (auto it2 = it->second._mapping.find(leaseId); it2 != it->second._mapping.end()) {
      it2->second->abort();
    }
  }
  // else nothing to do, lease already gone.
}