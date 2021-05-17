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

#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/LogFollower.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

struct FollowerWaitForTest : ReplicatedLogTest {
  auto makeFollower(ParticipantId id, LogTerm term, ParticipantId leaderId) -> std::shared_ptr<LogFollower> {
    auto core = makeLogCore(LogId{3});
    auto log = std::make_shared<ReplicatedLog>(std::move(core), _logMetricsMock);
    return log->becomeFollower(std::move(id), term, std::move(leaderId));
  }
};

TEST_F(FollowerWaitForTest, update_send_append_entries){
  auto follower = makeFollower("follower", LogTerm{5}, "leader");

  auto future = follower->waitFor(LogIndex{1});
  EXPECT_FALSE(future.isReady());
  MessageId nextMessageId{0};

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

  // Entry is there, but not committed. Future should not be resolved
  EXPECT_FALSE(future.isReady());
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

  ASSERT_TRUE(future.isReady());
  {
    auto const result = future.get();
    // TODO what do we expect the result so be?
  }
}
