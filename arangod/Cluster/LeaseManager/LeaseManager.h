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
#include "Basics/ResultT.h"
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

  // Note: The Following two share the same properties.
  // But the significant functions dtor and cancel are different
  // There was no point on creating a base class for this, and pay for virtual
  // calls.

  // Specialization to allow for API checks which type of Lease we have.
  // With this guard we have required a lease from a remote peer.
  struct LeaseFromRemoteGuard {
    LeaseFromRemoteGuard(PeerState peer, LeaseId id, LeaseManager* mgr)
        : _peerState(std::move(peer)), _id{id}, _manager(mgr) {}
   public:
    ~LeaseFromRemoteGuard();

    // This Guard is only allowed to be moved, so we move out the unique
    // ptr to the LeaseManager, so we can not cancel the lease twice.
    LeaseFromRemoteGuard(LeaseFromRemoteGuard&&) = default;
    LeaseFromRemoteGuard& operator=(LeaseFromRemoteGuard&&) = default;

    LeaseFromRemoteGuard(LeaseFromRemoteGuard const&) = delete;
    LeaseFromRemoteGuard& operator=(LeaseFromRemoteGuard const&) = delete;

    auto id() const noexcept -> LeaseId { return _id; }

    auto cancel() const noexcept -> void;

   private:
    PeerState _peerState;
    LeaseId _id;

    struct NoopDeleter {
      template<typename T>
      void operator()(T*){

      };
    };
    std::unique_ptr<LeaseManager, NoopDeleter> _manager;
  };

  // Specialization to allow for API checks which type of Lease we have.
  // With this guard we have leased a resource to a remote peer.
  struct LeaseToRemoteGuard {
    LeaseToRemoteGuard(PeerState peer, LeaseId id, LeaseManager* mgr)
        : _peerState(std::move(peer)), _id{id}, _manager(mgr) {}
   public:
    ~LeaseToRemoteGuard();

    // This Guard is only allowed to be moved, so we move out the unique
    // ptr to the LeaseManager, so we can not cancel the lease twice.
    LeaseToRemoteGuard(LeaseToRemoteGuard&&) = default;
    LeaseToRemoteGuard& operator=(LeaseToRemoteGuard&&) = default;

    LeaseToRemoteGuard(LeaseToRemoteGuard const&) = delete;
    LeaseToRemoteGuard& operator=(LeaseToRemoteGuard const&) = delete;

    auto id() const noexcept -> LeaseId { return _id; }

    auto cancel() const noexcept -> void;

   private:
    PeerState _peerState;
    LeaseId _id;

    struct NoopDeleter {
      template<typename T>
      void operator()(T*){

      };
    };

    std::unique_ptr<LeaseManager, NoopDeleter> _manager;
  };

  struct LeaseListOfPeer {
    CallbackGuard _serverAbortCallback;
    std::unordered_map<LeaseId, std::unique_ptr<LeaseEntry>> _mapping;
  };

  LeaseManager(RebootTracker& rebootTracker, std::unique_ptr<ILeaseManagerNetworkHandler> networkHandler);

  template<typename F>
  [[nodiscard]] auto requireLease(PeerState const& requestFrom, F&& onLeaseLost)
      -> LeaseFromRemoteGuard {
    static_assert(std::is_nothrow_invocable_r_v<void, F>,
                  "The abort method of a leaser must be noexcept");
    return requireLeaseInternal(
        requestFrom,
        std::make_unique<LeaseEntry_Impl<F>>(std::forward<F>(onLeaseLost)));
  }

  template<typename F>
  [[nodiscard]] auto handoutLease(PeerState const& requestedBy, LeaseId leaseId,
                                  F&& onLeaseLost) -> ResultT<LeaseToRemoteGuard> {
    static_assert(std::is_nothrow_invocable_r_v<void, F>,
                  "The abort method of a leaser must be noexcept");
    return handoutLeaseInternal(
        requestedBy, leaseId,
        std::make_unique<LeaseEntry_Impl<F>>(std::forward<F>(onLeaseLost)));
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

  [[nodiscard]] auto requireLeaseInternal(PeerState const& requestFrom,
                    std::unique_ptr<LeaseEntry> abortMethod) -> LeaseFromRemoteGuard;

  [[nodiscard]] auto handoutLeaseInternal(
      PeerState const& requestedBy, LeaseId leaseId,
      std::unique_ptr<LeaseEntry> abortMethod) -> ResultT<LeaseToRemoteGuard>;

  auto sendAbortRequestsForAbandonedLeases() noexcept -> void;

  auto returnLeaseFromRemote(PeerState const& peerState, LeaseId const& leaseId) noexcept -> void;

  auto cancelLeaseFromRemote(PeerState const& peerState, LeaseId const& leaseId) noexcept -> void;

  auto returnLeaseToRemote(PeerState const& peerState, LeaseId const& leaseId) noexcept -> void;

  auto cancelLeaseToRemote(PeerState const& peerState, LeaseId const& leaseId) noexcept -> void;


  uint64_t _lastUsedLeaseId{0};
  RebootTracker& _rebootTracker;
  std::unique_ptr<ILeaseManagerNetworkHandler> _networkHandler;

  // NOTE: The two lists of leases are using more or less the same implementation
  // We only added this differentiation, so we can easier inspect if the local server
  // was provider or consumer of the lease, and avoid confusion or misuse of the API.
  struct OpenLeases {
    std::unordered_map<PeerState, LeaseListOfPeer> list;
  };
  Guarded<OpenLeases> _leasedFromRemotePeers;
  Guarded<OpenLeases> _leasedToRemotePeers;

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
