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
#include "TestHelper.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

struct MultiTermTest : ReplicatedLogTest {};

TEST_F(MultiTermTest, add_follower_test) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  {
    auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {}, 1);
    auto idx = leader->insert(LogPayload::createFromString("first entry"),
                              false, LogLeader::doNotTriggerAsyncReplication);
    auto f = leader->waitFor(idx);
    // Note that the leader inserts an empty log entry in becomeLeader
    ASSERT_EQ(idx, LogIndex{2});
    EXPECT_FALSE(f.Ready());
    leader->triggerAsyncReplication();
    {
      ASSERT_TRUE(f.Ready());
      auto const& result = std::move(f).Get().Ok();
      EXPECT_EQ(result.quorum->quorum, std::vector<ParticipantId>{"leader"});
    }
    {
      auto stats =
          std::get<LeaderStatus>(leader->getStatus().getVariant()).local;
      EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm{1}, LogIndex{2}));
      EXPECT_EQ(stats.commitIndex, LogIndex{2});
    }
  }

  auto followerLog = makeReplicatedLog(LogId{2});
  {
    auto follower =
        followerLog->becomeFollower("follower", LogTerm{2}, "leader");
    auto leader = leaderLog->becomeLeader("leader", LogTerm{2}, {follower}, 2);

    {
      auto stats =
          std::get<LeaderStatus>(leader->getStatus().getVariant()).local;
      // Note that the leader inserts an empty log entry in becomeLeader, which
      // happened twice already.
      EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm{2}, LogIndex{3}));
      EXPECT_EQ(stats.commitIndex, LogIndex{0});
    }

    auto f = leader->waitFor(LogIndex{1});
    EXPECT_FALSE(f.Ready());
    leader->triggerAsyncReplication();
    ASSERT_TRUE(follower->hasPendingAppendEntries());
    while (follower->hasPendingAppendEntries()) {
      follower->runAsyncAppendEntries();
    }

    EXPECT_TRUE(f.Ready());
    {
      auto stats =
          std::get<FollowerStatus>(follower->getStatus().getVariant()).local;
      EXPECT_EQ(stats.spearHead, TermIndexPair(LogTerm{2}, LogIndex{3}));
      EXPECT_EQ(stats.commitIndex, LogIndex{3});
    }
  }
}

TEST_F(MultiTermTest, resign_leader_wait_for) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto followerLog = makeReplicatedLog(LogId{2});
  {
    auto follower =
        followerLog->becomeFollower("follower", LogTerm{1}, "leader");
    auto leader = leaderLog->becomeLeader("leader", LogTerm{1}, {follower}, 2);

    auto idx = leader->insert(LogPayload::createFromString("first entry"),
                              false, LogLeader::doNotTriggerAsyncReplication);
    auto f = leader->waitFor(idx);
    EXPECT_FALSE(f.Ready());
    leader->triggerAsyncReplication();
    auto newLeader =
        leaderLog->becomeLeader("leader", LogTerm{2}, {follower}, 2);
    ASSERT_TRUE(f.Ready());
    EXPECT_ANY_THROW({ std::ignore = std::move(f).Get().Ok(); });
    EXPECT_ANY_THROW({ std::ignore = leader->getStatus(); });
    EXPECT_ANY_THROW({
      std::ignore =
          leader->insert(LogPayload::createFromString("second entry"), false,
                         LogLeader::doNotTriggerAsyncReplication);
    });
  }
}

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
    EXPECT_FALSE(f.Ready());
    leader->triggerAsyncReplication();
    follower->runAsyncAppendEntries();
    auto newLeader =
        leaderLog->becomeLeader("leader", LogTerm{2}, {follower}, 2);
    ASSERT_TRUE(f.Ready());
    ASSERT_FALSE(std::as_const(f).Touch().State() ==
                 yaclib::ResultState::Exception);
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
    EXPECT_FALSE(f.Ready());
    leader->triggerAsyncReplication();
    follower->runAllAsyncAppendEntries();
    auto newFollower =
        followerLog->becomeFollower("follower", LogTerm{2}, "leader");
    ASSERT_TRUE(f.Ready());
    ASSERT_FALSE(std::as_const(f).Touch().State() ==
                 yaclib::ResultState::Exception);
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
    EXPECT_FALSE(f.Ready());
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
      -> yaclib::Future<AppendEntriesResult> override {
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
    EXPECT_FALSE(f.Ready());
    leader->triggerAsyncReplication();
    EXPECT_FALSE(f.Ready());

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
    ASSERT_TRUE(f.Ready());
    EXPECT_TRUE(std::as_const(f).Touch().State() ==
                yaclib::ResultState::Exception);

    auto f2 = leader->waitFor(idx);
    EXPECT_FALSE(f2.Ready());
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

    ASSERT_FALSE(f2.Ready());
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

    ASSERT_TRUE(f2.Ready());
    {
      auto result = std::move(f2).Get().Ok();
      EXPECT_EQ(result.currentCommitIndex, LogIndex{3});
      EXPECT_EQ(result.quorum->index, LogIndex{3});
      EXPECT_EQ(result.quorum->term, LogTerm{2});
    }
  }
}
