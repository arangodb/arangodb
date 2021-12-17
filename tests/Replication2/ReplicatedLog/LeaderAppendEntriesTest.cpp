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

#include "Basics/voc-errors.h"

#include "Replication2/ReplicatedLog/LogFollower.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/types.h"

#include "Replication2/Mocks/RecordingFollower.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

struct LeaderAppendEntriesTest : ReplicatedLogTest {};

TEST_F(LeaderAppendEntriesTest, simple_append_entries) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto follower = std::make_shared<RecordingFollower>("follower");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{4}, {follower}, 2);

  auto const firstIdx =
      leader->insert(LogPayload::createFromString("first entry"), false,
                     LogLeader::doNotTriggerAsyncReplication);
  // Note that the leader inserts an empty log entry in becomeLeader already
  ASSERT_EQ(firstIdx, LogIndex{2});

  leader->triggerAsyncReplication();
  ASSERT_TRUE(follower->hasPendingRequests());
  {
    auto req = follower->currentRequest();
    EXPECT_EQ(req.messageId, MessageId{1});
    // Note that the leader inserts an empty log entry in becomeLeader already
    EXPECT_EQ(req.entries.size(), 2);
    EXPECT_EQ(req.leaderId, ParticipantId{"leader"});
    EXPECT_EQ(req.prevLogEntry.term, LogTerm{0});
    EXPECT_EQ(req.prevLogEntry.index, LogIndex{0});
    EXPECT_EQ(req.leaderTerm, LogTerm{4});
    EXPECT_EQ(req.leaderCommit, LogIndex{0});
  }

  {
    auto result = AppendEntriesResult{LogTerm{4},
                                      TRI_ERROR_NO_ERROR,
                                      {},
                                      follower->currentRequest().messageId};
    follower->resolveRequest(std::move(result));
  }

  {
    auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant());
    EXPECT_EQ(stats.local.commitIndex, firstIdx);
  }

  ASSERT_TRUE(follower->hasPendingRequests());
  {
    auto req = follower->currentRequest();
    EXPECT_EQ(req.messageId, MessageId{2});
    EXPECT_EQ(req.entries.size(), 0);
    EXPECT_EQ(req.leaderId, ParticipantId{"leader"});
    EXPECT_EQ(req.prevLogEntry.term, LogTerm{4});
    EXPECT_EQ(req.prevLogEntry.index, firstIdx);
    EXPECT_EQ(req.leaderTerm, LogTerm{4});
    EXPECT_EQ(req.leaderCommit, firstIdx);
  }
}

TEST_F(LeaderAppendEntriesTest, response_exception) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto follower = std::make_shared<RecordingFollower>("follower");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{4}, {follower}, 2);

  auto const firstIdx =
      leader->insert(LogPayload::createFromString("first entry"), false,
                     LogLeader::doNotTriggerAsyncReplication);
  // Note that the leader inserts an empty log entry in becomeLeader already
  ASSERT_EQ(firstIdx, LogIndex{2});

  leader->triggerAsyncReplication();
  ASSERT_TRUE(follower->hasPendingRequests());
  {
    auto req = follower->currentRequest();
    EXPECT_EQ(req.messageId, MessageId{1});
    // Note that the leader inserts an empty log entry in becomeLeader already
    EXPECT_EQ(req.entries.size(), 2);
    EXPECT_EQ(req.leaderId, ParticipantId{"leader"});
    EXPECT_EQ(req.prevLogEntry.term, LogTerm{0});
    EXPECT_EQ(req.prevLogEntry.index, LogIndex{0});
    EXPECT_EQ(req.leaderTerm, LogTerm{4});
    EXPECT_EQ(req.leaderCommit, LogIndex{0});
  }

  { follower->resolveRequestWithException(std::logic_error("logic error")); }

  {
    auto stats = std::get<LeaderStatus>(leader->getStatus().getVariant());
    EXPECT_EQ(stats.local.commitIndex, LogIndex{0});  // do not commit yet
  }

  // we expect a retry
  ASSERT_TRUE(follower->hasPendingRequests());
  {
    auto req = follower->currentRequest();
    EXPECT_EQ(req.messageId, MessageId{2});
    // Note that the leader inserts an empty log entry in becomeLeader already
    EXPECT_EQ(req.entries.size(), 2);
    EXPECT_EQ(req.leaderId, ParticipantId{"leader"});
    EXPECT_EQ(req.prevLogEntry.term, LogTerm{0});
    EXPECT_EQ(req.prevLogEntry.index, LogIndex{0});
    EXPECT_EQ(req.leaderTerm, LogTerm{4});
    EXPECT_EQ(req.leaderCommit, LogIndex{0});
  }
}

TEST_F(LeaderAppendEntriesTest, test_wait_for_sync_flag_set_by_config) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto follower = std::make_shared<RecordingFollower>("follower");

  auto config = LogConfig{};
  config.waitForSync = true;
  config.writeConcern = 2;
  auto leader = leaderLog->becomeLeader(config, "leader", LogTerm{4}, {follower});

  auto const firstIdx =
      leader->insert(LogPayload::createFromString("first entry"), false,
                     LogLeader::doNotTriggerAsyncReplication);
  // Note that the leader inserts an empty log entry in becomeLeader already
  ASSERT_EQ(firstIdx, LogIndex{2});

  leader->triggerAsyncReplication();
  ASSERT_TRUE(follower->hasPendingRequests());
  {
    auto req = follower->currentRequest();
    EXPECT_EQ(req.messageId, MessageId{1});
    // Note that the leader inserts an empty log entry in becomeLeader already
    EXPECT_EQ(req.entries.size(), 2);
    EXPECT_EQ(req.leaderId, ParticipantId{"leader"});
    EXPECT_EQ(req.prevLogEntry.term, LogTerm{0});
    EXPECT_EQ(req.prevLogEntry.index, LogIndex{0});
    EXPECT_EQ(req.leaderTerm, LogTerm{4});
    EXPECT_EQ(req.leaderCommit, LogIndex{0});
    EXPECT_TRUE(req.waitForSync);
  }
}

// TODO Enable this test, it's currently known to fail, as it's not yet implemented.
TEST_F(LeaderAppendEntriesTest, DISABLED_test_wait_for_sync_flag_set_by_param) {
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto follower = std::make_shared<RecordingFollower>("follower");

  auto config = LogConfig{};
  config.waitForSync = false;
  config.writeConcern = 2;
  auto leader = leaderLog->becomeLeader(config, "leader", LogTerm{4}, {follower});

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
    EXPECT_EQ(req.entries.size(), 2);
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
  auto follower = std::make_shared<RecordingFollower>("follower");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{4}, {follower}, 2);

  // The first entry written by the leader has always set the waitForSync flag
  leader->triggerAsyncReplication();
  ASSERT_TRUE(follower->hasPendingRequests());
  {
    auto req = follower->currentRequest();
    EXPECT_EQ(req.messageId, MessageId{1});
    // Note that the leader inserts an empty log entry in becomeLeader already
    EXPECT_EQ(req.entries.size(), 1);
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
    EXPECT_EQ(req.entries.size(), 1);
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
  auto follower1 = std::make_shared<RecordingFollower>("follower1");
  auto follower2 = std::make_shared<RecordingFollower>("follower2");
  auto leader = leaderLog->becomeLeader("leader", LogTerm{4}, {follower1, follower2}, 2);

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
    EXPECT_EQ(value.largestCommonIndex, LogIndex{0});
  }

  {
    auto const& request = follower1->requests.front().request;
    EXPECT_EQ(request.leaderCommit, LogIndex{0});
    EXPECT_EQ(request.largestCommonIndex, LogIndex{0});
  }
  // let follower1 run and raise the commit index
  follower1->resolveWithOk();

  {
    auto status = leader->getStatus();
    ASSERT_TRUE(std::holds_alternative<LeaderStatus>(status.getVariant()));
    auto value = std::get<LeaderStatus>(status.getVariant());
    EXPECT_EQ(value.local.commitIndex, LogIndex{1});
    EXPECT_EQ(value.largestCommonIndex, LogIndex{0});
  }

  // follower1 received a message with that commitIndex is now 1
  ASSERT_TRUE(follower1->hasPendingRequests());
  {
    auto const& request = follower1->requests.front().request;
    EXPECT_EQ(request.leaderCommit, LogIndex{1});
    EXPECT_EQ(request.largestCommonIndex, LogIndex{0});
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
    EXPECT_EQ(value.largestCommonIndex, LogIndex{0});
  }

  // follower2 is now updated with the commit index
  ASSERT_TRUE(follower2->hasPendingRequests());
  {
    auto const& request = follower2->requests.front().request;
    EXPECT_EQ(request.leaderCommit, LogIndex{1});
    EXPECT_EQ(request.largestCommonIndex, LogIndex{0});
  }
  // let follower2 run again - this will raise the lci
  follower2->resolveWithOk();

  {
    auto status = leader->getStatus();
    ASSERT_TRUE(std::holds_alternative<LeaderStatus>(status.getVariant()));
    auto value = std::get<LeaderStatus>(status.getVariant());
    EXPECT_EQ(value.local.commitIndex, LogIndex{1});
    EXPECT_EQ(value.largestCommonIndex, LogIndex{1});
  }

  // now follower2 received an update with lci = 1
  ASSERT_TRUE(follower2->hasPendingRequests());
  {
    auto const& request = follower2->requests.front().request;
    EXPECT_EQ(request.leaderCommit, LogIndex{1});
    EXPECT_EQ(request.largestCommonIndex, LogIndex{1});
  }

  // now follower1 received an update with lci = 1
  ASSERT_TRUE(follower1->hasPendingRequests());
  {
    auto const& request = follower1->requests.front().request;
    EXPECT_EQ(request.leaderCommit, LogIndex{1});
    EXPECT_EQ(request.largestCommonIndex, LogIndex{1});
  }
}
