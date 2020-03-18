////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
std::vector<std::size_t> permutation(std::size_t n) {
  std::vector<std::size_t> v;

  v.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    v.emplace_back(i);
  }

  std::random_device rd;
  std::mt19937_64 g(rd());
  std::shuffle(v.begin(), v.end(), g);

  return v;
}

bool diffAsExpected(arangodb::containers::MerkleTree<3, 64>& t1,
                    arangodb::containers::MerkleTree<3, 64>& t2,
                    std::vector<std::pair<std::size_t, std::size_t>>& expected) {
  std::vector<std::pair<std::size_t, std::size_t>> d1 = t1.diff(t2);
  std::vector<std::pair<std::size_t, std::size_t>> d2 = t2.diff(t1);

  auto printTrees = [&t1, &t2]() -> void {
    LOG_DEVEL << "T1: " << t1;
    LOG_DEVEL << "T2: " << t2;
  };

  if (d1.size() != expected.size() || d2.size() != expected.size()) {
    printTrees();
    return false;
  }
  for (std::size_t i = 0; i < expected.size(); ++i) {
    if (d1[i].first != expected[i].first || d1[i].second != expected[i].second ||
        d2[i].first != expected[i].first || d2[i].second != expected[i].second) {
      printTrees();
      return false;
    }
  }
  return true;
}
}  // namespace

class InternalMerkleTreeTest : public ::arangodb::containers::MerkleTree<3, 64>,
                               public ::testing::Test {
 public:
  InternalMerkleTreeTest() : MerkleTree<3, 64>(2, 0, 64) {}
};

TEST_F(InternalMerkleTreeTest, test_childrenAreLeaves) {
  ASSERT_FALSE(childrenAreLeaves(0));
  for (std::size_t index = 1; index < 9; ++index) {
    ASSERT_TRUE(childrenAreLeaves(index));
  }
  for (std::size_t index = 9; index < 73; ++index) {
    ASSERT_FALSE(childrenAreLeaves(index));
  }
}

TEST_F(InternalMerkleTreeTest, test_chunkRange) {
  auto r = chunkRange(0, 0);
  ASSERT_EQ(r.first, 0);
  ASSERT_EQ(r.second, 63);

  for (std::size_t chunk = 0; chunk < 8; ++chunk) {
    auto r = chunkRange(chunk, 1);
    ASSERT_EQ(r.first, chunk * 8);
    ASSERT_EQ(r.second, ((chunk + 1) * 8) - 1);
  }

  for (std::size_t chunk = 0; chunk < 64; ++chunk) {
    auto r = chunkRange(chunk, 2);
    ASSERT_EQ(r.first, chunk);
    ASSERT_EQ(r.second, chunk);
  }
}

TEST_F(InternalMerkleTreeTest, test_index) {
  std::pair<std::size_t, std::size_t> range = this->range();
  ASSERT_EQ(range.first, 0);
  ASSERT_EQ(range.second, 64);

  // make sure depth 0 always gets us 0
  ASSERT_EQ(index(0, 0), 0);
  ASSERT_EQ(index(63, 0), 0);

  // check boundaries at level 1
  for (std::size_t chunk = 0; chunk < 8; ++chunk) {
    std::size_t left = chunk * 8;
    std::size_t right = ((chunk + 1) * 8) - 1;
    ASSERT_EQ(index(left, 1), chunk + 1);
    ASSERT_EQ(index(right, 1), chunk + 1);
  }

  // check boundaries at level 2
  for (std::size_t chunk = 0; chunk < 64; ++chunk) {
    std::size_t left = chunk;  // only one value per chunk
    ASSERT_EQ(index(left, 2), chunk + 9);
  }
}

TEST_F(InternalMerkleTreeTest, test_modify) {
  std::pair<std::size_t, std::size_t> range = this->range();
  ASSERT_EQ(range.first, 0);
  ASSERT_EQ(range.second, 64);

  ASSERT_EQ(count(), 0);
  // check that an attempt to remove will fail if it's empty
  ASSERT_THROW(modify(0, TRI_FnvHashPod(0), false), std::invalid_argument);
  ASSERT_EQ(count(), 0);

  // insert a single value
  modify(0, TRI_FnvHashPod(0), true);
  ASSERT_EQ(count(), 1);

  // build set of indexes that should be touched
  std::set<std::size_t> indices0;
  for (std::size_t depth = 0; depth <= meta().maxDepth; ++depth) {
    indices0.emplace(this->index(0, depth));
  }

  // check that it sets everything it should, and nothing it shouldn't
  for (std::size_t index = 0; index < 73; ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::size_t expectedCount = inSet0 ? 1 : 0;
    std::size_t expectedHash = inSet0 ? TRI_FnvHashPod(0) : 0;

    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, expectedCount);
    ASSERT_EQ(node.hash, expectedHash);
  }

  // insert another value, minimal overlap
  modify(63, TRI_FnvHashPod(63), true);
  ASSERT_EQ(count(), 2);

  // build set of indexes that should be touched
  std::set<std::size_t> indices63;
  for (std::size_t depth = 0; depth <= meta().maxDepth; ++depth) {
    indices63.emplace(this->index(63, depth));
  }

  // check that it sets everything it should, and nothing it shouldn't
  for (std::size_t index = 0; index < 73; ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::size_t expectedCount = inSet0 ? 1 : 0;
    std::size_t expectedHash = inSet0 ? TRI_FnvHashPod(0) : 0;
    if (indices63.find(index) != indices63.end()) {
      ++expectedCount;
      expectedHash ^= TRI_FnvHashPod(63);
    }

    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, expectedCount);
    ASSERT_EQ(node.hash, expectedHash);
  }

  // insert another value, more overlap
  modify(1, TRI_FnvHashPod(1), true);
  ASSERT_EQ(count(), 3);

  // build set of indexes that should be touched
  std::set<std::size_t> indices1;
  for (std::size_t depth = 0; depth <= meta().maxDepth; ++depth) {
    indices1.emplace(this->index(1, depth));
  }

  // check that it sets everything it should, and nothing it shouldn't
  for (std::size_t index = 0; index < 73; ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::size_t expectedCount = inSet0 ? 1 : 0;
    std::size_t expectedHash = inSet0 ? TRI_FnvHashPod(0) : 0;
    if (indices1.find(index) != indices1.end()) {
      ++expectedCount;
      expectedHash ^= TRI_FnvHashPod(1);
    }
    if (indices63.find(index) != indices63.end()) {
      ++expectedCount;
      expectedHash ^= TRI_FnvHashPod(63);
    }

    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, expectedCount);
    ASSERT_EQ(node.hash, expectedHash);
  }

  // remove a value, minimal overlap
  modify(63, TRI_FnvHashPod(63), false);
  ASSERT_EQ(count(), 2);

  // check that it sets everything it should, and nothing it shouldn't
  for (std::size_t index = 0; index < 73; ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::size_t expectedCount = inSet0 ? 1 : 0;
    std::size_t expectedHash = inSet0 ? TRI_FnvHashPod(0) : 0;
    if (indices1.find(index) != indices1.end()) {
      ++expectedCount;
      expectedHash ^= TRI_FnvHashPod(1);
    }

    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, expectedCount);
    ASSERT_EQ(node.hash, expectedHash);
  }

  // remove a value, maximal overlap
  modify(1, TRI_FnvHashPod(1), false);
  ASSERT_EQ(count(), 1);

  // check that it sets everything it should, and nothing it shouldn't
  for (std::size_t index = 0; index < 73; ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::size_t expectedCount = inSet0 ? 1 : 0;
    std::size_t expectedHash = inSet0 ? TRI_FnvHashPod(0) : 0;

    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, expectedCount);
    ASSERT_EQ(node.hash, expectedHash);
  }

  // remove a value, maximal overlap
  modify(0, TRI_FnvHashPod(0), false);
  ASSERT_EQ(count(), 0);

  // check that it sets everything it should, and nothing it shouldn't
  for (std::size_t index = 0; index < 73; ++index) {
    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, 0);
    ASSERT_EQ(node.hash, 0);
  }
}

TEST_F(InternalMerkleTreeTest, test_grow) {
  std::pair<std::size_t, std::size_t> range = this->range();
  ASSERT_EQ(range.first, 0);
  ASSERT_EQ(range.second, 64);

  // fill the tree, but not enough that it grows
  for (std::size_t i = 0; i < 64; ++i) {
    insert(i, TRI_FnvHashPod(i));
  }
  range = this->range();
  ASSERT_EQ(range.first, 0);
  ASSERT_EQ(range.second, 64);

  // check that tree state is as expected prior to growing
  {
    std::size_t hash0 = 0;
    std::size_t hash1[8] = {0};
    std::size_t hash2[64] = {0};
    for (std::size_t i = 0; i < 64; ++i) {
      hash0 ^= TRI_FnvHashPod(i);
      hash1[i / 8] ^= TRI_FnvHashPod(i);
      hash2[i] ^= TRI_FnvHashPod(i);
    }
    for (std::size_t i = 0; i < 64; ++i) {
      Node& node = this->node(this->index(i, 2));
      ASSERT_EQ(node.count, 1);
      ASSERT_EQ(node.hash, hash2[i]);
    }
    for (std::size_t i = 0; i < 8; ++i) {
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
  for (std::size_t i = 64; i < 128; ++i) {
    insert(i, TRI_FnvHashPod(i));
  }
  range = this->range();
  ASSERT_EQ(range.first, 0);
  ASSERT_EQ(range.second, 128);

  // check that tree state is as expected after growing
  {
    std::size_t hash0 = 0;
    std::size_t hash1[8] = {0};
    std::size_t hash2[64] = {0};
    for (std::size_t i = 0; i < 128; ++i) {
      hash0 ^= TRI_FnvHashPod(i);
      hash1[i / 16] ^= TRI_FnvHashPod(i);
      hash2[i / 2] ^= TRI_FnvHashPod(i);
    }
    for (std::size_t i = 0; i < 64; ++i) {
      Node& node = this->node(i + 9);
      ASSERT_EQ(node.count, 2);
      if (node.hash != hash2[i]) {
      }
      ASSERT_EQ(node.hash, hash2[i]);
    }
    for (std::size_t i = 0; i < 8; ++i) {
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

TEST(MerkleTreeTest, test_diff_equal) {
  ::arangodb::containers::MerkleTree<3, 64> t1(2, 0, 64);
  ::arangodb::containers::MerkleTree<3, 64> t2(2, 0, 64);

  std::vector<std::pair<std::size_t, std::size_t>> expected;  // empty
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));

  std::vector<std::size_t> order = ::permutation(64);
  for (std::size_t i : order) {
    t1.insert(i, TRI_FnvHashPod(i));
    t2.insert(i, TRI_FnvHashPod(i));
    ASSERT_TRUE(::diffAsExpected(t1, t2, expected));
  }

  order = ::permutation(64);
  for (std::size_t i : order) {
    t1.remove(i, TRI_FnvHashPod(i));
    t2.remove(i, TRI_FnvHashPod(i));
    ASSERT_TRUE(::diffAsExpected(t1, t2, expected));
  }
}

TEST(MerkleTreeTest, test_diff_one_empty) {
  ::arangodb::containers::MerkleTree<3, 64> t1(2, 0, 64);
  ::arangodb::containers::MerkleTree<3, 64> t2(2, 0, 64);

  std::vector<std::pair<std::size_t, std::size_t>> expected;
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));

  for (std::size_t i = 0; i < 8; ++i) {
    t1.insert(8 * i, TRI_FnvHashPod(8 * i));
    expected.emplace_back(std::make_pair(8 * i, 8 * i));
    ASSERT_TRUE(::diffAsExpected(t1, t2, expected));
  }

  expected.clear();
  for (std::size_t i = 0; i < 8; ++i) {
    t1.insert((8 * i) + 1, TRI_FnvHashPod((8 * i) + 1));
    expected.emplace_back(std::make_pair(8 * i, (8 * i) + 1));
  }
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));

  expected.clear();
  for (std::size_t i = 0; i < 8; ++i) {
    t1.insert((8 * i) + 2, TRI_FnvHashPod((8 * i) + 2));
    t1.insert((8 * i) + 3, TRI_FnvHashPod((8 * i) + 3));
    expected.emplace_back(std::make_pair(8 * i, (8 * i) + 3));
  }
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));

  expected.clear();
  for (std::size_t i = 0; i < 8; ++i) {
    t1.insert((8 * i) + 4, TRI_FnvHashPod((8 * i) + 4));
    t1.insert((8 * i) + 5, TRI_FnvHashPod((8 * i) + 5));
    t1.insert((8 * i) + 6, TRI_FnvHashPod((8 * i) + 6));
    t1.insert((8 * i) + 7, TRI_FnvHashPod((8 * i) + 7));
  }
  expected.emplace_back(std::make_pair(0, 63));
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));
}

TEST(MerkleTreeTest, test_diff_misc) {
  ::arangodb::containers::MerkleTree<3, 64> t1(2, 0, 64);
  ::arangodb::containers::MerkleTree<3, 64> t2(2, 0, 64);

  std::vector<std::pair<std::size_t, std::size_t>> expected;
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));

  for (std::size_t i = 0; i < 32; ++i) {
    t1.insert(2 * i, TRI_FnvHashPod(2 * i));
    expected.emplace_back(std::make_pair(2 * i, 2 * i));
  }
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));

  expected.clear();
  for (std::size_t i = 0; i < 32; ++i) {
    t2.insert((2 * i) + 1, TRI_FnvHashPod((2 * i) + 1));
  }
  expected.emplace_back(std::make_pair(0, 63));
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));

  expected.clear();
  for (std::size_t i = 0; i < 16; ++i) {
    t1.insert((2 * i) + 1, TRI_FnvHashPod((2 * i) + 1));
    expected.emplace_back(std::make_pair(2 * i, 2 * i));
  }
  expected.emplace_back(std::make_pair(32, 63));
  ASSERT_TRUE(::diffAsExpected(t1, t2, expected));
}

TEST(MerkleTreeTest, test_serializeBinary) {
  ::arangodb::containers::MerkleTree<3, 64> t1(2, 0, 64);

  for (std::size_t i = 0; i < 32; ++i) {
    t1.insert(2 * i, TRI_FnvHashPod(2 * i));
  }

  std::string t1s;
  t1.serializeBinary(t1s, true);
  std::unique_ptr<::arangodb::containers::MerkleTree<3, 64>> t2 =
      ::arangodb::containers::MerkleTree<3, 64>::fromBuffer(t1s);
  ASSERT_NE(t2, nullptr);
  ASSERT_TRUE(t1.diff(*t2).empty());
  ASSERT_TRUE(t2->diff(t1).empty());
}

TEST(MerkleTreeTest, test_serializePortable) {
  ::arangodb::containers::MerkleTree<3, 64> t1(2, 0, 64);

  for (std::size_t i = 0; i < 32; ++i) {
    t1.insert(2 * i, TRI_FnvHashPod(2 * i));
  }

  ::arangodb::velocypack::Builder t1s;
  t1.serialize(t1s);
  std::unique_ptr<::arangodb::containers::MerkleTree<3, 64>> t2 =
      ::arangodb::containers::MerkleTree<3, 64>::deserialize(t1s.slice());
  ASSERT_NE(t2, nullptr);
  ASSERT_TRUE(t1.diff(*t2).empty());
  ASSERT_TRUE(t2->diff(t1).empty());
}
