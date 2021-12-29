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

#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogLeader.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

struct AppendEntriesBatchTest : ReplicatedLogTest {};

TEST_F(AppendEntriesBatchTest, test_with_two_batches) {
  auto leaderLog = std::invoke([&] {
    auto persistedLog = makePersistedLog(LogId{1});
    for (size_t i = 1; i <= 2000; i++) {
      persistedLog->setEntry(LogIndex{i}, LogTerm{4},
                             LogPayload::createFromString("log entry"));
    }
    return std::make_shared<TestReplicatedLog>(
        std::make_unique<LogCore>(persistedLog), _logMetricsMock,
        LoggerContext(Logger::REPLICATION2));
  });

  auto followerLog = makeReplicatedLog(LogId{1});
  auto follower = followerLog->becomeFollower("follower", LogTerm{4}, "leader");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{4}, {follower}, 2);

  {
    auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant());
    EXPECT_EQ(stats.local.spearHead, TermIndexPair(LogTerm{4}, LogIndex{2000}));
    EXPECT_EQ(stats.local.commitIndex, LogIndex{0});
    EXPECT_EQ(stats.follower.at("follower").spearHead.index, LogIndex{1999});
  }
  {
    auto stats = std::get<FollowerStatus>(follower->getStatus().getVariant());
    EXPECT_EQ(stats.local.spearHead.index, LogIndex{0});
    EXPECT_EQ(stats.local.commitIndex, LogIndex{0});
  }

  leader->triggerAsyncReplication();
  ASSERT_TRUE(follower->hasPendingAppendEntries());

  {
    std::size_t num_requests = 0;
    while (follower->hasPendingAppendEntries()) {
      follower->runAsyncAppendEntries();
      num_requests += 1;
    }

    // 0. Decrement lastAckedIndex to 0
    // 1. AppendEntries 1..1000
    // 2. AppendEntries 2..2000
    // 3. AppendEntries CommitIndex
    // 4. AppendEntries LCI
    EXPECT_EQ(num_requests, 3 + 1 + 1);
  }

  {
    auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant());
    EXPECT_EQ(stats.local.spearHead.index, LogIndex{2000});
    EXPECT_EQ(stats.local.commitIndex, LogIndex{2000});
  }
  {
    auto stats = std::get<FollowerStatus>(follower->getStatus().getVariant());
    EXPECT_EQ(stats.local.spearHead.index, LogIndex{2000});
    EXPECT_EQ(stats.local.commitIndex, LogIndex{2000});
  }
}
