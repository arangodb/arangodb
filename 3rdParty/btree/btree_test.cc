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

#include "gtest/gtest.h"
#include "btree_map.h"
#include "btree_set.h"
#include "btree_test.h"

namespace btree {
namespace {

template <typename K, int N>
void SetTest() {
  typedef TestAllocator<K> TestAlloc;
  ASSERT_EQ(sizeof(btree_set<K>), sizeof(void*));
  BtreeTest<btree_set<K, std::less<K>, std::allocator<K>, N>, std::set<K> >();
  BtreeAllocatorTest<btree_set<K, std::less<K>, TestAlloc, N> >();
}

template <typename K, int N>
void MapTest() {
  typedef TestAllocator<K> TestAlloc;
  ASSERT_EQ(sizeof(btree_map<K, K>), sizeof(void*));
  BtreeTest<btree_map<K, K, std::less<K>, std::allocator<K>, N>, std::map<K, K> >();
  BtreeAllocatorTest<btree_map<K, K, std::less<K>, TestAlloc, N> >();
  BtreeMapTest<btree_map<K, K, std::less<K>, std::allocator<K>, N> >();
}

TEST(Btree, set_int32_32)   { SetTest<int32_t, 32>(); }
TEST(Btree, set_int32_64)   { SetTest<int32_t, 64>(); }
TEST(Btree, set_int32_128)  { SetTest<int32_t, 128>(); }
TEST(Btree, set_int32_256)  { SetTest<int32_t, 256>(); }
TEST(Btree, set_int64_256)  { SetTest<int64_t, 256>(); }
TEST(Btree, set_string_256) { SetTest<std::string, 256>(); }
TEST(Btree, set_pair_256)   { SetTest<std::pair<int, int>, 256>(); }
TEST(Btree, map_int32_256)  { MapTest<int32_t, 256>(); }
TEST(Btree, map_int64_256)  { MapTest<int64_t, 256>(); }
TEST(Btree, map_string_256) { MapTest<std::string, 256>(); }
TEST(Btree, map_pair_256)   { MapTest<std::pair<int, int>, 256>(); }

// Large-node tests
TEST(Btree, map_int32_1024)   { MapTest<int32_t, 1024>(); }
TEST(Btree, map_int32_1032)   { MapTest<int32_t, 1032>(); }
TEST(Btree, map_int32_1040)   { MapTest<int32_t, 1040>(); }
TEST(Btree, map_int32_1048)   { MapTest<int32_t, 1048>(); }
TEST(Btree, map_int32_1056)   { MapTest<int32_t, 1056>(); }

TEST(Btree, map_int32_2048)   { MapTest<int32_t, 2048>(); }
TEST(Btree, map_int32_4096)   { MapTest<int32_t, 4096>(); }
TEST(Btree, set_int32_1024)   { SetTest<int32_t, 1024>(); }
TEST(Btree, set_int32_2048)   { SetTest<int32_t, 2048>(); }
TEST(Btree, set_int32_4096)   { SetTest<int32_t, 4096>(); }
TEST(Btree, map_string_1024)   { MapTest<std::string, 1024>(); }
TEST(Btree, map_string_2048)   { MapTest<std::string, 2048>(); }
TEST(Btree, map_string_4096)   { MapTest<std::string, 4096>(); }
TEST(Btree, set_string_1024)   { SetTest<std::string, 1024>(); }
TEST(Btree, set_string_2048)   { SetTest<std::string, 2048>(); }
TEST(Btree, set_string_4096)   { SetTest<std::string, 4096>(); }

template <typename K, int N>
void MultiSetTest() {
  typedef TestAllocator<K> TestAlloc;
  ASSERT_EQ(sizeof(btree_multiset<K>), sizeof(void*));
  BtreeMultiTest<btree_multiset<K, std::less<K>, std::allocator<K>, N>,
      std::multiset<K> >();
  BtreeAllocatorTest<btree_multiset<K, std::less<K>, TestAlloc, N> >();
}

template <typename K, int N>
void MultiMapTest() {
  typedef TestAllocator<K> TestAlloc;
  ASSERT_EQ(sizeof(btree_multimap<K, K>), sizeof(void*));
  BtreeMultiTest<btree_multimap<K, K, std::less<K>, std::allocator<K>, N>,
      std::multimap<K, K> >();
  BtreeMultiMapTest<btree_multimap<K, K, std::less<K>, std::allocator<K>, N> >();
  BtreeAllocatorTest<btree_multimap<K, K, std::less<K>, TestAlloc, N> >();
}

TEST(Btree, multiset_int32_256)  { MultiSetTest<int32_t, 256>(); }
TEST(Btree, multiset_int64_256)  { MultiSetTest<int64_t, 256>(); }
TEST(Btree, multiset_string_256) { MultiSetTest<std::string, 256>(); }
TEST(Btree, multiset_pair_256)   { MultiSetTest<std::pair<int, int>, 256>(); }
TEST(Btree, multimap_int32_256)  { MultiMapTest<int32_t, 256>(); }
TEST(Btree, multimap_int64_256)  { MultiMapTest<int64_t, 256>(); }
TEST(Btree, multimap_string_256) { MultiMapTest<std::string, 256>(); }
TEST(Btree, multimap_pair_256)   { MultiMapTest<std::pair<int, int>, 256>(); }

// Large-node tests
TEST(Btree, multimap_int32_1024)   { MultiMapTest<int32_t, 1024>(); }
TEST(Btree, multimap_int32_2048)   { MultiMapTest<int32_t, 2048>(); }
TEST(Btree, multimap_int32_4096)   { MultiMapTest<int32_t, 4096>(); }
TEST(Btree, multiset_int32_1024)   { MultiSetTest<int32_t, 1024>(); }
TEST(Btree, multiset_int32_2048)   { MultiSetTest<int32_t, 2048>(); }
TEST(Btree, multiset_int32_4096)   { MultiSetTest<int32_t, 4096>(); }
TEST(Btree, multimap_string_1024)   { MultiMapTest<std::string, 1024>(); }
TEST(Btree, multimap_string_2048)   { MultiMapTest<std::string, 2048>(); }
TEST(Btree, multimap_string_4096)   { MultiMapTest<std::string, 4096>(); }
TEST(Btree, multiset_string_1024)   { MultiSetTest<std::string, 1024>(); }
TEST(Btree, multiset_string_2048)   { MultiSetTest<std::string, 2048>(); }
TEST(Btree, multiset_string_4096)   { MultiSetTest<std::string, 4096>(); }

// Verify that swapping btrees swaps the key comparision functors.
struct SubstringLess {
  SubstringLess() : n(2) {}
  SubstringLess(size_t length)
      : n(length) {
  }
  bool operator()(const std::string &a, const std::string &b) const {
    std::string as(a.data(), std::min(n, a.size()));
    std::string bs(b.data(), std::min(n, b.size()));
    return as < bs;
  }
  size_t n;
};

TEST(Btree, SwapKeyCompare) {
  typedef btree_set<std::string, SubstringLess> SubstringSet;
  SubstringSet s1(SubstringLess(1), SubstringSet::allocator_type());
  SubstringSet s2(SubstringLess(2), SubstringSet::allocator_type());

  ASSERT_TRUE(s1.insert("a").second);
  ASSERT_FALSE(s1.insert("aa").second);

  ASSERT_TRUE(s2.insert("a").second);
  ASSERT_TRUE(s2.insert("aa").second);
  ASSERT_FALSE(s2.insert("aaa").second);

  swap(s1, s2);

  ASSERT_TRUE(s1.insert("b").second);
  ASSERT_TRUE(s1.insert("bb").second);
  ASSERT_FALSE(s1.insert("bbb").second);

  ASSERT_TRUE(s2.insert("b").second);
  ASSERT_FALSE(s2.insert("bb").second);
}

TEST(Btree, UpperBoundRegression) {
  // Regress a bug where upper_bound would default-construct a new key_compare
  // instead of copying the existing one.
  typedef btree_set<std::string, SubstringLess> SubstringSet;
  SubstringSet my_set(SubstringLess(3));
  my_set.insert("aab");
  my_set.insert("abb");
  // We call upper_bound("aaa").  If this correctly uses the length 3
  // comparator, aaa < aab < abb, so we should get aab as the result.
  // If it instead uses the default-constructed length 2 comparator,
  // aa == aa < ab, so we'll get abb as our result.
  SubstringSet::iterator it = my_set.upper_bound("aaa");
  ASSERT_TRUE(it != my_set.end());
  EXPECT_EQ("aab", *it);
}


TEST(Btree, IteratorIncrementBy) {
  // Test that increment_by returns the same position as increment.
  const int kSetSize = 2341;
  btree_set<int32_t> my_set;
  for (int i = 0; i < kSetSize; ++i) {
    my_set.insert(i);
  }

  {
    // Simple increment vs. increment by.
    btree_set<int32_t>::iterator a = my_set.begin();
    btree_set<int32_t>::iterator b = my_set.begin();
    a.increment();
    b.increment_by(1);
    EXPECT_EQ(*a, *b);
  }

  btree_set<int32_t>::iterator a = my_set.begin();
  for (int i = 1; i < kSetSize; ++i) {
    ++a;
    // increment_by
    btree_set<int32_t>::iterator b = my_set.begin();
    b.increment_by(i);
    EXPECT_EQ(*a, *b) << ": i=" << i;
  }
}

TEST(Btree, Comparison) {
  const int kSetSize = 1201;
  btree_set<int64_t> my_set;
  for (int i = 0; i < kSetSize; ++i) {
    my_set.insert(i);
  }
  btree_set<int64_t> my_set_copy(my_set);
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

  btree_map<std::string, int64_t> my_map;
  for (int i = 0; i < kSetSize; ++i) {
    my_map[std::string(i, 'a')] = i;
  }
  btree_map<std::string, int64_t> my_map_copy(my_map);
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

TEST(Btree, RangeCtorSanity) {
  typedef btree_set<int, std::less<int>, std::allocator<int>, 256> test_set;
  typedef btree_map<int, int, std::less<int>, std::allocator<int>, 256> 
      test_map;
  typedef btree_multiset<int, std::less<int>, std::allocator<int>, 256> 
      test_mset;
  typedef btree_multimap<int, int, std::less<int>, std::allocator<int>, 256> 
      test_mmap;
  std::vector<int> ivec;
  ivec.push_back(1);
  std::map<int, int> imap;
  imap.insert(std::make_pair(1, 2));
  test_mset tmset(ivec.begin(), ivec.end());
  test_mmap tmmap(imap.begin(), imap.end());
  test_set tset(ivec.begin(), ivec.end());
  test_map tmap(imap.begin(), imap.end());
  EXPECT_EQ(1, tmset.size());
  EXPECT_EQ(1, tmmap.size());
  EXPECT_EQ(1, tset.size());
  EXPECT_EQ(1, tmap.size());
}

} // namespace
} // namespace btree
