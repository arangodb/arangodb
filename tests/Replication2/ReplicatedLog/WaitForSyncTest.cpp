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

#include "Replication2/Mocks/FakeFollower.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

struct WaitForSyncTest : ReplicatedLogTest {};


TEST_F(WaitForSyncTest, no_wait_for_sync) {
  auto const waitForSync = false;
  auto const term = LogTerm{4};

  auto leaderLog = makeReplicatedLog(LogId{1});
  auto follower = std::make_shared<FakeFollower>("follower");
  auto leader = leaderLog->becomeLeader(LogConfig(2, 2, 2, waitForSync), "leader", term, {follower});
  // first entry is always with waitForSync
  leader->triggerAsyncReplication();
  follower->handleAllRequestsWithOk();

  auto const firstIdx =
      leader->insert(LogPayload::createFromString("first entry"));
  // Note that the leader inserts an empty log entry in becomeLeader already
  ASSERT_EQ(firstIdx, LogIndex{2});

  ASSERT_TRUE(follower->hasPendingRequests());
  {
    auto req = follower->currentRequest();
    EXPECT_EQ(req.messageId, MessageId{4});
    EXPECT_FALSE(req.waitForSync);
  }

  {
    auto result = AppendEntriesResult{term, TRI_ERROR_NO_ERROR,
                                      AppendEntriesErrorReason::NONE,
                                      follower->currentRequest().messageId};
    follower->resolveRequest(std::move(result));
  }
}

TEST_F(WaitForSyncTest, global_wait_for_sync) {
  auto const waitForSync = true;
  auto const term = LogTerm{4};

  auto leaderLog = makeReplicatedLog(LogId{1});
  auto follower = std::make_shared<FakeFollower>("follower");
  auto leader = leaderLog->becomeLeader(LogConfig(2, 2, 2, waitForSync), "leader", term, {follower});
  // first entry is always with waitForSync
  leader->triggerAsyncReplication();
  follower->handleAllRequestsWithOk();

  auto const firstIdx =
      leader->insert(LogPayload::createFromString("first entry"));
  // Note that the leader inserts an empty log entry in becomeLeader already
  ASSERT_EQ(firstIdx, LogIndex{2});

  ASSERT_TRUE(follower->hasPendingRequests());
  {
    auto req = follower->currentRequest();
    EXPECT_EQ(req.messageId, MessageId{4});
    EXPECT_TRUE(req.waitForSync);
  }

  {
    auto result = AppendEntriesResult{term, TRI_ERROR_NO_ERROR,
                                      AppendEntriesErrorReason::NONE,
                                      follower->currentRequest().messageId};
    follower->resolveRequest(std::move(result));
  }
}

TEST_F(WaitForSyncTest, per_entry_wait_for_sync) {
  auto const waitForSync = false;
  auto const term = LogTerm{4};

  auto leaderLog = makeReplicatedLog(LogId{1});
  auto follower = std::make_shared<FakeFollower>("follower");
  auto leader = leaderLog->becomeLeader(LogConfig(2, 2, 2, waitForSync), "leader", term, {follower});
  // first entry is always with waitForSync
  leader->triggerAsyncReplication();
  follower->handleAllRequestsWithOk();

  auto const firstIdx =
      leader->insert(LogPayload::createFromString("first entry"), true);
  // Note that the leader inserts an empty log entry in becomeLeader already
  ASSERT_EQ(firstIdx, LogIndex{2});

  ASSERT_TRUE(follower->hasPendingRequests());
  {
    auto req = follower->currentRequest();
    EXPECT_EQ(req.messageId, MessageId{4});
    EXPECT_TRUE(req.waitForSync);
  }

  {
    auto result = AppendEntriesResult{term, TRI_ERROR_NO_ERROR,
                                      AppendEntriesErrorReason::NONE,
                                      follower->currentRequest().messageId};
    follower->resolveRequest(std::move(result));
  }
}
