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

#include "TestHelper.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::test;

TEST(LogIndexTest, compareOperators) {
  auto one = LogIndex{1};
  auto two = LogIndex{2};

  EXPECT_TRUE(one == one);
  EXPECT_FALSE(one != one);
  EXPECT_FALSE(one < one);
  EXPECT_FALSE(one > one);
  EXPECT_TRUE(one <= one);
  EXPECT_TRUE(one >= one);

  EXPECT_FALSE(one == two);
  EXPECT_TRUE(one != two);
  EXPECT_TRUE(one < two);
  EXPECT_FALSE(one > two);
  EXPECT_TRUE(one <= two);
  EXPECT_FALSE(one >= two);

  EXPECT_FALSE(two == one);
  EXPECT_TRUE(two != one);
  EXPECT_FALSE(two < one);
  EXPECT_TRUE(two > one);
  EXPECT_FALSE(two <= one);
  EXPECT_TRUE(two >= one);
}

TEST(TermIndexPair, compare_operator) {
  auto A = TermIndexPair{LogTerm{1}, LogIndex{1}};
  auto B = TermIndexPair{LogTerm{1}, LogIndex{5}};
  auto C = TermIndexPair{LogTerm{2}, LogIndex{2}};

  EXPECT_TRUE(A < B);
  EXPECT_TRUE(B < C);
  EXPECT_TRUE(A < C);
}

struct SimpleReplicatedLogTest : ReplicatedLogTest {
  LogId const logId{12};
  std::shared_ptr<TestReplicatedLog> const log = makeReplicatedLog(logId);
};

TEST_F(SimpleReplicatedLogTest, become_leader_test) {
  auto leader = log->becomeLeader("leader", LogTerm{1}, {}, 1);
  ASSERT_NE(leader, nullptr);
}

TEST_F(SimpleReplicatedLogTest, become_follower_test) {
  auto follower = log->becomeFollower("follower", LogTerm{1}, "leader");
  ASSERT_NE(follower, nullptr);
}

TEST_F(SimpleReplicatedLogTest, become_leader_test_same_term) {
  auto leader = log->becomeLeader("leader", LogTerm{1}, {}, 1);
  ASSERT_NE(leader, nullptr);
  EXPECT_ANY_THROW(
      { auto newLeader = log->becomeLeader("leader", LogTerm{1}, {}, 1); });
  auto newLeader = log->becomeLeader("leader", LogTerm{2}, {}, 1);
  ASSERT_NE(newLeader, nullptr);
  EXPECT_TRUE(leader->waitForResign().isReady());
}

TEST_F(SimpleReplicatedLogTest, become_follower_test_same_term) {
  auto follower = log->becomeFollower("follower", LogTerm{1}, "leader");
  ASSERT_NE(follower, nullptr);
  EXPECT_ANY_THROW({
    auto newFollower = log->becomeFollower("follower", LogTerm{1}, "leader");
  });
  auto newFollower = log->becomeFollower("follower", LogTerm{2}, "leader");
  ASSERT_NE(newFollower, nullptr);
  EXPECT_TRUE(follower->waitForResign().isReady());
}
