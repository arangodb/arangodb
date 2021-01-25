////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <random>
#include <vector>

#include "gtest/gtest.h"

#include "Basics/hashes.h"
#include "Containers/MerkleTree.h"
#include "Logger/LogMacros.h"

namespace {
std::vector<std::uint64_t> permutation(std::uint64_t n) {
  std::vector<std::uint64_t> v;

  v.reserve(n);
  for (std::uint64_t i = 0; i < n; ++i) {
    v.emplace_back(i);
  }

  std::random_device rd;
  std::mt19937_64 g(rd());
  std::shuffle(v.begin(), v.end(), g);

  return v;
}

bool diffAsExpected(
    ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64>& t1,
    ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64>& t2,
    std::vector<std::pair<std::uint64_t, std::uint64_t>>& expected) {
  std::vector<std::pair<std::uint64_t, std::uint64_t>> d1 = t1.diff(t2);
  std::vector<std::pair<std::uint64_t, std::uint64_t>> d2 = t2.diff(t1);

  auto printTrees = [&t1, &t2]() -> void {
    LOG_DEVEL << "T1: " << t1;
    LOG_DEVEL << "T2: " << t2;
  };

  if (d1.size() != expected.size() || d2.size() != expected.size()) {
    printTrees();
    return false;
  }
  for (std::uint64_t i = 0; i < expected.size(); ++i) {
    if (d1[i].first != expected[i].first || d1[i].second != expected[i].second ||
        d2[i].first != expected[i].first || d2[i].second != expected[i].second) {
      printTrees();
      return false;
    }
  }
  return true;
}

bool partitionAsExpected(
    ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64>& tree,
    std::uint64_t count, std::vector<std::pair<std::uint64_t, std::uint64_t>> expected) {
  std::vector<std::pair<std::uint64_t, std::uint64_t>> partitions =
      tree.partitionKeys(count);

  if (partitions.size() != expected.size()) {
    return false;
  }
  for (std::uint64_t i = 0; i < expected.size(); ++i) {
    if (partitions[i].first != expected[i].first ||
        partitions[i].second != expected[i].second) {
      return false;
    }
  }
  return true;
}
}  // namespace

class TestNodeCountAtDepth
    : public arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64> {
  static_assert(nodeCountAtDepth(0) == 1);
  static_assert(nodeCountAtDepth(1) == 8);
  static_assert(nodeCountAtDepth(2) == 64);
  static_assert(nodeCountAtDepth(3) == 512);
  // ...
  static_assert(nodeCountAtDepth(10) == 1073741824);
};

class InternalMerkleTreeTest
    : public ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64>,
      public ::testing::Test {
 public:
  InternalMerkleTreeTest()
      : MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64>(2, 0, 64) {}
};

TEST_F(InternalMerkleTreeTest, test_childrenAreLeaves) {
  ASSERT_FALSE(childrenAreLeaves(0));
  for (std::uint64_t index = 1; index < 9; ++index) {
    ASSERT_TRUE(childrenAreLeaves(index));
  }
  for (std::uint64_t index = 9; index < 73; ++index) {
    ASSERT_FALSE(childrenAreLeaves(index));
  }
}

TEST_F(InternalMerkleTreeTest, test_chunkRange) {
  auto r = chunkRange(0, 0);
  ASSERT_EQ(r.first, 0);
  ASSERT_EQ(r.second, 63);

  for (std::uint64_t chunk = 0; chunk < 8; ++chunk) {
    auto r = chunkRange(chunk, 1);
    ASSERT_EQ(r.first, chunk * 8);
    ASSERT_EQ(r.second, ((chunk + 1) * 8) - 1);
  }

  for (std::uint64_t chunk = 0; chunk < 64; ++chunk) {
    auto r = chunkRange(chunk, 2);
    ASSERT_EQ(r.first, chunk);
    ASSERT_EQ(r.second, chunk);
  }
}

TEST_F(InternalMerkleTreeTest, test_index) {
  std::pair<std::uint64_t, std::uint64_t> range = this->range();
  ASSERT_EQ(range.first, 0);
  ASSERT_EQ(range.second, 64);

  // make sure depth 0 always gets us 0
  ASSERT_EQ(index(0, 0), 0);
  ASSERT_EQ(index(63, 0), 0);

  // check boundaries at level 1
  for (std::uint64_t chunk = 0; chunk < 8; ++chunk) {
    std::uint64_t left = chunk * 8;
    std::uint64_t right = ((chunk + 1) * 8) - 1;
    ASSERT_EQ(index(left, 1), chunk + 1);
    ASSERT_EQ(index(right, 1), chunk + 1);
  }

  // check boundaries at level 2
  for (std::uint64_t chunk = 0; chunk < 64; ++chunk) {
    std::uint64_t left = chunk;  // only one value per chunk
    ASSERT_EQ(index(left, 2), chunk + 9);
  }
}

TEST_F(InternalMerkleTreeTest, test_modify) {
  std::pair<std::uint64_t, std::uint64_t> range = this->range();
  ASSERT_EQ(range.first, 0);
  ASSERT_EQ(range.second, 64);

  ASSERT_EQ(count(), 0);
  // check that an attempt to remove will fail if it's empty
  ASSERT_THROW(modify(0, false), std::invalid_argument);
  ASSERT_EQ(count(), 0);

  // insert a single value
  modify(0, true);
  ASSERT_EQ(count(), 1);

  // build set of indexes that should be touched
  std::set<std::uint64_t> indices0;
  for (std::uint64_t depth = 0; depth <= meta().maxDepth; ++depth) {
    indices0.emplace(this->index(0, depth));
  }

  ::arangodb::containers::FnvHashProvider hasher;
  // check that it sets everything it should, and nothing it shouldn't
  for (std::uint64_t index = 0; index < 73; ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::uint64_t expectedCount = inSet0 ? 1 : 0;
    std::uint64_t expectedHash = inSet0 ? hasher.hash(0) : 0;

    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, expectedCount);
    ASSERT_EQ(node.hash, expectedHash);
  }

  // insert another value, minimal overlap
  modify(63, true);
  ASSERT_EQ(count(), 2);

  // build set of indexes that should be touched
  std::set<std::uint64_t> indices63;
  for (std::uint64_t depth = 0; depth <= meta().maxDepth; ++depth) {
    indices63.emplace(this->index(63, depth));
  }

  // check that it sets everything it should, and nothing it shouldn't
  for (std::uint64_t index = 0; index < 73; ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::uint64_t expectedCount = inSet0 ? 1 : 0;
    std::uint64_t expectedHash = inSet0 ? hasher.hash(0) : 0;
    if (indices63.find(index) != indices63.end()) {
      ++expectedCount;
      expectedHash ^= hasher.hash(63);
    }

    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, expectedCount);
    ASSERT_EQ(node.hash, expectedHash);
  }

  // insert another value, more overlap
  modify(1, true);
  ASSERT_EQ(count(), 3);

  // build set of indexes that should be touched
  std::set<std::uint64_t> indices1;
  for (std::uint64_t depth = 0; depth <= meta().maxDepth; ++depth) {
    indices1.emplace(this->index(1, depth));
  }

  // check that it sets everything it should, and nothing it shouldn't
  for (std::uint64_t index = 0; index < 73; ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::uint64_t expectedCount = inSet0 ? 1 : 0;
    std::uint64_t expectedHash = inSet0 ? hasher.hash(0) : 0;
    if (indices1.find(index) != indices1.end()) {
      ++expectedCount;
      expectedHash ^= hasher.hash(1);
    }
    if (indices63.find(index) != indices63.end()) {
      ++expectedCount;
      expectedHash ^= hasher.hash(63);
    }

    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, expectedCount);
    ASSERT_EQ(node.hash, expectedHash);
  }

  // remove a value, minimal overlap
  modify(63, false);
  ASSERT_EQ(count(), 2);

  // check that it sets everything it should, and nothing it shouldn't
  for (std::uint64_t index = 0; index < 73; ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::uint64_t expectedCount = inSet0 ? 1 : 0;
    std::uint64_t expectedHash = inSet0 ? hasher.hash(0) : 0;
    if (indices1.find(index) != indices1.end()) {
      ++expectedCount;
      expectedHash ^= hasher.hash(1);
    }

    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, expectedCount);
    ASSERT_EQ(node.hash, expectedHash);
  }

  // remove a value, maximal overlap
  modify(1, false);
  ASSERT_EQ(count(), 1);

  // check that it sets everything it should, and nothing it shouldn't
  for (std::uint64_t index = 0; index < 73; ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::uint64_t expectedCount = inSet0 ? 1 : 0;
    std::uint64_t expectedHash = inSet0 ? hasher.hash(0) : 0;

    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, expectedCount);
    ASSERT_EQ(node.hash, expectedHash);
  }

  // remove a value, maximal overlap
  modify(0, false);
  ASSERT_EQ(count(), 0);

  // check that it sets everything it should, and nothing it shouldn't
  for (std::uint64_t index = 0; index < 73; ++index) {
    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, 0);
    ASSERT_EQ(node.hash, 0);
  }
}

TEST_F(InternalMerkleTreeTest, test_grow) {
  std::pair<std::uint64_t, std::uint64_t> range = this->range();
  ASSERT_EQ(range.first, 0);
  ASSERT_EQ(range.second, 64);

  // fill the tree, but not enough that it grows
  for (std::uint64_t i = 0; i < 64; ++i) {
    insert(i);
  }
  range = this->range();
  ASSERT_EQ(range.first, 0);
  ASSERT_EQ(range.second, 64);

  ::arangodb::containers::FnvHashProvider hasher;
  // check that tree state is as expected prior to growing
  {
    std::uint64_t hash0 = 0;
    std::uint64_t hash1[8] = {0};
    std::uint64_t hash2[64] = {0};
    for (std::uint64_t i = 0; i < 64; ++i) {
      hash0 ^= hasher.hash(static_cast<std::uint64_t>(i));
      hash1[i / 8] ^= hasher.hash(static_cast<std::uint64_t>(i));
      hash2[i] ^= hasher.hash(static_cast<std::uint64_t>(i));
    }
    for (std::uint64_t i = 0; i < 64; ++i) {
      Node& node = this->node(this->index(static_cast<std::uint64_t>(i), 2));
      ASSERT_EQ(node.count, 1);
      ASSERT_EQ(node.hash, hash2[i]);
    }
    for (std::uint64_t i = 0; i < 8; ++i) {
      Node& node = this->node(i + 1);
      ASSERT_EQ(node.count, 8);
      ASSERT_EQ(node.hash, hash1[i]);
    }
    {
      Node& node = this->node(0);
      ASSERT_EQ(node.count, 64);
      ASSERT_EQ(node.hash, hash0);
    }
  }

  // insert some more and cause it to grow
  for (std::uint64_t i = 64; i < 128; ++i) {
    insert(i);
  }
  range = this->range();
  ASSERT_EQ(range.first, 0);
  ASSERT_EQ(range.second, 128);

  // check that tree state is as expected after growing
  {
    std::uint64_t hash0 = 0;
    std::uint64_t hash1[8] = {0};
    std::uint64_t hash2[64] = {0};
    for (std::uint64_t i = 0; i < 128; ++i) {
      hash0 ^= hasher.hash(static_cast<std::uint64_t>(i));
      hash1[i / 16] ^= hasher.hash(static_cast<std::uint64_t>(i));
      hash2[i / 2] ^= hasher.hash(static_cast<std::uint64_t>(i));
    }
    for (std::uint64_t i = 0; i < 64; ++i) {
      Node& node = this->node(i + 9);
      ASSERT_EQ(node.count, 2);
      if (node.hash != hash2[i]) {
      }
      ASSERT_EQ(node.hash, hash2[i]);
    }
    for (std::uint64_t i = 0; i < 8; ++i) {
      Node& node = this->node(i + 1);
      ASSERT_EQ(node.count, 16);
      ASSERT_EQ(node.hash, hash1[i]);
    }
    {
      Node& node = this->node(0);
      ASSERT_EQ(node.count, 128);
      ASSERT_EQ(node.hash, hash0);
    }
  }
}

TEST_F(InternalMerkleTreeTest, test_partition) {
  ASSERT_TRUE(::partitionAsExpected(*this, 0, {{0, 64}}));

  for (std::uint64_t i = 0; i < 32; ++i) {
    this->insert(2 * i);
  }

  ASSERT_TRUE(::partitionAsExpected(*this, 0, {{0, 64}}));
  ASSERT_TRUE(::partitionAsExpected(*this, 1, {{0, 64}}));
  ASSERT_TRUE(::partitionAsExpected(*this, 2, {{0, 30}, {31, 63}}));
  ASSERT_TRUE(::partitionAsExpected(*this, 3, {{0, 18}, {19, 40}, {41, 63}}));
  ASSERT_TRUE(::partitionAsExpected(*this, 4, {{0, 14}, {15, 30}, {31, 46}, {47, 63}}));
  ///
  ASSERT_TRUE(::partitionAsExpected(
      *this, 42,
      {{0, 0},   {1, 2},   {3, 4},   {5, 6},   {7, 8},   {9, 10},  {11, 12},
       {13, 14}, {15, 16}, {17, 18}, {19, 20}, {21, 22}, {23, 24}, {25, 26},
       {27, 28}, {29, 30}, {31, 32}, {33, 34}, {35, 36}, {37, 38}, {39, 40},
       {41, 42}, {43, 44}, {45, 46}, {47, 48}, {49, 50}, {51, 52}, {53, 54},
       {55, 56}, {57, 58}, {59, 60}, {61, 62}}));

  // now let's make the distribution more uneven and see how things go
  this->grow(511);

  ASSERT_TRUE(::partitionAsExpected(*this, 3, {{0, 23}, {24, 47}, {48, 511}}));
  ASSERT_TRUE(::partitionAsExpected(*this, 4, {{0, 15}, {16, 31}, {32, 47}, {48, 511}}));

  // lump it all in one cell
  this->grow(4095);

  ASSERT_TRUE(::partitionAsExpected(*this, 4, {{0, 63}}));
}

TEST(MerkleTreeTest, test_diff_equal) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64> t1(2, 0, 64);
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64> t2(2, 0, 64);

  std::vector<std::pair<std::uint64_t, std::uint64_t>> expected;  // empty
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));

  std::vector<std::uint64_t> order = ::permutation(64);
  for (std::uint64_t i : order) {
    t1.insert(i);
    t2.insert(i);
    ASSERT_TRUE(::diffAsExpected(t1, t2, expected));
  }

  order = ::permutation(64);
  for (std::uint64_t i : order) {
    t1.remove(i);
    t2.remove(i);
    ASSERT_TRUE(::diffAsExpected(t1, t2, expected));
  }
}

TEST(MerkleTreeTest, test_diff_one_empty) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64> t1(2, 0, 64);
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64> t2(2, 0, 64);

  std::vector<std::pair<std::uint64_t, std::uint64_t>> expected;
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));

  for (std::uint64_t i = 0; i < 8; ++i) {
    t1.insert(8 * i);
    expected.emplace_back(std::make_pair(8 * i, 8 * i));
    ASSERT_TRUE(::diffAsExpected(t1, t2, expected));
  }

  expected.clear();
  for (std::uint64_t i = 0; i < 8; ++i) {
    t1.insert((8 * i) + 1);
    expected.emplace_back(std::make_pair(8 * i, (8 * i) + 1));
  }
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));

  expected.clear();
  for (std::uint64_t i = 0; i < 8; ++i) {
    t1.insert((8 * i) + 2);
    t1.insert((8 * i) + 3);
    expected.emplace_back(std::make_pair(8 * i, (8 * i) + 3));
  }
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));

  expected.clear();
  for (std::uint64_t i = 0; i < 8; ++i) {
    t1.insert((8 * i) + 4);
    t1.insert((8 * i) + 5);
    t1.insert((8 * i) + 6);
    t1.insert((8 * i) + 7);
  }
  expected.emplace_back(std::make_pair(0, 63));
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));
}

TEST(MerkleTreeTest, test_diff_misc) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64> t1(2, 0, 64);
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64> t2(2, 0, 64);

  std::vector<std::pair<std::uint64_t, std::uint64_t>> expected;
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));

  for (std::uint64_t i = 0; i < 32; ++i) {
    t1.insert(2 * i);
    expected.emplace_back(std::make_pair(2 * i, 2 * i));
  }
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));

  expected.clear();
  for (std::uint64_t i = 0; i < 32; ++i) {
    t2.insert((2 * i) + 1);
  }
  expected.emplace_back(std::make_pair(0, 63));
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));

  expected.clear();
  for (std::uint64_t i = 0; i < 16; ++i) {
    t1.insert((2 * i) + 1);
    expected.emplace_back(std::make_pair(2 * i, 2 * i));
  }
  expected.emplace_back(std::make_pair(32, 63));
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));
}

TEST(MerkleTreeTest, test_serializeBinary) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64> t1(2, 0, 64);

  for (std::uint64_t i = 0; i < 32; ++i) {
    t1.insert(2 * i);
  }

  std::string t1s;
  t1.serializeBinary(t1s, true);
  std::unique_ptr<::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64>> t2 =
      ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64>::fromBuffer(t1s);
  ASSERT_NE(t2, nullptr);
  ASSERT_TRUE(t1.diff(*t2).empty());
  ASSERT_TRUE(t2->diff(t1).empty());
}

TEST(MerkleTreeTest, test_serializePortable) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64> t1(2, 0, 64);

  for (std::uint64_t i = 0; i < 32; ++i) {
    t1.insert(2 * i);
  }

  ::arangodb::velocypack::Builder t1s;
  t1.serialize(t1s);
  std::unique_ptr<::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64>> t2 =
      ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64>::deserialize(
          t1s.slice());
  ASSERT_NE(t2, nullptr);
  ASSERT_TRUE(t1.diff(*t2).empty());
  ASSERT_TRUE(t2->diff(t1).empty());
}

TEST(MerkleTreeTest, test_deepen) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64> t1(2, 0, 64);
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3, 64> t2(2, 0, 64);

  std::vector<std::pair<std::uint64_t, std::uint64_t>> expected;  // empty
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));

  std::vector<std::uint64_t> order = ::permutation(64);
  for (std::uint64_t i : order) {
    t1.insert(i);
    t2.insert(i);
    ASSERT_TRUE(::diffAsExpected(t1, t2, expected));
  }

  order = ::permutation(64);
  for (std::uint64_t i : order) {
    t1.remove(i);
    t2.remove(i);
    ASSERT_TRUE(::diffAsExpected(t1, t2, expected));
  }

  auto t3 = t2.cloneWithDepth(3);
  ASSERT_TRUE(::diffAsExpected(t1, *t3, expected));
  ASSERT_TRUE(::diffAsExpected(*t3, t1, expected));
  ASSERT_TRUE(::diffAsExpected(t2, *t3, expected));
  ASSERT_TRUE(::diffAsExpected(*t3, t2, expected));

  auto t4 = t1.cloneWithDepth(3);
  ASSERT_TRUE(::diffAsExpected(t1, *t4, expected));
  ASSERT_TRUE(::diffAsExpected(*t4, t1, expected));
  ASSERT_TRUE(::diffAsExpected(t2, *t4, expected));
  ASSERT_TRUE(::diffAsExpected(*t4, t2, expected));
  ASSERT_TRUE(::diffAsExpected(*t3, *t4, expected));
  ASSERT_TRUE(::diffAsExpected(*t4, *t3, expected));

  auto t5 = t1.cloneWithDepth(4);
  ASSERT_TRUE(::diffAsExpected(t1, *t5, expected));
  ASSERT_TRUE(::diffAsExpected(*t5, t1, expected));
  ASSERT_TRUE(::diffAsExpected(t2, *t5, expected));
  ASSERT_TRUE(::diffAsExpected(*t5, t2, expected));
  ASSERT_TRUE(::diffAsExpected(*t3, *t5, expected));
  ASSERT_TRUE(::diffAsExpected(*t5, *t3, expected));
  ASSERT_TRUE(::diffAsExpected(*t4, *t5, expected));
  ASSERT_TRUE(::diffAsExpected(*t5, *t4, expected));
}
