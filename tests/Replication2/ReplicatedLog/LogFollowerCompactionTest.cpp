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

#include "Replication2/ReplicatedLog/TestHelper.h"
#include "Replication2/Mocks/PersistedLog.h"

#include "Replication2/ReplicatedLog/LogFollower.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"

#include <Basics/VelocyPackHelper.h>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

struct LogFollowerCompactionTest : ReplicatedLogTest {};

TEST_F(LogFollowerCompactionTest, simple_release) {
  using namespace std::string_literals;

  auto leaderLog = makeReplicatedLog(LogId{1});
  auto followerLog = makeReplicatedLog(LogId{1});

  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);

  leader->triggerAsyncReplication();
  follower->runAllAsyncAppendEntries();
  ASSERT_TRUE(leader->isLeadershipEstablished());

  auto i = 0;
  // compaction will only run if at least 1000 entries are to be compacted,
  // so let's start with that
  for (; i < 1000; ++i) {
    leader->insert(
        LogPayload::createFromString("log entry #"s + std::to_string(i)));
  }
  // let's add some more
  auto const stopCompactionIdx = leader->insert(
      LogPayload::createFromString("log entry #"s + std::to_string(i++)));
  auto const firstUncompactedIdx = leader->insert(
      LogPayload::createFromString("log entry #"s + std::to_string(i++)));
  leader->insert(
      LogPayload::createFromString("log entry #"s + std::to_string(i++)));
  auto const latestIdx = leader->insert(
      LogPayload::createFromString("log entry #"s + std::to_string(i++)));

  // replicate entries
  follower->runAllAsyncAppendEntries();
  ASSERT_EQ(latestIdx,
            leader->getQuickStatus().getLocalStatistics().value().commitIndex);
  ASSERT_EQ(
      latestIdx,
      follower->getQuickStatus().getLocalStatistics().value().commitIndex);
  auto const fullLog = follower->copyInMemoryLog();
  ASSERT_EQ(LogIndex{1}, fullLog.getFirstIndex());
  ASSERT_EQ(latestIdx, fullLog.getLastIndex());
  // release some entries
  follower->release(stopCompactionIdx);
  auto const compactedLog = follower->copyInMemoryLog();
  ASSERT_EQ(firstUncompactedIdx, compactedLog.getFirstIndex());
  ASSERT_EQ(latestIdx, compactedLog.getLastIndex());
}

// TODO Add a test that runs a `checkCompaction` during an appendEntries,
//      specifically after core->insertAsync is called (and the new InMemoryLog
//      is created), and before the new InMemoryLog is applied.
//      We probably need another Mock for the PersistedLog, so we can control
//      when the core->insertAsync promise will be resolved.

TEST_F(LogFollowerCompactionTest, run_compaction_during_append_entries) {
  using namespace std::string_literals;

  auto leaderLog = makeReplicatedLog(LogId{1});
  // Use a DelayedMockLog as a mock for PersistedLog, so we can control when
  // "insertAsync" promises will be resolved.
  // Note that it does not delay "removeFront", so it will only delay
  // appendEntries, but not release/checkCompaction.
  auto followerLog = makeReplicatedLog<DelayedMockLog>(LogId{1});

  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);
  auto followersDelayedPersistedLog =
      getPersistedLogById<DelayedMockLog>(followerLog->getId());

  leader->triggerAsyncReplication();
  follower->runAllAsyncAppendEntries();
  followersDelayedPersistedLog->runAsyncInsert();
  ASSERT_TRUE(leader->isLeadershipEstablished());

  auto i = 0;
  // compaction will only run if at least 1000 entries are to be compacted,
  // so let's start with that
  for (; i < 1000; ++i) {
    leader->insert(
        LogPayload::createFromString("log entry #"s + std::to_string(i)));
  }
  // let's add some more
  auto const stopCompactionIdx = leader->insert(
      LogPayload::createFromString("log entry #"s + std::to_string(i++)));
  auto const firstUncompactedIdx = leader->insert(
      LogPayload::createFromString("log entry #"s + std::to_string(i++)));
  auto const secondToLastIdx = leader->insert(
      LogPayload::createFromString("log entry #"s + std::to_string(i++)));

  // replicate entries
  follower->runAllAsyncAppendEntries();
  followersDelayedPersistedLog->runAsyncInsert();
  follower->runAllAsyncAppendEntries();
  ASSERT_EQ(secondToLastIdx,
            leader->getQuickStatus().getLocalStatistics().value().commitIndex);
  ASSERT_EQ(
      secondToLastIdx,
      follower->getQuickStatus().getLocalStatistics().value().commitIndex);
  // Check that we can actually release indexes now
  ASSERT_EQ(secondToLastIdx, follower->getQuickStatus()
                                 .getLocalStatistics()
                                 .value()
                                 .lowestIndexToKeep);

  // add another one to trigger another appendEntries request
  auto const latestIdx = leader->insert(
      LogPayload::createFromString("log entry #"s + std::to_string(i++)));
  follower->runAllAsyncAppendEntries();
  // The follower's appendEntries code path is now waiting for the PersistedLog
  // to resolve the insertAsync promise.
  auto const logBeforeCompaction = follower->copyInMemoryLog();
  ASSERT_EQ(LogIndex{1}, logBeforeCompaction.getFirstIndex());
  // the last entry should not yet be visible
  ASSERT_EQ(secondToLastIdx, logBeforeCompaction.getLastIndex());

  // now run the compaction
  // TODO The next line currently deadlocks in LogCore::removeFront
  follower->release(stopCompactionIdx);
  auto const compactedLog = follower->copyInMemoryLog();
  ASSERT_EQ(firstUncompactedIdx, compactedLog.getFirstIndex());
  ASSERT_EQ(secondToLastIdx, compactedLog.getLastIndex());

  // resolve promises, so appendEntries can finish
  followersDelayedPersistedLog->runAsyncInsert();
  ASSERT_EQ(latestIdx,
            leader->getQuickStatus().getLocalStatistics().value().commitIndex);
  // run append entries, so the commit index at the follower gets updated as
  // well
  follower->runAllAsyncAppendEntries();
  ASSERT_EQ(
      latestIdx,
      follower->getQuickStatus().getLocalStatistics().value().commitIndex);

  // check that both the compaction and the latest append entries have had
  // their effect on the log
  auto const finalLog = follower->copyInMemoryLog();
  ASSERT_EQ(firstUncompactedIdx, finalLog.getFirstIndex());
  ASSERT_EQ(latestIdx, finalLog.getLastIndex());
}
