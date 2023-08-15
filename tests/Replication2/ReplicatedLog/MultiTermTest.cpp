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

#include <utility>

#include "Replication2/ReplicatedLog/types.h"
#include "Replication2/Helper/ReplicatedLogTestSetup.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

struct MultiTermTest : ReplicatedLogTest {};

TEST_F(MultiTermTest, add_follower_test) {
  // TODO Add expectations for internalStatus calls
  auto leaderLogContainer = makeLogWithFakes({});
  auto leaderLog = leaderLogContainer.log;
  auto config = makeConfig(leaderLogContainer, {}, {.term = 1_T});

  config.installConfig(true);
  {
    auto idx = leaderLogContainer.stateHandleMock->logLeaderMethods->insert(
        LogPayload::createFromString("first entry"), false);
    auto f = leaderLog->getParticipant()->waitFor(idx);
    // Note that the leader inserts a first log entry to establish leadership
    ASSERT_EQ(idx, LogIndex{2});
    EXPECT_FALSE(f.isReady());
    leaderLogContainer.runAll();
    {
      ASSERT_TRUE(f.isReady());
      auto const& result = f.get();
      EXPECT_THAT(
          result.quorum->quorum,
          testing::ElementsAre(leaderLogContainer.serverInstance.serverId));
    }
    {
      auto stats = leaderLog->getQuickStatus().local.value();
      EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm{1}, LogIndex{2}));
      EXPECT_EQ(stats.commitIndex, LogIndex{2});
    }
  }

  auto followerLogContainer = makeLogWithFakes({});
  auto followerLog = followerLogContainer.log;
  EXPECT_CALL(*followerLogContainer.stateHandleMock, updateCommitIndex)
      .Times(testing::AtLeast(1));

  // TODO Move the config manipulation into a function on LogConfig.
  //      Maybe move the makeConfig code into a LogConfig constructor.
  config =
      makeConfig(leaderLogContainer, {followerLogContainer}, {.term = 2_T});

  config.installConfig(false);
  {
    {
      auto stats = leaderLog->getQuickStatus().local.value();
      // Note that the leader inserts an empty log entry in becomeLeader, which
      // happened twice already.
      EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm{2}, LogIndex{3}));
      EXPECT_EQ(stats.commitIndex, LogIndex{0});
    }

    auto f = leaderLog->getParticipant()->waitFor(LogIndex{1});
    EXPECT_FALSE(f.isReady());
    IHasScheduler::runAll(leaderLogContainer, followerLogContainer);
    EXPECT_TRUE(f.isReady());
    {
      auto stats = followerLog->getQuickStatus().local.value();
      EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm{2}, LogIndex{3}));
      EXPECT_EQ(stats.commitIndex, LogIndex{3});
    }
    auto entries = PartialLogEntries{
        {.term = 1_T, .index = 1_Lx, .payload = PartialLogEntry::IsMeta{}},
        {.term = 1_T, .index = 2_Lx, .payload = PartialLogEntry::IsPayload{}},
        {.term = 2_T, .index = 3_Lx, .payload = PartialLogEntry::IsMeta{}},
    };
    auto expectedEntries = testing::Pointwise(MatchesMapLogEntry(), entries);
    EXPECT_THAT(leaderLogContainer.storageContext->log, expectedEntries);
    EXPECT_THAT(followerLogContainer.storageContext->log, expectedEntries);
  }
}

TEST_F(MultiTermTest, resign_leader_wait_for) {
  // TODO Add expectations for internalStatus calls
  auto leaderLogContainer = makeLogWithFakes({});
  auto leaderLog = leaderLogContainer.log;
  auto followerLogContainer = makeLogWithFakes({});
  auto followerLog = followerLogContainer.log;
  auto config = makeConfig(leaderLogContainer, {followerLogContainer},
                           {.term = 1_T, .writeConcern = 2});

  config.installConfig(true);
  {
    auto idx = leaderLogContainer.stateHandleMock->logLeaderMethods->insert(
        LogPayload::createFromString("first entry"), false);
    auto f = leaderLog->getParticipant()->waitFor(idx);
    EXPECT_FALSE(f.isReady());
    leaderLogContainer.runAll();
    // note we don't run the follower, so the leader can't commit the entry
    auto oldLeader = leaderLog->getParticipant();
    // TODO Move config manipulation into LogConfig methods
    config = makeConfig(leaderLogContainer, {followerLogContainer},
                        {.term = 2_T, .writeConcern = 2});
    config.installConfig(false);
    ASSERT_TRUE(f.isReady());
    EXPECT_ANY_THROW({ std::ignore = f.get(); });
    EXPECT_ANY_THROW({ std::ignore = oldLeader->getStatus(); });
    EXPECT_EQ(leaderLog->getQuickStatus().local.value().spearHead,
              TermIndexPair(LogTerm{2}, LogIndex{3}));
  }
  // TODO Implement dropWork on IHasScheduler, use it here and drop the
  //      EXPECT_CALL.
  EXPECT_CALL(*followerLogContainer.stateHandleMock, updateCommitIndex)
      .Times(testing::AtLeast(1));
  IHasScheduler::runAll(leaderLogContainer, followerLogContainer);
}

// TODO Reenable the rest as well
#if 0
TEST_F(MultiTermTest, resign_leader_release) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto followerLog = makeReplicatedLog(LogId{2});
  {
    auto follower =
        followerLog->becomeFollower("follower", LogTerm{1}, "leader");
    auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);

    auto idx = leader->insert(LogPayload::createFromString("first entry"),
                              false, LogLeader::doNotTriggerAsyncReplication);
    auto f = leader->waitFor(idx);
    EXPECT_FALSE(f.isReady());
    leader->triggerAsyncReplication();
    follower->runAsyncAppendEntries();
    auto newLeader =
        leaderLog->becomeLeader("leader", LogTerm{2}, {follower}, 2);
    ASSERT_TRUE(f.isReady());
    ASSERT_FALSE(f.hasException());
    auto res = leader->release(idx);
    EXPECT_EQ(res.errorNumber(),
              TRI_ERROR_REPLICATION_REPLICATED_LOG_LEADER_RESIGNED);
  }
}

TEST_F(MultiTermTest, resign_follower_release) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto followerLog = makeReplicatedLog(LogId{2});
  {
    auto follower =
        followerLog->becomeFollower("follower", LogTerm{1}, "leader");
    auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);

    auto idx = leader->insert(LogPayload::createFromString("first entry"),
                              false, LogLeader::doNotTriggerAsyncReplication);
    auto f = follower->waitFor(idx);
    EXPECT_FALSE(f.isReady());
    leader->triggerAsyncReplication();
    follower->runAllAsyncAppendEntries();
    auto newFollower =
        followerLog->becomeFollower("follower", LogTerm{2}, "leader");
    ASSERT_TRUE(f.isReady());
    ASSERT_FALSE(f.hasException());
    auto res = follower->release(idx);
    EXPECT_EQ(res.errorNumber(),
              TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED);
  }
}

TEST_F(MultiTermTest, resign_follower_wait_for) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto followerLog = makeReplicatedLog(LogId{2});
  {
    auto follower =
        followerLog->becomeFollower("follower", LogTerm{1}, "leader");
    auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);

    auto idx = leader->insert(LogPayload::createFromString("first entry"),
                              false, LogLeader::doNotTriggerAsyncReplication);
    auto f = leader->waitFor(idx);
    EXPECT_FALSE(f.isReady());
    leader->triggerAsyncReplication();

    {
      auto stats =
          std::get<LeaderStatus>(leader->getStatus().getVariant()).local;
      // Note that the leader inserts an empty log entry in becomeLeader
      EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm{1}, LogIndex{2}));
      EXPECT_EQ(stats.commitIndex, LogIndex{0});
    }

    EXPECT_TRUE(follower->hasPendingAppendEntries());
    auto newFollower =
        followerLog->becomeFollower("follower", LogTerm{2}, "leader");

    // now run the old followers append entry requests
    follower->runAsyncAppendEntries();
    // we expect a leader retry
    EXPECT_TRUE(follower->hasPendingAppendEntries());
    EXPECT_ANY_THROW({ std::ignore = follower->getStatus(); });

    // now create a new leader
    auto newLeader =
        leaderLog->becomeLeader("leader", LogTerm{2}, {newFollower}, 2);
    newLeader->triggerAsyncReplication();
    EXPECT_TRUE(newFollower->hasPendingAppendEntries());

    // run the old followers append entries
    follower->runAsyncAppendEntries();
    // we expect no new append entries
    EXPECT_FALSE(follower->hasPendingAppendEntries());

    while (newFollower->hasPendingAppendEntries()) {
      newFollower->runAsyncAppendEntries();
    }

    {
      auto stats =
          std::get<FollowerStatus>(newFollower->getStatus().getVariant()).local;
      // Note that the leader inserts an empty log entry in becomeLeader, which
      // happened twice already.
      EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm{2}, LogIndex{3}));
      EXPECT_EQ(stats.commitIndex, LogIndex{3});
    }
  }
}

struct FollowerProxy : AbstractFollower {
  explicit FollowerProxy(std::shared_ptr<AbstractFollower> follower)
      : _follower(std::move(follower)) {}

  void replaceFollower(std::shared_ptr<AbstractFollower> newFollower) {
    _follower = std::move(newFollower);
  }
  auto getParticipantId() const noexcept -> ParticipantId const& override {
    return _follower->getParticipantId();
  }
  auto appendEntries(AppendEntriesRequest request)
      -> arangodb::futures::Future<AppendEntriesResult> override {
    return _follower->appendEntries(std::move(request));
  }

 private:
  std::shared_ptr<AbstractFollower> _follower;
};

TEST_F(MultiTermTest, resign_leader_append_entries) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto followerLog = makeReplicatedLog(LogId{2});
  {
    auto follower =
        followerLog->becomeFollower("follower", LogTerm{1}, "leader");
    auto followerProxy = std::make_shared<FollowerProxy>(follower);

    auto leader =
        leaderLog->becomeLeader("leader", LogTerm{1}, {followerProxy}, 2);

    auto idx = leader->insert(LogPayload::createFromString("first entry"),
                              false, LogLeader::doNotTriggerAsyncReplication);
    auto f = leader->waitFor(idx);
    EXPECT_FALSE(f.isReady());
    leader->triggerAsyncReplication();
    EXPECT_FALSE(f.isReady());

    {
      auto stats =
          std::get<LeaderStatus>(leader->getStatus().getVariant()).local;
      // Note that the leader inserts an empty log entry in becomeLeader
      EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm{1}, LogIndex{2}));
      EXPECT_EQ(stats.commitIndex, LogIndex{0});
    }

    // now create a new leader and delete the old one
    leader =
        leaderLog->becomeLeader("newLeader", LogTerm{2}, {followerProxy}, 2);

    EXPECT_TRUE(follower->hasPendingAppendEntries());
    follower->runAsyncAppendEntries();
    // the old leader is already gone, so we expect no new append entries
    EXPECT_FALSE(follower->hasPendingAppendEntries());

    // the old future should have failed
    ASSERT_TRUE(f.isReady());
    EXPECT_TRUE(f.hasException());

    auto f2 = leader->waitFor(idx);
    EXPECT_FALSE(f2.isReady());
    leader->triggerAsyncReplication();

    // run the old followers append entries
    follower->runAsyncAppendEntries();
    // we expect a retry request
    EXPECT_TRUE(follower->hasPendingAppendEntries());
    auto newFollower =
        followerLog->becomeFollower("newFollower", LogTerm{2}, "newLeader");
    // simulate the database server has updated its follower
    followerProxy->replaceFollower(newFollower);

    EXPECT_TRUE(follower->hasPendingAppendEntries());
    follower->runAsyncAppendEntries();
    EXPECT_FALSE(follower->hasPendingAppendEntries());

    ASSERT_FALSE(f2.isReady());
    while (newFollower->hasPendingAppendEntries()) {
      newFollower->runAsyncAppendEntries();
    }

    {
      auto stats =
          std::get<FollowerStatus>(newFollower->getStatus().getVariant()).local;
      // Note that the leader inserts an empty log entry in becomeLeader, which
      // happened twice already.
      EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm{2}, LogIndex{3}));
      EXPECT_EQ(stats.commitIndex, LogIndex{3});
    }

    ASSERT_TRUE(f2.isReady());
    {
      auto result = f2.get();
      EXPECT_EQ(result.currentCommitIndex, LogIndex{3});
      EXPECT_EQ(result.quorum->index, LogIndex{3});
      EXPECT_EQ(result.quorum->term, LogTerm{2});
    }
  }
}
#endif
