////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "TestHelper.h"

#include "Replication2/ReplicatedLog/LogFollower.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

struct LogFollowerCompactionTest : ReplicatedLogTest {};

TEST_F(LogFollowerCompactionTest, simple_release) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto followerLog = makeReplicatedLog(LogId{1});

  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);

  leader->triggerAsyncReplication();
  follower->runAllAsyncAppendEntries();
  ASSERT_TRUE(leader->isLeadershipEstablished());

  leader->insert(LogPayload::createFromString("log entry #1"));
  auto const stopCompactionIdx =
      leader->insert(LogPayload::createFromString("log entry #2"));
  auto const latestIdx =
      leader->insert(LogPayload::createFromString("log entry #3"));

  follower->runAllAsyncAppendEntries();
  ASSERT_EQ(latestIdx,
            leader->getQuickStatus().getLocalStatistics().value().commitIndex);
  auto const fullLog = follower->copyInMemoryLog();
  ASSERT_EQ(LogIndex{1}, fullLog.getFirstIndex());
  ASSERT_EQ(latestIdx, fullLog.getLastIndex());
  follower->release(stopCompactionIdx);
  auto const compactedLog = follower->copyInMemoryLog();
  ASSERT_EQ(stopCompactionIdx, compactedLog.getFirstIndex());
  ASSERT_EQ(latestIdx, compactedLog.getLastIndex());
}
