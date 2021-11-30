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

#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"
#include "Basics/hashes.h"
#include "Containers/MerkleTree.h"
#include "Logger/LogMacros.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

using namespace arangodb;

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
    ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>& t1,
    ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>& t2,
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
    ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>& tree,
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
    : public arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> {
  static_assert(nodeCountAtDepth(0) == 1);
  static_assert(nodeCountAtDepth(1) == 8);
  static_assert(nodeCountAtDepth(2) == 64);
  static_assert(nodeCountAtDepth(3) == 512);
  // ...
  static_assert(nodeCountAtDepth(10) == 1073741824);
};

class InternalMerkleTreeTest
    : public ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>,
      public ::testing::Test {
 public:
  InternalMerkleTreeTest()
      : MerkleTree<::arangodb::containers::FnvHashProvider, 3>(2, 0, 64) {}
};

TEST_F(InternalMerkleTreeTest, test_chunkRange) {
  auto r = chunkRange(0, 0);
  EXPECT_EQ(r.first, 0);
  EXPECT_EQ(r.second, 63);

  for (std::uint64_t chunk = 0; chunk < 8; ++chunk) {
    auto r = chunkRange(chunk, 1);
    EXPECT_EQ(r.first, chunk * 8);
    EXPECT_EQ(r.second, ((chunk + 1) * 8) - 1);
  }

  for (std::uint64_t chunk = 0; chunk < 64; ++chunk) {
    auto r = chunkRange(chunk, 2);
    EXPECT_EQ(r.first, chunk);
    EXPECT_EQ(r.second, chunk);
  }
}

TEST_F(InternalMerkleTreeTest, test_index) {
  std::pair<std::uint64_t, std::uint64_t> range = this->range();
  EXPECT_EQ(range.first, 0);
  EXPECT_EQ(range.second, 64);

  // check boundaries at level 2
  for (std::uint64_t chunk = 0; chunk < 64; ++chunk) {
    std::uint64_t left = chunk;  // only one value per chunk
    EXPECT_EQ(index(left), chunk);
  }
}

TEST_F(InternalMerkleTreeTest, test_modify) {
  std::pair<std::uint64_t, std::uint64_t> range = this->range();
  EXPECT_EQ(range.first, 0);
  EXPECT_EQ(range.second, 64);

  EXPECT_EQ(count(), 0);
  // check that an attempt to remove will fail if it's empty
  EXPECT_THROW(modify(0, false), std::invalid_argument);
  EXPECT_EQ(count(), 0);

  // insert a single value
  modify(0, true);
  EXPECT_EQ(count(), 1);

  // build set of indexes that should be touched
  std::set<std::uint64_t> indices0{ this->index(0) };

  ::arangodb::containers::FnvHashProvider hasher;
  // check that it sets everything it should, and nothing it shouldn't
  for (std::uint64_t index = 0; index < 64; ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::uint64_t expectedCount = inSet0 ? 1 : 0;
    std::uint64_t expectedHash = inSet0 ? hasher.hash(0) : 0;

    InternalMerkleTreeTest::Node& node = this->node(index);
    EXPECT_EQ(node.count, expectedCount);
    EXPECT_EQ(node.hash, expectedHash);
  }

  // insert another value, minimal overlap
  modify(63, true);
  EXPECT_EQ(count(), 2);

  // build set of indexes that should be touched
  std::set<std::uint64_t> indices63{ this->index(63) };

  // check that it sets everything it should, and nothing it shouldn't
  for (std::uint64_t index = 0; index < 64; ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::uint64_t expectedCount = inSet0 ? 1 : 0;
    std::uint64_t expectedHash = inSet0 ? hasher.hash(0) : 0;
    if (indices63.find(index) != indices63.end()) {
      ++expectedCount;
      expectedHash ^= hasher.hash(63);
    }

    InternalMerkleTreeTest::Node& node = this->node(index);
    EXPECT_EQ(node.count, expectedCount);
    EXPECT_EQ(node.hash, expectedHash);
  }

  // insert another value, more overlap
  modify(1, true);
  EXPECT_EQ(count(), 3);

  // build set of indexes that should be touched
  std::set<std::uint64_t> indices1{ this->index(1) };

  // check that it sets everything it should, and nothing it shouldn't
  for (std::uint64_t index = 0; index < 64; ++index) {
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
    EXPECT_EQ(node.count, expectedCount);
    EXPECT_EQ(node.hash, expectedHash);
  }

  // remove a value, minimal overlap
  modify(63, false);
  EXPECT_EQ(count(), 2);

  // check that it sets everything it should, and nothing it shouldn't
  for (std::uint64_t index = 0; index < 64; ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::uint64_t expectedCount = inSet0 ? 1 : 0;
    std::uint64_t expectedHash = inSet0 ? hasher.hash(0) : 0;
    if (indices1.find(index) != indices1.end()) {
      ++expectedCount;
      expectedHash ^= hasher.hash(1);
    }

    InternalMerkleTreeTest::Node& node = this->node(index);
    EXPECT_EQ(node.count, expectedCount);
    EXPECT_EQ(node.hash, expectedHash);
  }

  // remove a value, maximal overlap
  modify(1, false);
  EXPECT_EQ(count(), 1);

  // check that it sets everything it should, and nothing it shouldn't
  for (std::uint64_t index = 0; index < 64; ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::uint64_t expectedCount = inSet0 ? 1 : 0;
    std::uint64_t expectedHash = inSet0 ? hasher.hash(0) : 0;

    InternalMerkleTreeTest::Node& node = this->node(index);
    EXPECT_EQ(node.count, expectedCount);
    EXPECT_EQ(node.hash, expectedHash);
  }

  // remove a value, maximal overlap
  modify(0, false);
  EXPECT_EQ(count(), 0);

  // check that it sets everything it should, and nothing it shouldn't
  for (std::uint64_t index = 0; index < 64; ++index) {
    InternalMerkleTreeTest::Node& node = this->node(index);
    EXPECT_EQ(node.count, 0);
    EXPECT_EQ(node.hash, 0);
  }
}

TEST_F(InternalMerkleTreeTest, test_grow) {
  std::pair<std::uint64_t, std::uint64_t> range = this->range();
  EXPECT_EQ(range.first, 0);
  EXPECT_EQ(range.second, 64);

  // fill the tree, but not enough that it grows
  for (std::uint64_t i = 0; i < 64; ++i) {
    insert(i);
  }
  range = this->range();
  EXPECT_EQ(range.first, 0);
  EXPECT_EQ(range.second, 64);

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
      Node& node = this->node(this->index(static_cast<std::uint64_t>(i)));
      EXPECT_EQ(node.count, 1);
      EXPECT_EQ(node.hash, hash2[i]);
    }
    {
      Node const& node = this->meta().summary;
      EXPECT_EQ(node.count, 64);
      EXPECT_EQ(node.hash, hash0);
    }
  }

  // insert some more and cause it to grow
  for (std::uint64_t i = 64; i < 128; ++i) {
    insert(i);
  }
  range = this->range();
  EXPECT_EQ(range.first, 0);
  EXPECT_EQ(range.second, 128);

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
      Node const& node = this->node(i);
      EXPECT_EQ(node.count, 2);
      EXPECT_EQ(node.hash, hash2[i]);
    }
    {
      Node const& node = this->meta().summary;
      EXPECT_EQ(node.count, 128);
      EXPECT_EQ(node.hash, hash0);
    }
  }
}

TEST_F(InternalMerkleTreeTest, test_partition) {
  EXPECT_TRUE(::partitionAsExpected(*this, 0, {{0, 64}}));

  for (std::uint64_t i = 0; i < 32; ++i) {
    this->insert(2 * i);
  }

  EXPECT_TRUE(::partitionAsExpected(*this, 0, {{0, 64}}));
  EXPECT_TRUE(::partitionAsExpected(*this, 1, {{0, 64}}));
  EXPECT_TRUE(::partitionAsExpected(*this, 2, {{0, 30}, {31, 63}}));
  EXPECT_TRUE(::partitionAsExpected(*this, 3, {{0, 18}, {19, 40}, {41, 63}}));
  EXPECT_TRUE(::partitionAsExpected(*this, 4, {{0, 14}, {15, 30}, {31, 46}, {47, 63}}));
  ///
  EXPECT_TRUE(::partitionAsExpected(
      *this, 42,
      {{0, 0},   {1, 2},   {3, 4},   {5, 6},   {7, 8},   {9, 10},  {11, 12},
       {13, 14}, {15, 16}, {17, 18}, {19, 20}, {21, 22}, {23, 24}, {25, 26},
       {27, 28}, {29, 30}, {31, 32}, {33, 34}, {35, 36}, {37, 38}, {39, 40},
       {41, 42}, {43, 44}, {45, 46}, {47, 48}, {49, 50}, {51, 52}, {53, 54},
       {55, 56}, {57, 58}, {59, 60}, {61, 62}}));

  // now let's make the distribution more uneven and see how things go
  this->growRight(511);

  EXPECT_TRUE(::partitionAsExpected(*this, 3, {{0, 23}, {24, 47}, {48, 511}}));
  EXPECT_TRUE(::partitionAsExpected(*this, 4, {{0, 15}, {16, 31}, {32, 47}, {48, 511}}));

  // lump it all in one cell
  this->growRight(4095);

  EXPECT_TRUE(::partitionAsExpected(*this, 4, {{0, 63}}));
}

TEST(MerkleTreeTest, test_allocation_size) {
  {
    ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t(2, 0, 64);
    EXPECT_EQ(t.allocationSize(2), t.MetaSize + 64 * t.NodeSize);
  }
  
  {
    ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t(3, 0, 512);
    EXPECT_EQ(t.allocationSize(3), t.MetaSize + 512 * t.NodeSize);
  }
  
  {
    ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t(4, 0, 4096);
    EXPECT_EQ(t.allocationSize(4), t.MetaSize + 4096 * t.NodeSize);
  }
  
  {
    ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t(5, 0, 32768);
    EXPECT_EQ(t.allocationSize(5), t.MetaSize + 32768 * t.NodeSize);
  }
 
  {
    ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t(6, 0, 1ULL << 18);
    EXPECT_EQ(t.allocationSize(6), t.MetaSize + 262144 * t.NodeSize);
  }
}

TEST(MerkleTreeTest, test_number_of_shards) {
  {
    ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(2, 0, 64);
    EXPECT_EQ(1, t1.numberOfShards());
  }
 
  {
    ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(3, 0, 512);
    EXPECT_EQ(1, t1.numberOfShards());
  }
  
  {
    ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(4, 0, 4096);
    EXPECT_EQ(1, t1.numberOfShards());
  }
  
  {
    ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(5, 0, 32768);
    EXPECT_EQ(8, t1.numberOfShards());
  }
 
  {
    ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t2(6, 0, 1ULL << 18);
    EXPECT_EQ(64, t2.numberOfShards());
  }
}

TEST(MerkleTreeTest, test_stats) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t(3, 0, 512);
 
  // tree empty
  EXPECT_EQ(3, t.depth());
  EXPECT_EQ(0, t.count());
  EXPECT_EQ(0, t.rootValue());
  EXPECT_EQ(8256, t.byteSize());
  EXPECT_EQ(0, t.memoryUsage());

  for (std::uint64_t i = 0; i < 100'000; ++i) {
    t.insert((8 * i) + 1);
  }
  
  // populated with some values
  EXPECT_EQ(3, t.depth());
  EXPECT_EQ(100000, t.count());
  EXPECT_EQ(1167003112264018688ULL, t.rootValue());
  EXPECT_EQ(8256, t.byteSize());
  EXPECT_EQ(65536, t.memoryUsage());

  t.clear();
  EXPECT_EQ(3, t.depth());
  EXPECT_EQ(0, t.count());
  EXPECT_EQ(0, t.rootValue());
  EXPECT_EQ(8256, t.byteSize());
  EXPECT_EQ(0, t.memoryUsage());
}

TEST(MerkleTreeTest, test_diff_equal) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(2, 0, 64);
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t2(2, 0, 64);

  std::vector<std::pair<std::uint64_t, std::uint64_t>> expected;  // empty
  EXPECT_TRUE(::diffAsExpected(t1, t2, expected));

  std::vector<std::uint64_t> order = ::permutation(64);
  for (std::uint64_t i : order) {
    t1.insert(i);
    t2.insert(i);
    EXPECT_TRUE(::diffAsExpected(t1, t2, expected));
  }

  order = ::permutation(64);
  for (std::uint64_t i : order) {
    t1.remove(i);
    t2.remove(i);
    EXPECT_TRUE(::diffAsExpected(t1, t2, expected));
  }
}

TEST(MerkleTreeTest, test_diff_one_empty) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(2, 0, 64);
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t2(2, 0, 64);

  EXPECT_EQ(2, t1.depth());
  EXPECT_EQ(0, t1.count());
  EXPECT_EQ(0, t1.rootValue());
  EXPECT_EQ(1088, t1.byteSize());
  EXPECT_EQ(0, t1.memoryUsage());

  EXPECT_EQ(2, t1.depth());
  EXPECT_EQ(0, t2.count());
  EXPECT_EQ(0, t2.rootValue());
  EXPECT_EQ(1088, t2.byteSize());
  EXPECT_EQ(0, t2.memoryUsage());

  std::vector<std::pair<std::uint64_t, std::uint64_t>> expected;
  EXPECT_TRUE(::diffAsExpected(t1, t2, expected));

  for (std::uint64_t i = 0; i < 8; ++i) {
    t1.insert(8 * i);
    expected.emplace_back(std::make_pair(8 * i, 8 * i));
    EXPECT_TRUE(::diffAsExpected(t1, t2, expected));
  }
  
  EXPECT_EQ(2, t1.depth());
  EXPECT_EQ(8, t1.count());
  EXPECT_EQ(119171121494394880ULL, t1.rootValue());
  EXPECT_EQ(1088, t1.byteSize());
  EXPECT_EQ(65536, t1.memoryUsage());

  expected.clear();
  for (std::uint64_t i = 0; i < 8; ++i) {
    t1.insert((8 * i) + 1);
    expected.emplace_back(std::make_pair(8 * i, (8 * i) + 1));
  }
  EXPECT_TRUE(::diffAsExpected(t1, t2, expected));
  
  EXPECT_EQ(2, t1.depth());
  EXPECT_EQ(16, t1.count());
  EXPECT_EQ(150767561592393728ULL, t1.rootValue());
  EXPECT_EQ(1088, t1.byteSize());
  EXPECT_EQ(65536, t1.memoryUsage());

  expected.clear();
  for (std::uint64_t i = 0; i < 8; ++i) {
    t1.insert((8 * i) + 2);
    t1.insert((8 * i) + 3);
    expected.emplace_back(std::make_pair(8 * i, (8 * i) + 3));
  }
  EXPECT_TRUE(::diffAsExpected(t1, t2, expected));
  
  EXPECT_EQ(2, t1.depth());
  EXPECT_EQ(32, t1.count());
  EXPECT_EQ(13554546285411683328ULL, t1.rootValue());
  EXPECT_EQ(1088, t1.byteSize());
  EXPECT_EQ(65536, t1.memoryUsage());

  expected.clear();
  for (std::uint64_t i = 0; i < 8; ++i) {
    t1.insert((8 * i) + 4);
    t1.insert((8 * i) + 5);
    t1.insert((8 * i) + 6);
    t1.insert((8 * i) + 7);
  }
  expected.emplace_back(std::make_pair(0, 63));
  EXPECT_TRUE(::diffAsExpected(t1, t2, expected));
}

TEST(MerkleTreeTest, test_diff_misc) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(2, 0, 64);
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t2(2, 0, 64);
  
  EXPECT_EQ(2, t1.depth());
  EXPECT_EQ(0, t1.count());
  EXPECT_EQ(0, t1.rootValue());
  EXPECT_EQ(1088, t1.byteSize());
  EXPECT_EQ(0, t1.memoryUsage());

  EXPECT_EQ(2, t1.depth());
  EXPECT_EQ(0, t2.count());
  EXPECT_EQ(0, t2.rootValue());
  EXPECT_EQ(1088, t2.byteSize());
  EXPECT_EQ(0, t2.memoryUsage());

  std::vector<std::pair<std::uint64_t, std::uint64_t>> expected;
  EXPECT_TRUE(::diffAsExpected(t1, t2, expected));

  for (std::uint64_t i = 0; i < 32; ++i) {
    t1.insert(2 * i);
    expected.emplace_back(std::make_pair(2 * i, 2 * i));
  }
  EXPECT_TRUE(::diffAsExpected(t1, t2, expected));

  expected.clear();
  for (std::uint64_t i = 0; i < 32; ++i) {
    t2.insert((2 * i) + 1);
  }
  expected.emplace_back(std::make_pair(0, 63));
  EXPECT_TRUE(::diffAsExpected(t1, t2, expected));

  expected.clear();
  for (std::uint64_t i = 0; i < 16; ++i) {
    t1.insert((2 * i) + 1);
    expected.emplace_back(std::make_pair(2 * i, 2 * i));
  }
  expected.emplace_back(std::make_pair(32, 63));
  EXPECT_TRUE(::diffAsExpected(t1, t2, expected));
}

TEST(MerkleTreeTest, test_serializeBinarySnappyFullSmall) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(2, 0, 64);

  for (std::uint64_t i = 0; i < 32; ++i) {
    t1.insert(2 * i);
  }

  std::string t1s;
  t1.serializeBinary(t1s, arangodb::containers::MerkleTreeBase::BinaryFormat::CompressedSnappyFull);
  EXPECT_EQ(520, t1s.size());

  std::unique_ptr<::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>> t2 =
      ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>::fromBuffer(t1s);
  EXPECT_NE(t2, nullptr);
  EXPECT_TRUE(t1.diff(*t2).empty());
  EXPECT_TRUE(t2->diff(t1).empty());
}

TEST(MerkleTreeTest, test_serializeBinarySnappyFullLarge) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(6, 0, 1ULL << 18);
  
  std::vector<std::uint64_t> keys;
  for (std::uint64_t i = 10'000'000; i < 60'000'000; i += 5) {
    keys.emplace_back(i);
    if (keys.size() == 5000) {
      t1.insert(keys);
      keys.clear();
    }
  }
  EXPECT_EQ(10'000'000, t1.count());

  std::string t1s;
  t1.serializeBinary(t1s, arangodb::containers::MerkleTreeBase::BinaryFormat::CompressedSnappyFull);
  EXPECT_EQ(2143871, t1s.size());

  std::unique_ptr<::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>> t2 =
      ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>::fromBuffer(t1s);
  EXPECT_NE(t2, nullptr);
  EXPECT_TRUE(t1.diff(*t2).empty());
  EXPECT_TRUE(t2->diff(t1).empty());
}

TEST(MerkleTreeTest, test_serializeBinarySnappyLazySmall) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(2, 0, 64);

  for (std::uint64_t i = 0; i < 32; ++i) {
    t1.insert(2 * i);
  }

  std::string t1s;
  t1.serializeBinary(t1s, arangodb::containers::MerkleTreeBase::BinaryFormat::CompressedSnappyLazy);
  EXPECT_EQ(548, t1s.size());

  std::unique_ptr<::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>> t2 =
      ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>::fromBuffer(t1s);
  EXPECT_NE(t2, nullptr);
  EXPECT_TRUE(t1.diff(*t2).empty());
  EXPECT_TRUE(t2->diff(t1).empty());
}

TEST(MerkleTreeTest, test_serializeBinarySnappyLazyLarge) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(6, 0, 1ULL << 18);

  std::vector<std::uint64_t> keys;
  for (std::uint64_t i = 10'000'000; i < 60'000'000; i += 5) {
    keys.emplace_back(i);
    if (keys.size() == 5000) {
      t1.insert(keys);
      keys.clear();
    }
  }
  EXPECT_EQ(10'000'000, t1.count());

  std::string t1s;
  t1.serializeBinary(t1s, arangodb::containers::MerkleTreeBase::BinaryFormat::CompressedSnappyLazy);
  EXPECT_EQ(2116630, t1s.size());

  std::unique_ptr<::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>> t2 =
      ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>::fromBuffer(t1s);
  EXPECT_NE(t2, nullptr);
  EXPECT_TRUE(t1.diff(*t2).empty());
  EXPECT_TRUE(t2->diff(t1).empty());
}

TEST(MerkleTreeTest, test_serializeBinaryOnlyPopulatedSmall) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(2, 0, 64);

  for (std::uint64_t i = 0; i < 32; ++i) {
    t1.insert(2 * i);
  }

  std::string t1s;
  t1.serializeBinary(t1s, arangodb::containers::MerkleTreeBase::BinaryFormat::OnlyPopulated);
  EXPECT_EQ(690, t1s.size());

  std::unique_ptr<::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>> t2 =
      ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>::fromBuffer(t1s);
  EXPECT_NE(t2, nullptr);
  EXPECT_TRUE(t1.diff(*t2).empty());
  EXPECT_TRUE(t2->diff(t1).empty());
}

TEST(MerkleTreeTest, test_serializeBinaryOnlyPopulatedLarge) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(6, 0, 1ULL << 18);
  
  std::vector<std::uint64_t> keys;
  for (std::uint64_t i = 10'000'000; i < 60'000'000; i += 5) {
    keys.emplace_back(i);
    if (keys.size() == 5000) {
      t1.insert(keys);
      keys.clear();
    }
  }
  EXPECT_EQ(10'000'000, t1.count());

  std::string t1s;
  t1.serializeBinary(t1s, arangodb::containers::MerkleTreeBase::BinaryFormat::OnlyPopulated);
  EXPECT_EQ(3906310, t1s.size());

  std::unique_ptr<::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>> t2 =
      ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>::fromBuffer(t1s);
  EXPECT_NE(t2, nullptr);
  EXPECT_TRUE(t1.diff(*t2).empty());
  EXPECT_TRUE(t2->diff(t1).empty());
}

TEST(MerkleTreeTest, test_serializeBinaryUncompressedSmall) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(2, 0, 1ULL << 18);

  for (std::uint64_t i = 0; i < 32; ++i) {
    t1.insert(2 * i);
  }

  std::string t1s;
  t1.serializeBinary(t1s, arangodb::containers::MerkleTreeBase::BinaryFormat::Uncompressed);
  EXPECT_EQ(1090, t1s.size());

  std::unique_ptr<::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>> t2 =
      ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>::fromBuffer(t1s);
  EXPECT_NE(t2, nullptr);
  EXPECT_TRUE(t1.diff(*t2).empty());
  EXPECT_TRUE(t2->diff(t1).empty());
}

TEST(MerkleTreeTest, test_serializeBinaryUncompressedLarge) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(6, 0, 1ULL << 18);
  
  std::vector<std::uint64_t> keys;
  for (std::uint64_t i = 10'000'000; i < 60'000'000; i += 5) {
    keys.emplace_back(i);
    if (keys.size() == 5000) {
      t1.insert(keys);
      keys.clear();
    }
  }
  EXPECT_EQ(10'000'000, t1.count());

  std::string t1s;
  t1.serializeBinary(t1s, arangodb::containers::MerkleTreeBase::BinaryFormat::Uncompressed);
  EXPECT_EQ(4194370, t1s.size());

  std::unique_ptr<::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>> t2 =
      ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>::fromBuffer(t1s);
  EXPECT_NE(t2, nullptr);
  EXPECT_TRUE(t1.diff(*t2).empty());
  EXPECT_TRUE(t2->diff(t1).empty());
}

TEST(MerkleTreeTest, test_serializePortableSmall) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(2, 0, 64);

  for (std::uint64_t i = 0; i < 32; ++i) {
    t1.insert(2 * i);
  }

  ::arangodb::velocypack::Builder t1s;
  t1.serialize(t1s, false);

  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeVersion).isNumber());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeMaxDepth).isNumber());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeRangeMax).isString());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeRangeMin).isString());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeInitialRangeMin).isString());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeCount).isNumber());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeHash).isNumber());
  
  EXPECT_EQ(2, t1s.slice().get(StaticStrings::RevisionTreeMaxDepth).getNumber<int>());
  EXPECT_EQ(32, t1s.slice().get(StaticStrings::RevisionTreeCount).getNumber<int>());

  std::unique_ptr<::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>> t2 =
      ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>::deserialize(
          t1s.slice());
  EXPECT_NE(t2, nullptr);

  EXPECT_EQ(2, t2->depth());
  EXPECT_EQ(32, t2->count());
  EXPECT_EQ(t1.range(), t2->range());
  EXPECT_EQ(t1.rootValue(), t2->rootValue());

  EXPECT_TRUE(t1.diff(*t2).empty());
  EXPECT_TRUE(t2->diff(t1).empty());
}

TEST(MerkleTreeTest, test_serializePortableLarge) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(6, 0, 1ULL << 18);

  std::vector<std::uint64_t> keys;
  for (std::uint64_t i = 10'000'000; i < 60'000'000; i += 5) {
    keys.emplace_back(i);
    if (keys.size() == 5000) {
      t1.insert(keys);
      keys.clear();
    }
  }
  EXPECT_EQ(10'000'000, t1.count());

  ::arangodb::velocypack::Builder t1s;
  t1.serialize(t1s, false);

  EXPECT_GT(t1s.slice().byteSize(), 7'000'000);
  EXPECT_LT(t1s.slice().byteSize(), 8'000'000);
  
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeVersion).isNumber());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeMaxDepth).isNumber());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeRangeMax).isString());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeRangeMin).isString());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeInitialRangeMin).isString());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeCount).isNumber());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeHash).isNumber());
  
  EXPECT_EQ(6, t1s.slice().get(StaticStrings::RevisionTreeMaxDepth).getNumber<int>());
  EXPECT_EQ(10'000'000, t1s.slice().get(StaticStrings::RevisionTreeCount).getNumber<int>());
  EXPECT_EQ(t1.nodeCountAtDepth(6), t1s.slice().get(StaticStrings::RevisionTreeNodes).length());
  
  for (auto it : arangodb::velocypack::ArrayIterator(t1s.slice().get(StaticStrings::RevisionTreeNodes))) {
    EXPECT_TRUE(it.isObject());
    EXPECT_TRUE(it.hasKey(StaticStrings::RevisionTreeHash));
    EXPECT_TRUE(it.hasKey(StaticStrings::RevisionTreeCount));
  }

  std::unique_ptr<::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>> t2 =
      ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>::deserialize(
          t1s.slice());
  EXPECT_NE(t2, nullptr);
  
  EXPECT_EQ(6, t2->depth());
  EXPECT_EQ(10'000'000, t2->count());
  EXPECT_EQ(t1.range(), t2->range());
  EXPECT_EQ(t1.rootValue(), t2->rootValue());

  EXPECT_TRUE(t1.diff(*t2).empty());
  EXPECT_TRUE(t2->diff(t1).empty());
}

TEST(MerkleTreeTest, test_serializePortableLargeOnlyPopulated) {
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(6, 0, 1ULL << 18);

  std::vector<std::uint64_t> keys;
  for (std::uint64_t i = 10'000'000; i < 60'000'000; i += 5) {
    keys.emplace_back(i);
    if (keys.size() == 5000) {
      t1.insert(keys);
      keys.clear();
    }
  }
  EXPECT_EQ(10'000'000, t1.count());

  ::arangodb::velocypack::Builder t1s;
  t1.serialize(t1s, true);
 
  EXPECT_GT(t1s.slice().byteSize(), 6'000'000);
  EXPECT_LT(t1s.slice().byteSize(), 7'000'000);
  
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeVersion).isNumber());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeMaxDepth).isNumber());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeRangeMax).isString());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeRangeMin).isString());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeInitialRangeMin).isString());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeCount).isNumber());
  EXPECT_TRUE(t1s.slice().get(StaticStrings::RevisionTreeHash).isNumber());
  
  EXPECT_EQ(6, t1s.slice().get(StaticStrings::RevisionTreeMaxDepth).getNumber<int>());
  EXPECT_EQ(10'000'000, t1s.slice().get(StaticStrings::RevisionTreeCount).getNumber<int>());
  EXPECT_GE(t1.nodeCountAtDepth(6), t1s.slice().get(StaticStrings::RevisionTreeNodes).length());
 
  size_t populated = 0;
  size_t empty = 0;
  for (auto it : arangodb::velocypack::ArrayIterator(t1s.slice().get(StaticStrings::RevisionTreeNodes))) {
    EXPECT_TRUE(it.isObject());

    if (it.hasKey(StaticStrings::RevisionTreeHash)) {
      ++populated;
    } else {
      ++empty;
    }
  }

  EXPECT_GT(empty, 0);
  EXPECT_GT(populated, 0);

  EXPECT_GE(t1.nodeCountAtDepth(6), empty + populated);

  std::unique_ptr<::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>> t2 =
      ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>::deserialize(
          t1s.slice());
  EXPECT_NE(t2, nullptr);
  
  EXPECT_EQ(6, t2->depth());
  EXPECT_EQ(10'000'000, t2->count());
  EXPECT_EQ(t1.range(), t2->range());
  EXPECT_EQ(t1.rootValue(), t2->rootValue());

  EXPECT_TRUE(t1.diff(*t2).empty());
  EXPECT_TRUE(t2->diff(t1).empty());
}

TEST(MerkleTreeTest, test_tree_based_on_2021_hlcs) {
  uint64_t rangeMin = uint64_t(1609459200000000000ULL);
  uint64_t rangeMax = uint64_t(1609459200016777216ULL);

  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> tree(6, rangeMin);
  
  EXPECT_EQ(6, tree.depth());
  EXPECT_EQ(0, tree.count());
  EXPECT_EQ(0, tree.rootValue());
  
  auto [left, right] = tree.range();
  EXPECT_EQ(rangeMin, left);
  EXPECT_EQ(rangeMax, right);

  for (std::uint64_t i = rangeMin; i < rangeMin + 10000; ++i) {
    tree.insert(i);
  }
  
  EXPECT_EQ(6, tree.depth());
  EXPECT_EQ(10000, tree.count());
  EXPECT_EQ(4298149919775466880ULL, tree.rootValue());
  EXPECT_EQ(65536, tree.memoryUsage());
  std::tie(left, right) = tree.range();
  EXPECT_EQ(rangeMin, left);
  EXPECT_EQ(rangeMax, right);
  
  for (std::uint64_t i = rangeMin; i < rangeMin + 10000; ++i) {
    tree.remove(i);
  }
  
  EXPECT_EQ(6, tree.depth());
  EXPECT_EQ(0, tree.count());
  EXPECT_EQ(0, tree.rootValue());
  EXPECT_EQ(65536, tree.memoryUsage());
  std::tie(left, right) = tree.range();
  EXPECT_EQ(rangeMin, left);
  EXPECT_EQ(rangeMax, right);

  // increase the pace
  constexpr std::uint64_t n = 10'000'000;
  constexpr std::uint64_t batchSize = 10'000;

  std::vector<std::uint64_t> revisions;
  revisions.reserve(batchSize);
  for (std::uint64_t i = rangeMin; i < rangeMin + n; i += batchSize) {
    revisions.clear();
    for (std::uint64_t j = 0; j < batchSize; ++j) {
      revisions.push_back(i + j);
    }
    tree.insert(revisions);
  }
  
  EXPECT_EQ(6, tree.depth());
  EXPECT_EQ(10'000'000, tree.count());
  EXPECT_EQ(9116977756596679424ULL, tree.rootValue());
  EXPECT_EQ(2555904, tree.memoryUsage());
  std::tie(left, right) = tree.range();
  EXPECT_EQ(rangeMin, left);
  EXPECT_EQ(rangeMax, right);
  
  for (std::uint64_t i = rangeMin; i < rangeMin + n; i += batchSize) {
    revisions.clear();
    for (std::uint64_t j = 0; j < batchSize; ++j) {
      revisions.push_back(i + j);
    }
    tree.remove(revisions);
  }
  
  EXPECT_EQ(6, tree.depth());
  EXPECT_EQ(0, tree.count());
  EXPECT_EQ(0, tree.rootValue());
  EXPECT_EQ(2555904, tree.memoryUsage());
  std::tie(left, right) = tree.range();
  EXPECT_EQ(rangeMin, left);
  EXPECT_EQ(rangeMax, right);
}

TEST(MerkleTreeTest, test_large_steps) {
  uint64_t rangeMin = uint64_t(1609459200000000000ULL);
  uint64_t rangeMax = uint64_t(1609459200016777216ULL);
  
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> tree(6, rangeMin);
  
  EXPECT_EQ(6, tree.depth());
  EXPECT_EQ(0, tree.count());
  EXPECT_EQ(0, tree.rootValue());
  EXPECT_EQ(0, tree.memoryUsage());
  
  auto [left, right] = tree.range();
  EXPECT_EQ(rangeMin, left);
  EXPECT_EQ(rangeMax, right);

  constexpr std::uint64_t n = 100'000'000'000;
  constexpr std::uint64_t step = 10'000;

  for (std::uint64_t i = rangeMin; i < rangeMin + n; i += step) {
    tree.insert(i);
  }
  
  EXPECT_EQ(6, tree.depth());
  EXPECT_EQ(10'000'000, tree.count());
  EXPECT_EQ(7036974261486883938ULL, tree.rootValue());
  EXPECT_EQ(4194304, tree.memoryUsage());
  std::tie(left, right) = tree.range();
  rangeMax = 1609459337438953472ULL;
  EXPECT_EQ(rangeMin, left);
  EXPECT_EQ(rangeMax, right);
  
  for (std::uint64_t i = rangeMin; i < rangeMin + n; i += step) {
    tree.remove(i);
  }
  
  EXPECT_EQ(6, tree.depth());
  EXPECT_EQ(0, tree.count());
  EXPECT_EQ(0, tree.rootValue());
  EXPECT_EQ(4194304, tree.memoryUsage());
  std::tie(left, right) = tree.range();
  EXPECT_EQ(rangeMin, left);
  EXPECT_EQ(rangeMax, right);
}

TEST(MerkleTreeTest, test_clear) {
  uint64_t rangeMin = uint64_t(1609459200000000000ULL);
  uint64_t rangeMax = uint64_t(1609459200016777216ULL);
  
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> tree(6, rangeMin);
  
  EXPECT_EQ(6, tree.depth());
  EXPECT_EQ(0, tree.count());
  EXPECT_EQ(0, tree.rootValue());
  EXPECT_EQ(0, tree.memoryUsage());
  
  auto [left, right] = tree.range();
  EXPECT_EQ(rangeMin, left);
  EXPECT_EQ(rangeMax, right);

  constexpr std::uint64_t n = 100'000'000'000;
  constexpr std::uint64_t step = 50'000;

  for (std::uint64_t i = rangeMin; i < rangeMin + n; i += step) {
    tree.insert(i);
  }
  
  EXPECT_EQ(6, tree.depth());
  EXPECT_EQ(2'000'000, tree.count());
  EXPECT_EQ(12929699950823344265ULL, tree.rootValue());
  EXPECT_EQ(4194304, tree.memoryUsage());
  std::tie(left, right) = tree.range();
  rangeMax = 1609459337438953472ULL;
  EXPECT_EQ(rangeMin, left);
  EXPECT_EQ(rangeMax, right);
 
  tree.clear();
  
  EXPECT_EQ(6, tree.depth());
  EXPECT_EQ(0, tree.count());
  EXPECT_EQ(0, tree.rootValue());
  EXPECT_EQ(0, tree.memoryUsage());
  std::tie(left, right) = tree.range();
  EXPECT_EQ(rangeMin, left);
  EXPECT_EQ(rangeMax, right);
}

TEST(MerkleTreeTest, test_check_consistency) {
  uint64_t rangeMin = uint64_t(1609459200000000000ULL);
  uint64_t rangeMax = uint64_t(1609459200016777216ULL);
  
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> tree(6, rangeMin);
  
  EXPECT_EQ(6, tree.depth());
  EXPECT_EQ(0, tree.count());
  EXPECT_EQ(0, tree.rootValue());
  
  // must not throw
  tree.checkConsistency();
  
  auto [left, right] = tree.range();
  EXPECT_EQ(rangeMin, left);
  EXPECT_EQ(rangeMax, right);

  constexpr std::uint64_t n = 100'000'000'000;
  constexpr std::uint64_t step = 10'000;

  for (std::uint64_t i = rangeMin; i < rangeMin + n; i += step) {
    tree.insert(i);
  }

  // must not throw
  tree.checkConsistency();
 
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  tree.corrupt(42, 23);

  // must throw
  try {
    tree.checkConsistency();
    EXPECT_TRUE(false);
  } catch (std::invalid_argument const&) {
  }
#endif
}

class MerkleTreeGrowTests
    : public ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>,
      public ::testing::Test {
 public:
  MerkleTreeGrowTests()
      : MerkleTree<::arangodb::containers::FnvHashProvider, 3>(
          6, uint64_t(1609459200000000000ULL)) {}
 public:
  uint64_t rangeMin = range().first;
  uint64_t initWidth = 1ULL << 24;
  uint64_t bucketWidth = 1ULL << 6;
  uint64_t rangeMax = range().second;
};

TEST_F(MerkleTreeGrowTests, test_grow_left_simple) {
  EXPECT_EQ(rangeMin + initWidth, rangeMax);

  arangodb::containers::FnvHashProvider hasher;

  EXPECT_EQ(6, depth());
  EXPECT_EQ(0, count());
  EXPECT_EQ(0, rootValue());
  
  insert(rangeMin);
  insert(rangeMin + bucketWidth);
  insert(rangeMin + 47 * bucketWidth);

  EXPECT_EQ(6, depth());
  EXPECT_EQ(3, count());
  EXPECT_EQ(hasher.hash(rangeMin) ^ hasher.hash(rangeMin + bucketWidth) ^
            hasher.hash(rangeMin + 47 * bucketWidth), rootValue());

  // Now grow to the left:
  insert(rangeMin - 1);

  // Must not throw:
  checkConsistency();

  EXPECT_EQ(6, depth());
  EXPECT_EQ(4, count());
  EXPECT_EQ(hasher.hash(rangeMin) ^ hasher.hash(rangeMin + bucketWidth) ^
            hasher.hash(rangeMin + 47 * bucketWidth) ^
            hasher.hash(rangeMin - 1), rootValue());
  EXPECT_EQ(rangeMin - initWidth, range().first);
  EXPECT_EQ(rangeMax, range().second);

  // Now check the bottommost buckets:
  Node& n = node(index(rangeMin));
  EXPECT_EQ(2, n.count);
  EXPECT_EQ(hasher.hash(rangeMin) ^ hasher.hash(rangeMin + bucketWidth),
            n.hash);
  Node& n2 = node(index(rangeMin - 1));
  EXPECT_EQ(1, n2.count);
  EXPECT_EQ(hasher.hash(rangeMin - 1), n2.hash);
  Node& n3 = node(index(rangeMin + 47 * bucketWidth));
  EXPECT_EQ(1, n3.count);
  EXPECT_EQ(hasher.hash(rangeMin + 47 * bucketWidth), n3.hash);
}

TEST_F(MerkleTreeGrowTests, test_grow_left_with_shift) {
  EXPECT_EQ(rangeMin + initWidth, rangeMax);

  arangodb::containers::FnvHashProvider hasher;

  EXPECT_EQ(6, depth());
  EXPECT_EQ(0, count());
  EXPECT_EQ(0, rootValue());
  
  // We grow once to the left, so that initialRangeMin - rangeMin is 2^24.
  // Then we grow to the right until the width is 2^(18+24) = 2^42.
  // The next grow operation after that needs to shift, since then
  // the size of a bucket becomes 2^24 and with the next grow operation
  // the difference initialRangeMin - rangeMin would no longer be divisble
  // by the bucket size.
  growLeft(rangeMin - 1);
  for (int i = 0; i < 17; ++i) {
    growRight(rangeMax);
    rangeMax = range().second;
  }

  EXPECT_EQ(6, depth());
  EXPECT_EQ(0, count());
  EXPECT_EQ(0, rootValue());
  EXPECT_EQ(rangeMin - initWidth, range().first);
  rangeMin = range().first;
  rangeMax = range().second;
  EXPECT_EQ(rangeMin + (1ULL << 42), rangeMax);
  bucketWidth = (range().second - range().first) >> 18;
  EXPECT_EQ(1ULL << 24, bucketWidth);

  insert(rangeMin);
  insert(rangeMin + bucketWidth);
  insert(rangeMin + 47 * bucketWidth);

  EXPECT_EQ(6, depth());
  EXPECT_EQ(3, count());
  EXPECT_EQ(rangeMin, range().first);
  EXPECT_EQ(rangeMax, range().second);
  EXPECT_EQ(hasher.hash(rangeMin) ^ hasher.hash(rangeMin + bucketWidth) ^
            hasher.hash(rangeMin + 47 * bucketWidth), rootValue());

  // Now grow to the left:
  insert(rangeMin - 1);

  // Must not throw:
  checkConsistency();

  EXPECT_EQ(6, depth());
  EXPECT_EQ(4, count());
  EXPECT_EQ(hasher.hash(rangeMin) ^ hasher.hash(rangeMin + bucketWidth) ^
            hasher.hash(rangeMin + 47 * bucketWidth) ^
            hasher.hash(rangeMin - 1), rootValue());
  EXPECT_EQ(rangeMin - (rangeMax - rangeMin) + bucketWidth, range().first);
  EXPECT_EQ(rangeMax + bucketWidth, range().second);

  // Now check the bottommost buckets:
  Node& n = node(index(rangeMin));
  EXPECT_EQ(2, n.count);
  EXPECT_EQ(hasher.hash(rangeMin) ^ hasher.hash(rangeMin - 1), n.hash);
  Node& n2 = node(index(rangeMin + bucketWidth));
  EXPECT_EQ(1, n2.count);
  EXPECT_EQ(hasher.hash(rangeMin + bucketWidth), n2.hash);
  Node& n3 = node(index(rangeMin + 47 * bucketWidth));
  EXPECT_EQ(1, n3.count);
  EXPECT_EQ(hasher.hash(rangeMin + 47 * bucketWidth), n3.hash);
}

TEST_F(MerkleTreeGrowTests, test_grow_right_simple) {
  EXPECT_EQ(rangeMin + initWidth, rangeMax);

  arangodb::containers::FnvHashProvider hasher;

  EXPECT_EQ(6, depth());
  EXPECT_EQ(0, count());
  EXPECT_EQ(0, rootValue());
  
  insert(rangeMin);
  insert(rangeMin + bucketWidth);
  insert(rangeMin + 47 * bucketWidth);

  EXPECT_EQ(6, depth());
  EXPECT_EQ(3, count());
  EXPECT_EQ(hasher.hash(rangeMin) ^ hasher.hash(rangeMin + bucketWidth) ^
            hasher.hash(rangeMin + 47 * bucketWidth), rootValue());

  // Now grow to the right:
  insert(rangeMax + 42);

  // Must not throw:
  checkConsistency();

  EXPECT_EQ(6, depth());
  EXPECT_EQ(4, count());
  EXPECT_EQ(hasher.hash(rangeMin) ^ hasher.hash(rangeMin + bucketWidth) ^
            hasher.hash(rangeMin + 47 * bucketWidth) ^
            hasher.hash(rangeMax + 42), rootValue());
  EXPECT_EQ(rangeMin, range().first);
  EXPECT_EQ(rangeMax + initWidth, range().second);

  // Now check the bottommost buckets:
  Node& n = node(index(rangeMin));
  EXPECT_EQ(2, n.count);
  EXPECT_EQ(hasher.hash(rangeMin) ^ hasher.hash(rangeMin + bucketWidth),
            n.hash);
  Node& n2 = node(index(rangeMax + 42));
  EXPECT_EQ(1, n2.count);
  EXPECT_EQ(hasher.hash(rangeMax + 42), n2.hash);
  Node& n3 = node(index(rangeMin + 47 * bucketWidth));
  EXPECT_EQ(1, n3.count);
  EXPECT_EQ(hasher.hash(rangeMin + 47 * bucketWidth), n3.hash);
}

TEST_F(MerkleTreeGrowTests, test_grow_right_with_shift) {
  EXPECT_EQ(rangeMin + initWidth, rangeMax);

  arangodb::containers::FnvHashProvider hasher;

  EXPECT_EQ(6, depth());
  EXPECT_EQ(0, count());
  EXPECT_EQ(0, rootValue());
  
  // We grow once to the left, so that initialRangeMin - rangeMin is 2^24.
  // Then we grow to the right until the width is 2^(18+24) = 2^42.
  // The next grow operation after that needs to shift, since then
  // the size of a bucket becomes 2^24 and with the next grow operation
  // the difference initialRangeMin - rangeMin would no longer be divisble
  // by the bucket size.
  growLeft(rangeMin - 1);
  for (int i = 0; i < 17; ++i) {
    growRight(rangeMax);
    rangeMax = range().second;
  }

  EXPECT_EQ(6, depth());
  EXPECT_EQ(0, count());
  EXPECT_EQ(0, rootValue());
  EXPECT_EQ(rangeMin - initWidth, range().first);
  rangeMin = range().first;
  rangeMax = range().second;
  EXPECT_EQ(rangeMin + (1ULL << 42), rangeMax);
  bucketWidth = (range().second - range().first) >> 18;
  EXPECT_EQ(1ULL << 24, bucketWidth);

  insert(rangeMin);
  insert(rangeMin + bucketWidth);
  insert(rangeMin + 47 * bucketWidth);

  EXPECT_EQ(6, depth());
  EXPECT_EQ(3, count());
  EXPECT_EQ(rangeMin, range().first);
  EXPECT_EQ(rangeMax, range().second);
  EXPECT_EQ(hasher.hash(rangeMin) ^ hasher.hash(rangeMin + bucketWidth) ^
            hasher.hash(rangeMin + 47 * bucketWidth), rootValue());

  // Now grow to the right:
  insert(rangeMax);

  // Must not throw:
  checkConsistency();

  EXPECT_EQ(6, depth());
  EXPECT_EQ(4, count());
  EXPECT_EQ(hasher.hash(rangeMin) ^ hasher.hash(rangeMin + bucketWidth) ^
            hasher.hash(rangeMin + 47 * bucketWidth) ^
            hasher.hash(rangeMax), rootValue());
  EXPECT_EQ(rangeMin - bucketWidth, range().first);
  EXPECT_EQ(rangeMax + (rangeMax - rangeMin) - bucketWidth, range().second);

  // Now check the bottommost buckets:
  Node& n = node(index(rangeMin));
  EXPECT_EQ(1, n.count);
  EXPECT_EQ(hasher.hash(rangeMin), n.hash);
  Node& n2 = node(index(rangeMin + bucketWidth));
  EXPECT_EQ(1, n2.count);
  EXPECT_EQ(hasher.hash(rangeMin + bucketWidth), n2.hash);
  Node& n3 = node(index(rangeMin + 47 * bucketWidth));
  EXPECT_EQ(1, n3.count);
  EXPECT_EQ(hasher.hash(rangeMin + 47 * bucketWidth), n3.hash);
  Node& n4 = node(index(rangeMax));
  EXPECT_EQ(1, n4.count);
  EXPECT_EQ(hasher.hash(rangeMax), n4.hash);
}

TEST(MerkleTreeTest, test_diff_with_shift_1) {
  constexpr uint64_t M = 1234567;     // some large constant
  constexpr uint64_t W = 1ULL << 20;  // width, 4 values in each bucket
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(6, M, M + W, M + 16);
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t2(6, M + 16, M + W + 16, M + 16);   // four buckets further right

  std::vector<std::pair<std::uint64_t, std::uint64_t>> expected;
  EXPECT_TRUE(::diffAsExpected(t1, t2, expected));

  // Now insert something into t1 left of tree 2 as well as in the overlap:
  t1.insert(M);      // first bucket in t1
  expected.emplace_back(std::pair(M, M+3));
  t1.insert(M + 8);  // third bucket in t1
  expected.emplace_back(std::pair(M+8, M+11));
  t1.insert(M + 16); // fifth bucket in t1, first bucket in t2
  expected.emplace_back(std::pair(M+16, M+19));
  EXPECT_TRUE(::diffAsExpected(t1, t2, expected));
  t1.clear();
  expected.clear();

  // Now insert something into t1 left of tree 2 as well as in the overlap, but expect one contiguous interval:
  t1.insert(M);      // first bucket in t1
  t1.insert(M + 4);  // second bucket in t1
  t1.insert(M + 8);  // third bucket in t1
  t1.insert(M + 12); // fourth bucket in t1
  t1.insert(M + 16); // fifth bucket in t1, first bucket in t2
  expected.emplace_back(std::pair(M, M+19));
  EXPECT_TRUE(::diffAsExpected(t1, t2, expected));
  t1.clear();
  expected.clear();

  // Now insert something into t2 to the right of tree 1 as well as in the overlap:
  t2.insert(M + W - 8);      // second last bucket in t1, 6th last in t2
  expected.emplace_back(std::pair(M + W - 8, M + W - 5));
  t2.insert(M + W);          // not in t1, fourth last bucket in t2
  expected.emplace_back(std::pair(M + W, M + W + 3));
  t2.insert(M + W + 8);      // not in t1, second last bucket in t2
  expected.emplace_back(std::pair(M + W + 8, M + W + 11));
  EXPECT_TRUE(::diffAsExpected(t1, t2, expected));
  t2.clear();
  expected.clear();
  
  // Now insert something into t2 to the right of tree 1 as well as in the overlap, but expect one contiguous interval:
  t2.insert(M + W - 8);      // second last bucket in t1, 6th last in t2
  t2.insert(M + W - 4);      // last bucket in t1, 5th last in t2
  t2.insert(M + W);          // not in t1, fourth last bucket in t2
  t2.insert(M + W + 4);      // not in t1, third last bucket in t2
  t2.insert(M + W + 8);      // not in t1, second last bucket in t2
  expected.emplace_back(std::pair(M + W - 8, M + W + 11));
  EXPECT_TRUE(::diffAsExpected(t1, t2, expected));
  t2.clear();
  expected.clear();

  // And finally some changes in t1 and some in t2:
  t1.insert(M);
  expected.emplace_back(std::pair(M, M + 3));
  t1.insert(M + 16);
  t2.insert(M + 16);
  // Nothing in this bucket, since both have the same!
  t1.insert(M + 21);
  t2.insert(M + 22);
  expected.emplace_back(std::pair(M + 20, M + 23));
  t1.insert(M + W - 8);
  t2.insert(M + W - 5);
  expected.emplace_back(std::pair(M + W - 8, M + W - 5));
  t2.insert(M + W);
  t2.insert(M + W + 5);
  expected.emplace_back(std::pair(M + W, M + W + 7));
  EXPECT_TRUE(::diffAsExpected(t1, t2, expected));
}

TEST(MerkleTreeTest, test_diff_empty_random_data_shifted) {
  constexpr uint64_t M = (1ULL << 32) + 17;     // some large constant
  constexpr uint64_t W = 1ULL << 20;  // initial width, 4 values in each bucket
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(6, M, M + W, M + 16);
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t2(6, M + 16, M + W + 16, M + 16);   // four buckets further right

  // Now produce a large list of random keys, in the covered range and beyond on both sides.
  // We then shuffle the vector and insert into both trees in a different order.
  // This will eventually grow the trees in various ways, but in the end, it should always
  // find no differences whatsoever.

  auto rd {std::random_device{}()};
  auto mt {std::mt19937_64(rd)};
  std::uniform_int_distribution<uint64_t> uni{M - (1ULL << 12), M + (1ULL << 28)};
  std::vector<uint64_t> original;
  for (size_t i = 0; i < 100000; ++i) {
    original.push_back(uni(mt));
  }
  std::vector<uint64_t> shuffled{original};
  std::shuffle(shuffled.begin(), shuffled.end(), mt);

  for (auto x : original) {
    t1.insert(x);
  }
  for (auto x : shuffled) {
    t2.insert(x);
  }

  auto t1c = t1.clone();
  auto t2c = t2.clone();

  std::vector<std::pair<std::uint64_t, std::uint64_t>> expected;
  EXPECT_TRUE(::diffAsExpected(t1, t2, expected));
  EXPECT_TRUE(::diffAsExpected(t2, t1, expected));
}

TEST(MerkleTreeTest, test_clone_compare_clean) {
  constexpr uint64_t M = 1234567;     // some large constant
  constexpr uint64_t W = 1ULL << 20;  // width, 4 values in each bucket
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(6, M, M + W, M + 16);

  // Prepare a tree:
  auto rd {std::random_device{}()};
  auto mt {std::mt19937_64(rd)};
  std::uniform_int_distribution<uint64_t> uni{M, M + (1ULL << 20)};
  std::vector<uint64_t> data;
  for (size_t i = 0; i < 1000; ++i) {
    data.push_back(uni(mt));
  }
  for (auto x : data) {
    t1.insert(x);
  }
  
  // Now clone tree:
  auto t2 = t1.clone();

  // And compare the two:
  std::vector<std::pair<std::uint64_t, std::uint64_t>> expected;
  EXPECT_TRUE(::diffAsExpected(t1, *t2, expected));

  // And compare bitwise:
  auto format = arangodb::containers::MerkleTreeBase::BinaryFormat::Uncompressed;
  std::string s1;
  t1.serializeBinary(s1, format);
  std::string s2;
  t2->serializeBinary(s2, format);
  EXPECT_EQ(s1, s2);
  
  format = arangodb::containers::MerkleTreeBase::BinaryFormat::OnlyPopulated;
  s1.clear();
  t1.serializeBinary(s1, format);
  s2.clear();
  t2->serializeBinary(s2, format);
  EXPECT_EQ(s1, s2);

  format = arangodb::containers::MerkleTreeBase::BinaryFormat::CompressedSnappyFull;
  s1.clear();
  t1.serializeBinary(s1, format);
  s2.clear();
  t2->serializeBinary(s2, format);
  EXPECT_EQ(s1, s2);
  
  format = arangodb::containers::MerkleTreeBase::BinaryFormat::CompressedSnappyLazy;
  s1.clear();
  t1.serializeBinary(s1, format);
  s2.clear();
  t2->serializeBinary(s2, format);
  EXPECT_EQ(s1, s2);

  // Now use operator=
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t3(6, M, M + W, M + 16);

  t3 = std::move(t2);
  // And compare bitwise:
  format = arangodb::containers::MerkleTreeBase::BinaryFormat::Uncompressed;
  s1.clear();
  t1.serializeBinary(s1, format);
  s2.clear();
  t3.serializeBinary(s2, format);
  EXPECT_EQ(s1, s2);

  format = arangodb::containers::MerkleTreeBase::BinaryFormat::OnlyPopulated;
  s1.clear();
  t1.serializeBinary(s1, format);
  s2.clear();
  t3.serializeBinary(s2, format);
  EXPECT_EQ(s1, s2);
  
  format = arangodb::containers::MerkleTreeBase::BinaryFormat::CompressedSnappyFull;
  s1.clear();
  t1.serializeBinary(s1, format);
  s2.clear();
  t3.serializeBinary(s2, format);
  EXPECT_EQ(s1, s2);

  format = arangodb::containers::MerkleTreeBase::BinaryFormat::CompressedSnappyLazy;
  s1.clear();
  t1.serializeBinary(s1, format);
  s2.clear();
  t3.serializeBinary(s2, format);
  EXPECT_EQ(s1, s2);
}

TEST(MerkleTreeTest, test_clone_compare_clean_large) {
  constexpr uint64_t M = 1234567;     // some large constant
  constexpr uint64_t W = 1ULL << 20;  // width, 4 values in each bucket
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(6, M, M + W, M + 16);

  // Prepare a tree:
  auto rd {std::random_device{}()};
  auto mt {std::mt19937_64(rd)};
  std::uniform_int_distribution<uint64_t> uni{M, M + (1ULL << 20)};
  std::vector<uint64_t> data;
  for (size_t i = 0; i < 20000; ++i) {
    data.push_back(uni(mt));
  }
  for (auto x : data) {
    t1.insert(x);
  }
  
  // Now clone tree:
  auto t2 = t1.clone();

  // And compare the two:
  std::vector<std::pair<std::uint64_t, std::uint64_t>> expected;
  EXPECT_TRUE(::diffAsExpected(t1, *t2, expected));

  // And compare bitwise:
  auto format = arangodb::containers::MerkleTreeBase::BinaryFormat::Uncompressed;
  std::string s1;
  t1.serializeBinary(s1, format);
  std::string s2;
  t2->serializeBinary(s2, format);
  EXPECT_EQ(s1, s2);
  
  format = arangodb::containers::MerkleTreeBase::BinaryFormat::OnlyPopulated;
  s1.clear();
  t1.serializeBinary(s1, format);
  s2.clear();
  t2->serializeBinary(s2, format);
  EXPECT_EQ(s1, s2);
  
  format = arangodb::containers::MerkleTreeBase::BinaryFormat::CompressedSnappyFull;
  s1.clear();
  t1.serializeBinary(s1, format);
  s2.clear();
  t2->serializeBinary(s2, format);
  EXPECT_EQ(s1, s2);

  format = arangodb::containers::MerkleTreeBase::BinaryFormat::CompressedSnappyLazy;
  s1.clear();
  t1.serializeBinary(s1, format);
  s2.clear();
  t2->serializeBinary(s2, format);
  EXPECT_EQ(s1, s2);

  // Now use operator=
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t3(6, M, M + W, M + 16);

  t3 = std::move(t2);
  // And compare bitwise:
  format = arangodb::containers::MerkleTreeBase::BinaryFormat::Uncompressed;
  s1.clear();
  t1.serializeBinary(s1, format);
  s2.clear();
  t3.serializeBinary(s2, format);
  EXPECT_EQ(s1, s2);
  
  format = arangodb::containers::MerkleTreeBase::BinaryFormat::OnlyPopulated;
  s1.clear();
  t1.serializeBinary(s1, format);
  s2.clear();
  t3.serializeBinary(s2, format);
  EXPECT_EQ(s1, s2);
  
  format = arangodb::containers::MerkleTreeBase::BinaryFormat::CompressedSnappyFull;
  s1.clear();
  t1.serializeBinary(s1, format);
  s2.clear();
  t3.serializeBinary(s2, format);
  EXPECT_EQ(s1, s2);

  format = arangodb::containers::MerkleTreeBase::BinaryFormat::CompressedSnappyLazy;
  s1.clear();
  t1.serializeBinary(s1, format);
  s2.clear();
  t3.serializeBinary(s2, format);
  EXPECT_EQ(s1, s2);
}

TEST(MerkleTreeTest, test_to_string) {
  constexpr uint64_t M = 1234567;     // some large constant
  constexpr uint64_t W = 1ULL << 20;
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(2, M, M + W, M);

  // Prepare a tree:
  auto rd {std::random_device{}()};
  auto mt {std::mt19937_64(rd)};
  std::uniform_int_distribution<uint64_t> uni{M, M + (1ULL << 20)};
  std::vector<uint64_t> data;
  for (size_t i = 0; i < 100; ++i) {
    data.push_back(uni(mt));
  }
  t1.insert(data);
 
  // the exact size of the response is unclear here, due to the pseudo-random
  // inserts
  std::string s = t1.toString(false);
  EXPECT_LE(800, s.size());
  s = t1.toString(true);
  EXPECT_LE(950, s.size());
}

TEST(MerkleTreeTest, test_diff_one_side_empty_random_data_shifted) {
  constexpr uint64_t M = (1ULL << 32) + 17;     // some large constant
  constexpr uint64_t W = 1ULL << 20;  // initial width, 4 values in each bucket
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t1(6, M, M + W, M + 16);
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t2(6, M + 16, M + W + 16, M + 16);   // four buckets further right

  // Now produce a large list of random keys, in the covered range and beyond on both sides.
  // We then shuffle the vector and insert into both trees in a different order.
  // This will eventually grow the trees in various ways, but in the end, it should always
  // find no differences whatsoever.

  auto rd {std::random_device{}()};
  auto mt {std::mt19937_64(rd)};
  std::uniform_int_distribution<uint64_t> uni{M - (1ULL << 12), M + (1ULL << 28)};
  std::vector<uint64_t> data;
  for (size_t i = 0; i < 100000; ++i) {
    data.push_back(uni(mt));
  }
  std::vector<uint64_t> sorted{data};
  std::sort(sorted.begin(), sorted.end());

  for (auto x : data) {
    t1.insert(x);
  }

  auto t1c = t1.clone();
  auto t2c = t2.clone();
  std::vector<std::pair<std::uint64_t, std::uint64_t>> d1 = t1.diff(t2);
  std::vector<std::pair<std::uint64_t, std::uint64_t>> d2 = t2c->diff(*t1c);

  // Now do a check of the result, first, the two should be the same:
  EXPECT_EQ(d1.size(), d2.size());
  for (size_t i = 0; i < d1.size(); ++i) {
    EXPECT_EQ(d1[i].first, d2[i].first);
    EXPECT_EQ(d1[i].second, d2[i].second);
  }

  // Now check that each of the intervals contains at least one entry
  // in the sorted data list:
  size_t pos = 0;
  size_t posi = 0;  // position in intervals
  while (pos < sorted.size() && posi < d1.size()) {
    // Next key in the sorted list must be in next interval:
    EXPECT_LE(d1[posi].first, sorted[pos]);
    EXPECT_LE(sorted[pos], d1[posi].second);
    // Now skip all points in the sorted list in that interval:
    while (pos < sorted.size() &&
           d1[posi].first <= sorted[pos] && sorted[pos] <= d1[posi].second) {
      ++pos;
    }
    // Now skip this interval:
    ++posi;
  }
  EXPECT_EQ(pos, sorted.size());  // All points should be consumed
  EXPECT_EQ(posi, d1.size());     // All intervals should be consumed
}

TEST(MerkleTreeTest, overflow_underflow) {
  constexpr uint64_t M = (std::numeric_limits<uint64_t>::max() >> 1) + 1;
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t(6, 0, M, 0);
  try {
    t.insert(M);   // must throw because of overflow
    EXPECT_TRUE(false);
  } catch (std::out_of_range const& exc) {
    std::string msg{exc.what()};
    EXPECT_TRUE(msg.find("overflow") != std::string::npos);
  }
  ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3> t2(6, 1, M + 1, 1);
  try {
    t2.insert(0);   // must throw because of underflow
    EXPECT_TRUE(false);
  } catch (std::out_of_range const& exc) {
    std::string msg{exc.what()};
    EXPECT_TRUE(msg.find("underflow") != std::string::npos);
  }
};

class MerkleTreeSpecialGrowTests
    : public ::arangodb::containers::MerkleTree<::arangodb::containers::FnvHashProvider, 3>,
      public ::testing::Test {
 public:
  MerkleTreeSpecialGrowTests()
      : MerkleTree<::arangodb::containers::FnvHashProvider, 3>(
          6, 0 /* rangeMin */, 1ULL << 24 /* rangeMax */, 0 /* initial */) {}
 public:
  uint64_t rangeMin = range().first;
  uint64_t initWidth = 1ULL << 24;
  uint64_t bucketWidth = 1ULL << 6;
  uint64_t rangeMax = range().second;
};

TEST_F(MerkleTreeSpecialGrowTests, test_grow_right_simple) {
  EXPECT_EQ(rangeMin + initWidth, rangeMax);

  arangodb::containers::FnvHashProvider hasher;

  EXPECT_EQ(6, depth());
  EXPECT_EQ(0, count());
  EXPECT_EQ(0, rootValue());

  // There are 2^18 buckets, and initWidth is 2^24, so 2^6=64 values
  // per bucket. We put something in bucket 1, but nothing in buckets
  // 2 and 3. When we grow right, it does a leftCombine without shift
  // and then buckets 0 and 1 are combined into 0 and buckets 2 and 3
  // (both empty) must be combined into the new bucket 1.

  insert(64);
  // Now grow to the right:
  insert(rangeMax);

  // Must not throw:
  checkConsistency();
}
