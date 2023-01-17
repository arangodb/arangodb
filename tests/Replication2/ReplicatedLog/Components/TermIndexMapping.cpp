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
#include <gtest/gtest.h>
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Basics/debugging.h"

#include "Replication2/ReplicatedLog/TermIndexMapping.h"

using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

namespace {
auto operator"" _Lx(unsigned long long x) -> LogIndex { return LogIndex{x}; }
auto operator"" _T(unsigned long long x) -> LogTerm { return LogTerm{x}; }
}  // namespace

struct TermIndexMappingTest : ::testing::Test {
  TermIndexMapping mapping;
};

TEST_F(TermIndexMappingTest, insert_range_and_query) {
  auto range = LogRange{10_Lx, 20_Lx};
  mapping.insert(range, 4_T);

  EXPECT_EQ(mapping.getTermRange(5_T), std::nullopt);
  EXPECT_EQ(mapping.getTermRange(3_T), std::nullopt);
  EXPECT_EQ(mapping.getTermRange(4_T), range);

  mapping.insert({20_Lx, 40_Lx}, 4_T);
  EXPECT_EQ(mapping.getTermRange(4_T), (LogRange{10_Lx, 40_Lx}));

  mapping.insert({40_Lx, 60_Lx}, 5_T);
  EXPECT_EQ(mapping.getTermRange(4_T), (LogRange{10_Lx, 40_Lx}));
  EXPECT_EQ(mapping.getTermRange(5_T), (LogRange{40_Lx, 60_Lx}));
}

TEST_F(TermIndexMappingTest, remove_front_and_query) {
  mapping.insert({10_Lx, 40_Lx}, 4_T);
  mapping.insert({40_Lx, 60_Lx}, 5_T);

  EXPECT_EQ(mapping.getTermRange(4_T), (LogRange{10_Lx, 40_Lx}));
  EXPECT_EQ(mapping.getTermRange(5_T), (LogRange{40_Lx, 60_Lx}));

  mapping.removeFront(30_Lx);

  EXPECT_EQ(mapping.getTermRange(4_T), (LogRange{30_Lx, 40_Lx}));
  EXPECT_EQ(mapping.getTermRange(5_T), (LogRange{40_Lx, 60_Lx}));

  mapping.removeFront(50_Lx);

  EXPECT_EQ(mapping.getTermRange(4_T), std::nullopt);
  EXPECT_EQ(mapping.getTermRange(5_T), (LogRange{50_Lx, 60_Lx}));
}

TEST_F(TermIndexMappingTest, remove_back_and_query) {
  mapping.insert({10_Lx, 40_Lx}, 4_T);
  mapping.insert({40_Lx, 60_Lx}, 5_T);

  EXPECT_EQ(mapping.getTermRange(4_T), (LogRange{10_Lx, 40_Lx}));
  EXPECT_EQ(mapping.getTermRange(5_T), (LogRange{40_Lx, 60_Lx}));

  mapping.removeBack(50_Lx);

  EXPECT_EQ(mapping.getTermRange(4_T), (LogRange{10_Lx, 40_Lx}));
  EXPECT_EQ(mapping.getTermRange(5_T), (LogRange{40_Lx, 50_Lx}));

  mapping.removeBack(30_Lx);

  EXPECT_EQ(mapping.getTermRange(4_T), (LogRange{10_Lx, 30_Lx}));
  EXPECT_EQ(mapping.getTermRange(5_T), std::nullopt);

  mapping.removeBack(5_Lx);

  EXPECT_EQ(mapping.getTermRange(4_T), std::nullopt);
  EXPECT_EQ(mapping.getTermRange(5_T), std::nullopt);
}

TEST_F(TermIndexMappingTest, get_first_index_of_term) {
  mapping.insert({10_Lx, 40_Lx}, 4_T);
  mapping.insert({40_Lx, 60_Lx}, 5_T);

  EXPECT_EQ(mapping.getFirstIndexOfTerm(3_T), std::nullopt);
  EXPECT_EQ(mapping.getFirstIndexOfTerm(4_T), 10_Lx);
  EXPECT_EQ(mapping.getFirstIndexOfTerm(5_T), 40_Lx);
  EXPECT_EQ(mapping.getFirstIndexOfTerm(6_T), std::nullopt);
}

TEST_F(TermIndexMappingTest, get_term_of_index) {
  mapping.insert({10_Lx, 40_Lx}, 4_T);
  mapping.insert({40_Lx, 60_Lx}, 5_T);

  EXPECT_EQ(mapping.getTermOfIndex(8_Lx), std::nullopt);
  EXPECT_EQ(mapping.getTermOfIndex(15_Lx), 4_T);
  EXPECT_EQ(mapping.getTermOfIndex(39_Lx), 4_T);
  EXPECT_EQ(mapping.getTermOfIndex(40_Lx), 5_T);
  EXPECT_EQ(mapping.getTermOfIndex(59_Lx), 5_T);
  EXPECT_EQ(mapping.getTermOfIndex(60_Lx), std::nullopt);
}

TEST_F(TermIndexMappingTest, get_last_and_first_index) {
  EXPECT_EQ(mapping.getFirstIndex(), std::nullopt);
  EXPECT_EQ(mapping.getLastIndex(), std::nullopt);

  mapping.insert({10_Lx, 50_Lx}, 4_T);
  mapping.insert({50_Lx, 60_Lx}, 5_T);

  EXPECT_EQ(mapping.getFirstIndex(), (TermIndexPair{4_T, 10_Lx}));
  EXPECT_EQ(mapping.getLastIndex(), (TermIndexPair{5_T, 59_Lx}));
}

TEST_F(TermIndexMappingTest, insert_single_entry) {
  mapping.insert(1_Lx, 1_T);
  mapping.insert(2_Lx, 2_T);
  mapping.insert(3_Lx, 2_T);
  mapping.insert(4_Lx, 3_T);

  EXPECT_EQ(mapping.getFirstIndex(), (TermIndexPair{1_T, 1_Lx}));
  EXPECT_EQ(mapping.getLastIndex(), (TermIndexPair{3_T, 4_Lx}));
}
