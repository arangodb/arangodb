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

#include "Replication2/Helper/ReplicatedLogTestSetup.h"

#include "Futures/Utilities.h"

namespace arangodb::replication2::test {

void LogConfig::installConfig(bool establishLeadership) {
  auto futures = std::vector<futures::Future<futures::Unit>>{};
  for (auto& follower : followers) {
    futures.emplace_back(follower.get().updateConfig(*this));
  }
  futures.emplace_back(leader.updateConfig(*this));

  // futures.emplace_back(leader.installLogMethodsOnLeadership());
  // for (auto& follower : followers) {
  //   EXPECT_CALL(*follower.get().stateHandleMock, resignCurrentState);
  // }

  if (establishLeadership) {
    auto future = futures::collectAll(std::move(futures));
    while (leader.hasWork() || IHasScheduler::hasWork(followers)) {
      leader.runAll();
      IHasScheduler::runAll(followers);
    }

    EXPECT_TRUE(future.hasValue());
    auto leaderStatus = leader.log->getQuickStatus();
    EXPECT_EQ(leaderStatus.role, replicated_log::ParticipantRole::kLeader);
    EXPECT_TRUE(leaderStatus.leadershipEstablished);
    for (auto& follower : followers) {
      auto followerStatus = follower.get().log->getQuickStatus();
      EXPECT_EQ(followerStatus.role,
                replicated_log::ParticipantRole::kFollower);
      EXPECT_EQ(followerStatus.localState,
                replicated_log::LocalStateMachineStatus::kOperational);
    }
  }
}

}  // namespace arangodb::replication2::test
