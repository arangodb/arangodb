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
#include <gtest/gtest.h>
#include "Replication2/ReplicatedLog/LogCommon.h"

using namespace arangodb;
using namespace arangodb::replication2;

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

TEST(LogRangeTest, empty_range) {
  auto range = LogRange(LogIndex{5}, LogIndex{5});
  EXPECT_TRUE(range.empty());
  auto otherRange = LogRange(LogIndex{6}, LogIndex{6});
  EXPECT_EQ(range, otherRange);
  EXPECT_EQ(range, range);

  auto nonEmptyRange = LogRange(LogIndex{5}, LogIndex{6});
  EXPECT_NE(nonEmptyRange, range);
  EXPECT_EQ(nonEmptyRange, nonEmptyRange);
  EXPECT_FALSE(nonEmptyRange.empty());
}

TEST(LogRangeTest, count) {
  EXPECT_EQ(LogRange(LogIndex{5}, LogIndex{8}).count(), 3);
  EXPECT_EQ(LogRange(LogIndex{8}, LogIndex{8}).count(), 0);
}

TEST(LogRangeTest, contains) {
  EXPECT_TRUE(LogRange(LogIndex{5}, LogIndex{6}).contains(LogIndex{5}));
  EXPECT_FALSE(LogRange(LogIndex{5}, LogIndex{5}).contains(LogIndex{5}));
  EXPECT_FALSE(LogRange(LogIndex{50}, LogIndex{60}).contains(LogIndex{5}));
}

TEST(LogRangeTest, intersect) {
  auto a = LogRange(LogIndex{0}, LogIndex{10});
  auto b = LogRange(LogIndex{5}, LogIndex{15});
  auto c = LogRange(LogIndex{10}, LogIndex{20});

  EXPECT_EQ(intersect(a, b), LogRange(LogIndex{5}, LogIndex{10}));
  EXPECT_EQ(intersect(b, c), LogRange(LogIndex{10}, LogIndex{15}));
  EXPECT_TRUE(intersect(a, c).empty());
}

TEST(LogRangeTest, iterate) {
  std::vector<std::uint64_t> vec;
  for (auto idx : LogRange(LogIndex{14}, LogIndex{18})) {
    vec.push_back(idx.value);
  }

  auto expected = std::vector<std::uint64_t>{14, 15, 16, 17};
  EXPECT_EQ(vec, expected);
}
