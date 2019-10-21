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

#include "gtest/gtest.h"

#include "Containers/MerkleTree.h"

class InternalMerkleTreeTest : public ::arangodb::containers::MerkleTree<>,
                               public ::testing::Test {
 public:
  InternalMerkleTreeTest() : MerkleTree<>(2, 0, 64) {}
};

TEST_F(InternalMerkleTreeTest, test_log2ceil) {
  ASSERT_EQ(log2ceil(0), 1);
  ASSERT_EQ(log2ceil(1), 1);
  ASSERT_EQ(log2ceil(2), 1);
  ASSERT_EQ(log2ceil(3), 2);
  ASSERT_EQ(log2ceil(4), 2);
  ASSERT_EQ(log2ceil(5), 3);
  ASSERT_EQ(log2ceil(8), 3);
  ASSERT_EQ(log2ceil(9), 4);
  ASSERT_EQ(log2ceil(16), 4);
  ASSERT_EQ(log2ceil(17), 5);
  // ...
  ASSERT_EQ(log2ceil((std::numeric_limits<std::size_t>::max() / 2) + 1),
            8 * sizeof(std::size_t) - 1);
}

TEST_F(InternalMerkleTreeTest, test_minimumFactorFor) {
  // test error on target <= current
  ASSERT_THROW(minimumFactorFor(2, 1), std::invalid_argument);

  // test error on current not power of 2
  ASSERT_THROW(minimumFactorFor(3, 4), std::invalid_argument);

  // test valid inputs
  ASSERT_EQ(minimumFactorFor(1, 2), 2);
  ASSERT_EQ(minimumFactorFor(1, 4), 4);
  ASSERT_EQ(minimumFactorFor(1, 12), 16);
  ASSERT_EQ(minimumFactorFor(1, 16), 16);
  ASSERT_EQ(minimumFactorFor(2, 4), 2);
  ASSERT_EQ(minimumFactorFor(2, 5), 4);
  ASSERT_EQ(minimumFactorFor(2, 8), 4);
  ASSERT_EQ(minimumFactorFor(2, 17), 16);
  ASSERT_EQ(minimumFactorFor(2, 31), 16);
  ASSERT_EQ(minimumFactorFor(2, 32), 16);
  ASSERT_EQ(minimumFactorFor(65536, 90000), 2);
  ASSERT_EQ(minimumFactorFor(8192, 2147483600), 262144);
}

TEST_F(InternalMerkleTreeTest, test_nodeCountAtDepth) {
  ASSERT_EQ(nodeCountAtDepth(0), 1);
  ASSERT_EQ(nodeCountAtDepth(1), 8);
  ASSERT_EQ(nodeCountAtDepth(2), 64);
  ASSERT_EQ(nodeCountAtDepth(3), 512);
  // ...
  ASSERT_EQ(nodeCountAtDepth(10), 1073741824);
}

TEST_F(InternalMerkleTreeTest, test_nodeCountUpToDepth) {
  ASSERT_EQ(nodeCountUpToDepth(0), 1);
  ASSERT_EQ(nodeCountUpToDepth(1), 9);
  ASSERT_EQ(nodeCountUpToDepth(2), 73);
  ASSERT_EQ(nodeCountUpToDepth(3), 585);
  // ...
  ASSERT_EQ(nodeCountUpToDepth(10), 1227133513);
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
  ASSERT_EQ(nodeCountAtDepth(2), 64);
  std::pair<std::size_t, std::size_t> range = this->range();
  ASSERT_EQ(range.first, 0);
  ASSERT_EQ(range.second, 64);

  ASSERT_EQ(count(), 0);
  // check that an attempt to remove will fail if it's empty
  ASSERT_THROW(modify(0, 1, false), std::invalid_argument);
  ASSERT_EQ(count(), 0);

  // insert a single value
  modify(0, 1, true);
  ASSERT_EQ(count(), 1);

  // build set of indexes that should be touched
  std::set<std::size_t> indices0;
  for (std::size_t depth = 0; depth <= meta().maxDepth; ++depth) {
    indices0.emplace(this->index(0, depth));
  }

  // check that it sets everything it should, and nothing it shouldn't
  for (std::size_t index = 0; index < nodeCountUpToDepth(meta().maxDepth); ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::size_t expectedCount = inSet0 ? 1 : 0;
    std::size_t expectedHash = inSet0 ? 1 : 0;

    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, expectedCount);
    ASSERT_EQ(node.hash, expectedHash);
  }

  // insert another value, minimal overlap
  modify(63, 64, true);
  ASSERT_EQ(count(), 2);

  // build set of indexes that should be touched
  std::set<std::size_t> indices63;
  for (std::size_t depth = 0; depth <= meta().maxDepth; ++depth) {
    indices63.emplace(this->index(63, depth));
  }

  // check that it sets everything it should, and nothing it shouldn't
  for (std::size_t index = 0; index < nodeCountUpToDepth(meta().maxDepth); ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::size_t expectedCount = inSet0 ? 1 : 0;
    std::size_t expectedHash = inSet0 ? 1 : 0;
    if (indices63.find(index) != indices63.end()) {
      ++expectedCount;
      expectedHash ^= 64;
    }

    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, expectedCount);
    ASSERT_EQ(node.hash, expectedHash);
  }

  // insert another value, more overlap
  modify(1, 2, true);
  ASSERT_EQ(count(), 3);

  // build set of indexes that should be touched
  std::set<std::size_t> indices1;
  for (std::size_t depth = 0; depth <= meta().maxDepth; ++depth) {
    indices1.emplace(this->index(1, depth));
  }

  // check that it sets everything it should, and nothing it shouldn't
  for (std::size_t index = 0; index < nodeCountUpToDepth(meta().maxDepth); ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::size_t expectedCount = inSet0 ? 1 : 0;
    std::size_t expectedHash = inSet0 ? 1 : 0;
    if (indices1.find(index) != indices1.end()) {
      ++expectedCount;
      expectedHash ^= 2;
    }
    if (indices63.find(index) != indices63.end()) {
      ++expectedCount;
      expectedHash ^= 64;
    }

    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, expectedCount);
    ASSERT_EQ(node.hash, expectedHash);
  }

  // remove a value, minimal overlap
  modify(63, 64, false);
  ASSERT_EQ(count(), 2);

  // check that it sets everything it should, and nothing it shouldn't
  for (std::size_t index = 0; index < nodeCountUpToDepth(meta().maxDepth); ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::size_t expectedCount = inSet0 ? 1 : 0;
    std::size_t expectedHash = inSet0 ? 1 : 0;
    if (indices1.find(index) != indices1.end()) {
      ++expectedCount;
      expectedHash ^= 2;
    }

    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, expectedCount);
    ASSERT_EQ(node.hash, expectedHash);
  }

  // remove a value, maximal overlap
  modify(1, 2, false);
  ASSERT_EQ(count(), 1);

  // check that it sets everything it should, and nothing it shouldn't
  for (std::size_t index = 0; index < nodeCountUpToDepth(meta().maxDepth); ++index) {
    bool inSet0 = indices0.find(index) != indices0.end();
    std::size_t expectedCount = inSet0 ? 1 : 0;
    std::size_t expectedHash = inSet0 ? 1 : 0;

    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, expectedCount);
    ASSERT_EQ(node.hash, expectedHash);
  }

  // remove a value, maximal overlap
  modify(0, 1, false);
  ASSERT_EQ(count(), 0);

  // check that it sets everything it should, and nothing it shouldn't
  for (std::size_t index = 0; index < nodeCountUpToDepth(meta().maxDepth); ++index) {
    InternalMerkleTreeTest::Node& node = this->node(index);
    ASSERT_EQ(node.count, 0);
    ASSERT_EQ(node.hash, 0);
  }
}

TEST_F(InternalMerkleTreeTest, test_grow) {
  ASSERT_EQ(nodeCountAtDepth(1), 8);
  ASSERT_EQ(nodeCountAtDepth(2), 64);

  std::pair<std::size_t, std::size_t> range = this->range();
  ASSERT_EQ(range.first, 0);
  ASSERT_EQ(range.second, 64);

  // fill the tree, but not enough that it grows
  for (std::size_t i = 0; i < 64; ++i) {
    insert(i, i + 1024);
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
      hash0 ^= i + 1024;
      hash1[i / 8] ^= i + 1024;
      hash2[i] ^= i + 1024;
    }
    {
      Node& node = this->node(0);
      ASSERT_EQ(node.count, 64);
      ASSERT_EQ(node.hash, hash0);
    }
    for (std::size_t i = 0; i < 8; ++i) {
      Node& node = this->node(i + 1);
      ASSERT_EQ(node.count, 8);
      ASSERT_EQ(node.hash, hash1[i]);
    }
    for (std::size_t i = 0; i < 64; ++i) {
      Node& node = this->node(this->index(i, 2));
      ASSERT_EQ(node.count, 1);
      ASSERT_EQ(node.hash, hash2[i]);
    }
  }

  // insert some more and cause it to grow
  for (std::size_t i = 64; i < 128; ++i) {
    insert(i, i + 1024);
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
      hash0 ^= i + 1024;
      hash1[i / 16] ^= i + 1024;
      hash2[i / 2] ^= i + 1024;
    }
    {
      Node& node = this->node(0);
      ASSERT_EQ(node.count, 128);
      ASSERT_EQ(node.hash, hash0);
    }
    for (std::size_t i = 0; i < 8; ++i) {
      Node& node = this->node(i + 1);
      ASSERT_EQ(node.count, 16);
      ASSERT_EQ(node.hash, hash1[i]);
    }
    for (std::size_t i = 0; i < 64; ++i) {
      Node& node = this->node(this->index(i, 2));
      ASSERT_EQ(node.count, 2);
      ASSERT_EQ(node.hash, hash2[i]);
    }
  }
}
