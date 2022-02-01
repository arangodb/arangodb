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

struct EstablishLeadershipTest : ReplicatedLogTest {};

TEST_F(EstablishLeadershipTest, wait_for_leadership) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto followerLog = makeReplicatedLog(LogId{1});

  auto follower = followerLog->becomeFollower("follower", LogTerm{4}, "leader");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{4}, {follower}, 2);

  auto f = leader->waitForLeadership();

  {
    auto status = leader->getStatus();
    ASSERT_TRUE(std::holds_alternative<LeaderStatus>(status.getVariant()));
    EXPECT_FALSE(
        std::get<LeaderStatus>(status.getVariant()).leadershipEstablished);
  }

  EXPECT_FALSE(follower->hasPendingAppendEntries());
  EXPECT_FALSE(leader->isLeadershipEstablished());
  EXPECT_FALSE(f.isReady());
  leader->triggerAsyncReplication();
  EXPECT_TRUE(follower->hasPendingAppendEntries());

  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  EXPECT_TRUE(leader->isLeadershipEstablished());
  {
    auto status = leader->getStatus();
    ASSERT_TRUE(std::holds_alternative<LeaderStatus>(status.getVariant()));
    EXPECT_TRUE(
        std::get<LeaderStatus>(status.getVariant()).leadershipEstablished);
  }

  EXPECT_TRUE(f.isReady());
}

TEST_F(EstablishLeadershipTest, excluded_follower) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto followerLog = makeReplicatedLog(LogId{1});

  auto follower = followerLog->becomeFollower("follower", LogTerm{4}, "leader");

  auto config = LogConfig{2, 2, 2, false};
  auto participants = std::unordered_map<ParticipantId, ParticipantFlags>{
      {"leader", {}}, {"follower", {.excluded = true}}};
  auto participantsConfig =
      std::make_shared<ParticipantsConfig>(ParticipantsConfig{
          .generation = 1,
          .participants = std::move(participants),
      });
  auto leader = leaderLog->becomeLeader(config, "leader", LogTerm{4},
                                        {follower}, participantsConfig);

  auto f = leader->waitForLeadership();
  {
    auto status = leader->getStatus();
    ASSERT_TRUE(std::holds_alternative<LeaderStatus>(status.getVariant()));
    EXPECT_FALSE(
        std::get<LeaderStatus>(status.getVariant()).leadershipEstablished);
  }

  EXPECT_FALSE(follower->hasPendingAppendEntries());
  EXPECT_FALSE(leader->isLeadershipEstablished());
  EXPECT_FALSE(f.isReady());
  leader->triggerAsyncReplication();
  EXPECT_TRUE(follower->hasPendingAppendEntries());

  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  // The leadership should not be established because the follower is excluded
  EXPECT_FALSE(leader->isLeadershipEstablished());
  {
    auto status = leader->getStatus();
    ASSERT_TRUE(std::holds_alternative<LeaderStatus>(status.getVariant()));
    EXPECT_FALSE(
        std::get<LeaderStatus>(status.getVariant()).leadershipEstablished);
  }

  EXPECT_FALSE(f.isReady());

  {
    auto oldConfig =
        leader->getStatus().asLeaderStatus()->activeParticipantsConfig;
    auto newConfig = std::make_shared<ParticipantsConfig>(oldConfig);
    newConfig->generation = 2;
    newConfig->participants["follower"] = replication2::ParticipantFlags{};
    leader->updateParticipantsConfig(newConfig, oldConfig.generation, {}, {});
  }

  {
    // Leadership is established immediately because the first entry was already
    // acknowledged by the follower, just not committed. Right after the
    // follower becomes available, the first entry is committed.
    EXPECT_TRUE(f.isReady());
    EXPECT_TRUE(leader->isLeadershipEstablished());
    auto [active, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(active, 2);
    // The first entry was not committed by generation 1, because during this
    // generation the follower was excluded.
    // Because the active generation is currently 2, 1 can no longer be the
    // committed generation. The committed generation will become 2 after the
    // updateParticipantsConfig entry has been committed.
    EXPECT_EQ(committed, std::nullopt);
  }

  EXPECT_TRUE(follower->hasPendingAppendEntries());
  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  {
    auto [active, committed] = leader->getParticipantConfigGenerations();
    EXPECT_EQ(active, 2);
    EXPECT_EQ(committed, 2);
  }
}
