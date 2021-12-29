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
/// @author Alex Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "TestHelper.h"

#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/ReplicatedLog/types.h"

#include "Replication2/Mocks/FakeFollower.h"

using namespace arangodb::replication2;
using namespace arangodb::replication2::test;

struct AppendEntriesErrorReasonTest : ReplicatedLogTest {
  std::shared_ptr<TestReplicatedLog> leaderLog;
  std::shared_ptr<FakeFollower> follower;
  std::shared_ptr<LogLeader> leader;

  void SetUp() override {
    leaderLog = makeReplicatedLog(LogId{1});
    follower = std::make_shared<FakeFollower>("follower");
    leader = leaderLog->becomeLeader("leader", LogTerm{4}, {follower}, 2);

    auto const firstIdx =
        leader->insert(LogPayload::createFromString("first entry"), false,
                       LogLeader::doNotTriggerAsyncReplication);
    // Note that the leader inserts an empty log entry in becomeLeader already
    ASSERT_EQ(firstIdx, LogIndex{2});
    leader->triggerAsyncReplication();
    ASSERT_TRUE(follower->hasPendingRequests());
  }

  void TearDown() override {
    leaderLog.reset();
    follower.reset();
    leader.reset();
  }
};

TEST_F(AppendEntriesErrorReasonTest, append_entries_communication_error) {
  follower->resolveRequestWithException(std::logic_error("logic error"));

  auto status = std::get<LeaderStatus>(leader->getStatus().getVariant());
  auto followerStatus = status.follower[follower->getParticipantId()];
  EXPECT_EQ(followerStatus.lastErrorReason,
            (AppendEntriesErrorReason{
                AppendEntriesErrorReason::ErrorType::kCommunicationError,
                "logic error"}));
}

TEST_F(AppendEntriesErrorReasonTest, append_entries_with_conflict) {
  follower->resolveRequest(
      AppendEntriesResult::withConflict({}, MessageId{1}, {}));

  auto status = std::get<LeaderStatus>(leader->getStatus().getVariant());
  auto followerStatus = status.follower[follower->getParticipantId()];
  EXPECT_EQ(followerStatus.lastErrorReason,
            AppendEntriesErrorReason{
                AppendEntriesErrorReason::ErrorType::kNoPrevLogMatch});
}

TEST_F(AppendEntriesErrorReasonTest, append_entries_with_persistence_error) {
  follower->resolveRequest(AppendEntriesResult::withPersistenceError(
      {}, MessageId{1}, {ErrorCode{3}, "errorCode3"}));

  auto status = std::get<LeaderStatus>(leader->getStatus().getVariant());
  auto followerStatus = status.follower[follower->getParticipantId()];
  EXPECT_EQ(followerStatus.lastErrorReason,
            (AppendEntriesErrorReason{
                AppendEntriesErrorReason::ErrorType::kPersistenceFailure,
                "errorCode3"}));
}

TEST_F(AppendEntriesErrorReasonTest, append_entries_with_rejection) {
  follower->resolveRequest(AppendEntriesResult::withRejection(
      {}, MessageId{1},
      {AppendEntriesErrorReason::ErrorType::kWrongTerm, "wrong term"}));

  auto status = std::get<LeaderStatus>(leader->getStatus().getVariant());
  auto followerStatus = status.follower[follower->getParticipantId()];
  EXPECT_EQ(
      followerStatus.lastErrorReason,
      (AppendEntriesErrorReason{AppendEntriesErrorReason::ErrorType::kWrongTerm,
                                "wrong term"}));
}

TEST_F(AppendEntriesErrorReasonTest, append_entries_with_ok) {
  // This is not an error, but we still check for the expected error reason
  follower->resolveRequest(AppendEntriesResult::withOk({}, MessageId{1}));

  auto status = std::get<LeaderStatus>(leader->getStatus().getVariant());
  auto followerStatus = status.follower[follower->getParticipantId()];
  EXPECT_EQ(followerStatus.lastErrorReason, AppendEntriesErrorReason{});
}
