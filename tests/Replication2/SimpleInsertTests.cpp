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

#include "Replication2/ReplicatedLog/LogCore.h"
#include "TestHelper.h"
#include "Replication2/ReplicatedLog/rtypes.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

TEST_F(ReplicatedLogTest, write_single_entry_to_follower) {
  auto coreA = makeLogCore(LogId{1});
  auto coreB = makeLogCore(LogId{2});

  auto leaderId = ParticipantId{"leader"};
  auto followerId = ParticipantId{"follower"};

  auto follower = std::make_shared<DelayedFollowerLog>(followerId, std::move(coreB),
                                                       LogTerm{1}, leaderId);
  auto leader = LogLeader::construct(leaderId, std::move(coreA), LogTerm{1},
                           std::vector<std::shared_ptr<AbstractFollower>>{follower}, 2);

  {
    // Nothing written on the leader
    auto status = std::get<LeaderStatus>(leader->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{0});
    EXPECT_EQ(status.local.spearHead, LogIndex{0});
  }
  {
    // Nothing written on the follower
    auto status = std::get<FollowerStatus>(follower->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{0});
    EXPECT_EQ(status.local.spearHead, LogIndex{0});
  }

  {
    // Insert first entry on the follower, expect the spearhead to be one
    auto idx = leader->insert(LogPayload{"first entry"});
    {
      auto status = std::get<LeaderStatus>(leader->getStatus());
      EXPECT_EQ(status.local.commitIndex, LogIndex{0});
      EXPECT_EQ(status.local.spearHead, LogIndex{1});
    }
    {
      auto status = std::get<FollowerStatus>(follower->getStatus());
      EXPECT_EQ(status.local.commitIndex, LogIndex{0});
      EXPECT_EQ(status.local.spearHead, LogIndex{0});
    }
    auto f = leader->waitFor(idx);
    EXPECT_FALSE(f.isReady());

    // Nothing sent to the follower yet, but only after runAsyncStep
    EXPECT_FALSE(follower->hasPendingAppendEntries());
    leader->runAsyncStep();
    EXPECT_TRUE(follower->hasPendingAppendEntries());

    {
      // check the leader log, there should be one entry written
      auto entry = std::optional<LogEntry>{};
      auto followerLog = getPersistedLogById(LogId{1});
      auto iter = followerLog->read(LogIndex{1});

      entry = iter->next();
      EXPECT_TRUE(entry.has_value())
          << "expect one entry in leader log, found nothing";
      if (entry.has_value()) {
        EXPECT_EQ(entry->logIndex(), LogIndex{1});
        EXPECT_EQ(entry->logTerm(), LogTerm{1});
        EXPECT_EQ(entry->logPayload(), LogPayload{"first entry"});
      }

      entry = iter->next();
      EXPECT_FALSE(entry.has_value());
    }

    // Run async step, now the future should be fulfilled
    EXPECT_FALSE(f.isReady());
    follower->runAsyncAppendEntries();
    EXPECT_TRUE(f.isReady());

    {
      // Leader commit index is 1
      auto status = std::get<LeaderStatus>(leader->getStatus());
      EXPECT_EQ(status.local.commitIndex, LogIndex{1});
      EXPECT_EQ(status.local.spearHead, LogIndex{1});
    }
    {
      // Follower has spearhead 1 and commitIndex still 0
      auto status = std::get<FollowerStatus>(follower->getStatus());
      EXPECT_EQ(status.local.commitIndex, LogIndex{0});
      EXPECT_EQ(status.local.spearHead, LogIndex{1});
    }

    {
      // check the follower log, there should be one entry written
      auto entry = std::optional<LogEntry>{};
      auto followerLog = getPersistedLogById(LogId{2});
      auto iter = followerLog->read(LogIndex{1});

      entry = iter->next();
      ASSERT_TRUE(entry.has_value())
          << "expect one entry in follower log, found nothing";
      EXPECT_EQ(entry->logIndex(), LogIndex{1});
      EXPECT_EQ(entry->logTerm(), LogTerm{1});
      EXPECT_EQ(entry->logPayload(), LogPayload{"first entry"});

      entry = iter->next();
      EXPECT_FALSE(entry.has_value());
    }

    {
      // Expect the quorum to consist of the follower only
      ASSERT_TRUE(f.isReady());
      auto quorum = f.get();
      EXPECT_EQ(quorum->index, LogIndex{1});
      EXPECT_EQ(quorum->term, LogTerm{1});
      EXPECT_EQ(quorum->quorum, (std::vector<ParticipantId>{leaderId, followerId}));
    }

    // Follower should have pending append entries
    // containing the commitIndex update
    EXPECT_TRUE(follower->hasPendingAppendEntries());
    follower->runAsyncAppendEntries();

    {
      // Follower has commitIndex 1
      auto status = std::get<FollowerStatus>(follower->getStatus());
      EXPECT_EQ(status.local.commitIndex, LogIndex{1});
      EXPECT_EQ(status.local.spearHead, LogIndex{1});
    }

    EXPECT_FALSE(follower->hasPendingAppendEntries());
  }
}

TEST_F(ReplicatedLogTest, wake_up_as_leader_with_persistent_data) {
  auto const entries = {
      replication2::LogEntry(LogTerm{1}, LogIndex{1}, LogPayload{"first entry"}),
      replication2::LogEntry(LogTerm{1}, LogIndex{2}, LogPayload{"second entry"}),
      replication2::LogEntry(LogTerm{2}, LogIndex{3}, LogPayload{"third entry"})};

  auto coreA = std::unique_ptr<LogCore>(nullptr);
  {
    auto leaderLog = makePersistedLog(LogId{1});
    for (auto const& entry : entries) {
      leaderLog->setEntry(entry);
    }
    coreA = std::make_unique<LogCore>(leaderLog);
  }

  auto leaderId = ParticipantId{"leader"};
  auto followerId = ParticipantId{"follower"};


  auto coreB = makeLogCore(LogId{2});
  auto follower = std::make_shared<DelayedFollowerLog>(followerId, std::move(coreB),
                                                       LogTerm{3}, leaderId);
  auto leader =
      LogLeader::construct(leaderId, std::move(coreA), LogTerm{3},
                           std::vector<std::shared_ptr<AbstractFollower>>{follower}, 1);

  {
    // Leader should know it spearhead, but commitIndex is 0
    auto status = std::get<LeaderStatus>(leader->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{0});
    EXPECT_EQ(status.local.spearHead, LogIndex{3});
  }
  {
    // Nothing written on the follower
    auto status = std::get<FollowerStatus>(follower->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{0});
    EXPECT_EQ(status.local.spearHead, LogIndex{0});
  }

  // Nothing should be ready
  auto f = leader->waitFor(LogIndex{3});
  EXPECT_FALSE(f.isReady());

  // this should trigger a sendAppendEntries to all follower
  EXPECT_FALSE(follower->hasPendingAppendEntries());
  leader->runAsyncStep();
  EXPECT_TRUE(follower->hasPendingAppendEntries());
  {
    std::size_t number_of_runs = 0;
    while (follower->hasPendingAppendEntries()) {
      follower->runAsyncAppendEntries();
      number_of_runs += 1;
    }
    // AppendEntries with prevLogIndex 2 -> success = false
    // AppendEntries with prevLogIndex 1 -> success = false
    // AppendEntries with prevLogIndex 0 -> success = true
    // AppendEntries with new commitIndex
    EXPECT_EQ(number_of_runs, 4);
  }

  {
    // Leader has replicated all 3 entries
    auto status = std::get<LeaderStatus>(leader->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{3});
    EXPECT_EQ(status.local.spearHead, LogIndex{3});
  }
  {
    // Follower knows that everything is replicated
    auto status = std::get<FollowerStatus>(follower->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{3});
    EXPECT_EQ(status.local.spearHead, LogIndex{3});
  }

  {
    // check that the follower has all log entries in its store
    auto iter = getPersistedLogById(LogId{2})->read(LogIndex{0});
    for (auto const& test : entries) {
      auto follower_entry = iter->next();
      ASSERT_TRUE(follower_entry.has_value());
      EXPECT_EQ(follower_entry.value(), test);
    }
  }

}


TEST_F(ReplicatedLogTest, multiple_follower) {

  auto coreA = makeLogCore(LogId{1});
  auto coreB = makeLogCore(LogId{2});
  auto coreC = makeLogCore(LogId{3});

  auto leaderId = ParticipantId{"leader"};
  auto followerId_1 = ParticipantId{"follower1"};
  auto followerId_2 = ParticipantId{"follower1"};

  auto follower_1 = std::make_shared<DelayedFollowerLog>(followerId_1, std::move(coreB),
                                                         LogTerm{1}, leaderId);
  auto follower_2 = std::make_shared<DelayedFollowerLog>(followerId_2, std::move(coreC),
                                                         LogTerm{1}, leaderId);
  // create leader with write concern 2
  auto leader =
      LogLeader::construct(leaderId, std::move(coreA), LogTerm{1},
                           std::vector<std::shared_ptr<AbstractFollower>>{follower_1, follower_2},
                           3);

  auto index = leader->insert(LogPayload{"first entry"});
  auto future = leader->waitFor(index);
  EXPECT_FALSE(future.isReady());

  {
    // Leader has spearhead at 1 but not committed
    auto status = std::get<LeaderStatus>(leader->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{0});
    EXPECT_EQ(status.local.spearHead, LogIndex{1});
  }
  {
    // Follower has nothing
    auto status = std::get<FollowerStatus>(follower_1->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{0});
    EXPECT_EQ(status.local.spearHead, LogIndex{0});
  }
  {
    // Follower has nothing
    auto status = std::get<FollowerStatus>(follower_2->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{0});
    EXPECT_EQ(status.local.spearHead, LogIndex{0});
  }

  // sendAppendEntries
  leader->runAsyncStep();
  EXPECT_TRUE(follower_1->hasPendingAppendEntries());
  EXPECT_TRUE(follower_2->hasPendingAppendEntries());

  // follower 1 answers AppendEntries request
  follower_1->runAsyncAppendEntries();
  // We do not expect any requests pending
  EXPECT_FALSE(follower_1->hasPendingAppendEntries());
  EXPECT_FALSE(future.isReady());
  {
    // Leader has spearhead at 1 but not committed
    auto status = std::get<LeaderStatus>(leader->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{0});
    EXPECT_EQ(status.local.spearHead, LogIndex{1});
  }
  {
    // Follower has written 1 entry but not committed
    auto status = std::get<FollowerStatus>(follower_1->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{0});
    EXPECT_EQ(status.local.spearHead, LogIndex{1});
  }
  {
    // Follower has nothing
    auto status = std::get<FollowerStatus>(follower_2->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{0});
    EXPECT_EQ(status.local.spearHead, LogIndex{0});
  }

  // handle append entries on second follower
  follower_2->runAsyncAppendEntries();
  // now write concern 2 is reached, future is ready
  // and update of commitIndex on both follower
  {
    ASSERT_TRUE(future.isReady());
    auto quorum = future.get();
    EXPECT_EQ(quorum->term, LogTerm{1});
    EXPECT_EQ(quorum->index, LogIndex{1});
    EXPECT_EQ(quorum->quorum, (std::vector{leaderId, followerId_1, followerId_2}));
  }

  EXPECT_TRUE(follower_1->hasPendingAppendEntries());
  EXPECT_TRUE(follower_2->hasPendingAppendEntries());
  {
    // Leader has committed 1
    auto status = std::get<LeaderStatus>(leader->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{1});
    EXPECT_EQ(status.local.spearHead, LogIndex{1});
  }
  {
    // Follower has written 1 entry but not committed
    auto status = std::get<FollowerStatus>(follower_1->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{0});
    EXPECT_EQ(status.local.spearHead, LogIndex{1});
  }
  {
    // Follower has written 1 entry but not committed
    auto status = std::get<FollowerStatus>(follower_2->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{0});
    EXPECT_EQ(status.local.spearHead, LogIndex{1});
  }

  follower_1->runAsyncAppendEntries();
  EXPECT_FALSE(follower_1->hasPendingAppendEntries());
  follower_2->runAsyncAppendEntries();
  EXPECT_FALSE(follower_2->hasPendingAppendEntries());

  {
    // Leader has committed 1
    auto status = std::get<LeaderStatus>(leader->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{1});
    EXPECT_EQ(status.local.spearHead, LogIndex{1});
  }
  {
    // Follower has committed 1
    auto status = std::get<FollowerStatus>(follower_1->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{1});
    EXPECT_EQ(status.local.spearHead, LogIndex{1});
  }
  {
    // Follower has committed 1
    auto status = std::get<FollowerStatus>(follower_2->getStatus());
    EXPECT_EQ(status.local.commitIndex, LogIndex{1});
    EXPECT_EQ(status.local.spearHead, LogIndex{1});
  }
}
