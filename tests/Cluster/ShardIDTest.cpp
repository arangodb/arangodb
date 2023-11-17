////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Cluster/Utils/ShardID.h"
#include "Containers/FlatHashMap.h"
#include "Containers/FlatHashSet.h"

#include <unordered_set>

using namespace arangodb;

TEST(ShardIDTest, canCompareShardIDs) {
  ShardID one{1};
  ShardID two{2};
  ShardID three{3};
  ShardID twoAgain{2};
  EXPECT_GT(two, one);
  EXPECT_LT(two, three);
  EXPECT_EQ(two, twoAgain);
}

TEST(ShardIDTest, sortingDifferentDigitLengths) {
  for (auto const& multi : std::vector<uint64_t>{1, 10, 100, 1000}) {
    ShardID a{8 * multi};
    ShardID b{9 * multi};
    ShardID c{10 * multi};
    ShardID d{11 * multi};
    EXPECT_LT(a, b);
    EXPECT_LT(a, c);
    EXPECT_LT(a, d);
    EXPECT_LT(b, c);
    EXPECT_LT(b, d);
    EXPECT_LT(c, d);
  }
}

TEST(ShardIDTest, canBeAddedToSet) {
  std::set<ShardID> shardIds;
  shardIds.emplace(8);
  shardIds.emplace(11);
  shardIds.emplace(9);
  shardIds.emplace(10);
  EXPECT_EQ(shardIds.size(), 4);

  uint64_t i = 8;
  // The set needs to retain the elements in increasing
  // numerical order
  for (auto const& s : shardIds) {
    EXPECT_EQ(s.id, i);
    ++i;
  }
}

TEST(ShardIDTest, canBeAddedToUnorderedSet) {
  std::unordered_set<ShardID> shardIds;
  shardIds.emplace(8);
  shardIds.emplace(11);
  shardIds.emplace(9);
  shardIds.emplace(10);
  EXPECT_EQ(shardIds.size(), 4);

  for (uint64_t i = 8; i < 12; ++i) {
    EXPECT_TRUE(shardIds.contains(ShardID{i}));
  }
}

TEST(ShardIDTest, canBeAddedToFlatHashSet) {
  containers::FlatHashSet<ShardID> shardIds;
  shardIds.emplace(8);
  shardIds.emplace(11);
  shardIds.emplace(9);
  shardIds.emplace(10);
  EXPECT_EQ(shardIds.size(), 4);

  for (uint64_t i = 8; i < 12; ++i) {
    EXPECT_TRUE(shardIds.contains(ShardID{i}));
  }
}

TEST(ShardIDTest, canBeAddedToFlatHashMap) {
  containers::FlatHashMap<ShardID, uint64_t> shardIds;
  shardIds.emplace(8, 8);
  shardIds.emplace(11, 11);
  shardIds.emplace(9, 9);
  shardIds.emplace(10, 10);
  EXPECT_EQ(shardIds.size(), 4);

  for (uint64_t i = 8; i < 12; ++i) {
    EXPECT_TRUE(shardIds.contains(ShardID{i}));
    EXPECT_EQ(shardIds.at(ShardID{i}), i);
  }
}