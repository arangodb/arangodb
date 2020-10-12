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
#include <cmath>
#include <cstddef>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <snappy.h>

#include <velocypack/Iterator.h>

#include "MerkleTree.h"

#include "Basics/HybridLogicalClock.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"
#include "Basics/hashes.h"

namespace {
static constexpr std::uint8_t CurrentVersion = 0x01;
static constexpr char CompressedCurrent = '1';
static constexpr char UncompressedCurrent = '2';
}  // namespace

namespace arangodb {
namespace containers {

template <std::size_t const BranchingBits, std::size_t const LockStripes>
bool MerkleTree<BranchingBits, LockStripes>::Node::operator==(Node const& other) {
  return ((this->count == other.count) && (this->hash == other.hash));
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
constexpr std::size_t MerkleTree<BranchingBits, LockStripes>::nodeCountUpToDepth(std::size_t maxDepth) {
  return ((static_cast<std::size_t>(1) << (BranchingBits * (maxDepth + 1))) - 1) /
         (BranchingFactor - 1);
}
class TestNodeCountUpToDepth : public MerkleTree<3, 64> {
  static_assert(nodeCountUpToDepth(0) == 1);
  static_assert(nodeCountUpToDepth(1) == 9);
  static_assert(nodeCountUpToDepth(2) == 73);
  static_assert(nodeCountUpToDepth(3) == 585);
  // ...
  static_assert(nodeCountUpToDepth(10) == 1227133513);
};

template <std::size_t const BranchingBits, std::size_t const LockStripes>
constexpr std::size_t MerkleTree<BranchingBits, LockStripes>::allocationSize(std::size_t maxDepth) {
  return MetaSize + (NodeSize * nodeCountUpToDepth(maxDepth));
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
constexpr std::uint64_t MerkleTree<BranchingBits, LockStripes>::log2ceil(std::uint64_t n) {
  if (n > (std::numeric_limits<std::uint64_t>::max() / 2)) {
    return 8 * sizeof(std::uint64_t) - 1;
  }
  std::uint64_t i = 1;
  for (; (static_cast<std::uint64_t>(1) << i) < n; ++i) {
  }
  return i;
}
class TestLog2Ceil : public MerkleTree<3, 64> {
  static_assert(log2ceil(0) == 1);
  static_assert(log2ceil(1) == 1);
  static_assert(log2ceil(2) == 1);
  static_assert(log2ceil(3) == 2);
  static_assert(log2ceil(4) == 2);
  static_assert(log2ceil(5) == 3);
  static_assert(log2ceil(8) == 3);
  static_assert(log2ceil(9) == 4);
  static_assert(log2ceil(16) == 4);
  static_assert(log2ceil(17) == 5);
  // ...
  static_assert(log2ceil((std::numeric_limits<std::uint64_t>::max() / 2) + 1) ==
                8 * sizeof(std::uint64_t) - 1);
};

template <std::size_t const BranchingBits, std::size_t const LockStripes>
constexpr std::uint64_t MerkleTree<BranchingBits, LockStripes>::minimumFactorFor(
    std::uint64_t current, std::uint64_t target) {
  if (target < current) {
    throw std::invalid_argument("Was expecting target >= current.");
  }

  if (!NumberUtils::isPowerOf2(current)) {
    throw std::invalid_argument("Was expecting current to be power of 2.");
  }

  // special fast case
  if (target == current) {
    return 2;
  }

  std::uint64_t rawFactor = target / current;
  std::uint64_t correctedFactor = static_cast<std::uint64_t>(1)
                                  << log2ceil(rawFactor + 1);  // force power of 2
  TRI_ASSERT(NumberUtils::isPowerOf2(correctedFactor));
  TRI_ASSERT(target >= (current * correctedFactor / 2));
  return correctedFactor;
}
class TestMinimumFactorFor : public MerkleTree<3, 64> {
  static_assert(minimumFactorFor(1, 2) == 4);
  static_assert(minimumFactorFor(1, 4) == 8);
  static_assert(minimumFactorFor(1, 12) == 16);
  static_assert(minimumFactorFor(1, 16) == 32);
  static_assert(minimumFactorFor(2, 3) == 2);
  static_assert(minimumFactorFor(2, 4) == 4);
  static_assert(minimumFactorFor(2, 5) == 4);
  static_assert(minimumFactorFor(2, 7) == 4);
  static_assert(minimumFactorFor(2, 8) == 8);
  static_assert(minimumFactorFor(2, 15) == 8);
  static_assert(minimumFactorFor(2, 16) == 16);
  static_assert(minimumFactorFor(2, 17) == 16);
  static_assert(minimumFactorFor(2, 31) == 16);
  static_assert(minimumFactorFor(2, 32) == 32);
  static_assert(minimumFactorFor(65536, 90000) == 2);
  static_assert(minimumFactorFor(8192, 2147483600) == 262144);
};

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::uint64_t MerkleTree<BranchingBits, LockStripes>::defaultRange(std::size_t maxDepth) {
  // start with 64 revisions per leaf; this is arbitrary, but the key is we want
  // to start with a relatively fine-grained tree so we can differentiate well,
  // but we don't want to go so small that we have to resize immediately
  return nodeCountAtDepth(maxDepth) * 64;
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::unique_ptr<MerkleTree<BranchingBits, LockStripes>>
MerkleTree<BranchingBits, LockStripes>::fromBuffer(std::string_view buffer) {
  bool compressed = false;
  std::uint8_t version = static_cast<uint8_t>(buffer[buffer.size() - 1]);
  if (version == 0x01) {
    compressed = ::CompressedCurrent == buffer[buffer.size() - 2];
  } else {
    throw std::invalid_argument(
        "Buffer does not contain a properly versioned tree.");
  }

  std::string scratch;
  if (compressed) {
    snappy::Uncompress(buffer.data(), buffer.size() - 2, &scratch);
    buffer = std::string_view(scratch);
  } else {
    buffer = std::string_view(buffer.data(), buffer.size() - 2);
  }

  if (buffer.size() < MetaSize) {
    // not enough space to even store the meta info, can't proceed
    return nullptr;
  }

  Meta const& meta = *reinterpret_cast<Meta const*>(buffer.data());
  if (buffer.size() != MetaSize + (NodeSize * nodeCountUpToDepth(meta.maxDepth))) {
    // allocation size doesn't match meta data, can't proceed
    return nullptr;
  }

  return std::unique_ptr<MerkleTree<BranchingBits, LockStripes>>(
      new MerkleTree<BranchingBits, LockStripes>(buffer));
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::unique_ptr<MerkleTree<BranchingBits, LockStripes>>
MerkleTree<BranchingBits, LockStripes>::deserialize(velocypack::Slice slice) {
  std::unique_ptr<MerkleTree<BranchingBits, LockStripes>> tree{nullptr};

  if (!slice.isObject()) {
    return tree;
  }

  velocypack::Slice read = slice.get(StaticStrings::RevisionTreeVersion);
  if (!read.isNumber() || read.getNumber<std::uint8_t>() != ::CurrentVersion) {
    return tree;
  }

  read = slice.get(StaticStrings::RevisionTreeBranchingFactor);
  if (!read.isNumber() || read.getNumber<std::size_t>() != BranchingFactor) {
    return tree;
  }

  read = slice.get(StaticStrings::RevisionTreeMaxDepth);
  if (!read.isNumber()) {
    return tree;
  }
  std::size_t maxDepth = read.getNumber<std::size_t>();

  read = slice.get(StaticStrings::RevisionTreeRangeMax);
  if (!read.isString()) {
    return tree;
  }
  velocypack::ValueLength l;
  char const* p = read.getString(l);
  std::uint64_t rangeMax = basics::HybridLogicalClock::decodeTimeStamp(p, l);
  if (rangeMax == std::numeric_limits<std::uint64_t>::max()) {
    return tree;
  }

  read = slice.get(StaticStrings::RevisionTreeRangeMin);
  if (!read.isString()) {
    return tree;
  }
  p = read.getString(l);
  std::uint64_t rangeMin = basics::HybridLogicalClock::decodeTimeStamp(p, l);
  if (rangeMin == std::numeric_limits<std::uint64_t>::max()) {
    return tree;
  }

  velocypack::Slice nodes = slice.get(StaticStrings::RevisionTreeNodes);
  if (!nodes.isArray() || nodes.length() < nodeCountUpToDepth(maxDepth)) {
    return tree;
  }

  // allocate the tree
  tree.reset(new MerkleTree<BranchingBits, LockStripes>(maxDepth, rangeMin, rangeMax));

  std::size_t index = 0;
  for (velocypack::Slice nodeSlice : velocypack::ArrayIterator(nodes)) {
    read = nodeSlice.get(StaticStrings::RevisionTreeCount);
    if (!read.isNumber()) {
      tree.reset();
      return tree;
    }
    std::size_t count = read.getNumber<std::size_t>();

    read = nodeSlice.get(StaticStrings::RevisionTreeHash);
    if (!read.isString()) {
      tree.reset();
      return tree;
    }
    p = nodeSlice.get(StaticStrings::RevisionTreeHash).getString(l);
    std::uint64_t hash = basics::HybridLogicalClock::decodeTimeStamp(p, l);
    if (hash == std::numeric_limits<std::uint64_t>::max()) {
      tree.reset();
      return tree;
    }

    std::unique_lock<std::mutex> guard(tree->lock(index));
    Node& node = tree->node(index);
    node.hash = hash;
    node.count = count;

    ++index;
  }

  return tree;
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
MerkleTree<BranchingBits, LockStripes>::MerkleTree(std::size_t maxDepth, std::uint64_t rangeMin,
                                                   std::uint64_t rangeMax) {
  if (maxDepth < 2) {
    throw std::invalid_argument("Must specify a maxDepth >= 2.");
  }
  _buffer.reset(new uint8_t[allocationSize(maxDepth)]);

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

template <std::size_t const BranchingBits, std::size_t const LockStripes>
MerkleTree<BranchingBits, LockStripes>::~MerkleTree() {
  std::unique_lock<std::shared_mutex> guard(_bufferLock);

  if (!_buffer) {
    return;
  }

  std::size_t const last = nodeCountUpToDepth(meta().maxDepth);
  for (std::size_t i = 0; i < last; ++i) {
    node(i).~Node();
  }

  meta().~Meta();
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
MerkleTree<BranchingBits, LockStripes>& MerkleTree<BranchingBits, LockStripes>::operator=(
    std::unique_ptr<MerkleTree<BranchingBits, LockStripes>>&& other) {
  if (!other) {
    return *this;
  }

  std::unique_lock<std::shared_mutex> guard1(_bufferLock);
  std::unique_lock<std::shared_mutex> guard2(other->_bufferLock);

  TRI_ASSERT(this->meta().maxDepth == other->meta().maxDepth);
  TRI_ASSERT(this->_buffer);
  TRI_ASSERT(other->_buffer);

  _buffer = std::move(other->_buffer);

  TRI_ASSERT(this->_buffer);
  TRI_ASSERT(!other->_buffer);

  return *this;
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::size_t MerkleTree<BranchingBits, LockStripes>::count() const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  std::unique_lock<std::mutex> lock(this->lock(0));
  return node(0).count;
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::uint64_t MerkleTree<BranchingBits, LockStripes>::rootValue() const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  std::unique_lock<std::mutex> lock(this->lock(0));
  return node(0).hash;
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::pair<std::uint64_t, std::uint64_t> MerkleTree<BranchingBits, LockStripes>::range() const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  return {meta().rangeMin, meta().rangeMax};
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::size_t MerkleTree<BranchingBits, LockStripes>::maxDepth() const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  return meta().maxDepth;
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
void MerkleTree<BranchingBits, LockStripes>::insert(std::uint64_t key, std::uint64_t value) {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  if (key < meta().rangeMin) {
    throw std::out_of_range("Cannot insert, key " + std::to_string(key) +
                            " less than range minimum " +
                            std::to_string(meta().rangeMin) + ".");
  }

  if (key >= meta().rangeMax) {
    // unlock so we can get exclusive access to grow the range
    guard.unlock();
    grow(key);
    guard.lock();
  }

  modify(key, value, /* isInsert */ true);
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
void MerkleTree<BranchingBits, LockStripes>::insert(std::vector<std::uint64_t> const& keys) {
  if (keys.empty()) {
    return;
  }

  std::vector<std::uint64_t> sortedKeys = keys;
  std::sort(sortedKeys.begin(), sortedKeys.end());

  std::uint64_t minKey = sortedKeys[0];
  std::uint64_t maxKey = sortedKeys[sortedKeys.size() - 1];

  std::unique_lock<std::shared_mutex> guard(_bufferLock);

  if (minKey < meta().rangeMin) {
    throw std::out_of_range("Cannot insert, key " + std::to_string(minKey) +
                            " less than range minimum " +
                            std::to_string(meta().rangeMin) + ".");
  }

  if (maxKey >= meta().rangeMax) {
    // unlock so we can get exclusive access to grow the range
    guard.unlock();
    grow(maxKey);
    guard.lock();
  }

  modify(sortedKeys, true);
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
void MerkleTree<BranchingBits, LockStripes>::remove(std::uint64_t key, std::uint64_t value) {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  if (key < meta().rangeMin || key >= meta().rangeMax) {
    throw std::out_of_range("Cannot remove, key out of current range.");
  }

  modify(key, value, /* isInsert */ false);
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
void MerkleTree<BranchingBits, LockStripes>::remove(std::vector<std::uint64_t> const& keys) {
  if (keys.empty()) {
    return;
  }

  std::vector<std::uint64_t> sortedKeys = keys;
  std::sort(sortedKeys.begin(), sortedKeys.end());

  std::uint64_t minKey = sortedKeys[0];
  std::uint64_t maxKey = sortedKeys[sortedKeys.size() - 1];

  std::unique_lock<std::shared_mutex> guard(_bufferLock);

  if (minKey < meta().rangeMin || maxKey >= meta().rangeMax) {
    throw std::out_of_range("Cannot remove, key out of current range.");
  }

  modify(sortedKeys, false);
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
void MerkleTree<BranchingBits, LockStripes>::clear() {
  std::unique_lock<std::shared_mutex> guard(_bufferLock);
  std::size_t const last = nodeCountUpToDepth(meta().maxDepth);
  for (std::size_t i = 0; i < last; ++i) {
    new (&node(i)) Node{0, 0};
  }
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::unique_ptr<MerkleTree<BranchingBits, LockStripes>> MerkleTree<BranchingBits, LockStripes>::clone() {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  return std::unique_ptr<MerkleTree<BranchingBits, LockStripes>>(
      new MerkleTree<BranchingBits, LockStripes>(*this));
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::unique_ptr<MerkleTree<BranchingBits, LockStripes>>
MerkleTree<BranchingBits, LockStripes>::cloneWithDepth(std::size_t newDepth) const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);

  if (newDepth == meta().maxDepth) {
    return std::unique_ptr<MerkleTree<BranchingBits, LockStripes>>(
        new MerkleTree<BranchingBits, LockStripes>(*this));
  }

  if (newDepth < meta().maxDepth) {
    std::unique_ptr<MerkleTree<BranchingBits, LockStripes>> newTree =
        std::make_unique<MerkleTree<BranchingBits, LockStripes>>(newDepth,
                                                                 meta().rangeMin,
                                                                 meta().rangeMax);
    for (std::size_t index = 0; index < nodeCountUpToDepth(newDepth); ++index) {
      Node& n = this->node(index);
      Node& m = newTree->node(index);
      m = n;
    }
    return newTree;
  }

  // Otherwise let's grow the tree deeper. We're going to grow one level at a
  // time, recursively. Typically we'll only be requesting to grow one level
  // deeper at a time anyway, and we should very, very rarely be growing more
  // than a couple levels at a time.
  std::uint64_t newRangeMax =
      meta().rangeMin + ((meta().rangeMax - meta().rangeMin) * BranchingFactor);
  std::unique_ptr<MerkleTree<BranchingBits, LockStripes>> newTree =
      std::make_unique<MerkleTree<BranchingBits, LockStripes>>(newDepth, meta().rangeMin,
                                                               newRangeMax);
  {
    for (std::size_t d = 0; d <= meta().maxDepth; ++d) {
      // copy each cell into the same index at the next level of the deeper tree
      std::size_t const offset = (d == 0 ? 0 : nodeCountUpToDepth(d - 1));
      std::size_t const offsetNew = nodeCountUpToDepth(d);
      for (std::size_t i = 0; i < nodeCountAtDepth(d); ++i) {
        Node& n = this->node(offset + i);
        Node& m = newTree->node(offsetNew + i);
        m = n;
      }
      // now copy the root into the root, special case
      Node& n = this->node(0);
      Node& m = newTree->node(0);
      m = n;
    }
  }

  if (newDepth == meta().maxDepth + 1) {
    return newTree;
  }
  return newTree->cloneWithDepth(newDepth);
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::vector<std::pair<std::uint64_t, std::uint64_t>> MerkleTree<BranchingBits, LockStripes>::diff(
    MerkleTree<BranchingBits, LockStripes>& other) {
  std::shared_lock<std::shared_mutex> guard1(_bufferLock);
  std::shared_lock<std::shared_mutex> guard2(other._bufferLock);

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

  std::size_t const maxDepth = std::min(meta().maxDepth, other.meta().maxDepth);
  std::vector<std::pair<std::uint64_t, std::uint64_t>> result;
  std::queue<std::size_t> candidates;
  candidates.emplace(0);

  while (!candidates.empty()) {
    std::size_t index = candidates.front();
    candidates.pop();
    if (!equalAtIndex(other, index)) {
      bool leaves = childrenAreLeaves(index) || other.childrenAreLeaves(index);
      for (std::size_t child = (BranchingFactor * index) + 1;
           child < (BranchingFactor * (index + 1) + 1); ++child) {
        if (!leaves) {
          // internal children, queue them all up for further investigation
          candidates.emplace(child);
        } else {
          if (!equalAtIndex(other, child)) {
            // actually work with key ranges now
            std::size_t chunk = child - nodeCountUpToDepth(maxDepth - 1);
            std::pair<std::uint64_t, std::uint64_t> range = chunkRange(chunk, maxDepth);
            if (!result.empty() && result.back().second >= range.first - 1) {
              // we are in a continuous range here, just extend it
              result.back().second = range.second;
            } else {
              // discontinuous range, add a new entry
              result.emplace_back(range);
            }
          }
        }
      }
    }
  }

  return result;
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::string MerkleTree<BranchingBits, LockStripes>::toString() const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  std::string output("{");
  for (std::size_t depth = 0; depth <= meta().maxDepth; ++depth) {
    output.append(std::to_string(depth));
    output.append(": [");
    for (std::size_t chunk = 0; chunk < nodeCountAtDepth(depth); ++chunk) {
      std::size_t index = this->index(chunk, depth);
      std::unique_lock<std::mutex> guard(this->lock(index));
      Node& node = this->node(index);
      output.append("[");
      output.append(std::to_string(node.count));
      output.append(",");
      output.append(std::to_string(node.hash));
      output.append("],");
    }
    output.append("],");
  }
  output.append("}");
  return output;
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
void MerkleTree<BranchingBits, LockStripes>::serialize(velocypack::Builder& output,
                                                       std::size_t maxDepth) const {
  std::unique_lock<std::shared_mutex> guard(_bufferLock);
  TRI_ASSERT(output.isEmpty());
  char ridBuffer[11];
  std::size_t depth = std::min(maxDepth, meta().maxDepth);

  velocypack::ObjectBuilder topLevelGuard(&output);
  output.add(StaticStrings::RevisionTreeVersion, velocypack::Value(::CurrentVersion));
  output.add(StaticStrings::RevisionTreeMaxDepth, velocypack::Value(depth));
  output.add(StaticStrings::RevisionTreeBranchingFactor, velocypack::Value(BranchingFactor));
  output.add(StaticStrings::RevisionTreeRangeMax,
             basics::HybridLogicalClock::encodeTimeStampToValuePair(meta().rangeMax, ridBuffer));
  output.add(StaticStrings::RevisionTreeRangeMin,
             basics::HybridLogicalClock::encodeTimeStampToValuePair(meta().rangeMin, ridBuffer));
  velocypack::ArrayBuilder nodeArrayGuard(&output, StaticStrings::RevisionTreeNodes);
  for (std::size_t index = 0; index < nodeCountUpToDepth(depth); ++index) {
    velocypack::ObjectBuilder nodeGuard(&output);
    std::unique_lock<std::mutex> guard(this->lock(index));
    Node& node = this->node(index);
    output.add(StaticStrings::RevisionTreeHash,
               basics::HybridLogicalClock::encodeTimeStampToValuePair(node.hash, ridBuffer));
    output.add(StaticStrings::RevisionTreeCount, velocypack::Value(node.count));
  }
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
void MerkleTree<BranchingBits, LockStripes>::serializeBinary(std::string& output,
                                                             bool compress) const {
  std::unique_lock<std::shared_mutex> guard(_bufferLock);
  TRI_ASSERT(output.empty());
  if (compress) {
    snappy::Compress(reinterpret_cast<char*>(_buffer.get()),
                     allocationSize(meta().maxDepth), &output);
    output.push_back(::CompressedCurrent);
  } else {
    output.append(reinterpret_cast<char*>(_buffer.get()), allocationSize(meta().maxDepth));
    output.push_back(::UncompressedCurrent);
  }
  output.push_back(static_cast<char>(::CurrentVersion));
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
MerkleTree<BranchingBits, LockStripes>::MerkleTree(std::string_view buffer)
    : _buffer(new uint8_t[buffer.size()]) {
  std::memcpy(_buffer.get(), buffer.data(), buffer.size());
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
MerkleTree<BranchingBits, LockStripes>::MerkleTree(MerkleTree<BranchingBits, LockStripes> const& other)
    : _buffer(new uint8_t[allocationSize(other.meta().maxDepth)]) {
  new (&meta()) Meta{other.meta().rangeMin, other.meta().rangeMax, other.meta().maxDepth};

  std::size_t const last = nodeCountUpToDepth(meta().maxDepth);
  for (std::size_t i = 0; i < last; ++i) {
    std::unique_lock<std::mutex> nodeGuard(other.lock(i));
    new (&this->node(i)) Node{other.node(i).hash, other.node(i).count};
  }
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
typename MerkleTree<BranchingBits, LockStripes>::Meta& MerkleTree<BranchingBits, LockStripes>::meta() const {
  // not thread-safe, lock buffer from outside
  TRI_ASSERT(_buffer);
  return *reinterpret_cast<Meta*>(_buffer.get());
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
typename MerkleTree<BranchingBits, LockStripes>::Node&
MerkleTree<BranchingBits, LockStripes>::node(std::size_t index) const {
  // not thread-safe, lock buffer from outside
  TRI_ASSERT(index < nodeCountUpToDepth(meta().maxDepth));
  uint8_t* ptr = _buffer.get() + MetaSize + (NodeSize * index);
  return *reinterpret_cast<Node*>(ptr);
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::mutex& MerkleTree<BranchingBits, LockStripes>::lock(std::size_t index) const {
  return _nodeLocks[TRI_FnvHashPod(index) % LockStripes];
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::size_t MerkleTree<BranchingBits, LockStripes>::index(std::uint64_t key,
                                                          std::size_t depth) const {
  // not thread-safe, lock buffer from outside
  TRI_ASSERT(depth <= meta().maxDepth);
  TRI_ASSERT(key >= meta().rangeMin);
  TRI_ASSERT(key < meta().rangeMax);

  // special fast case
  if (depth == 0) {
    return 0;
  }

  std::size_t offset = key - meta().rangeMin;
  std::size_t chunkSizeAtDepth = (meta().rangeMax - meta().rangeMin) /
                                 (static_cast<std::size_t>(1) << (BranchingBits * depth));
  std::size_t chunk = offset / chunkSizeAtDepth;

  return chunk + nodeCountUpToDepth(depth - 1);
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
void MerkleTree<BranchingBits, LockStripes>::modify(std::uint64_t key,
                                                    std::uint64_t value, bool isInsert) {
  // not thread-safe, shared-lock buffer from outside
  for (std::size_t depth = 0; depth <= meta().maxDepth; ++depth) {
    modifyLocal(depth, key, value, isInsert, true);
  }
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
void MerkleTree<BranchingBits, LockStripes>::modify(std::vector<std::uint64_t> const& keys,
                                                    bool isInsert) {
  // not thread-safe, unique-lock buffer from outside
  for (std::size_t depth = 0; depth <= meta().maxDepth; ++depth) {
    for (std::uint64_t key : keys) {
      modifyLocal(depth, key, TRI_FnvHashPod(key), isInsert, false);
    }
  }
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
void MerkleTree<BranchingBits, LockStripes>::modifyLocal(std::size_t depth, std::uint64_t key,
                                                         std::uint64_t value,
                                                         bool isInsert, bool doLock) {
  // only use via modify
  std::size_t index = this->index(key, depth);
  Node& node = this->node(index);
  std::unique_lock<std::mutex> lock(this->lock(index), std::defer_lock);
  if (doLock) {
    lock.lock();
  }
  if (isInsert) {
    ++node.count;
  } else {
    if (node.count == 0) {
      throw std::invalid_argument("Tried to remove key that is not present.");
    }
    --node.count;
  }
  node.hash ^= value;
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
void MerkleTree<BranchingBits, LockStripes>::grow(std::uint64_t key) {
  std::unique_lock<std::shared_mutex> guard(_bufferLock);
  // no need to lock nodes as we have an exclusive lock on the buffer

  std::uint64_t rangeMin = meta().rangeMin;
  std::uint64_t rangeMax = meta().rangeMax;
  if (key < rangeMax) {
    // someone else resized already while we were waiting for the lock
    return;
  }

  std::uint64_t factor = minimumFactorFor(rangeMax - rangeMin, key - rangeMin);

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
  TRI_ASSERT(key < meta().rangeMax);
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
bool MerkleTree<BranchingBits, LockStripes>::equalAtIndex(
    MerkleTree<BranchingBits, LockStripes> const& other, std::size_t index) const {
  // not fully thread-safe, lock buffer from outside
  std::unique_lock<std::mutex> lock1(this->lock(index));
  std::unique_lock<std::mutex> lock2(other.lock(index));
  return (this->node(index) == other.node(index));
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
bool MerkleTree<BranchingBits, LockStripes>::childrenAreLeaves(std::size_t index) {
  // not thread-safe, lock buffer from outside
  std::size_t maxDepth = meta().maxDepth;
  return index >= nodeCountUpToDepth(maxDepth - 2) &&
         index < nodeCountUpToDepth(maxDepth - 1);
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::pair<std::uint64_t, std::uint64_t> MerkleTree<BranchingBits, LockStripes>::chunkRange(
    std::size_t chunk, std::size_t depth) {
  // not thread-safe, lock buffer from outside
  std::uint64_t rangeMin = meta().rangeMin;
  std::uint64_t rangeMax = meta().rangeMax;
  std::uint64_t chunkSizeAtDepth =
      (rangeMax - rangeMin) / (static_cast<std::uint64_t>(1) << (BranchingBits * depth));
  return std::make_pair(rangeMin + (chunkSizeAtDepth * chunk),
                        rangeMin + (chunkSizeAtDepth * (chunk + 1)) - 1);
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::ostream& operator<<(std::ostream& stream,
                         MerkleTree<BranchingBits, LockStripes> const& tree) {
  return stream << tree.toString();
}

/// INSTANTIATIONS
template class MerkleTree<3, 64>;
template std::ostream& operator<<(std::ostream& stream, MerkleTree<3, 64> const&);

}  // namespace containers
}  // namespace arangodb
