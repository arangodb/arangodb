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

struct DetectConflictTest : ::testing::Test {

};

struct TestInMemoryLog : InMemoryLog {
  explicit TestInMemoryLog(InMemoryLog::log_type log) : InMemoryLog(std::move(log)) {}
};

TEST_F(DetectConflictTest, log_empty) {
  auto log = TestInMemoryLog{{}};
  auto res = algorithms::detectConflict(log, TermIndexPair{LogTerm{1}, LogIndex{3}});
  ASSERT_TRUE(res.has_value());
  auto [reason, next] = *res;
  EXPECT_EQ(reason, ConflictReason::LOG_EMPTY);
  EXPECT_EQ(TermIndexPair{}, next);
}

TEST_F(DetectConflictTest, log_skip_term) {
  auto log = TestInMemoryLog{{
      InMemoryLogEntry(PersistingLogEntry(LogTerm{1}, LogIndex{1}, LogPayload::createFromString("A"))),
      InMemoryLogEntry(PersistingLogEntry(LogTerm{1}, LogIndex{2}, LogPayload::createFromString("A"))),
      InMemoryLogEntry(PersistingLogEntry(LogTerm{1}, LogIndex{3}, LogPayload::createFromString("A"))),
      InMemoryLogEntry(PersistingLogEntry(LogTerm{3}, LogIndex{4}, LogPayload::createFromString("AB"))),
      InMemoryLogEntry(PersistingLogEntry(LogTerm{3}, LogIndex{5}, LogPayload::createFromString("AB"))),
      InMemoryLogEntry(PersistingLogEntry(LogTerm{3}, LogIndex{6}, LogPayload::createFromString("AB"))),
  }};
  auto res = algorithms::detectConflict(log, TermIndexPair{LogTerm{4}, LogIndex{6}});
  ASSERT_TRUE(res.has_value());
  auto [reason, next] = *res;
  EXPECT_EQ(reason, ConflictReason::LOG_ENTRY_NO_MATCH);
  EXPECT_EQ(TermIndexPair(LogTerm{3}, LogIndex{4}), next);
}

TEST_F(DetectConflictTest, log_missing_after) {
  auto log = TestInMemoryLog{{
    InMemoryLogEntry(PersistingLogEntry(LogTerm{1}, LogIndex{1}, LogPayload::createFromString("A"))),
    InMemoryLogEntry(PersistingLogEntry(LogTerm{1}, LogIndex{2}, LogPayload::createFromString("A"))),
    InMemoryLogEntry(PersistingLogEntry(LogTerm{1}, LogIndex{3}, LogPayload::createFromString("A"))),
    }};
  auto res = algorithms::detectConflict(log, TermIndexPair{LogTerm{4}, LogIndex{6}});
  ASSERT_TRUE(res.has_value());
  auto [reason, next] = *res;
  EXPECT_EQ(reason, ConflictReason::LOG_ENTRY_AFTER_END);
  EXPECT_EQ(TermIndexPair(LogTerm{1}, LogIndex{4}), next);
}

TEST_F(DetectConflictTest, log_missing_before) {
  auto log = TestInMemoryLog{{
      InMemoryLogEntry(PersistingLogEntry(LogTerm{4}, LogIndex{11},
                                          LogPayload::createFromString("A"))),
      InMemoryLogEntry(PersistingLogEntry(LogTerm{4}, LogIndex{12},
                                          LogPayload::createFromString("A"))),
      InMemoryLogEntry(PersistingLogEntry(LogTerm{4}, LogIndex{13},
                                          LogPayload::createFromString("A"))),
  }};
  auto res = algorithms::detectConflict(log, TermIndexPair{LogTerm{4}, LogIndex{6}});
  ASSERT_TRUE(res.has_value());
  auto [reason, next] = *res;
  EXPECT_EQ(reason, ConflictReason::LOG_ENTRY_BEFORE_BEGIN);
  EXPECT_EQ(TermIndexPair(LogTerm{0}, LogIndex{0}), next);
}

TEST_F(DetectConflictTest, log_missing_before_wrong_term) {
  auto log = TestInMemoryLog{{
      InMemoryLogEntry(PersistingLogEntry(LogTerm{4}, LogIndex{11},
                                          LogPayload::createFromString("A"))),
      InMemoryLogEntry(PersistingLogEntry(LogTerm{4}, LogIndex{12},
                                          LogPayload::createFromString("A"))),
      InMemoryLogEntry(PersistingLogEntry(LogTerm{4}, LogIndex{13},
                                          LogPayload::createFromString("A"))),
  }};
  auto res = algorithms::detectConflict(log, TermIndexPair{LogTerm{5}, LogIndex{12}});
  ASSERT_TRUE(res.has_value());
  auto [reason, next] = *res;
  EXPECT_EQ(reason, ConflictReason::LOG_ENTRY_NO_MATCH);
  EXPECT_EQ(TermIndexPair(LogTerm{4}, LogIndex{11}), next);
}
