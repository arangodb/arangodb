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

#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/Storage/IStorageEngineMethods.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

namespace arangodb::replication2::test {
struct ParticipantsFactoryMock : IParticipantsFactory {
  MOCK_METHOD(std::shared_ptr<ILogFollower>, constructFollower,
              (std::unique_ptr<storage::IStorageEngineMethods> && methods,
               FollowerTermInfo info, ParticipantContext context),
              (override));

  MOCK_METHOD(std::shared_ptr<ILogLeader>, constructLeader,
              (std::unique_ptr<storage::IStorageEngineMethods> && methods,
               LeaderTermInfo info, ParticipantContext context),
              (override));
};
}  // namespace arangodb::replication2::test