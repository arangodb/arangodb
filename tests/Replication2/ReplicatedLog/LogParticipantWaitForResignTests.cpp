////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Replication2/ReplicatedLog/LogLeader.h"

#include "Replication2/ReplicatedLog/TestHelper.h"

#include <gtest/gtest.h>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

struct WaitForResignTest : ReplicatedLogTest {
  bool resigned = false;

  auto getSetResignStatusCallback() {
    using namespace arangodb::futures;
    return [&](Try<Unit>&& result) {
      ASSERT_TRUE(result.valid());
      ASSERT_TRUE(result.hasValue());
      resigned = true;
    };
  }
};

TEST_F(WaitForResignTest, wait_for_resign_unconfigured_participant_resign) {
  auto testLog = makeReplicatedLog(LogId{1});
  auto participant = testLog->getParticipant();
  participant->waitForResign().thenFinal(getSetResignStatusCallback());

  ASSERT_FALSE(resigned);
  std::ignore = testLog->drop();
  ASSERT_TRUE(resigned);
}

TEST_F(WaitForResignTest,
       wait_for_resign_unconfigured_participant_become_follower) {
  auto testLog = makeReplicatedLog(LogId{1});
  auto participant = testLog->getParticipant();
  participant->waitForResign().thenFinal(getSetResignStatusCallback());

  ASSERT_FALSE(resigned);
  testLog->becomeFollower("follower", LogTerm{1}, "leader");
  ASSERT_TRUE(resigned);
}

TEST_F(WaitForResignTest,
       wait_for_resign_unconfigured_participant_become_leader) {
  auto testLog = makeReplicatedLog(LogId{1});
  auto participant = testLog->getParticipant();
  participant->waitForResign().thenFinal(getSetResignStatusCallback());

  ASSERT_FALSE(resigned);
  testLog->becomeLeader("leader", LogTerm{1}, {}, 1);
  ASSERT_TRUE(resigned);
}

TEST_F(WaitForResignTest, wait_for_resign_unconfigured_participant_destroy) {
  {
    auto testLog = makeReplicatedLog(LogId{1});
    auto participant = testLog->getParticipant();
    participant->waitForResign().thenFinal(getSetResignStatusCallback());

    ASSERT_FALSE(resigned);
  }
  ASSERT_TRUE(resigned);
}

TEST_F(WaitForResignTest, wait_for_resign_follower_resign) {
  auto testLog = makeReplicatedLog(LogId{1});
  testLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto participant = testLog->getParticipant();
  participant->waitForResign().thenFinal(getSetResignStatusCallback());

  ASSERT_FALSE(resigned);
  std::ignore = testLog->drop();
  ASSERT_TRUE(resigned);
}

TEST_F(WaitForResignTest, wait_for_resign_follower_become_follower) {
  auto testLog = makeReplicatedLog(LogId{1});
  testLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto participant = testLog->getParticipant();
  participant->waitForResign().thenFinal(getSetResignStatusCallback());

  ASSERT_FALSE(resigned);
  testLog->becomeFollower("follower", LogTerm{2}, "leader");
  ASSERT_TRUE(resigned);
}

TEST_F(WaitForResignTest, wait_for_resign_follower_become_leader) {
  auto testLog = makeReplicatedLog(LogId{1});
  testLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto participant = testLog->getParticipant();
  participant->waitForResign().thenFinal(getSetResignStatusCallback());

  ASSERT_FALSE(resigned);
  testLog->becomeLeader("leader", LogTerm{2}, {}, 1);
  ASSERT_TRUE(resigned);
}

TEST_F(WaitForResignTest, wait_for_resign_follower_destroy) {
  {
    auto testLog = makeReplicatedLog(LogId{1});
    testLog->becomeFollower("follower", LogTerm{1}, "leader");
    auto participant = testLog->getParticipant();
    participant->waitForResign().thenFinal(getSetResignStatusCallback());

    ASSERT_FALSE(resigned);
  }
  ASSERT_TRUE(resigned);
}

TEST_F(WaitForResignTest, wait_for_resign_leader_resign) {
  auto testLog = makeReplicatedLog(LogId{1});
  testLog->becomeLeader("leader", LogTerm{1}, {}, 1);
  auto participant = testLog->getParticipant();
  participant->waitForResign().thenFinal(getSetResignStatusCallback());

  ASSERT_FALSE(resigned);
  std::ignore = testLog->drop();
  ASSERT_TRUE(resigned);
}

TEST_F(WaitForResignTest, wait_for_resign_leader_become_follower) {
  auto testLog = makeReplicatedLog(LogId{1});
  testLog->becomeLeader("leader", LogTerm{1}, {}, 1);
  auto participant = testLog->getParticipant();
  participant->waitForResign().thenFinal(getSetResignStatusCallback());

  ASSERT_FALSE(resigned);
  testLog->becomeFollower("follower", LogTerm{2}, "leader");
  ASSERT_TRUE(resigned);
}

TEST_F(WaitForResignTest, wait_for_resign_leader_become_leader) {
  auto testLog = makeReplicatedLog(LogId{1});
  testLog->becomeLeader("leader", LogTerm{1}, {}, 1);
  auto participant = testLog->getParticipant();
  participant->waitForResign().thenFinal(getSetResignStatusCallback());

  ASSERT_FALSE(resigned);
  testLog->becomeLeader("leader", LogTerm{2}, {}, 1);
  ASSERT_TRUE(resigned);
}

TEST_F(WaitForResignTest, wait_for_resign_leader_destroy) {
  {
    auto testLog = makeReplicatedLog(LogId{1});
    testLog->becomeLeader("leader", LogTerm{1}, {}, 1);
    auto participant = testLog->getParticipant();
    participant->waitForResign().thenFinal(getSetResignStatusCallback());

    ASSERT_FALSE(resigned);
  }
  ASSERT_TRUE(resigned);
}
