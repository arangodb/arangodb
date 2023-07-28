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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogEntry.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/types.h"

#include "Replication2/Helper/ReplicatedLogTestSetup.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

struct RewriteLogTest : ReplicatedLogTest {};

TEST_F(RewriteLogTest, rewrite_old_leader) {
  // create one log that has three entries:
  // (1:1), (2:2), (3:2)
  auto followerLogContainer =
      makeLogWithFakes({.initialLogRange = LogRange{LogIndex{1}, LogIndex{2}}});
  followerLogContainer.storageContext->emplaceLogRange(
      LogRange{LogIndex{2}, LogIndex{4}}, LogTerm{2});

  // create different log that has only one entry
  // (1:1)
  auto leaderLogContainer =
      makeLogWithFakes({.initialLogRange = LogRange{LogIndex{1}, LogIndex{2}}});

  auto config = makeConfig(leaderLogContainer, {followerLogContainer},
                           {.term = LogTerm{3}, .writeConcern = 2});

  EXPECT_CALL(*followerLogContainer.stateHandleMock, acquireSnapshot).Times(1);
  auto followerMethodsFuture = followerLogContainer.waitToBecomeFollower();
  auto leaderMethodsFuture = leaderLogContainer.waitForLeadership();

  {
    auto fut = followerLogContainer.updateConfig(config);
    EXPECT_TRUE(fut.isReady());
    ASSERT_TRUE(followerMethodsFuture.isReady());
    ASSERT_TRUE(followerMethodsFuture.result().hasValue());
  }

  auto followerMethods = std::move(followerMethodsFuture).get();
  ASSERT_NE(followerMethods, nullptr);

  {
    auto fut = leaderLogContainer.updateConfig(config);
    // write concern is 2, leadership can't be established yet
    EXPECT_FALSE(fut.isReady());
    EXPECT_FALSE(leaderMethodsFuture.isReady());
  }

  auto leader = leaderLogContainer.getAsLeader();
  auto follower = followerLogContainer.getAsFollower();

  {
    auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant()).local;
    EXPECT_EQ(stats.commitIndex, LogIndex{0});
    // Note that the leader inserts an empty log entry to establish leadership
    EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm(3), LogIndex(2)));
  }
  {
    auto stats =
        std::get<FollowerStatus>(follower->getStatus().getVariant()).local;
    EXPECT_EQ(stats.commitIndex, LogIndex{0});
    EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm(2), LogIndex(3)));
  }

  // TODO figure out which schedulers etc. to run in which order
  // have the leader send the append entries request
  EXPECT_EQ(leaderLogContainer.logScheduler->runAll(), 1);
  // have the follower process the append entries requests
  // this should rewrite its log
  EXPECT_EQ(followerLogContainer.delayedLogFollower->runAsyncAppendEntries(),
            1);
  EXPECT_EQ(followerLogContainer.logScheduler->runAll(), 1);

  {
    ASSERT_TRUE(leaderMethodsFuture.isReady());
    ASSERT_TRUE(leaderMethodsFuture.result().hasValue());
  }
  auto leaderMethods = std::move(leaderMethodsFuture).get();
  ASSERT_NE(leaderMethods, nullptr);

  {
    auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant()).local;
    EXPECT_EQ(stats.commitIndex, LogIndex{2});
    EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm(3), LogIndex(2)));
  }
  {
    auto stats =
        std::get<FollowerStatus>(follower->getStatus().getVariant()).local;
    EXPECT_EQ(stats.commitIndex, LogIndex{2});
    EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm(3), LogIndex(2)));
  }

  {
    auto storageContext = followerLogContainer.storageContext;
    EXPECT_EQ(storageContext->log.size(), 4);
    // TODO: reactive the rest of the test
#if 0
    auto entry = std::optional<LogEntry>();
    auto log = getPersistedLogById(LogId{1});
    auto iter = log->read(LogIndex{1});  // The mock log returns all entries

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), LogIndex{1});
    EXPECT_EQ(entry->logTerm(), LogTerm{1});
    ASSERT_NE(entry->logPayload(), nullptr);
    EXPECT_EQ(*entry->logPayload(),
              LogPayload::createFromString("first entry"));
    // This is the leader entry inserted in becomeLeader
    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), LogIndex{2});
    EXPECT_EQ(entry->logTerm(), LogTerm{3});
    EXPECT_EQ(entry->logPayload(), nullptr)
        << entry->logPayload()->slice().toJson();

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), LogIndex{3});
    EXPECT_EQ(entry->logTerm(), LogTerm{3});
    ASSERT_NE(entry->logPayload(), nullptr);
    EXPECT_EQ(*entry->logPayload(),
              LogPayload::createFromString("new second entry"));
    entry = iter->next();
    EXPECT_FALSE(entry.has_value());
#endif
  }
}
