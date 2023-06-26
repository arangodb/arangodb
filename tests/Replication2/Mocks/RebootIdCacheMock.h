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

#pragma once

#include <gmock/gmock.h>

#include "Cluster/CallbackGuard.h"
#include "Replication2/ReplicatedLog/IRebootIdCache.h"

namespace arangodb::replication2::test {

struct RebootIdCacheMock : IRebootIdCache {
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
