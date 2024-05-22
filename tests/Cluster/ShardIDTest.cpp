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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"  // for Test, CmpHelperLT, Message

#include "absl/container/flat_hash_map.h"  // for BitMask
#include "Cluster/Utils/ShardID.h"         // for ShardID, operator==, hash
#include "Containers/FlatHashMap.h"        // for FlatHashMap
#include "Containers/FlatHashSet.h"        // for FlatHashSet
#include "Inspection/VPack.h"

#include <cstdint>        // for uint64_t
#include <set>            // for set, operator==, _Rb_tree_...
#include <string>         // for allocator, string
#include <unordered_map>  // for unordered_set
#include <unordered_set>  // for unordered_set
#include <vector>         // for vector

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
    EXPECT_EQ(s.id(), i);
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

TEST(ShardIDTest, canBeWrittenToStreamOutput) {
  {
    ShardID a{42};
    std::stringstream ss;
    ss << a;
    EXPECT_EQ(ss.str(), "s42");
  }
  {
    ShardID a{1337};
    std::stringstream ss;
    ss << a;
    EXPECT_EQ(ss.str(), "s1337");
  }
}

TEST(ShardIDTest, canBeSerializedStandalone) {
  ShardID a{42};
  auto result = velocypack::serialize(a);
  ASSERT_TRUE(result.isString());
  EXPECT_TRUE(result.isEqualString("s42"));
  auto b = velocypack::deserialize<ShardID>(result.slice());
  EXPECT_EQ(b, a);
}

TEST(ShardIDTest, canBeSerializedAsPartOfSet) {
  ShardID a{42};
  ShardID b{1337};
  ShardID c{91};
  std::set<ShardID> shardIds{a, b, c};
  auto result = velocypack::serialize(shardIds);
  // As this is a set, the ordering is guaranteed. Note: it is different from
  // injection order!
  ASSERT_TRUE(result.isArray());
  EXPECT_TRUE(result.at(0).isEqualString("s42"));
  EXPECT_TRUE(result.at(1).isEqualString("s91"));
  EXPECT_TRUE(result.at(2).isEqualString("s1337"));

  auto deserialized =
      velocypack::deserialize<std::set<ShardID>>(result.slice());
  EXPECT_EQ(deserialized, shardIds);
}

TEST(ShardIDTest, canBeSerializedAsUnorderdMap) {
  std::unordered_map<ShardID, uint64_t> shardIds{};
  shardIds.emplace(ShardID{42}, 42);
  shardIds.emplace(ShardID{1337}, 1337);
  shardIds.emplace(ShardID{91}, 91);

  auto result = velocypack::serialize(shardIds);
  // As this is a set, the ordering is guaranteed. Note: it is different from
  // injection order!
  ASSERT_TRUE(result.isObject());
  EXPECT_TRUE(result.hasKey("s42"));
  EXPECT_TRUE(result.get("s42").isNumber());
  EXPECT_EQ(result.get("s42").getNumber<uint64_t>(), 42);

  EXPECT_TRUE(result.hasKey("s1337"));
  EXPECT_TRUE(result.get("s1337").isNumber());
  EXPECT_EQ(result.get("s1337").getNumber<uint64_t>(), 1337);

  EXPECT_TRUE(result.hasKey("s91"));
  EXPECT_TRUE(result.get("s91").isNumber());
  EXPECT_EQ(result.get("s91").getNumber<uint64_t>(), 91);

  auto deserialized =
      velocypack::deserialize<std::unordered_map<ShardID, uint64_t>>(
          result.slice());

  EXPECT_EQ(deserialized, shardIds);
}

TEST(ShardIDTest, canBeSerializedAsFlatHashMap) {
  containers::FlatHashMap<ShardID, uint64_t> shardIds{};
  shardIds.emplace(ShardID{42}, 42);
  shardIds.emplace(ShardID{1337}, 1337);
  shardIds.emplace(ShardID{91}, 91);

  auto result = velocypack::serialize(shardIds);
  // As this is a set, the ordering is guaranteed. Note: it is different from
  // injection order!
  ASSERT_TRUE(result.isObject());
  EXPECT_TRUE(result.hasKey("s42"));
  EXPECT_TRUE(result.get("s42").isNumber());
  EXPECT_EQ(result.get("s42").getNumber<uint64_t>(), 42);

  EXPECT_TRUE(result.hasKey("s1337"));
  EXPECT_TRUE(result.get("s1337").isNumber());
  EXPECT_EQ(result.get("s1337").getNumber<uint64_t>(), 1337);

  EXPECT_TRUE(result.hasKey("s91"));
  EXPECT_TRUE(result.get("s91").isNumber());
  EXPECT_EQ(result.get("s91").getNumber<uint64_t>(), 91);

  auto deserialized =
      velocypack::deserialize<containers::FlatHashMap<ShardID, uint64_t>>(
          result.slice());

  EXPECT_EQ(deserialized, shardIds);
}

TEST(ShardIDTest, canBeFMTFormatted) {
  ShardID shard{42};
  EXPECT_EQ(fmt::format("{}", shard), "s42");
}

TEST(ShardIDTest, canBeConcateatedWithStrings) {
  ShardID shard{42};
  EXPECT_EQ(shard + "foo", "s42foo");
  EXPECT_EQ("foo" + shard, "foos42");
}

TEST(ShardIDTest, testInvalidShard) {
  EXPECT_FALSE(ShardID::invalidShard().isValid());
  EXPECT_FALSE(ShardID{0}.isValid());
  EXPECT_TRUE(ShardID{1}.isValid());
  EXPECT_TRUE(ShardID{42}.isValid());
  EXPECT_TRUE(ShardID{1337}.isValid());
}