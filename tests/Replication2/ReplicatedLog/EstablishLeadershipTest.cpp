////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "TestHelper.h"

#include "Replication2/ReplicatedLog/Algorithms.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::algorithms;
using namespace arangodb::replication2::test;

struct EstablishLeadershipTest : ReplicatedLogTest {};

TEST_F(EstablishLeadershipTest, wait_for_leadership) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto followerLog = makeReplicatedLog(LogId{1});

  auto follower = followerLog->becomeFollower("follower", LogTerm{4}, "leader");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{4}, {follower}, 2);

  auto f = leader->waitForLeadership();

  {
    auto status = leader->getStatus();
    ASSERT_TRUE(std::holds_alternative<LeaderStatus>(status.getVariant()));
    EXPECT_FALSE(std::get<LeaderStatus>(status.getVariant()).leadershipEstablished);
  }

  EXPECT_FALSE(follower->hasPendingAppendEntries());
  EXPECT_FALSE(leader->isLeadershipEstablished());
  EXPECT_FALSE(f.isReady());
  leader->triggerAsyncReplication();
  EXPECT_TRUE(follower->hasPendingAppendEntries());

  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  EXPECT_TRUE(leader->isLeadershipEstablished());
  {
    auto status = leader->getStatus();
    ASSERT_TRUE(std::holds_alternative<LeaderStatus>(status.getVariant()));
    EXPECT_TRUE(std::get<LeaderStatus>(status.getVariant()).leadershipEstablished);
  }

  EXPECT_TRUE(f.isReady());
}
