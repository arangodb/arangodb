// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// TODO(pmattis): Add some tests that iterators are not invalidated by
// insertion and deletion.

#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "btree_test.h"
#include "safe_btree_map.h"
#include "safe_btree_set.h"

class UnsafeArena;

namespace btree {
namespace {

template <typename K, int N>
void SetTest() {
  typedef TestAllocator<K> TestAlloc;
  BtreeTest<safe_btree_set<K, std::less<K>, std::allocator<K>, N>, std::set<K> >();
  BtreeAllocatorTest<safe_btree_set<K, std::less<K>, TestAlloc, N> >();
}

template <typename K, int N>
void MapTest() {
  typedef TestAllocator<K> TestAlloc;
  BtreeTest<safe_btree_map<K, K, std::less<K>, std::allocator<K>, N>, std::map<K, K> >();
  BtreeAllocatorTest<safe_btree_map<K, K, std::less<K>, TestAlloc, N> >();
  BtreeMapTest<safe_btree_map<K, K, std::less<K>, std::allocator<K>, N> >();
}

TEST(SafeBtree, set_int32_32)   { SetTest<int32_t, 32>(); }
TEST(SafeBtree, set_int32_64)   { SetTest<int32_t, 64>(); }
TEST(SafeBtree, set_int32_128)  { SetTest<int32_t, 128>(); }
TEST(SafeBtree, set_int32_256)  { SetTest<int32_t, 256>(); }
TEST(SafeBtree, set_int64_256)  { SetTest<int64_t, 256>(); }
TEST(SafeBtree, set_string_256) { SetTest<std::string, 256>(); }
TEST(SafeBtree, set_pair_256)   { SetTest<std::pair<int, int>, 256>(); }
TEST(SafeBtree, map_int32_256)  { MapTest<int32_t, 256>(); }
TEST(SafeBtree, map_int64_256)  { MapTest<int64_t, 256>(); }
TEST(SafeBtree, map_string_256) { MapTest<std::string, 256>(); }
TEST(SafeBtree, map_pair_256)   { MapTest<std::pair<int, int>, 256>(); }

TEST(SafeBtree, Comparison) {
  const int kSetSize = 1201;
  safe_btree_set<int64_t> my_set;
  for (int i = 0; i < kSetSize; ++i) {
    my_set.insert(i);
  }
  safe_btree_set<int64_t> my_set_copy(my_set);
  EXPECT_TRUE(my_set_copy == my_set);
  EXPECT_TRUE(my_set == my_set_copy);
  EXPECT_FALSE(my_set_copy != my_set);
  EXPECT_FALSE(my_set != my_set_copy);

  my_set.insert(kSetSize);
  EXPECT_FALSE(my_set_copy == my_set);
  EXPECT_FALSE(my_set == my_set_copy);
  EXPECT_TRUE(my_set_copy != my_set);
  EXPECT_TRUE(my_set != my_set_copy);

  my_set.erase(kSetSize - 1);
  EXPECT_FALSE(my_set_copy == my_set);
  EXPECT_FALSE(my_set == my_set_copy);
  EXPECT_TRUE(my_set_copy != my_set);
  EXPECT_TRUE(my_set != my_set_copy);

  safe_btree_map<std::string, int64_t> my_map;
  for (int i = 0; i < kSetSize; ++i) {
    my_map[std::string(i, 'a')] = i;
  }
  safe_btree_map<std::string, int64_t> my_map_copy(my_map);
  EXPECT_TRUE(my_map_copy == my_map);
  EXPECT_TRUE(my_map == my_map_copy);
  EXPECT_FALSE(my_map_copy != my_map);
  EXPECT_FALSE(my_map != my_map_copy);

  ++my_map_copy[std::string(7, 'a')];
  EXPECT_FALSE(my_map_copy == my_map);
  EXPECT_FALSE(my_map == my_map_copy);
  EXPECT_TRUE(my_map_copy != my_map);
  EXPECT_TRUE(my_map != my_map_copy);

  my_map_copy = my_map;
  my_map["hello"] = kSetSize;
  EXPECT_FALSE(my_map_copy == my_map);
  EXPECT_FALSE(my_map == my_map_copy);
  EXPECT_TRUE(my_map_copy != my_map);
  EXPECT_TRUE(my_map != my_map_copy);

  my_map.erase(std::string(kSetSize - 1, 'a'));
  EXPECT_FALSE(my_map_copy == my_map);
  EXPECT_FALSE(my_map == my_map_copy);
  EXPECT_TRUE(my_map_copy != my_map);
  EXPECT_TRUE(my_map != my_map_copy);
}

} // namespace
} // namespace btree
