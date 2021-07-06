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

#include "ReplicatedLogMetricsMock.h"

#include "Replication2/ReplicatedLog/LogFollower.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/types.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

struct FollowerAppendEntriesTest : ReplicatedLogTest {
  auto makeFollower(ParticipantId id, LogTerm term, ParticipantId leaderId) -> std::shared_ptr<LogFollower> {
    auto core = makeLogCore(LogId{3});
    auto log = std::make_shared<ReplicatedLog>(std::move(core), _logMetricsMock, defaultLogger());
    return log->becomeFollower(std::move(id), term, std::move(leaderId));
  }

  MessageId nextMessageId{0};
};

TEST_F(FollowerAppendEntriesTest, valid_append_entries) {
  auto follower = makeFollower("follower", LogTerm{5}, "leader");

  {
    AppendEntriesRequest request;
    request.leaderId = "leader";
    request.leaderTerm = LogTerm{5};
    request.prevLogIndex = LogIndex{0};
    request.prevLogTerm = LogTerm{0};
    request.leaderCommit = LogIndex{0};
    request.messageId = ++nextMessageId;
    request.entries = {LogEntry(LogTerm{1}, LogIndex{1}, LogPayload{"some payload"})};

    auto f = follower->appendEntries(std::move(request));
    ASSERT_TRUE(f.isReady());
    {
      auto result = f.get();
      EXPECT_EQ(result.logTerm, LogTerm{5});
      EXPECT_EQ(result.errorCode, TRI_ERROR_NO_ERROR);
      EXPECT_EQ(result.reason, AppendEntriesErrorReason::NONE);
    }
  }

  {
    AppendEntriesRequest request;
    request.leaderId = "leader";
    request.leaderTerm = LogTerm{5};
    request.prevLogIndex = LogIndex{1};
    request.prevLogTerm = LogTerm{1};
    request.leaderCommit = LogIndex{1};
    request.messageId = ++nextMessageId;
    request.entries = {};

    auto f = follower->appendEntries(std::move(request));
    ASSERT_TRUE(f.isReady());
    {
      auto result = f.get();
      EXPECT_EQ(result.logTerm, LogTerm{5});
      EXPECT_EQ(result.errorCode, TRI_ERROR_NO_ERROR);
      EXPECT_EQ(result.reason, AppendEntriesErrorReason::NONE);
    }
  }
}

TEST_F(FollowerAppendEntriesTest, wrong_term) {
  auto follower = makeFollower("follower", LogTerm{5}, "leader");

  {
    AppendEntriesRequest request;
    request.leaderId = "leader";
    request.leaderTerm = LogTerm{4};
    request.prevLogIndex = LogIndex{0};
    request.prevLogTerm = LogTerm{0};
    request.leaderCommit = LogIndex{0};
    request.messageId = ++nextMessageId;
    request.entries = {LogEntry(LogTerm{1}, LogIndex{1}, LogPayload{"some payload"})};

    auto f = follower->appendEntries(std::move(request));
    ASSERT_TRUE(f.isReady());
    {
      auto result = f.get();
      EXPECT_EQ(result.logTerm, LogTerm{5});
      EXPECT_EQ(result.errorCode, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED);
      EXPECT_EQ(result.reason, AppendEntriesErrorReason::WRONG_TERM);
    }
  }
}

TEST_F(FollowerAppendEntriesTest, missing_prev_log_index) {
  auto follower = makeFollower("follower", LogTerm{5}, "leader");

  {
    AppendEntriesRequest request;
    request.leaderId = "leader";
    request.leaderTerm = LogTerm{5};
    request.prevLogIndex = LogIndex{1};
    request.prevLogTerm = LogTerm{1};
    request.leaderCommit = LogIndex{0};
    request.messageId = ++nextMessageId;
    request.entries = {LogEntry(LogTerm{1}, LogIndex{2}, LogPayload{"some payload"})};

    auto f = follower->appendEntries(std::move(request));
    ASSERT_TRUE(f.isReady());
    {
      auto result = f.get();
      EXPECT_EQ(result.logTerm, LogTerm{5});
      EXPECT_EQ(result.errorCode, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED);
      EXPECT_EQ(result.reason, AppendEntriesErrorReason::NO_PREV_LOG_MATCH);
    }
  }
}

TEST_F(FollowerAppendEntriesTest, missmatch_prev_log_term) {
  auto follower = makeFollower("follower", LogTerm{5}, "leader");

  {
    // First add a valid entry
    AppendEntriesRequest request;
    request.leaderId = "leader";
    request.leaderTerm = LogTerm{5};
    request.prevLogIndex = LogIndex{0};
    request.prevLogTerm = LogTerm{0};
    request.leaderCommit = LogIndex{0};
    request.messageId = ++nextMessageId;
    request.entries = {LogEntry(LogTerm{1}, LogIndex{1}, LogPayload{"some payload"})};

    auto f = follower->appendEntries(std::move(request));
    ASSERT_TRUE(f.isReady());
    {
      auto result = f.get();
      ASSERT_EQ(result.errorCode, TRI_ERROR_NO_ERROR);
    }
  }

  {
    // Now add another with wrong term in prevLogTerm
    AppendEntriesRequest request;
    request.leaderId = "leader";
    request.leaderTerm = LogTerm{5};
    request.prevLogIndex = LogIndex{1};
    request.prevLogTerm = LogTerm{3};
    request.leaderCommit = LogIndex{1};
    request.messageId = ++nextMessageId;
    request.entries = {LogEntry(LogTerm{5}, LogIndex{2}, LogPayload{"some payload"})};

    auto f = follower->appendEntries(std::move(request));
    ASSERT_TRUE(f.isReady());
    {
      auto result = f.get();
      EXPECT_EQ(result.logTerm, LogTerm{5});
      EXPECT_EQ(result.errorCode, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED);
      EXPECT_EQ(result.reason, AppendEntriesErrorReason::NO_PREV_LOG_MATCH);
    }
  }
}

TEST_F(FollowerAppendEntriesTest, wrong_leader_name) {
  auto follower = makeFollower("follower", LogTerm{5}, "leader");

  {
    AppendEntriesRequest request;
    request.leaderId = "odlLeader";
    request.leaderTerm = LogTerm{5};
    request.prevLogIndex = LogIndex{0};
    request.prevLogTerm = LogTerm{0};
    request.leaderCommit = LogIndex{0};
    request.messageId = ++nextMessageId;
    request.entries = {LogEntry(LogTerm{1}, LogIndex{1}, LogPayload{"some payload"})};

    auto f = follower->appendEntries(std::move(request));
    ASSERT_TRUE(f.isReady());
    {
      auto result = f.get();
      EXPECT_EQ(result.logTerm, LogTerm{5});
      // TODO this is known to fail
      EXPECT_EQ(result.errorCode, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED);
      EXPECT_EQ(result.reason, AppendEntriesErrorReason::INVALID_LEADER_ID);
    }
  }
}

TEST_F(FollowerAppendEntriesTest, resigned_follower) {
  auto follower = makeFollower("follower", LogTerm{5}, "leader");

  {
    // First add a valid entry
    AppendEntriesRequest request;
    request.leaderId = "leader";
    request.leaderTerm = LogTerm{5};
    request.prevLogIndex = LogIndex{0};
    request.prevLogTerm = LogTerm{0};
    request.leaderCommit = LogIndex{0};
    request.messageId = ++nextMessageId;
    request.entries = {LogEntry(LogTerm{1}, LogIndex{1}, LogPayload{"some payload"})};

    auto f = follower->appendEntries(std::move(request));
    ASSERT_TRUE(f.isReady());
    {
      auto result = f.get();
      ASSERT_EQ(result.errorCode, TRI_ERROR_NO_ERROR);
    }
  }
  std::ignore = std::move(*follower).resign();

  {
    AppendEntriesRequest request;
    request.leaderId = "leader";
    request.leaderTerm = LogTerm{5};
    request.prevLogIndex = LogIndex{1};
    request.prevLogTerm = LogTerm{1};
    request.leaderCommit = LogIndex{0};
    request.messageId = ++nextMessageId;
    request.entries = {LogEntry(LogTerm{5}, LogIndex{2}, LogPayload{"some payload"})};

    auto f = follower->appendEntries(std::move(request));
    ASSERT_TRUE(f.isReady());
    {
      auto result = f.get();
      EXPECT_EQ(result.logTerm, LogTerm{5});
      EXPECT_EQ(result.errorCode, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED);
      EXPECT_EQ(result.reason, AppendEntriesErrorReason::LOST_LOG_CORE);
    }
  }
}

TEST_F(FollowerAppendEntriesTest, outdated_message_id) {
  auto follower = makeFollower("follower", LogTerm{5}, "leader");

  {
    // First add a valid entry
    AppendEntriesRequest request;
    request.leaderId = "leader";
    request.leaderTerm = LogTerm{5};
    request.prevLogIndex = LogIndex{0};
    request.prevLogTerm = LogTerm{0};
    request.leaderCommit = LogIndex{0};
    request.messageId = MessageId{5};
    request.entries = {LogEntry(LogTerm{1}, LogIndex{1}, LogPayload{"some payload"})};

    auto f = follower->appendEntries(std::move(request));
    ASSERT_TRUE(f.isReady());
    {
      auto result = f.get();
      ASSERT_EQ(result.errorCode, TRI_ERROR_NO_ERROR);
    }
  }

  {
    AppendEntriesRequest request;
    request.leaderId = "leader";
    request.leaderTerm = LogTerm{5};
    request.prevLogIndex = LogIndex{1};
    request.prevLogTerm = LogTerm{1};
    request.leaderCommit = LogIndex{0};
    request.messageId = MessageId{4};
    request.entries = {LogEntry(LogTerm{5}, LogIndex{2}, LogPayload{"some payload"})};

    auto f = follower->appendEntries(std::move(request));
    ASSERT_TRUE(f.isReady());
    {
      auto result = f.get();
      EXPECT_EQ(result.logTerm, LogTerm{5});
      EXPECT_EQ(result.errorCode, TRI_ERROR_REPLICATION_REPLICATED_LOG_APPEND_ENTRIES_REJECTED);
      EXPECT_EQ(result.reason, AppendEntriesErrorReason::MESSAGE_OUTDATED);
    }
  }
}


