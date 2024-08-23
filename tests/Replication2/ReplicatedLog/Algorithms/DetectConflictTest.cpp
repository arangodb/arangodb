////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include <gtest/gtest.h>

#include "Replication2/ReplicatedLog/Algorithms.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/TermIndexMapping.h"
#include "Replication2/Storage/IteratorPosition.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::algorithms;

namespace {
void insertMapping(TermIndexMapping& log, LogIndex start, LogIndex end,
                   LogTerm term) {
  log.insert({start, end}, storage::IteratorPosition::fromLogIndex(start),
             term);
}
}  // namespace

struct DetectConflictTest : ::testing::Test {};

TEST_F(DetectConflictTest, log_empty) {
  auto log = TermIndexMapping{};
  auto res =
      algorithms::detectConflict(log, TermIndexPair{LogTerm{1}, LogIndex{3}});
  ASSERT_TRUE(res.has_value());
  auto [reason, next] = *res;
  EXPECT_EQ(reason, ConflictReason::LOG_EMPTY);
  EXPECT_EQ(TermIndexPair{}, next);
}

TEST_F(DetectConflictTest, log_skip_term) {
  auto log = TermIndexMapping{};
  insertMapping(log, LogIndex{1}, LogIndex{4}, LogTerm{1});
  insertMapping(log, LogIndex{4}, LogIndex{7}, LogTerm{3});
  auto res =
      algorithms::detectConflict(log, TermIndexPair{LogTerm{4}, LogIndex{6}});
  ASSERT_TRUE(res.has_value());
  auto [reason, next] = *res;
  EXPECT_EQ(reason, ConflictReason::LOG_ENTRY_NO_MATCH);
  EXPECT_EQ(TermIndexPair(LogTerm{3}, LogIndex{4}), next);
}

TEST_F(DetectConflictTest, log_missing_after) {
  auto log = TermIndexMapping{};
  insertMapping(log, LogIndex{1}, LogIndex{4}, LogTerm{1});
  auto res =
      algorithms::detectConflict(log, TermIndexPair{LogTerm{4}, LogIndex{6}});
  ASSERT_TRUE(res.has_value());
  auto [reason, next] = *res;
  EXPECT_EQ(reason, ConflictReason::LOG_ENTRY_AFTER_END);
  EXPECT_EQ(TermIndexPair(LogTerm{1}, LogIndex{4}), next);
}

TEST_F(DetectConflictTest, log_missing_before) {
  auto log = TermIndexMapping{};
  insertMapping(log, LogIndex{11}, LogIndex{14}, LogTerm{4});
  auto res =
      algorithms::detectConflict(log, TermIndexPair{LogTerm{4}, LogIndex{6}});
  ASSERT_TRUE(res.has_value());
  auto [reason, next] = *res;
  EXPECT_EQ(reason, ConflictReason::LOG_ENTRY_BEFORE_BEGIN);
  EXPECT_EQ(TermIndexPair(LogTerm{0}, LogIndex{0}), next);
}

TEST_F(DetectConflictTest, log_missing_before_wrong_term) {
  auto log = TermIndexMapping{};
  insertMapping(log, LogIndex{11}, LogIndex{14}, LogTerm{4});
  auto res =
      algorithms::detectConflict(log, TermIndexPair{LogTerm{5}, LogIndex{12}});
  ASSERT_TRUE(res.has_value());
  auto [reason, next] = *res;
  EXPECT_EQ(reason, ConflictReason::LOG_ENTRY_NO_MATCH);
  EXPECT_EQ(TermIndexPair(LogTerm{4}, LogIndex{11}), next);
}
