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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <gmock/gmock.h>

#include "Cluster/CallbackGuard.h"
#include "Replication2/ReplicatedLog/IRebootIdCache.h"

namespace arangodb::replication2::test {

struct RebootIdCacheMock : replicated_log::IRebootIdCache {
  MOCK_METHOD((std::unordered_map<ParticipantId, RebootId>), getRebootIdsFor,
              (std::vector<ParticipantId> const&), (const, override));
  MOCK_METHOD((cluster::CallbackGuard), registerCallbackOnChange,
              (PeerState, Callback, std::string), (override));

  RebootIdCacheMock() {
    ON_CALL(*this, getRebootIdsFor)
        .WillByDefault([](std::vector<ParticipantId> const& participants) {
          auto result = std::unordered_map<ParticipantId, RebootId>{};
          for (auto const& participant : participants) {
            result.emplace(participant, RebootId{0});
          }
          return result;
        });
  }
};

}  // namespace arangodb::replication2::test
