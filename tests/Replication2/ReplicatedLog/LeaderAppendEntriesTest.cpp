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

#include "Replication2/Helper/ReplicatedLogTestSetup.h"

#include "Basics/voc-errors.h"

#include "Replication2/ReplicatedLog/Components/LogFollower.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/types.h"

#include "Replication2/Mocks/FakeAbstractFollower.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

struct LeaderAppendEntriesTest : ReplicatedLogTest {};

TEST_F(LeaderAppendEntriesTest, simple_append_entries) {
  auto leaderLogContainer = createParticipant({});
  auto leaderLog = leaderLogContainer->log;
  auto followerLogContainer = createFakeFollower();

  auto config = addNewTerm(leaderLogContainer->serverId(),
                           {followerLogContainer->serverId()},
                           {.term = 4_T, .writeConcern = 2});
  config->installConfig(true);
  auto leader = leaderLogContainer->getAsLeader();

  auto const firstIdx =
      leaderLogContainer->insert(LogPayload::createFromString("first entry"));
  // Note that the leader inserts an empty log entry to establish leadership
  ASSERT_EQ(firstIdx, LogIndex{2});

  leaderLogContainer->runAll();

  auto follower = followerLogContainer->fakeAbstractFollower;
  ASSERT_TRUE(follower->hasPendingRequests());
  {
    auto req = follower->currentRequest();

    // Note that the leader inserted an empty log entry while establishing
    // leadership, and sent another message to update the commit index etc.
    EXPECT_EQ(req.messageId, MessageId{3});
    EXPECT_EQ(req.entries.size(), 1U);
    EXPECT_EQ(req.leaderId, leaderLogContainer->serverId());
    EXPECT_EQ(req.prevLogEntry.term, 4_T);
    EXPECT_EQ(req.prevLogEntry.index, LogIndex{1});
    EXPECT_EQ(req.leaderTerm, LogTerm{4});
    EXPECT_EQ(req.leaderCommit, LogIndex{1});
  }

  follower->resolveWithOk();

  {
    auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant());
    EXPECT_EQ(stats.local.commitIndex, firstIdx);
  }

  leaderLogContainer->runAll();

  ASSERT_TRUE(follower->hasPendingRequests());
  {
    auto req = follower->currentRequest();
    EXPECT_EQ(req.messageId, MessageId{4});
    EXPECT_EQ(req.entries.size(), 0U);
    EXPECT_EQ(req.leaderId, leaderLogContainer->serverId());
    EXPECT_EQ(req.prevLogEntry.term, LogTerm{4});
    EXPECT_EQ(req.prevLogEntry.index, firstIdx);
    EXPECT_EQ(req.leaderTerm, LogTerm{4});
    EXPECT_EQ(req.leaderCommit, firstIdx);
  }
}

TEST_F(LeaderAppendEntriesTest, response_exception) {
  auto leaderLogContainer = createParticipant({});
  auto leaderLog = leaderLogContainer->log;
  auto followerLogContainer = createFakeFollower();
  auto fakeFollower = followerLogContainer->fakeAbstractFollower;

  auto config = addNewTerm(leaderLogContainer->serverId(),
                           {followerLogContainer->serverId()},
                           {.term = 4_T, .writeConcern = 2});
  config->installConfig(true);
  auto leader = leaderLogContainer->getAsLeader();

  // Note that the leader inserts an empty log entry when establishing
  // leadership
  EXPECT_EQ(leader->getQuickStatus().local.value().commitIndex, LogIndex{1});

  auto const firstIdx =
      leaderLogContainer->insert(LogPayload::createFromString("first entry"));
  ASSERT_EQ(firstIdx, LogIndex{2});

  leaderLogContainer->runAll();

  ASSERT_TRUE(fakeFollower->hasPendingRequests());
  {
    auto req = fakeFollower->currentRequest();
    // Note that the leader inserted an empty log entry while establishing
    // leadership, and sent another message to update the commit index etc.
    EXPECT_EQ(req.messageId, MessageId{3});
    EXPECT_EQ(req.entries.size(), 1U);
    EXPECT_EQ(req.leaderId, leaderLogContainer->serverId());
    EXPECT_EQ(req.prevLogEntry.term, LogTerm{4});
    EXPECT_EQ(req.prevLogEntry.index, LogIndex{1});
    EXPECT_EQ(req.leaderTerm, LogTerm{4});
    EXPECT_EQ(req.leaderCommit, LogIndex{1});
  }

  fakeFollower->resolveRequestWithException(std::logic_error("logic error"));

  ASSERT_FALSE(fakeFollower->hasPendingRequests());

  // We expect the leader to retry, but not commit anything
  leaderLogContainer->runAll();
  EXPECT_EQ(leader->getQuickStatus().local.value().commitIndex, LogIndex{1});
  ASSERT_TRUE(fakeFollower->hasPendingRequests());

  {
    auto req = fakeFollower->currentRequest();
    EXPECT_EQ(req.messageId, MessageId{4});
    EXPECT_EQ(req.entries.size(), 1U);
    EXPECT_EQ(req.leaderId, leaderLogContainer->serverId());
    EXPECT_EQ(req.prevLogEntry.term, LogTerm{4});
    EXPECT_EQ(req.prevLogEntry.index, LogIndex{1});
    EXPECT_EQ(req.leaderTerm, LogTerm{4});
    EXPECT_EQ(req.leaderCommit, LogIndex{1});
  }
}

TEST_F(LeaderAppendEntriesTest, test_wait_for_sync_flag_set_by_config) {
  auto leaderLogContainer = createParticipant({});
  auto leaderLog = leaderLogContainer->log;
  auto followerLogContainer = createFakeFollower();
  auto fakeFollower = followerLogContainer->fakeAbstractFollower;

  // waitForSync=true in the config
  auto config = addNewTerm(
      leaderLogContainer->serverId(), {followerLogContainer->serverId()},
      {.term = 4_T, .writeConcern = 2, .waitForSync = true});
  config->installConfig(true);
  auto leader = leaderLogContainer->getAsLeader();

  // waitForSync=false in the request
  auto const firstIdx = leaderLogContainer->insert(
      LogPayload::createFromString("first entry"), false);
  // Note that the leader inserts an empty log entry when establishing
  // leadership
  ASSERT_EQ(firstIdx, LogIndex{2});

  leaderLogContainer->runAll();
  ASSERT_TRUE(fakeFollower->hasPendingRequests());
  {
    auto req = fakeFollower->currentRequest();
    EXPECT_EQ(req.messageId, MessageId{3});
    // Note that the leader inserts an empty log entry in becomeLeader already
    EXPECT_EQ(req.entries.size(), 1U);
    EXPECT_EQ(req.leaderId, leaderLogContainer->serverId());
    EXPECT_EQ(req.prevLogEntry.term, LogTerm{4});
    EXPECT_EQ(req.prevLogEntry.index, LogIndex{1});
    EXPECT_EQ(req.leaderTerm, LogTerm{4});
    EXPECT_EQ(req.leaderCommit, LogIndex{1});
    EXPECT_TRUE(req.waitForSync);
  }
}

// TODO convert the rest
//      continue here!
#if 0
// TODO Enable this test, it's currently known to fail, as it's not yet
// implemented.
TEST_F(LeaderAppendEntriesTest, DISABLED_test_wait_for_sync_flag_set_by_param) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto follower = std::make_shared<FakeAbstractFollower>("follower");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{4}, {follower}, 2);

  auto const firstIdx =
      leader->insert(LogPayload::createFromString("first entry"), true,
                     LogLeader::doNotTriggerAsyncReplication);
  // Note that the leader inserts an empty log entry in becomeLeader already
  ASSERT_EQ(firstIdx, LogIndex{2});

  leader->triggerAsyncReplication();
  ASSERT_TRUE(follower->hasPendingRequests());
  {
    auto req = follower->currentRequest();
    EXPECT_EQ(req.messageId, MessageId{1});
    // Note that the leader inserts an empty log entry in becomeLeader already
    EXPECT_EQ(req.entries.size(), 2U);
    EXPECT_EQ(req.leaderId, ParticipantId{"leader"});
    EXPECT_EQ(req.prevLogEntry.term, LogTerm{0});
    EXPECT_EQ(req.prevLogEntry.index, LogIndex{0});
    EXPECT_EQ(req.leaderTerm, LogTerm{4});
    EXPECT_EQ(req.leaderCommit, LogIndex{0});
    EXPECT_TRUE(req.waitForSync);
  }
}

TEST_F(LeaderAppendEntriesTest, test_wait_for_sync_flag_unset) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto follower = std::make_shared<FakeAbstractFollower>("follower");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{4}, {follower}, 2);

  // The first entry written by the leader has always set the waitForSync flag
  leader->triggerAsyncReplication();
  ASSERT_TRUE(follower->hasPendingRequests());
  {
    auto req = follower->currentRequest();
    EXPECT_EQ(req.messageId, MessageId{1});
    // Note that the leader inserts an empty log entry in becomeLeader already
    EXPECT_EQ(req.entries.size(), 1U);
    EXPECT_EQ(req.leaderId, ParticipantId{"leader"});
    EXPECT_EQ(req.prevLogEntry.term, LogTerm{0});
    EXPECT_EQ(req.prevLogEntry.index, LogIndex{0});
    EXPECT_EQ(req.leaderTerm, LogTerm{4});
    EXPECT_EQ(req.leaderCommit, LogIndex{0});
    EXPECT_TRUE(req.waitForSync);
  }

  follower->handleAllRequestsWithOk();

  auto const firstIdx =
      leader->insert(LogPayload::createFromString("first entry"), false,
                     LogLeader::doNotTriggerAsyncReplication);
  // Note that the leader inserts an empty log entry in becomeLeader already
  ASSERT_EQ(firstIdx, LogIndex{2});

  leader->triggerAsyncReplication();
  ASSERT_TRUE(follower->hasPendingRequests());
  {
    auto req = follower->currentRequest();
    EXPECT_EQ(req.messageId, MessageId{4});
    // Note that the leader inserts an empty log entry in becomeLeader already
    EXPECT_EQ(req.entries.size(), 1U);
    EXPECT_EQ(req.leaderId, ParticipantId{"leader"});
    EXPECT_EQ(req.prevLogEntry.term, LogTerm{4});
    EXPECT_EQ(req.prevLogEntry.index, LogIndex{1});
    EXPECT_EQ(req.leaderTerm, LogTerm{4});
    EXPECT_EQ(req.leaderCommit, LogIndex{1});
    EXPECT_FALSE(req.waitForSync);
  }
}

TEST_F(LeaderAppendEntriesTest, propagate_largest_common_index) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto follower1 = std::make_shared<FakeAbstractFollower>("follower1");
  auto follower2 = std::make_shared<FakeAbstractFollower>("follower2");
  auto leader =
      leaderLog->becomeLeader("leader", LogTerm{4}, {follower1, follower2}, 2);

  /*
   * Three participants with writeConcern two. The commitIndex is updated as
   * soon as two servers have acknowledged the index, the lci is updated as soon
   * as all the servers have acknowledged the new commit index.
   */
  leader->triggerAsyncReplication();
  // the leader has written one entry
  ASSERT_TRUE(follower1->hasPendingRequests());
  ASSERT_TRUE(follower2->hasPendingRequests());

  {
    auto status = leader->getStatus();
    ASSERT_TRUE(std::holds_alternative<LeaderStatus>(status.getVariant()));
    auto value = std::get<LeaderStatus>(status.getVariant());
    EXPECT_EQ(value.local.commitIndex, LogIndex{0});
    EXPECT_EQ(value.lowestIndexToKeep, LogIndex{0});
  }

  {
    auto const& request = follower1->requests.front().request;
    EXPECT_EQ(request.leaderCommit, LogIndex{0});
    EXPECT_EQ(request.lowestIndexToKeep, LogIndex{0});
  }
  // let follower1 run and raise the commit index
  follower1->resolveWithOk();

  {
    auto status = leader->getStatus();
    ASSERT_TRUE(std::holds_alternative<LeaderStatus>(status.getVariant()));
    auto value = std::get<LeaderStatus>(status.getVariant());
    EXPECT_EQ(value.local.commitIndex, LogIndex{1});
    EXPECT_EQ(value.lowestIndexToKeep, LogIndex{0});
  }

  // follower1 received a message with that commitIndex is now 1
  ASSERT_TRUE(follower1->hasPendingRequests());
  {
    auto const& request = follower1->requests.front().request;
    EXPECT_EQ(request.leaderCommit, LogIndex{1});
    EXPECT_EQ(request.lowestIndexToKeep, LogIndex{0});
  }
  follower1->resolveWithOk();
  // there are now no more requests pending for follower1
  ASSERT_FALSE(follower1->hasPendingRequests());

  // let follower2 run - this does not raise the lci because follower2 does not
  // yet know that index 1 is committed.
  follower2->resolveWithOk();

  {
    auto status = leader->getStatus();
    ASSERT_TRUE(std::holds_alternative<LeaderStatus>(status.getVariant()));
    auto value = std::get<LeaderStatus>(status.getVariant());
    EXPECT_EQ(value.local.commitIndex, LogIndex{1});
    EXPECT_EQ(value.lowestIndexToKeep, LogIndex{0});
  }

  // follower2 is now updated with the commit index
  ASSERT_TRUE(follower2->hasPendingRequests());
  {
    auto const& request = follower2->requests.front().request;
    EXPECT_EQ(request.leaderCommit, LogIndex{1});
    EXPECT_EQ(request.lowestIndexToKeep, LogIndex{0});
  }
  // let follower2 run again - this will raise the lci
  follower2->resolveWithOk();

  {
    auto status = leader->getStatus();
    ASSERT_TRUE(std::holds_alternative<LeaderStatus>(status.getVariant()));
    auto value = std::get<LeaderStatus>(status.getVariant());
    EXPECT_EQ(value.local.commitIndex, LogIndex{1});
    EXPECT_EQ(value.lowestIndexToKeep, LogIndex{1});
  }

  // now follower2 received an update with lci = 1
  ASSERT_TRUE(follower2->hasPendingRequests());
  {
    auto const& request = follower2->requests.front().request;
    EXPECT_EQ(request.leaderCommit, LogIndex{1});
    EXPECT_EQ(request.lowestIndexToKeep, LogIndex{1});
  }

  // now follower1 received an update with lci = 1
  ASSERT_TRUE(follower1->hasPendingRequests());
  {
    auto const& request = follower1->requests.front().request;
    EXPECT_EQ(request.leaderCommit, LogIndex{1});
    EXPECT_EQ(request.lowestIndexToKeep, LogIndex{1});
  }
}
#endif
