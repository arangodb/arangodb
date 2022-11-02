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

#include <algorithm>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::algorithms;
using namespace arangodb::replication2::test;

struct UpdateParticipantsFlagsTest : ReplicatedLogTest {
  void runAllAsyncAppendEntries() {
    while (std::any_of(followers.begin(), followers.end(),
                       [](auto const& follower) {
                         return follower->hasPendingAppendEntries();
                       })) {
      for (auto const& follower : followers) {
        follower->runAsyncAppendEntries();
      }
    }
  }

  LogId const logId = LogId{1};
  LogTerm const startTerm = LogTerm{4};

  std::shared_ptr<TestReplicatedLog> leaderLog = makeReplicatedLog(logId);
  std::shared_ptr<TestReplicatedLog> followerLog1 = makeReplicatedLog(logId);
  std::shared_ptr<TestReplicatedLog> followerLog2 = makeReplicatedLog(logId);

  std::shared_ptr<DelayedFollowerLog> follower1 =
      followerLog1->becomeFollower("follower1", startTerm, "leader");
  std::shared_ptr<DelayedFollowerLog> follower2 =
      followerLog2->becomeFollower("follower2", startTerm, "leader");
  std::shared_ptr<replicated_log::LogLeader> leader =
      leaderLog->becomeLeader("leader", startTerm, {follower1, follower2},
                              /* write concern = */ 2);

  std::vector<std::shared_ptr<DelayedFollowerLog>> followers = {follower1,
                                                                follower2};
};

TEST_F(UpdateParticipantsFlagsTest, wc2_but_server_forced) {
  // This test creates three participants with wc = 2.
  // Then it establishes leadership. After that, it updates the
  // participant configuration such that follower2 is forced.
  // After that we only run the leader and follower1 and expect
  // the log entry not to be committed.

  leader->triggerAsyncReplication();
  runAllAsyncAppendEntries();
  ASSERT_TRUE(leader->isLeadershipEstablished());

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 1);
    EXPECT_EQ(committed, 1);
  }

  auto idx = leader->insert(LogPayload::createFromString("entry #1"));
  // let only commit Follower1
  follower1->runAllAsyncAppendEntries();
  EXPECT_GE(leader->getCommitIndex(), idx);
  runAllAsyncAppendEntries();

  {
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
    newConfig->generation = 2;
    // make follower2 forced
    newConfig->participants["follower2"] =
        replication2::ParticipantFlags{true, false};
    leader->updateParticipantsConfig(newConfig, nullptr);
  }

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 2);
    EXPECT_EQ(committed, 1);
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

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 2);
    EXPECT_EQ(committed, 2);
  }
}

TEST_F(UpdateParticipantsFlagsTest, wc2_but_server_excluded) {
  // This test creates three participants with wc = 2.
  // Then it establishes leadership. After that, it updates the
  // participant configuration such that follower1 is excluded.
  // After that we only run the leader and follower1 and expect
  // the log entry not to be committed.
  leader->triggerAsyncReplication();
  runAllAsyncAppendEntries();
  ASSERT_TRUE(leader->isLeadershipEstablished());

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 1);
    EXPECT_EQ(committed, 1);
  }

  auto idx = leader->insert(LogPayload::createFromString("entry #1"));
  // let only commit Follower1
  follower1->runAllAsyncAppendEntries();
  EXPECT_GE(leader->getCommitIndex(), idx);
  runAllAsyncAppendEntries();

  {
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
    newConfig->generation = 2;
    // make follower1 excluded
    newConfig->participants["follower1"] = replication2::ParticipantFlags{
        .forced = false, .allowedInQuorum = false};
    leader->updateParticipantsConfig(newConfig, nullptr);
  }

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 2);
    EXPECT_EQ(committed, 1);
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

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 2);
    EXPECT_EQ(committed, 2);
  }
}

TEST_F(UpdateParticipantsFlagsTest,
       wc2_but_server_excluded_leadership_is_established) {
  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 1);
    EXPECT_EQ(committed, std::nullopt);
  }

  {
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
    newConfig->generation = 2;
    // make follower1 excluded
    newConfig->participants["follower1"] =
        replication2::ParticipantFlags{.allowedInQuorum = false};
    leader->updateParticipantsConfig(newConfig, nullptr);
  }

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 2);
    EXPECT_EQ(committed, std::nullopt);
  }

  auto idx2 = leader->insert(LogPayload::createFromString("entry #2"));
  // let only commit Follower1
  follower1->runAllAsyncAppendEntries();
  // entry should not be committed because follower1 is excluded
  // although wc is 2
  EXPECT_NE(leader->getCommitIndex(), idx2);
  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 2);
    EXPECT_EQ(committed, std::nullopt);
  }

  // now run both followers
  runAllAsyncAppendEntries();
  // the entry should now be committed
  EXPECT_GE(leader->getCommitIndex(), idx2);

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 2);
    EXPECT_EQ(committed, 2);
  }
}

TEST_F(UpdateParticipantsFlagsTest, multiple_updates_check) {
  // Here first update the config such that one follower is forced.
  // This config is never committed. We then change it back, such that
  // the follower is no longer forced and we can commit again.
  // The generation should be 2 at the end of the test.

  leader->triggerAsyncReplication();
  runAllAsyncAppendEntries();
  ASSERT_TRUE(leader->isLeadershipEstablished());

  // Force follower 2
  {
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
    newConfig->generation = 2;
    // make follower2 forced
    newConfig->participants["follower2"] =
        replication2::ParticipantFlags{true, false};
    leader->updateParticipantsConfig(newConfig, nullptr);
  }

  auto idx = leader->insert(LogPayload::createFromString("entry #1"));
  // let only commit Follower1
  follower1->runAllAsyncAppendEntries();
  // entry should not be committed because follower2 is forced
  // although wc is 2
  EXPECT_NE(leader->getCommitIndex(), idx);

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 2);
    EXPECT_EQ(committed, 1);
  }

  // change configuration back to non-forced follower 2
  {
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
    newConfig->participants["follower2"] = {};
    newConfig->generation = 3;
    leader->updateParticipantsConfig(newConfig, nullptr);
  }

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 3);
    EXPECT_EQ(committed, 1);
  }

  auto idx2 = leader->insert(LogPayload::createFromString("entry #2"));
  // let only commit Follower1
  follower1->runAllAsyncAppendEntries();
  // The entry should be committed now
  EXPECT_EQ(leader->getCommitIndex(), idx2);

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 3);
    EXPECT_EQ(committed, 3);
  }
}

TEST_F(UpdateParticipantsFlagsTest, update_without_additional_entry) {
  // Check the configuration is eventually committed even if the user
  // does not write additional entries.
  leader->triggerAsyncReplication();
  runAllAsyncAppendEntries();
  ASSERT_TRUE(leader->isLeadershipEstablished());

  // Force follower 2
  {
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
    newConfig->generation = 2;
    // make follower2 excluded
    newConfig->participants["follower2"] =
        replication2::ParticipantFlags{true, false};
    leader->updateParticipantsConfig(newConfig, nullptr);
  }

  EXPECT_EQ(leader->getCommitIndex(), LogIndex{1});

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 2);
    EXPECT_EQ(committed, 1);
  }
  // now run all followers
  runAllAsyncAppendEntries();

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 2);
    EXPECT_EQ(committed, 2);
  }
}

TEST_F(UpdateParticipantsFlagsTest, wc2_add_new_follower) {
  leader->triggerAsyncReplication();
  runAllAsyncAppendEntries();
  ASSERT_TRUE(leader->isLeadershipEstablished());

  // create follower3
  auto followerLog3 = makeReplicatedLog(logId);
  auto follower3 = std::shared_ptr<DelayedFollowerLog>{};
  follower3 = followerLog3->becomeFollower("follower3", startTerm, "leader");
  followers.emplace_back(follower3);
  auto const buildFollower = [&](ParticipantId const& participantId) {
    EXPECT_EQ("follower3", participantId);
    return follower3;
  };

  {  // First add the new follower3
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
    newConfig->participants["follower3"] = {};
    newConfig->generation = 2;

    // note that this adds a new log entry
    leader->updateParticipantsConfig(newConfig, buildFollower);
  }

  {  // checks
    EXPECT_EQ(leader->getParticipantConfigGenerations(),
              (std::pair<std::size_t, std::optional<std::size_t>>(2, 1)));
    EXPECT_EQ(leader->getCommitIndex(), LogIndex{1});

    follower3->runAllAsyncAppendEntries();

    EXPECT_EQ(leader->getParticipantConfigGenerations(),
              (std::pair<std::size_t, std::optional<std::size_t>>(2, 2)));
    EXPECT_EQ(leader->getCommitIndex(), LogIndex{2});

    auto fut = leader->waitFor(LogIndex{2});
    ASSERT_TRUE(fut.isReady());
    auto const& quorumData = *fut.get().quorum;
    EXPECT_EQ(quorumData.index, LogIndex{2});
    EXPECT_EQ(quorumData.term, startTerm);
    // follower 3 should now be part of the quorum
    EXPECT_NE(std::find(quorumData.quorum.begin(), quorumData.quorum.end(),
                        "follower3"),
              quorumData.quorum.end());

    // settle remaining followers
    runAllAsyncAppendEntries();
  }
}

TEST_F(UpdateParticipantsFlagsTest,
       wc2_add_new_follower_before_leadership_is_established) {
  // create follower3
  auto followerLog3 = makeReplicatedLog(logId);
  auto follower3 = std::shared_ptr<DelayedFollowerLog>{};
  follower3 = followerLog3->becomeFollower("follower3", startTerm, "leader");
  followers.emplace_back(follower3);
  auto const buildFollower = [&](ParticipantId const& participantId) {
    EXPECT_EQ("follower3", participantId);
    return follower3;
  };

  {  // First add the new follower3
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
    newConfig->generation = 2;
    newConfig->participants["follower3"] = replication2::ParticipantFlags{};

    // note that this adds a new log entry
    leader->updateParticipantsConfig(newConfig, buildFollower);
  }

  {  // checks
    EXPECT_EQ(
        leader->getParticipantConfigGenerations(),
        (std::pair<std::size_t, std::optional<std::size_t>>(2, std::nullopt)));
    EXPECT_EQ(leader->getCommitIndex(), LogIndex{0});

    follower3->runAllAsyncAppendEntries();

    EXPECT_EQ(leader->getParticipantConfigGenerations(),
              (std::pair<std::size_t, std::optional<std::size_t>>(2, 2)));
    EXPECT_EQ(leader->getCommitIndex(), LogIndex{2});

    auto fut = leader->waitFor(LogIndex{2});
    ASSERT_TRUE(fut.isReady());
    auto const& quorumData = *fut.get().quorum;
    EXPECT_EQ(quorumData.index, LogIndex{2});
    EXPECT_EQ(quorumData.term, startTerm);
    // follower 3 should now be part of the quorum
    EXPECT_NE(std::find(quorumData.quorum.begin(), quorumData.quorum.end(),
                        "follower3"),
              quorumData.quorum.end());

    // settle remaining followers
    runAllAsyncAppendEntries();
  }
}

TEST_F(UpdateParticipantsFlagsTest, wc2_remove_exclude_flag) {
  leader->triggerAsyncReplication();
  runAllAsyncAppendEntries();
  ASSERT_TRUE(leader->isLeadershipEstablished());

  // create follower3
  auto followerLog3 = makeReplicatedLog(logId);
  auto follower3 = std::shared_ptr<DelayedFollowerLog>{};
  follower3 = followerLog3->becomeFollower("follower3", startTerm, "leader");
  followers.emplace_back(follower3);
  auto const buildFollower = [&](ParticipantId const& participantId) {
    EXPECT_EQ("follower3", participantId);
    return follower3;
  };

  {  // First add the new follower3, but excluded
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
    newConfig->generation = 2;
    // exclude follower3
    newConfig->participants["follower3"] =
        replication2::ParticipantFlags{.allowedInQuorum = false};

    // note that this adds a new log entry
    leader->updateParticipantsConfig(newConfig, buildFollower);
  }

  {  // checks
    EXPECT_EQ(leader->getParticipantConfigGenerations(),
              (std::pair<std::size_t, std::optional<std::size_t>>(2, 1)));
    EXPECT_EQ(leader->getCommitIndex(), LogIndex{1});

    follower3->runAllAsyncAppendEntries();

    // must not be committed yet, as follower3 is excluded
    EXPECT_EQ(leader->getParticipantConfigGenerations(),
              (std::pair<std::size_t, std::optional<std::size_t>>(2, 1)));
    EXPECT_EQ(leader->getCommitIndex(), LogIndex{1});

    runAllAsyncAppendEntries();

    EXPECT_EQ(leader->getParticipantConfigGenerations(),
              (std::pair<std::size_t, std::optional<std::size_t>>(2, 2)));
    EXPECT_EQ(leader->getCommitIndex(), LogIndex{2});

    auto fut = leader->waitFor(LogIndex{2});
    ASSERT_TRUE(fut.isReady());
    auto const& quorumData = *fut.get().quorum;
    EXPECT_EQ(quorumData.index, LogIndex{2});
    EXPECT_EQ(quorumData.term, startTerm);
    // follower 3 must not be part of the quorum yet
    EXPECT_EQ(std::find(quorumData.quorum.begin(), quorumData.quorum.end(),
                        "follower3"),
              quorumData.quorum.end());
  }

  {  // set follower3.excluded = false; this is the central point of this test!
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
    newConfig->generation = 3;
    // exclude follower3
    auto& flags = (newConfig->participants["follower3"] =
                       oldConfig.participants.at("follower3"));
    flags.allowedInQuorum = true;

    leader->updateParticipantsConfig(newConfig, nullptr);
  }

  {  // checks
    EXPECT_EQ(leader->getParticipantConfigGenerations(),
              (std::pair<std::size_t, std::optional<std::size_t>>(3, 2)));
    EXPECT_EQ(leader->getCommitIndex(), LogIndex{2});

    follower3->runAllAsyncAppendEntries();

    EXPECT_EQ(leader->getParticipantConfigGenerations(),
              (std::pair<std::size_t, std::optional<std::size_t>>(3, 3)));
    EXPECT_EQ(leader->getCommitIndex(), LogIndex{3});

    auto fut = leader->waitFor(LogIndex{3});
    ASSERT_TRUE(fut.isReady());
    auto const& quorumData = *fut.get().quorum;
    EXPECT_EQ(quorumData.index, LogIndex{3});
    EXPECT_EQ(quorumData.term, startTerm);
    // follower 3 should now be part of the quorum
    EXPECT_NE(std::find(quorumData.quorum.begin(), quorumData.quorum.end(),
                        "follower3"),
              quorumData.quorum.end());

    // settle remaining followers
    runAllAsyncAppendEntries();
  }
}

TEST_F(UpdateParticipantsFlagsTest, wc2_remove_follower) {
  leader->triggerAsyncReplication();
  runAllAsyncAppendEntries();
  ASSERT_TRUE(leader->isLeadershipEstablished());

  {  // remove follower1
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
    newConfig->participants.erase("follower1");
    newConfig->generation = 2;

    // remove follower1
    leader->updateParticipantsConfig(newConfig, nullptr);
  }

  {  // checks
    EXPECT_EQ(leader->getParticipantConfigGenerations(),
              (std::pair<std::size_t, std::optional<std::size_t>>(2, 1)));
    EXPECT_EQ(leader->getCommitIndex(), LogIndex{1});

    // run follower1 first
    follower1->runAllAsyncAppendEntries();

    // nothing should have changed, as it was removed from the participants
    EXPECT_EQ(leader->getParticipantConfigGenerations(),
              (std::pair<std::size_t, std::optional<std::size_t>>(2, 1)));
    EXPECT_EQ(leader->getCommitIndex(), LogIndex{1});

    // now run everyone else
    runAllAsyncAppendEntries();

    EXPECT_EQ(leader->getParticipantConfigGenerations(),
              (std::pair<std::size_t, std::optional<std::size_t>>(2, 2)));
    EXPECT_EQ(leader->getCommitIndex(), LogIndex{2});

    auto fut = leader->waitFor(LogIndex{2});
    ASSERT_TRUE(fut.isReady());
    auto const& quorumData = *fut.get().quorum;
    EXPECT_EQ(quorumData.index, LogIndex{2});
    EXPECT_EQ(quorumData.term, startTerm);
    // follower 1 must not be part of the quorum any more
    EXPECT_EQ(std::find(quorumData.quorum.begin(), quorumData.quorum.end(),
                        "follower1"),
              quorumData.quorum.end());

    auto const status = leader->getStatus();
    auto const leaderStatus = status.asLeaderStatus();
    auto keys = std::vector<decltype(leaderStatus->follower)::key_type>{};
    std::transform(leaderStatus->follower.begin(), leaderStatus->follower.end(),
                   std::back_inserter(keys),
                   [&](auto const& it) { return it.first; });
    std::sort(keys.begin(), keys.end());
    EXPECT_EQ(keys, (decltype(keys){"follower2", "leader"}));
  }
}

TEST_F(UpdateParticipantsFlagsTest,
       wc2_remove_follower_before_leadership_is_established) {
  {  // remove follower1
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
    newConfig->participants.erase("follower1");
    newConfig->generation = 2;

    // remove follower1
    leader->updateParticipantsConfig(newConfig, nullptr);
  }

  {  // checks
    EXPECT_EQ(
        leader->getParticipantConfigGenerations(),
        (std::pair<std::size_t, std::optional<std::size_t>>(2, std::nullopt)));
    EXPECT_EQ(leader->getCommitIndex(), LogIndex{0});

    // run follower1 first
    follower1->runAllAsyncAppendEntries();

    // nothing should have changed, as it was removed from the participants
    EXPECT_EQ(
        leader->getParticipantConfigGenerations(),
        (std::pair<std::size_t, std::optional<std::size_t>>(2, std::nullopt)));
    EXPECT_EQ(leader->getCommitIndex(), LogIndex{0});

    // now run everyone else
    runAllAsyncAppendEntries();

    EXPECT_EQ(leader->getParticipantConfigGenerations(),
              (std::pair<std::size_t, std::optional<std::size_t>>(2, 2)));
    EXPECT_EQ(leader->getCommitIndex(), LogIndex{2});

    auto fut = leader->waitFor(LogIndex{2});
    ASSERT_TRUE(fut.isReady());
    auto const& quorumData = *fut.get().quorum;
    EXPECT_EQ(quorumData.index, LogIndex{2});
    EXPECT_EQ(quorumData.term, startTerm);
    // follower 1 must not be part of the quorum any more
    EXPECT_EQ(std::find(quorumData.quorum.begin(), quorumData.quorum.end(),
                        "follower1"),
              quorumData.quorum.end());

    auto const status = leader->getStatus();
    auto const leaderStatus = status.asLeaderStatus();
    auto keys = std::vector<decltype(leaderStatus->follower)::key_type>{};
    std::transform(leaderStatus->follower.begin(), leaderStatus->follower.end(),
                   std::back_inserter(keys),
                   [&](auto const& it) { return it.first; });
    std::sort(keys.begin(), keys.end());
    EXPECT_EQ(keys, (decltype(keys){"follower2", "leader"}));
  }
}

TEST_F(UpdateParticipantsFlagsTest, wc2_add_mismatching_config_should_fail) {
  leader->triggerAsyncReplication();
  runAllAsyncAppendEntries();
  ASSERT_TRUE(leader->isLeadershipEstablished());

  EXPECT_EQ(leader->getCommitIndex(), LogIndex{1});
  EXPECT_EQ(leader->getParticipantConfigGenerations(),
            (std::pair<std::size_t, std::optional<std::size_t>>(1, 1)));

  runAllAsyncAppendEntries();

  // should be unchanged
  EXPECT_EQ(leader->getCommitIndex(), LogIndex{1});
  EXPECT_EQ(leader->getParticipantConfigGenerations(),
            (std::pair<std::size_t, std::optional<std::size_t>>(1, 1)));

  {  // set a new config
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    EXPECT_EQ(oldConfig.generation, 1);
    auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
    newConfig->generation = 2;

    auto logIndex = leader->updateParticipantsConfig(newConfig, nullptr);
    EXPECT_EQ(logIndex, LogIndex{2});
  }

  EXPECT_EQ(leader->getCommitIndex(), LogIndex{1});
  EXPECT_EQ(leader->getParticipantConfigGenerations(),
            (std::pair<std::size_t, std::optional<std::size_t>>(2, 1)));
  runAllAsyncAppendEntries();
  EXPECT_EQ(leader->getCommitIndex(), LogIndex{2});
  EXPECT_EQ(leader->getParticipantConfigGenerations(),
            (std::pair<std::size_t, std::optional<std::size_t>>(2, 2)));

  // should be unchanged
  runAllAsyncAppendEntries();
  EXPECT_EQ(leader->getCommitIndex(), LogIndex{2});
  EXPECT_EQ(leader->getParticipantConfigGenerations(),
            (std::pair<std::size_t, std::optional<std::size_t>>(2, 2)));
}

TEST_F(UpdateParticipantsFlagsTest, check_update_participants_meta_entry) {
  // This test creates three participants with wc = 2.
  // Then it establishes leadership. After that, it updates the
  // participant configuration such that follower2 is forced.
  // After that we only run the leader and follower1 and expect
  // the log entry not to be committed.

  leader->triggerAsyncReplication();
  runAllAsyncAppendEntries();
  ASSERT_TRUE(leader->isLeadershipEstablished());

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 1);
    EXPECT_EQ(committed, 1);
  }

  auto idx = leader->insert(LogPayload::createFromString("entry #1"));
  EXPECT_EQ(idx, LogIndex{2});
  runAllAsyncAppendEntries();

  auto oldConfig =
      leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
  auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
  {
    newConfig->generation = 2;
    // make follower2 forced
    newConfig->participants["follower2"] =
        replication2::ParticipantFlags{true, false};
    leader->updateParticipantsConfig(newConfig, nullptr);
  }

  // commit this configuration
  runAllAsyncAppendEntries();
  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 2);
    EXPECT_EQ(committed, 2);
  }

  ASSERT_EQ(leader->getCommitIndex(), LogIndex{3});

  // check the log for the meta log entry
  {
    auto log = leader->copyInMemoryLog();
    auto entry = log.getEntryByIndex(LogIndex{3});
    ASSERT_NE(entry, std::nullopt);
    auto const& persenty = entry->entry();
    EXPECT_TRUE(persenty.hasMeta());
    ASSERT_NE(persenty.meta(), nullptr);
    auto const& meta = *persenty.meta();
    ASSERT_TRUE(
        std::holds_alternative<LogMetaPayload::UpdateParticipantsConfig>(
            meta.info));
    auto const& info =
        std::get<LogMetaPayload::UpdateParticipantsConfig>(meta.info);
    EXPECT_EQ(info.participants, *newConfig);
  }
}

TEST_F(UpdateParticipantsFlagsTest, refuse_old_generation) {
  leader->triggerAsyncReplication();
  runAllAsyncAppendEntries();
  ASSERT_TRUE(leader->isLeadershipEstablished());

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 1);
    EXPECT_EQ(committed, 1);
  }

  {
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
    // just update the generation
    newConfig->generation = 3;
    leader->updateParticipantsConfig(newConfig, nullptr);
  }

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 3);
    EXPECT_EQ(committed, 1);
  }

  {
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
    // purposefully try to update to an old generation
    newConfig->generation = 2;
    EXPECT_ANY_THROW(leader->updateParticipantsConfig(newConfig, nullptr));
  }

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 3);
    EXPECT_EQ(committed, 1);
  }

  runAllAsyncAppendEntries();

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 3);
    EXPECT_EQ(committed, 3);
  }

  {
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    auto newConfig = std::make_shared<agency::ParticipantsConfig>(oldConfig);
    // purposefully try to update to an old generation
    newConfig->generation = 2;
    EXPECT_ANY_THROW(leader->updateParticipantsConfig(newConfig, nullptr));
  }

  runAllAsyncAppendEntries();

  {
    auto [accepted, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(accepted, 3);
    EXPECT_EQ(committed, 3);
  }
}
