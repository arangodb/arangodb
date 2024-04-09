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

#pragma once

#include "Basics/Guarded.h"
#include "Cluster/ClusterTypes.h"
#include "Cluster/LeaseManager/LeaseEntry.h"
#include "Cluster/LeaseManager/LeaseId.h"
#include "Cluster/CallbackGuard.h"

#include <unordered_map>
#include <vector>

namespace arangodb {
namespace velocypack {
class Builder;
}
struct PeerState;
namespace cluster {
class LeaseId;
class RebootTracker;
struct ILeaseManagerNetworkHandler;

struct LeaseManager {

  struct LeaseIdGuard {
    LeaseIdGuard(PeerState peer, LeaseId id, LeaseManager& mgr)
        : _peerState(std::move(peer)), _id{id}, _manager(mgr) {}

    ~LeaseIdGuard();

    auto id() const noexcept -> LeaseId { return _id; }

    auto cancel() const noexcept -> void;

   private:
    PeerState _peerState;
    LeaseId _id;
    LeaseManager& _manager;
  };

  struct LeaseListOfPeer {
    CallbackGuard _serverAbortCallback;
    std::unordered_map<LeaseId, std::unique_ptr<LeaseEntry>> _mapping;
  };

  LeaseManager(RebootTracker& rebootTracker, std::unique_ptr<ILeaseManagerNetworkHandler> networkHandler);

  template<typename F>
  [[nodiscard]] auto requireLease(PeerState const& peerState, F&& onLeaseLost)
      -> LeaseIdGuard {
    static_assert(std::is_nothrow_invocable_r_v<void, F>,
                  "The abort method of a leaser must be noexcept");
    return requireLeaseInternal(peerState, std::make_unique<LeaseEntry_Impl<F>>(
                                               std::forward<F>(onLeaseLost)));
  }

  auto leasesToVPack() const -> arangodb::velocypack::Builder;

#ifdef ARANGODB_USE_GOOGLE_TESTS
  // Read only method to make testing easier.
  auto getLeasesToAbort() const
      -> std::unordered_map<ServerID, std::vector<LeaseId>> {
    std::unordered_map<ServerID, std::vector<LeaseId>> res;
    _leasesToAbort.doUnderLock([&res](auto const& list) {
      res = list.abortList;
    });
    return res;
  }

  // Get Access to the NetworkHandler for Mocking
  auto getNetworkHandler() const -> ILeaseManagerNetworkHandler* {
    return _networkHandler.get();
  }
#endif

 private:

  [[nodiscard]] auto requireLeaseInternal(PeerState const& peerState,
                    std::unique_ptr<LeaseEntry> abortMethod) -> LeaseIdGuard;

  auto sendAbortRequestsForAbandonedLeases() noexcept -> void;

  auto returnLease(PeerState const& peerState, LeaseId const& leaseId) noexcept -> void;

  auto cancelLease(PeerState const& peerState, LeaseId const& leaseId) noexcept -> void;

  uint64_t _lastUsedLeaseId{0};
  RebootTracker& _rebootTracker;
  std::unique_ptr<ILeaseManagerNetworkHandler> _networkHandler;
  struct OpenLeases {
    std::unordered_map<PeerState, LeaseListOfPeer> list;
  };
  Guarded<OpenLeases> _leases;
  // NOTE: We do not use the RebootID here.
  // We guarantee that the LeaseId is unique to our Local ServerID/RebootID combination
  // Hence we can safely abort all LeaseIds for a given ServerID, regardless if the server
  // has rebooted or not. This is also important if the Server was just disconnected and
  // got injected a new RebootID.
  struct LeasesToAbort {
    std::unordered_map<ServerID, std::vector<LeaseId>> abortList;
  };
  Guarded<LeasesToAbort> _leasesToAbort;

};
}  // namespace cluster
}  // namespace arangodb
