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

#include "Replication2/ReplicatedLog/ReplicatedLog.h"

struct TRI_vocbase_t;

namespace arangodb::replication2::test {

struct FakeFollowerFactory : replicated_log::IAbstractFollowerFactory {
  FakeFollowerFactory() {}

  auto constructFollower(ParticipantId const& participantId) -> std::shared_ptr<
      replication2::replicated_log::AbstractFollower> override {
    auto it = followerThunks.find(participantId);
    if (it != followerThunks.end()) {
      auto ret = it->second.operator()();
      followerThunks.erase(it);
      return ret;
    }
    return nullptr;
  }

  auto constructLeaderCommunicator(ParticipantId const& participantId)
      -> std::shared_ptr<replicated_log::ILeaderCommunicator> override {
    return leaderComm;
  }

  std::shared_ptr<replicated_log::ILeaderCommunicator> leaderComm;
  std::unordered_map<ParticipantId,
                     std::function<std::shared_ptr<
                         replication2::replicated_log::AbstractFollower>()>>
      followerThunks;
};
}  // namespace arangodb::replication2::test
