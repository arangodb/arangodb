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

struct UpdateParticipantsFlagsTest : ReplicatedLogTest {};

TEST_F(UpdateParticipantsFlagsTest, wc2_but_server_forced) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto followerLog1 = makeReplicatedLog(LogId{1});
  auto followerLog2 = makeReplicatedLog(LogId{1});

  auto follower1 = followerLog1->becomeFollower("follower1", LogTerm{4}, "leader");
  auto follower2 = followerLog2->becomeFollower("follower2", LogTerm{4}, "leader");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{4}, {follower1, follower2}, /* write concern = */ 2);

  auto const runAllAsyncAppendEntries = [&] {
    while (follower1->hasPendingAppendEntries() || follower2->hasPendingAppendEntries()) {
      follower1->runAsyncAppendEntries();
      follower2->runAsyncAppendEntries();
    }
  };

  leader->triggerAsyncReplication();
  runAllAsyncAppendEntries();
  ASSERT_TRUE(leader->isLeadershipEstablished());

  auto idx = leader->insert(LogPayload::createFromString("entry #1"));
  // let only commit Follower1
  follower1->runAllAsyncAppendEntries();
  EXPECT_GE(leader->getCommitIndex(), idx);
  runAllAsyncAppendEntries();

  {
    auto newConfig = std::make_shared<ParticipantsConfig>();
    newConfig->generation = 1;
    // make follower2 excluded
    newConfig->participants["follower2"] = replication2::ParticipantFlags{true, false};
    leader->updateParticipantsConfig(newConfig);
  }

  auto idx2 = leader->insert(LogPayload::createFromString("entry #2"));
  // let only commit Follower1
  follower1->runAllAsyncAppendEntries();
  // entry should not be committed because follower2 is forced
  // although wc is 2
  EXPECT_NE(leader->getCommitIndex(), idx2);

  // now run both followers
  runAllAsyncAppendEntries();
  // the entry should now be committed
  EXPECT_GE(leader->getCommitIndex(), idx2);
}


TEST_F(UpdateParticipantsFlagsTest, wc2_but_server_excluded) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto followerLog1 = makeReplicatedLog(LogId{1});
  auto followerLog2 = makeReplicatedLog(LogId{1});

  auto follower1 = followerLog1->becomeFollower("follower1", LogTerm{4}, "leader");
  auto follower2 = followerLog2->becomeFollower("follower2", LogTerm{4}, "leader");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{4}, {follower1, follower2}, /* write concern = */ 2);

  auto const runAllAsyncAppendEntries = [&] {
    while (follower1->hasPendingAppendEntries() || follower2->hasPendingAppendEntries()) {
      follower1->runAsyncAppendEntries();
      follower2->runAsyncAppendEntries();
    }
  };

  leader->triggerAsyncReplication();
  runAllAsyncAppendEntries();
  ASSERT_TRUE(leader->isLeadershipEstablished());

  auto idx = leader->insert(LogPayload::createFromString("entry #1"));
  // let only commit Follower1
  follower1->runAllAsyncAppendEntries();
  EXPECT_GE(leader->getCommitIndex(), idx);
  runAllAsyncAppendEntries();

  {
    auto newConfig = std::make_shared<ParticipantsConfig>();
    newConfig->generation = 1;
    // make follower1 excluded
    newConfig->participants["follower1"] = replication2::ParticipantFlags{false, true};
    leader->updateParticipantsConfig(newConfig);
  }

  auto idx2 = leader->insert(LogPayload::createFromString("entry #2"));
  // let only commit Follower1
  follower1->runAllAsyncAppendEntries();
  // entry should not be committed because follower1 is excluded
  // although wc is 2
  EXPECT_NE(leader->getCommitIndex(), idx2);

  // now run both followers
  runAllAsyncAppendEntries();
  // the entry should now be committed
  EXPECT_GE(leader->getCommitIndex(), idx2);
}
