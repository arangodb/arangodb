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
#include "Replication2/ReplicatedLog/types.h"
#include "Replication2/TestHelper.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

struct RewriteLogTest : ReplicatedLogTest {};

TEST_F(RewriteLogTest, rewrite_old_leader) {
  auto const entries = std::vector{
      replication2::LogEntry(LogTerm{1}, LogIndex{1}, LogPayload{"first entry"}),
      replication2::LogEntry(LogTerm{2}, LogIndex{2}, LogPayload{"second entry"}),
      replication2::LogEntry(LogTerm{2}, LogIndex{3}, LogPayload{"third entry"})};

  // create one log that has three entries
  auto followerLog = std::invoke([&] {
    auto persistedLog = makePersistedLog(LogId{1});
    for (auto const& entry : entries) {
      persistedLog->setEntry(entry);
    }
    return std::make_shared<TestReplicatedLog>(std::make_unique<LogCore>(persistedLog),
                                               _logMetricsMock, defaultLogger());
  });

  // create different log that has only one entry
  auto leaderLog = std::invoke([&] {
    auto persistedLog = makePersistedLog(LogId{2});
    persistedLog->setEntry(entries[0]);
    return std::make_shared<TestReplicatedLog>(std::make_unique<LogCore>(persistedLog),
                                               _logMetricsMock, defaultLogger());
  });

  auto follower = followerLog->becomeFollower("follower", LogTerm{3}, "leader");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{3}, {follower}, 2);

  {
    auto stats = std::get<LeaderStatus>(leader->getStatus()).local;
    EXPECT_EQ(stats.commitIndex, LogIndex{0});
    EXPECT_EQ(stats.spearHead.index, LogIndex{1});
  }
  {
    auto stats = std::get<FollowerStatus>(follower->getStatus()).local;
    EXPECT_EQ(stats.commitIndex, LogIndex{0});
    EXPECT_EQ(stats.spearHead.index, LogIndex{3});
  }
  {
    auto idx = leader->insert(LogPayload{"new second entry"});
    EXPECT_EQ(idx, LogIndex{2});
  }

  {
    auto stats = std::get<LeaderStatus>(leader->getStatus()).local;
    EXPECT_EQ(stats.commitIndex, LogIndex{0});
    EXPECT_EQ(stats.spearHead.index, LogIndex{2});
  }
  {
    auto stats = std::get<FollowerStatus>(follower->getStatus()).local;
    EXPECT_EQ(stats.commitIndex, LogIndex{0});
    EXPECT_EQ(stats.spearHead.index, LogIndex{3});
  }

  // now run the leader
  leader->runAsyncStep();

  // we expect the follower to rewrite its logs
  ASSERT_TRUE(follower->hasPendingAppendEntries());
  {
    std::size_t number_of_runs = 0;
    while (follower->hasPendingAppendEntries()) {
      follower->runAsyncAppendEntries();
      number_of_runs += 1;
    }
    // AppendEntries with prevLogIndex 0 -> success = true
    // AppendEntries with new commitIndex
    EXPECT_EQ(number_of_runs, 2);
  }

  {
    auto stats = std::get<LeaderStatus>(leader->getStatus()).local;
    EXPECT_EQ(stats.commitIndex, LogIndex{2});
    EXPECT_EQ(stats.spearHead.index, LogIndex{2});
  }
  {
    auto stats = std::get<FollowerStatus>(follower->getStatus()).local;
    EXPECT_EQ(stats.commitIndex, LogIndex{2});
    EXPECT_EQ(stats.spearHead.index, LogIndex{2});
  }

  {
    auto entry = std::optional<LogEntry>();
    auto log = getPersistedLogById(LogId{1});
    auto iter = log->read(LogIndex{1});

    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), LogIndex{1});
    EXPECT_EQ(entry->logTerm(), LogTerm{1});
    EXPECT_EQ(entry->logPayload(), LogPayload{"first entry"});
    entry = iter->next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->logIndex(), LogIndex{2});
    EXPECT_EQ(entry->logTerm(), LogTerm{3});
    EXPECT_EQ(entry->logPayload(), LogPayload{"new second entry"});
    entry = iter->next();
    EXPECT_FALSE(entry.has_value());
  }
}
