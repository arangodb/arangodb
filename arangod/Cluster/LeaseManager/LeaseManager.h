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

struct LeaseManager {
  using AbortMethod = std::function<void()>;

  struct LeaseIdGuard {
    LeaseIdGuard(LeaseId id) :_id{id} {}

    auto id() const noexcept -> LeaseId {
      return _id;
    }
   private:
    LeaseId _id;
  };
  struct LeaseListOfPeer {
    CallbackGuard _serverAbortCallback;
    std::unordered_map<LeaseId, LeaseEntry> _mapping;
  };

  LeaseManager(RebootTracker&);

  auto requireLease(PeerState const& peerState, AbortMethod abortCallback)
      -> LeaseIdGuard;

  auto leasesToVPack() const -> arangodb::velocypack::Builder;

 private:
  uint64_t _lastUsedLeaseId{0};
  RebootTracker& _rebootTracker;
  std::unordered_map<PeerState, LeaseListOfPeer> _leases;
};
}  // namespace cluster
}  // namespace arangodb
