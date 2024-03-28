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

#include "Basics/Identifier.h"

namespace arangodb {
struct PeerState;
namespace cluster {
struct LeaseManager {
  /// @brief server id type
  class LeaseId : public arangodb::basics::Identifier {
   public:
    constexpr LeaseId() noexcept : Identifier() {}
    constexpr explicit LeaseId(BaseType id) noexcept : Identifier(id) {}
  };

  static_assert(sizeof(LeaseId) == sizeof(LeaseId::BaseType),
                "invalid size of LeaseId");

  struct LeaseIdGuard {
    LeaseIdGuard(LeaseId id) : _id(std::move(id)) {}
    auto id() const -> LeaseId { return _id; }
   private:
    LeaseId _id;
  };

  auto requireLease(PeerState const& peerState) -> LeaseIdGuard;

 private:
  uint64_t _nextLeaseId{0};
};
}
}
DECLARE_HASH_FOR_IDENTIFIER(arangodb::cluster::LeaseManager::LeaseId)
