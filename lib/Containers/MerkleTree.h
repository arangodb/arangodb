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

#ifndef ARANGODB_CONTAINERS_MERKLE_TREE_H
#define ARANGODB_CONTAINERS_MERKLE_TREE_H 1

#include <atomic>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "Basics/debugging.h"

#include "Logger/LogMacros.h"

namespace arangodb {
namespace containers {

template <std::size_t const BranchingBits = 3,  // 8 children per internal node,
          std::size_t const LockStripes = 64  // number number of buckets that can be accessed simultaneously
          >
class MerkleTree {
 protected:
  static constexpr std::size_t CacheLineSize = 64;
  static constexpr std::size_t BranchingFactor = static_cast<std::size_t>(1) << BranchingBits;

  struct Node {
    std::size_t hash;
    std::size_t count;

    // Node(std::size_t h, std::size_t c) : hash(h), count(c) {}
  };
  static_assert(sizeof(Node) > CacheLineSize / 8,
                "Node size assumptions invalid.");
  static_assert(sizeof(Node) <= CacheLineSize,
                "Node size assumptions invalid.");
  static constexpr std::size_t NodeSize =
      sizeof(Node) > (CacheLineSize / 2)
          ? (CacheLineSize*((sizeof(Node) + (CacheLineSize - 1)) / CacheLineSize))
          : (sizeof(Node) > (CacheLineSize / 4) ? (CacheLineSize / 2)
                                                : (CacheLineSize / 4));

  struct Meta {
    std::size_t rangeMin;
    std::size_t rangeMax;
    std::size_t maxDepth;
  };
  static constexpr std::size_t MetaSize =
      (CacheLineSize * ((sizeof(Meta) + (CacheLineSize - 1)) / CacheLineSize));

  static constexpr std::size_t nodeCountAtDepth(std::size_t maxDepth) {
    return static_cast<std::size_t>(1) << (BranchingBits * maxDepth);
  }

  static constexpr std::size_t nodeCountUpToDepth(std::size_t maxDepth) {
    return ((static_cast<std::size_t>(1) << (BranchingBits * (maxDepth + 1))) - 1) /
           (BranchingFactor - 1);
  }

  static constexpr bool isPowerOf2(std::size_t n) {
    return n > 0 && (n & (n - 1)) == 0;
  }

  static constexpr std::size_t log2ceil(std::size_t n) {
    if (n > (std::numeric_limits<std::size_t>::max() / 2)) {
      return 8 * sizeof(std::size_t) - 1;
    }
    std::size_t i = 1;
    for (; (static_cast<std::size_t>(1) << i) < n; ++i) {
    }
    return i;
  }

  static constexpr std::size_t minimumFactorFor(std::size_t current, std::size_t target) {
    if (target < current) {
      throw std::invalid_argument("Was expecting target >= current.");
    }

    if (!isPowerOf2(current)) {
      throw std::invalid_argument("Was expecting current to be power of 2.");
    }

    // special fast case
    if (target == current) {
      return 2;
    }

    std::size_t rawFactor = target / current;
    std::size_t correction = (rawFactor * current == target) ? 0 : 1;
    std::size_t correctedFactor = static_cast<std::size_t>(1)
                                  << log2ceil(rawFactor + correction);  // force power of 2
    TRI_ASSERT(isPowerOf2(correctedFactor));
    return correctedFactor;
  }

 public:
  /**
   * @brief Chooses the default range width for a tree of a given depth.
   *
   * Most applications should use either this value or some power-of-two
   * mulitple of this value. The default is chosen so that each leaf bucket
   * initially covers a range of 64 keys.
   *
   * @param maxDepth The same depth value passed to the constructor
   */
  static std::size_t defaultRange(std::size_t maxDepth) {
    return nodeCountAtDepth(maxDepth) * 64;
  }

  /**
   * @brief Construct a Merkle tree of a given depth with a given minimum key
   *
   * @param maxDepth The depth of the tree. This determines how much memory The
   *                 tree will consume, and how fine-grained the hash is.
   * @param rangeMin The minimum key that can be stored in the tree over its
   *                 lifetime. An attempt to insert a smaller key will result
   *                 in an exception.
   * @param rangeMax Must be an offset from rangeMin of a multiple of the number
   *                 of leaf nodes. If 0, it will be  chosen using the
   *                 defaultRange method. This is just an initial value to
   *                 prevent immediate resizing; if a key larger than rangeMax
   *                 is inserted into the tree, it will be dynamically resized
   *                 so that a larger rangeMax is chosen, and adjacent nodes
   *                 merged as necessary.
   */
  MerkleTree(std::size_t maxDepth, std::size_t rangeMin, std::size_t rangeMax = 0)
      : _buffer(new uint8_t[MetaSize + (NodeSize * nodeCountUpToDepth(maxDepth))]) {
    if (rangeMax == 0) {
      rangeMax = rangeMin + defaultRange(maxDepth);
    }
    TRI_ASSERT(((rangeMax - rangeMin) / nodeCountAtDepth(maxDepth)) * nodeCountAtDepth(maxDepth) ==
               (rangeMax - rangeMin));
    new (&meta()) Meta{rangeMin, rangeMax, maxDepth};

    std::size_t const last = nodeCountUpToDepth(maxDepth);
    for (std::size_t i = 0; i < last; ++i) {
      new (&node(i)) Node{0, 0};
    }
  }

  ~MerkleTree() {
    std::unique_lock<std::shared_mutex> guard(_bufferLock);

    std::size_t const last = nodeCountUpToDepth(meta().maxDepth);
    for (std::size_t i = 0; i < last; ++i) {
      node(i).~Node();
    }

    meta().~Meta();
  }

  /**
   * @brief Returns the number of hashed keys contained in the tree
   */
  std::size_t count() const {
    std::shared_lock<std::shared_mutex> guard(_bufferLock);
    std::unique_lock<std::mutex> lock(this->lock(0));
    return node(0).count;
  }

  /**
   * @brief Returns the current range of the tree
   */
  std::pair<std::size_t, std::size_t> range() const {
    std::shared_lock<std::shared_mutex> guard(_bufferLock);
    return {meta().rangeMin, meta().rangeMax};
  }

  /**
   * @brief Returns the maximum depth of the tree
   */
  std::size_t maxDepth() const {
    std::shared_lock<std::shared_mutex> guard(_bufferLock);
    return meta().maxDepth;
  }

  /**
   * @brief Insert a value into the tree. May trigger a resize.
   *
   * @param key   The key for the item. If it is less than the minimum specified
   *              at construction, then it will trigger an exception. If it is
   *              greater than the current max, it will trigger a (potentially
   *              expensive) growth operation to ensure the key can be inserted.
   * @param value The hashed value associated with the key
   */
  void insert(std::size_t key, std::size_t value) {
    std::shared_lock<std::shared_mutex> guard(_bufferLock);
    if (key < meta().rangeMin) {
      throw std::out_of_range("Key out of current range.");
    }

    if (key >= meta().rangeMax) {
      // unlock so we can get exclusive access to grow the range
      guard.unlock();
      grow(key);
      guard.lock();
    }

    modify(key, value, /* isInsert */ true);
  }

  /**
   * @brief Remove a value from the tree.
   *
   * @param key   The key for the item. If it is outside the current tree range,
   *              then it will trigger an exception.
   * @param value The hashed value associated with the key
   */
  void remove(std::size_t key, std::size_t value) {
    std::shared_lock<std::shared_mutex> guard(_bufferLock);
    if (key < meta().rangeMin || key >= meta().rangeMax) {
      throw std::out_of_range("Key out of current range.");
    }

    modify(key, value, /* isInsert */ false);
  }

  /**
   * @brief Find the ranges of keys over which two trees differ.
   *
   * @param other The other tree to compare
   * @return  Vector of (inclusive) ranges of keys over which trees differ
   */
  std::vector<std::pair<std::size_t, std::size_t>> diff(
      MerkleTree<BranchingBits, LockStripes> const& other) const {
    std::shared_lock<std::shared_mutex> guard1(_bufferLock);
    std::shared_lock<std::shared_mutex> guard2(other._bufferLock);

    if (this->meta().maxDepth != other.meta().maxDepth) {
      throw std::invalid_argument("Expecting two trees with same maxDepth.");
    }

    if (this->meta().rangeMin != other.meta().rangeMin) {
      throw std::invalid_argument("Expecting two trees with same rangeMin.");
    }

    while (this->meta().rangeMax != other.meta().rangeMax) {
      if (this->meta().rangeMax < other.meta().rangeMax) {
        // grow this to match other range
        guard1.unlock();
        this->grow(other.meta().rangeMax - 1);
        guard1.lock();
      } else {
        // grow other to match this range
        guard2.unlock();
        other.grow(this->meta().rangeMax - 1);
        guard2.lock();
      }
      // loop to repeat check to make sure someone else didn't grow while we
      // switched between shared/exclusive locks
    }

    // check special case
    {
      if (equalAtIndex(other, 0)) {
        // trees should be the same, no need to check deeper
        return {};
      }
    }

    return diffInternal(other);
  }

 protected:
  Meta& meta() const {
    // not thread-safe, lock buffer from outside
    return *reinterpret_cast<Meta*>(_buffer.get());
  }

  Node& node(std::size_t index) const {
    // not thread-safe, lock buffer from outside
    TRI_ASSERT(index < nodeCountUpToDepth(meta().maxDepth));
    uint8_t* ptr = _buffer.get() + MetaSize + (NodeSize * index);
    return *reinterpret_cast<Node*>(ptr);
  }

  std::mutex& lock(std::size_t index) const {
    return _nodeLocks[_hasher(index) % LockStripes];
  }

  std::size_t index(std::size_t key, std::size_t depth) {
    // not thread-safe, lock buffer from outside
    TRI_ASSERT(depth <= meta().maxDepth);
    TRI_ASSERT(key >= meta().rangeMin);
    TRI_ASSERT(key < meta().rangeMax);

    // special fast case
    if (depth == 0) {
      return 0;
    }

    std::size_t offset = key - meta().rangeMin;
    std::size_t chunkSizeAtDepth =
        (meta().rangeMax - meta().rangeMin) / (1 << (BranchingBits * depth));
    std::size_t chunk = offset / chunkSizeAtDepth;

    return chunk + nodeCountUpToDepth(depth - 1);
  }

  void modify(std::size_t key, std::size_t value, bool isInsert) {
    // not thread-safe, lock buffer from outside
    for (std::size_t depth = 0; depth <= meta().maxDepth; ++depth) {
      std::size_t index = this->index(key, depth);
      Node& node = this->node(index);
      std::unique_lock<std::mutex> lock(this->lock(index));
      if (isInsert) {
        ++node.count;
      } else {
        if (node.count == 0) {
          throw std::invalid_argument(
              "Tried to remove key that is not present.");
        }
        --node.count;
      }
      node.hash ^= value;
    }
  }

  void grow(std::size_t key) {
    std::unique_lock<std::shared_mutex> guard(_bufferLock);
    // no need to lock nodes as we have an exclusive lock on the buffer

    std::size_t rangeMin = meta().rangeMin;
    std::size_t rangeMax = meta().rangeMax;
    if (key < rangeMax) {
      // someone else resized already while we were waiting for the lock
      return;
    }

    std::size_t factor = minimumFactorFor(rangeMax - rangeMin, key - rangeMin);

    for (std::size_t depth = 1; depth <= meta().maxDepth; ++depth) {
      // iterate over all nodes and left-combine, (skipping the first, identity)
      std::size_t offset = nodeCountUpToDepth(depth - 1);
      for (std::size_t index = 1; index < nodeCountAtDepth(depth); ++index) {
        Node& src = this->node(offset + index);
        Node& dst = this->node(offset + (index / factor));
        dst.count += src.count;
        dst.hash ^= src.hash;
        src = Node{0, 0};
      }
    }

    meta().rangeMax = rangeMin + ((rangeMax - rangeMin) * factor);
  }

  bool equalAtIndex(MerkleTree<BranchingBits, LockStripes> const& other,
                    std::size_t index) const {
    // not fully thread-safe, lock buffer from outside
    std::unique_lock<std::mutex> lock1(this->lock(index));
    std::unique_lock<std::mutex> lock2(other.lock(index));
    return this->node(index) == other.node(index);
  }

  std::vector<std::pair<std::size_t, std::size_t>> diffInternal(
      MerkleTree<BranchingBits, LockStripes> const& other) const {
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    bool openRange = false;
    std::size_t left = 0;
    std::size_t depth = 1;
    std::size_t chunkAt[meta().maxDepth + 1] = {0};

    auto range = [this](std::size_t index,
                        std::size_t depth) -> std::pair<std::size_t, std::size_t> {
      std::size_t rangeMin = meta().rangeMin;
      std::size_t rangeMax = meta().rangeMax;
      std::size_t chunkSizeAtDepth =
          (rangeMax - rangeMin) / (1 << (BranchingBits * depth));
      return std::make_pair(rangeMin + (chunkSizeAtDepth * index),
                            rangeMin + (chunkSizeAtDepth * (index + 1)) - 1);
    };
    auto doneWithSiblingSet = [&chunkAt, &depth, BranchingFactor]() -> bool {
      return chunkAt[depth] >= BranchingFactor * (chunkAt[depth - 1] + 1);
    };
    auto alreadyHandledChildren = [&chunkAt, &depth, BranchingFactor]() -> bool {
      return chunkAt[depth + 1] >= BranchingFactor * (chunkAt[depth] + 1);
    };
    auto advance = [&depth, &chunkAt, &doneWithSiblingSet] -> void {
      ++chunkAt[depth];
      if (doneWithSiblingSet()) {
        // finished all the chunks at this depth for our parent
        --depth;
      }
    };
    auto stepDown = [&chunkAt, &depth, BranchingFactor]() -> void {
      if (chunkAt[depth + 1] < chunkAt[depth] * BranchingFactor) {
        chunkAt[depth + 1] = chunkAt[depth] * BranchingFactor;
      }
      ++depth;
    };

    while (depth > 0) {
      if (!alreadyHandledChildren()) {
        if (openRange) {
          if (equalAtIndex(other, this->index(chunkAt[depth], depth))) {
            // we can close the range, everything here matches
            std::size_t right = range(chunkAt[depth], depth).second;
            ranges.emplace_back(left, right);
            openRange = false;
          } else if (depth < meta().maxDepth) {
            // still have potential differences, drop down to next level
            stepDown();
          }
        }  // else we are at max depth, and extend the range
      } else {
        if (!equalAtIndex(other, this->index(chunkAt[depth], depth))) {
          // new range to investigate
          if (depth < meta().maxDepth) {
            stepDown();
          } else {
            left = range(chunkAt[depth], depth).first;
            openRange = true;
          }
        }
        // else continue to skip over whatever keys we can
      }
    }
    advance();
  }

  return ranges;
}

private : std::unique_ptr<uint8_t[]>
              _buffer;
mutable std::shared_mutex _bufferLock;
std::hash<std::size_t> _hasher;
mutable std::mutex _nodeLocks[LockStripes];
};

}  // namespace containers
}  // namespace arangodb

#endif
