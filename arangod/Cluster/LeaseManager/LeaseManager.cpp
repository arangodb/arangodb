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

#include "Cluster/LeaseManager/LeaseManagerNetworkHandler.h"
#include "Cluster/RebootTracker.h"
#include "Inspection/VPack.h"

#include "Logger/LogMacros.h"

using namespace arangodb::cluster;

LeaseManager::LeaseFromRemoteGuard::~LeaseFromRemoteGuard() {
  _manager.returnLeaseFromRemote(_peerState, _id);
}

auto LeaseManager::LeaseFromRemoteGuard::cancel() const noexcept -> void {
  _manager.cancelLeaseFromRemote(_peerState, _id);
}

LeaseManager::LeaseToRemoteGuard::~LeaseToRemoteGuard() {
  _manager.returnLeaseToRemote(_peerState, _id);
}

auto LeaseManager::LeaseToRemoteGuard::cancel() const noexcept -> void {
  _manager.cancelLeaseToRemote(_peerState, _id);
}

LeaseManager::LeaseManager(
    RebootTracker& rebootTracker,
    std::unique_ptr<ILeaseManagerNetworkHandler> networkHandler)
    : _rebootTracker(rebootTracker),
      _networkHandler(std::move(networkHandler)),
      _leasedFromRemotePeers(),
      _leasedToRemotePeers() {
  TRI_ASSERT(_networkHandler != nullptr)
      << "Broken setup. We do not have a network handler in LeaseManager.";
}

auto LeaseManager::requireLeaseInternal(PeerState const& requestFrom, std::unique_ptr<LeaseEntry> leaseEntry) -> LeaseFromRemoteGuard {
  // NOTE: In theory _nextLeaseId can overflow here, but that should never be a problem.
  // if we ever reach that point without restarting the server, it is highly unlikely that
  // we still have handed out low number leases.

  auto id = LeaseId{_lastUsedLeaseId++};
  _leasedFromRemotePeers.doUnderLock([&](auto& guarded) {
    auto& list = guarded.list;
    if (auto it = list.find(requestFrom); it == list.end()) {
      auto trackerGuard = _rebootTracker.callMeOnChange(requestFrom, [&]() {
            // The server has rebooted make sure we erase all it's entries
            // This needs to call abort methods of all leases.
            _leasedFromRemotePeers.doUnderLock(
                [&](auto& guarded) { guarded.list.erase(requestFrom); });
          }, "Abort leases of the LeaseManager.");
      auto it2 = list.emplace(
          requestFrom,
          LeaseListOfPeer{._serverAbortCallback{std::move(trackerGuard)},
                          ._mapping{}});
      TRI_ASSERT(it2.second) << "Failed to register new peer state";
      it2.first->second._mapping.emplace(id, std::move(leaseEntry));
    } else {
      it->second._mapping.emplace(id, std::move(leaseEntry));
    }
  });

  return LeaseFromRemoteGuard{requestFrom, std::move(id), *this};
}

auto LeaseManager::handoutLeaseInternal(PeerState const& requestedBy,
                                        LeaseId leaseId,
                                        std::unique_ptr<LeaseEntry> leaseEntry)
    -> LeaseToRemoteGuard {
  _leasedToRemotePeers.doUnderLock([&](auto& guarded) {
    auto& list = guarded.list;
    if (auto it = list.find(requestedBy); it == list.end()) {
      auto trackerGuard = _rebootTracker.callMeOnChange(
          requestedBy,
          [&]() {
            // The server has rebooted make sure we erase all it's entries
            // This needs to call abort methods of all leases.
            _leasedToRemotePeers.doUnderLock(
                [&](auto& guarded) { guarded.list.erase(requestedBy); });
          },
          "Abort leases of the LeaseManager.");
      auto it2 = list.emplace(
          requestedBy,
          LeaseListOfPeer{._serverAbortCallback{std::move(trackerGuard)},
                          ._mapping{}});
      TRI_ASSERT(it2.second) << "Failed to register new peer state";
      it2.first->second._mapping.emplace(leaseId, std::move(leaseEntry));
    } else {
      it->second._mapping.emplace(leaseId, std::move(leaseEntry));
    }
  });

  return LeaseToRemoteGuard{requestedBy, std::move(leaseId), *this};
}

auto LeaseManager::leasesToVPack() const -> arangodb::velocypack::Builder {
  VPackBuilder builder;
  VPackObjectBuilder guard(&builder);
  {
    builder.add(VPackValue("leasedFromRemote"));
    VPackObjectBuilder fromGuard(&builder);
    _leasedFromRemotePeers.doUnderLock([&builder](auto& guarded) {
      for (auto const& [peerState, leaseEntry] : guarded.list) {
        builder.add(VPackValue(
            fmt::format("{}:{}", peerState.serverId,
                        basics::StringUtils::itoa(peerState.rebootId.value()))));
        VPackObjectBuilder leaseMappingGuard(&builder);
        for (auto const& [id, entry] : leaseEntry._mapping) {
          builder.add(VPackValue(basics::StringUtils::itoa(id.id())));
          velocypack::serialize(builder, entry);
        }
      }
    });
  }
  {
    builder.add(VPackValue("leasedToRemote"));
    VPackObjectBuilder toGuard(&builder);
    _leasedToRemotePeers.doUnderLock([&builder](auto& guarded) {
      for (auto const& [peerState, leaseEntry] : guarded.list) {
        builder.add(VPackValue(
            fmt::format("{}:{}", peerState.serverId,
                        basics::StringUtils::itoa(peerState.rebootId.value()))));
        VPackObjectBuilder leaseMappingGuard(&builder);
        for (auto const& [id, entry] : leaseEntry._mapping) {
          builder.add(VPackValue(basics::StringUtils::itoa(id.id())));
          velocypack::serialize(builder, entry);
        }
      }
    });
  }

  return builder;
}

auto LeaseManager::returnLeaseFromRemote(PeerState const& peerState, LeaseId const& leaseId) noexcept -> void {
  bool addLeaseToAbort = false;
  _leasedFromRemotePeers.doUnderLock([&](auto& guard) {
    auto& list = guard.list;
    if (auto it = list.find(peerState); it != list.end()) {
      // The lease may already be removed, e.g. by RebootTracker.
      // So we do not really care if it is removed from this list here or not.
      if (auto lease = it->second._mapping.find(leaseId); lease != it->second._mapping.end()) {
        // We should abort the Guard here.
        // We returned our lease no need to tell us to abort;
        lease->second->abort();
        addLeaseToAbort = true;
        // Now delete the lease from the list.
        it->second._mapping.erase(lease);
      }
    }
    // else nothing to do, lease already gone.
  });
  if (addLeaseToAbort) {
    _leasesToAbort.doUnderLock([&](auto& guard) {
      if (auto toAbortServer = guard.abortList.find(peerState.serverId);
          toAbortServer != guard.abortList.end()) {
        // Flag this leaseId to be erased in next run.
        toAbortServer->second.emplace_back(leaseId);
      } else {
        // Have nothing to erase for this server, schedule it.
        guard.abortList.emplace(peerState.serverId,
                                std::vector<LeaseId>{leaseId});
      }
    });
  }
  // TODO: Run this in Background
  sendAbortRequestsForAbandonedLeases();
}

auto LeaseManager::cancelLeaseFromRemote(PeerState const& peerState, LeaseId const& leaseId) noexcept -> void {
  _leasedFromRemotePeers.doUnderLock([&](auto& guard) {
    auto& list = guard.list;
    if (auto it = list.find(peerState); it != list.end()) {
      // The lease may already be removed, e.g. by RebootTracker.
      // So we do not really care if it is removed from this list here or not.
      if (auto it2 = it->second._mapping.find(leaseId); it2 != it->second._mapping.end()) {
        it2->second->abort();
      }
    }
    // else nothing to do, lease already gone.
  });
}

auto LeaseManager::returnLeaseToRemote(PeerState const& peerState, LeaseId const& leaseId) noexcept -> void {
  bool addLeaseToAbort = false;
  _leasedToRemotePeers.doUnderLock([&](auto& guard) {
    auto& list = guard.list;
    if (auto it = list.find(peerState); it != list.end()) {
      // The lease may already be removed, e.g. by RebootTracker.
      // So we do not really care if it is removed from this list here or not.
      if (auto lease = it->second._mapping.find(leaseId); lease != it->second._mapping.end()) {
        // We should abort the Guard here.
        // We returned our lease no need to tell us to abort;
        lease->second->abort();
        addLeaseToAbort = true;
        // Now delete the lease from the list.
        it->second._mapping.erase(lease);
      }
    }
    // else nothing to do, lease already gone.
  });
  if (addLeaseToAbort) {
    _leasesToAbort.doUnderLock([&](auto& guard) {
      if (auto toAbortServer = guard.abortList.find(peerState.serverId);
          toAbortServer != guard.abortList.end()) {
        // Flag this leaseId to be erased in next run.
        toAbortServer->second.emplace_back(leaseId);
      } else {
        // Have nothing to erase for this server, schedule it.
        guard.abortList.emplace(peerState.serverId,
                                std::vector<LeaseId>{leaseId});
      }
    });
  }
  // TODO: Run this in Background
  sendAbortRequestsForAbandonedLeases();
}

auto LeaseManager::cancelLeaseToRemote(PeerState const& peerState, LeaseId const& leaseId) noexcept -> void {
  _leasedToRemotePeers.doUnderLock([&](auto& guard) {
    auto& list = guard.list;
    if (auto it = list.find(peerState); it != list.end()) {
      // The lease may already be removed, e.g. by RebootTracker.
      // So we do not really care if it is removed from this list here or not.
      if (auto it2 = it->second._mapping.find(leaseId); it2 != it->second._mapping.end()) {
        it2->second->abort();
      }
    }
    // else nothing to do, lease already gone.
  });
}

auto LeaseManager::sendAbortRequestsForAbandonedLeases() noexcept -> void {
  std::vector<futures::Future<futures::Unit>> futures;
  std::unordered_map<ServerID, std::vector<LeaseId>> abortList{};
  // Steal the list from the guarded structure.
  // Now other can register again while we abort the open items.
  _leasesToAbort.doUnderLock(
      [&](auto& guarded) { std::swap(guarded.abortList, abortList); });
  for (auto const& [serverId, leaseIds] : abortList) {
    futures.emplace_back(_networkHandler->abortIds(serverId, leaseIds).thenValue([&](Result res) {
      if (!res.ok()) {
        // TODO: Abort if server is permanently gone!
        // TODO: Log?
        // We failed to send the abort request, we should try again later.
        // So let us push the leases back into the list.
        _leasesToAbort.doUnderLock([&](auto& guarded) {
          auto& insertList = guarded.abortList[serverId];
          insertList.insert(insertList.end(), leaseIds.begin(), leaseIds.end());
        });
      }
      // else: We successfully aborted the open IDs. Forget about them now.
    }));
  }
  // Wait on futures outside the lock, as the futures themselves will lock the guarded structure.
  futures::collectAll(futures.begin(), futures.end()).get();
}