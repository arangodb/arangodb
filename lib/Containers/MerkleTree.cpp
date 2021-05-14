////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Endian.h"
#include "Basics/HybridLogicalClock.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/debugging.h"
#include "Basics/hashes.h"

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
#include "Random/RandomGenerator.h"
#endif

// can turn this on for more aggressive and expensive consistency checks
// #define PARANOID_TREE_CHECKS

namespace {
static constexpr std::uint8_t CurrentVersion = 0x01;
static constexpr char CompressedSnappyCurrent = '1';
static constexpr char UncompressedCurrent = '2';
static constexpr char CompressedBottomMostCurrent = '3';

template<typename T> T readUInt(char const*& p) noexcept {
  T value;
  memcpy(&value, p, sizeof(T));
  p += sizeof(T);
  return arangodb::basics::littleToHost<T>(value);
};

}  // namespace

namespace arangodb {
namespace containers {

std::uint64_t FnvHashProvider::hash(std::uint64_t input) const {
  return TRI_FnvHashPod(input);
}

template <typename Hasher, std::uint64_t const BranchingBits>
bool MerkleTree<Hasher, BranchingBits>::Node::operator==(Node const& other) const noexcept{
  return ((this->count == other.count) && (this->hash == other.hash));
}

template <typename Hasher, std::uint64_t const BranchingBits>
constexpr std::uint64_t MerkleTree<Hasher, BranchingBits>::nodeCountUpToDepth(
    std::uint64_t maxDepth) noexcept {
  return ((static_cast<std::uint64_t>(1) << (BranchingBits * (maxDepth + 1))) - 1) /
         (BranchingFactor - 1);
}

template <typename Hasher, std::uint64_t const BranchingBits>
constexpr std::uint64_t MerkleTree<Hasher, BranchingBits>::nodeCountAboveDepth(
    std::uint64_t depth) noexcept {
  return ((static_cast<std::uint64_t>(1) << (BranchingBits * depth)) - 1) /
           (BranchingFactor - 1);
}

class TestNodeCountUpToDepth : public MerkleTree<FnvHashProvider, 3> {
  static_assert(nodeCountUpToDepth(0) == 1);
  static_assert(nodeCountUpToDepth(1) == 9);
  static_assert(nodeCountUpToDepth(2) == 73);
  static_assert(nodeCountUpToDepth(3) == 585);
  // ...
  static_assert(nodeCountUpToDepth(10) == 1227133513);
};

template <typename Hasher, std::uint64_t const BranchingBits>
constexpr std::uint64_t MerkleTree<Hasher, BranchingBits>::allocationSize(std::uint64_t maxDepth) noexcept {
  return MetaSize + (NodeSize * nodeCountUpToDepth(maxDepth));
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::uint64_t MerkleTree<Hasher, BranchingBits>::defaultRange(std::uint64_t maxDepth) {
  // start with 64 revisions per leaf; this is arbitrary, but the key is we want
  // to start with a relatively fine-grained tree so we can differentiate well,
  // but we don't want to go so small that we have to resize immediately
  return nodeCountAtDepth(maxDepth) * 64;
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::fromBuffer(std::string_view buffer) {
  if (buffer.size() < sizeof(Meta) + 2) {
    throw std::invalid_argument(
        "Input buffer is too small");
  }

  TRI_ASSERT(buffer.size() > 2);
  std::uint8_t version = static_cast<uint8_t>(buffer[buffer.size() - 1]);

  if (version != ::CurrentVersion) {
    throw std::invalid_argument(
        "Buffer does not contain a properly versioned tree");
  }
  
  if (buffer[buffer.size() - 2] == ::CompressedBottomMostCurrent) {
    // bottom-most level compressed data
    return fromBottomMostCompressed(std::string_view(buffer.data(), buffer.size() - 2));
  } else if (buffer[buffer.size() - 2] == ::CompressedSnappyCurrent) {
    // Snappy-compressed data
    return fromSnappyCompressed(std::string_view(buffer.data(), buffer.size() - 2));
  } else if (buffer[buffer.size() - 2] == ::UncompressedCurrent) {
    // uncompressed data
    return fromUncompressed(std::string_view(buffer.data(), buffer.size() - 2));
  } 
  
  throw std::invalid_argument("unknown tree serialization type");
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::fromUncompressed(std::string_view buffer) {
  if (buffer.size() < sizeof(Meta)) {
    // not enough space to even store the meta info, can't proceed
    return nullptr;
  }

  Meta const& meta = *reinterpret_cast<Meta const*>(buffer.data());
  if (buffer.size() != MetaSize + (NodeSize * nodeCountUpToDepth(meta.maxDepth))) {
    // allocation size doesn't match meta data, can't proceed
    return nullptr;
  }

  return std::unique_ptr<MerkleTree<Hasher, BranchingBits>>(
      new MerkleTree<Hasher, BranchingBits>(buffer));
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::fromSnappyCompressed(std::string_view buffer) {
  size_t length;
  bool canUncompress = snappy::GetUncompressedLength(buffer.data(), buffer.size(), &length);
  if (!canUncompress || length != allocationSize(6)) {
    throw std::invalid_argument(
        "Cannot determine size of Snappy-compressed data.");
  }
  
  auto uncompressed = std::make_unique<uint8_t[]>(length);

  if (!snappy::RawUncompress(buffer.data(), buffer.size(), reinterpret_cast<char*>(uncompressed.get()))) {
    throw std::invalid_argument(
        "Cannot uncompress Snappy-compressed data.");
  }
  
  return std::unique_ptr<MerkleTree<Hasher, BranchingBits>>(
      new MerkleTree<Hasher, BranchingBits>(std::move(uncompressed)));
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::fromBottomMostCompressed(std::string_view buffer) {
  char const* p = buffer.data();
  char const* e = p + buffer.size();

  if (p + sizeof(Meta) >= e) {
    throw std::invalid_argument("invalid compressed tree data");
  }

  std::uint64_t rangeMin = readUInt<uint64_t>(p);
  std::uint64_t rangeMax = readUInt<uint64_t>(p);
  std::uint64_t maxDepth = readUInt<uint64_t>(p);
  std::uint64_t initialRangeMin = readUInt<uint64_t>(p);

  auto tree = std::make_unique<MerkleTree<Hasher, BranchingBits>>(maxDepth,
      rangeMin, rangeMax, initialRangeMin);
  if (tree == nullptr) {
    throw std::invalid_argument("invalid compressed tree data");
  }

  std::uint64_t const offset = nodeCountUpToDepth(maxDepth - 1);

  // rebuild bottom-most level
  while (p + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t) <= e) {
    // enough data to read
    std::uint32_t pos = readUInt<uint32_t>(p);
    std::uint64_t count = readUInt<uint64_t>(p);
    std::uint64_t hash = readUInt<uint64_t>(p);

    Node& node = tree->node(offset + pos);
    node.count = count;
    node.hash = hash;
  }

  if (p != e) {
    throw std::invalid_argument("invalid compressed tree data with overflow values");
  }
  
  tree->completeFromDeepest();

#ifdef PARANOID_TREE_CHECKS
  tree->checkInternalConsistency();
#endif

  return tree;
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::deserialize(velocypack::Slice slice) {
  std::unique_ptr<MerkleTree<Hasher, BranchingBits>> tree;

  if (!slice.isObject()) {
    return tree;
  }

  velocypack::Slice read = slice.get(StaticStrings::RevisionTreeVersion);
  if (!read.isNumber() || read.getNumber<std::uint8_t>() != ::CurrentVersion) {
    return tree;
  }

  read = slice.get(StaticStrings::RevisionTreeBranchingFactor);
  if (!read.isNumber() || read.getNumber<std::uint64_t>() != BranchingFactor) {
    return tree;
  }

  read = slice.get(StaticStrings::RevisionTreeMaxDepth);
  if (!read.isNumber()) {
    return tree;
  }
  std::uint64_t maxDepth = read.getNumber<std::uint64_t>();

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

  read = slice.get(StaticStrings::RevisionTreeInitialRangeMin);
  if (!read.isString()) {
    return tree;
  }
  p = read.getString(l);
  std::uint64_t initialRangeMin = basics::HybridLogicalClock::decodeTimeStamp(p, l);
  if (initialRangeMin == std::numeric_limits<std::uint64_t>::max()) {
    return tree;
  }

  velocypack::Slice nodes = slice.get(StaticStrings::RevisionTreeNodes);
  if (!nodes.isArray() || nodes.length() < nodeCountUpToDepth(maxDepth)) {
    return tree;
  }

  // allocate the tree
  TRI_ASSERT(rangeMin < rangeMax);

  tree = std::make_unique<MerkleTree<Hasher, BranchingBits>>(maxDepth, rangeMin, rangeMax, initialRangeMin);

  std::uint64_t index = 0;
  for (velocypack::Slice nodeSlice : velocypack::ArrayIterator(nodes)) {
    read = nodeSlice.get(StaticStrings::RevisionTreeCount);
    if (!read.isNumber()) {
      tree.reset();
      return tree;
    }
    std::uint64_t count = read.getNumber<std::uint64_t>();

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

    Node& node = tree->node(index);
    node.hash = hash;
    node.count = count;

    ++index;
  }

  return tree;
}

template <typename Hasher, std::uint64_t const BranchingBits>
MerkleTree<Hasher, BranchingBits>::MerkleTree(std::uint64_t maxDepth,
                                              std::uint64_t rangeMin,
                                              std::uint64_t rangeMax,
                                              std::uint64_t initialRangeMin) {
  if (maxDepth < 2) {
    throw std::invalid_argument("Must specify a maxDepth >= 2");
  }
 
  TRI_ASSERT(rangeMax == 0 || rangeMax > rangeMin);

  if (rangeMax == 0) {
    // default value for rangeMax is 0
    rangeMax = rangeMin + defaultRange(maxDepth);
    TRI_ASSERT(rangeMin < rangeMax);
  }

  if (rangeMax <= rangeMin) {
    throw std::invalid_argument("rangeMax must be larger than rangeMin");
  }
 
  if (!NumberUtils::isPowerOf2(rangeMax - rangeMin)) {
    throw std::invalid_argument("Expecting difference between min and max to be power of 2");
  }

  if ((initialRangeMin - rangeMin) % 
      ((rangeMax - rangeMin) / nodeCountAtDepth(maxDepth)) != 0) {
    throw std::invalid_argument("Expecting difference between initial min and min to be divisible by (max-min)/nodeCountAt(maxDepth)");
  }

  TRI_ASSERT(((rangeMax - rangeMin) / nodeCountAtDepth(maxDepth)) * nodeCountAtDepth(maxDepth) ==
             (rangeMax - rangeMin));
  
  // no lock necessary here
  _buffer = std::make_unique<uint8_t[]>(allocationSize(maxDepth));

  new (&meta()) Meta{rangeMin, rangeMax, maxDepth, initialRangeMin};
  
  std::uint64_t const last = nodeCountUpToDepth(maxDepth);
  for (std::uint64_t i = 0; i < last; ++i) {
    new (&node(i)) Node{0, 0};
  }

  TRI_ASSERT(this->node(0).count == 0);
  TRI_ASSERT(this->node(0).hash == 0);

#ifdef PARANOID_TREE_CHECKS
  // checking an empty tree is overyl paranoid and only wastes
  // cycles, so we only do it if absolutely necessary
  checkInternalConsistency();
#endif
}

template <typename Hasher, std::uint64_t const BranchingBits>
MerkleTree<Hasher, BranchingBits>::~MerkleTree() {
  if (!_buffer) {
    return;
  }

  std::uint64_t const last = nodeCountUpToDepth(meta().maxDepth);
  for (std::uint64_t i = 0; i < last; ++i) {
    node(i).~Node();
  }

  meta().~Meta();
}

template <typename Hasher, std::uint64_t const BranchingBits>
MerkleTree<Hasher, BranchingBits>& MerkleTree<Hasher, BranchingBits>::
operator=(std::unique_ptr<MerkleTree<Hasher, BranchingBits>>&& other) {
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

template <typename Hasher, std::uint64_t const BranchingBits>
std::uint64_t MerkleTree<Hasher, BranchingBits>::count() const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  return node(0).count;
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::uint64_t MerkleTree<Hasher, BranchingBits>::rootValue() const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  return node(0).hash;
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::pair<std::uint64_t, std::uint64_t> MerkleTree<Hasher, BranchingBits>::range() const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  return {meta().rangeMin, meta().rangeMax};
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::uint64_t MerkleTree<Hasher, BranchingBits>::maxDepth() const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  return meta().maxDepth;
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::uint64_t MerkleTree<Hasher, BranchingBits>::byteSize() const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  return allocationSize(meta().maxDepth);
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::checkInsertMinMax(std::unique_lock<std::shared_mutex>& guard,
                                                          std::uint64_t minKey,
                                                          std::uint64_t maxKey) {
  if (minKey < meta().rangeMin) {
    // unlock so we can get exclusive access to grow the range
    guard.unlock();
    growLeft(minKey);
    guard.lock();
  }

  if (maxKey >= meta().rangeMax) {
    // unlock so we can get exclusive access to grow the range
    guard.unlock();
    growRight(maxKey);
    guard.lock();
  }
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::insert(std::uint64_t key) {
  std::unique_lock<std::shared_mutex> guard(_bufferLock);

  // may throw if key < rangeMin, or grow the tree if key >= rangeMax
  checkInsertMinMax(guard, key, key);

  modify(key, /*isInsert*/ true);
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::insert(std::vector<std::uint64_t> const& keys) {
  if (keys.size() == 1) {
    // optimization for a single key
    return this->insert(keys[0]);
  }
  
  if (ADB_UNLIKELY(keys.empty())) {
    return;
  }

  std::vector<std::uint64_t> sortedKeys = keys;
  std::sort(sortedKeys.begin(), sortedKeys.end());

  std::uint64_t minKey = sortedKeys[0];
  std::uint64_t maxKey = sortedKeys[sortedKeys.size() - 1];

  std::unique_lock<std::shared_mutex> guard(_bufferLock);
  
  // may throw if minKey < rangeMin, or grow the tree if maxKey >= rangeMax
  checkInsertMinMax(guard, minKey, maxKey);

  modify(sortedKeys, /*isInsert*/ true);
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::remove(std::uint64_t key) {
  std::unique_lock<std::shared_mutex> guard(_bufferLock);

  if (key < meta().rangeMin || key >= meta().rangeMax) {
    throw std::out_of_range("Cannot remove, key out of current range.");
  }

  modify(key, /*isInsert*/ false);
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::remove(std::vector<std::uint64_t> const& keys) {
  if (keys.size() == 1) {
    // optimization for a single key
    return remove(keys[0]);
  }

  if (ADB_UNLIKELY(keys.empty())) {
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

  modify(sortedKeys, /*isInsert*/ false);
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::clear() {
  std::unique_lock<std::shared_mutex> guard(_bufferLock);

  std::uint64_t const last = nodeCountUpToDepth(meta().maxDepth);
  for (std::uint64_t i = 0; i < last; ++i) {
    new (&node(i)) Node{0, 0};
  }
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::checkConsistency() const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);

  checkInternalConsistency();
}

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
// intentionally corrupts the tree. used for testing only
template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::corrupt(std::uint64_t count, std::uint64_t hash) {
  std::unique_lock<std::shared_mutex> guard(_bufferLock);

  Node& node = this->node(0);
  node.count = count;
  node.hash = hash;

  // we also need to corrupt the lowest level, simply because in case the bottom-most
  // level format is used, we will lose all corruption on upper levels

  for (std::uint64_t d = 1; d < meta().maxDepth; ++d) {
    std::uint64_t offset = nodeCountUpToDepth(d); 
    for (std::uint64_t i = 0; i < 4; ++i) {
      std::uint32_t pos = arangodb::RandomGenerator::interval(0, static_cast<uint32_t>(nodeCountAtDepth(d))); 

      Node& node = this->node(offset + pos);
      node.count = arangodb::RandomGenerator::interval(0, UINT32_MAX);
      node.hash = arangodb::RandomGenerator::interval(0, UINT32_MAX);
    }
  }
}
#endif

template <typename Hasher, std::uint64_t const BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>> MerkleTree<Hasher, BranchingBits>::clone() {
  // acquire the read-lock here to protect ourselves from concurrent modifications
  std::shared_lock<std::shared_mutex> guard(_bufferLock);

  return std::unique_ptr<MerkleTree<Hasher, BranchingBits>>(
      new MerkleTree<Hasher, BranchingBits>(*this));
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::vector<std::pair<std::uint64_t, std::uint64_t>> MerkleTree<Hasher, BranchingBits>::diff(
    MerkleTree<Hasher, BranchingBits>& other) {
  std::shared_lock<std::shared_mutex> guard1(_bufferLock);
  std::shared_lock<std::shared_mutex> guard2(other._bufferLock);

  if (this->meta().maxDepth != other.meta().maxDepth) {
    throw std::invalid_argument("Expecting two trees with same depth.");
  }
  std::uint64_t maxDepth = this->meta().maxDepth;

  while (true) {  // left by break
    std::uint64_t width = this->meta().rangeMax - this->meta().rangeMin;
    std::uint64_t widthOther = this->meta().rangeMax - other.meta().rangeMin;
    if (width == widthOther) {
      break;
    }
    if (width < widthOther) {
      // grow this times 2:
      guard1.unlock();
      this->growRight(this->meta().rangeMax);
      guard1.lock();
    } else {
      // grow other times 2:
      guard2.unlock();
      other.growRight(other.meta().rangeMax);
      guard2.lock();
    }
    // loop to repeat, this also helps to make sure someone else didn't
    // grow while we switched between shared/exclusive locks
  }
  // Now both trees have the same width, but they might still have a
  // different rangeMin. However, by invariant 2 we know that their
  // difference is divisible by the number keys in a bucket in the
  // bottommost level. Therefore, we can adjust the rest by shifting
  // by a multiple of a bucket.
  auto tree1 = this;
  auto tree2 = &other;
  if (tree2->meta().rangeMin < tree1->meta().rangeMin) {
    auto dummy = tree1;
    tree1 = tree2;
    tree2 = dummy;
  }
  // Now the rangeMin of tree1 is <= the rangeMin of tree2.

  std::vector<std::pair<std::uint64_t, std::uint64_t>> result;
  std::uint64_t offsetBottom = nodeCountAboveDepth(maxDepth);
  std::uint64_t n = nodeCountAtDepth(maxDepth);

  auto addRange = [&result](std::uint64_t min, std::uint64_t max) {
    if (result.size() == 0) {
      result.push_back(std::make_pair(min, max));
      return;
    }
    if (result.back().second + 1 == min) {
      // Extend range: //
      result.back().second = max;
      return;
    }
    result.push_back(std::make_pair(min, max));
  };

  // First do the stuff tree2 does not even have:
  std::uint64_t keysPerBucket
    = (tree1->meta().rangeMax - tree1->meta().rangeMin) / n;
  std::uint64_t index1 = 0;
  std::uint64_t pos =  tree1->meta().rangeMin;
  for ( ; pos < tree2->meta().rangeMin; pos += keysPerBucket) {
    Node& node1 = tree1->node(offsetBottom + index1);
    if (node1.count != 0) {
      addRange(pos, pos + keysPerBucket - 1);
    }
    ++index1;
  }
  // Now the buckets they both have:
  std::uint64_t index2 = 0;
  TRI_ASSERT(pos == tree2->meta().rangeMin);
  for ( ; pos < tree1->meta().rangeMax; pos += keysPerBucket) {
    Node& node1 = tree1->node(offsetBottom + index1);
    Node& node2 = tree2->node(offsetBottom + index2);
    if (node1.hash != node2.hash || node1.count != node2.count) {
      addRange(pos, pos + keysPerBucket - 1);
    }
    ++index1;
    ++index2;
  }
  // And finally the rest of tree2:
  for ( ; pos < tree2->meta().rangeMax; pos += keysPerBucket) {
    Node& node2 = tree2->node(offsetBottom + index2);
    if (node2.count != 0) {
      addRange(pos, pos + keysPerBucket - 1);
    }
    ++index2;
  }

  return result;
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::string MerkleTree<Hasher, BranchingBits>::toString(bool full) const {
  std::string output;

  std::shared_lock<std::shared_mutex> guard(_bufferLock);

  if (full) {
    output.append("Merkle-tree ");
    output.append("- maxDepth: ");
    output.append(std::to_string(meta().maxDepth));
    output.append(", rangeMin: ");
    output.append(std::to_string(meta().rangeMin));
    output.append(", rangeMax: ");
    output.append(std::to_string(meta().rangeMax));
    output.append(", initialRangeMin: ");
    output.append(std::to_string(meta().initialRangeMin));
    output.append(", count: ");
    output.append(std::to_string(count()));
    output.append(" ");
  }
  output.append("{");
  for (std::uint64_t depth = 0; depth <= meta().maxDepth; ++depth) {
    output.append(std::to_string(depth));
    output.append(": [");
    for (std::uint64_t chunk = 0; chunk < nodeCountAtDepth(depth); ++chunk) {
      std::uint64_t index = this->index(chunk, depth);
      Node const& node = this->node(index);
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

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::serialize(velocypack::Builder& output,
                                                  std::uint64_t maxDepth) const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);

  TRI_ASSERT(output.isEmpty());
  char ridBuffer[arangodb::basics::maxUInt64StringSize];
  std::uint64_t depth = std::min(maxDepth, meta().maxDepth);

  velocypack::ObjectBuilder topLevelGuard(&output);
  output.add(StaticStrings::RevisionTreeVersion, velocypack::Value(::CurrentVersion));
  output.add(StaticStrings::RevisionTreeMaxDepth, velocypack::Value(depth));
  output.add(StaticStrings::RevisionTreeBranchingFactor, velocypack::Value(BranchingFactor));
  output.add(StaticStrings::RevisionTreeRangeMax,
             basics::HybridLogicalClock::encodeTimeStampToValuePair(meta().rangeMax, ridBuffer));
  output.add(StaticStrings::RevisionTreeRangeMin,
             basics::HybridLogicalClock::encodeTimeStampToValuePair(meta().rangeMin, ridBuffer));
  output.add(StaticStrings::RevisionTreeInitialRangeMin,
             basics::HybridLogicalClock::encodeTimeStampToValuePair(meta().initialRangeMin, ridBuffer));
  velocypack::ArrayBuilder nodeArrayGuard(&output, StaticStrings::RevisionTreeNodes);
  for (std::uint64_t index = 0; index < nodeCountUpToDepth(depth); ++index) {
    velocypack::ObjectBuilder nodeGuard(&output);
    Node const& node = this->node(index);
    output.add(StaticStrings::RevisionTreeHash,
               basics::HybridLogicalClock::encodeTimeStampToValuePair(node.hash, ridBuffer));
    output.add(StaticStrings::RevisionTreeCount, velocypack::Value(node.count));
  }
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::serializeBinary(std::string& output,
                                                        bool compress) const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);

  TRI_ASSERT(output.empty());

  char format = ::UncompressedCurrent;
  if (compress) {
    // 15'000 is an arbitrary cutoff value for when to move from
    // bottom-most level compression to full tree compression with Snappy
    if (this->node(0).count <= 15'000) {
      format = ::CompressedBottomMostCurrent;
    } else {
      format = ::CompressedSnappyCurrent;
    }
  }

  TRI_IF_FAILURE("MerkleTree::serializeUncompressed") {
    format = ::UncompressedCurrent;
  }
  TRI_IF_FAILURE("MerkleTree::serializeBottomMost") {
    format = ::CompressedBottomMostCurrent;
  }
  TRI_IF_FAILURE("MerkleTree::serializeSnappy") {
    format = ::CompressedSnappyCurrent;
  }
   
  switch (format) {
    case ::CompressedBottomMostCurrent: {
      storeBottomMostCompressed(output);
      output.push_back(::CompressedBottomMostCurrent);
      break;
    }
    case ::CompressedSnappyCurrent: {
      snappy::Compress(reinterpret_cast<char*>(_buffer.get()),
                       allocationSize(meta().maxDepth), &output);
      output.push_back(::CompressedSnappyCurrent);
      break;
    }
    case ::UncompressedCurrent: {
      output.append(reinterpret_cast<char*>(_buffer.get()), allocationSize(meta().maxDepth));
      output.push_back(::UncompressedCurrent);
      break;
    }
  }

  output.push_back(static_cast<char>(::CurrentVersion));
}

template <typename Hasher, std::uint64_t const BranchingBits>
MerkleTree<Hasher, BranchingBits>::MerkleTree(std::string_view buffer)
    : _buffer(new uint8_t[buffer.size()]) {
  if (buffer.size() < allocationSize(/*minDepth*/ 2)) {
    throw std::invalid_argument("Invalid (too small) buffer size for tree");
  }
  
  // no lock necessary here, as no other thread can see us yet
  std::memcpy(_buffer.get(), buffer.data(), buffer.size());

  if (buffer.size() != allocationSize(meta().maxDepth)) {
    throw std::invalid_argument("Unexpected buffer size for tree");
  }
}

  template <typename Hasher, std::uint64_t const BranchingBits>
MerkleTree<Hasher, BranchingBits>::MerkleTree(std::unique_ptr<uint8_t[]> buffer)
    : _buffer(std::move(buffer)) {}

template <typename Hasher, std::uint64_t const BranchingBits>
MerkleTree<Hasher, BranchingBits>::MerkleTree(MerkleTree<Hasher, BranchingBits> const& other)
    : _buffer(new uint8_t[allocationSize(other.meta().maxDepth)]) {
  // this is a protected constructor, and we get here only via clone().
  // in this case `other`  is already properly locked

  // no lock necessary here for ourselves, as no other thread can see us yet
  new (&meta()) Meta{other.meta().rangeMin, other.meta().rangeMax, other.meta().maxDepth, other.meta().initialRangeMin};

  std::uint64_t const last = nodeCountUpToDepth(meta().maxDepth);
  for (std::uint64_t i = 0; i < last; ++i) {
    new (&this->node(i)) Node{other.node(i).count, other.node(i).hash};
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(meta().maxDepth == other.meta().maxDepth);
  TRI_ASSERT(meta().rangeMin == other.meta().rangeMin);
  TRI_ASSERT(meta().rangeMax == other.meta().rangeMax);
  TRI_ASSERT(meta().initialRangeMin == other.meta().initialRangeMin);
  
#ifdef PARANOID_TREE_CHECKS
  checkInternalConsistency();

  for (std::uint64_t i = 0; i < last; ++i) {
    TRI_ASSERT(this->node(i) == other.node(i));
  }
#endif
#endif
}

template <typename Hasher, std::uint64_t const BranchingBits>
typename MerkleTree<Hasher, BranchingBits>::Meta&
MerkleTree<Hasher, BranchingBits>::meta() const noexcept {
  // not thread-safe, lock buffer from outside
  TRI_ASSERT(_buffer);
  return *reinterpret_cast<Meta*>(_buffer.get());
}

template <typename Hasher, std::uint64_t const BranchingBits>
typename MerkleTree<Hasher, BranchingBits>::Node&
MerkleTree<Hasher, BranchingBits>::node(std::uint64_t index) noexcept {
  // not thread-safe, lock buffer from outside
  TRI_ASSERT(index < nodeCountUpToDepth(meta().maxDepth));
  uint8_t* ptr = _buffer.get() + MetaSize + (NodeSize * index);
  return *reinterpret_cast<Node*>(ptr);
}

template <typename Hasher, std::uint64_t const BranchingBits>
typename MerkleTree<Hasher, BranchingBits>::Node const&
MerkleTree<Hasher, BranchingBits>::node(std::uint64_t index) const noexcept {
  // not thread-safe, lock buffer from outside
  TRI_ASSERT(index < nodeCountUpToDepth(meta().maxDepth));
  uint8_t const* ptr = _buffer.get() + MetaSize + (NodeSize * index);
  return *reinterpret_cast<Node const*>(ptr);
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::uint64_t MerkleTree<Hasher, BranchingBits>::index(std::uint64_t key,
                                                       std::uint64_t depth) const noexcept {
  // not thread-safe, lock buffer from outside
  TRI_ASSERT(depth <= meta().maxDepth);
  TRI_ASSERT(key >= meta().rangeMin);
  TRI_ASSERT(key < meta().rangeMax);

  // special fast case
  if (depth == 0) {
    return 0;
  }

  std::uint64_t offset = key - meta().rangeMin;
  std::uint64_t chunkSizeAtDepth =
      (meta().rangeMax - meta().rangeMin) /
      (static_cast<std::uint64_t>(1) << (BranchingBits * depth));
  std::uint64_t chunk = offset / chunkSizeAtDepth;

  return chunk + nodeCountUpToDepth(depth - 1);
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::modify(std::uint64_t key, bool isInsert) {
  // not thread-safe, shared-lock buffer from outside
  Hasher h;
  std::uint64_t const value = h.hash(key);
  for (std::uint64_t depth = 0; depth <= meta().maxDepth; ++depth) {
    bool success = modifyLocal(depth, key, value, isInsert);
    if (ADB_UNLIKELY(!success)) {
      // roll back the changes we already made, using best effort
      for (std::uint64_t d = 0; d < depth; ++d) {
        [[maybe_unused]] bool rolledBack = modifyLocal(d, key, value, !isInsert);
      }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      checkInternalConsistency();
#endif
      throw std::invalid_argument("Tried to remove key that is not present.");
    }
  }
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::modify(std::vector<std::uint64_t> const& keys,
                                               bool isInsert) {
  // not thread-safe, unique-lock buffer from outside
  Hasher h;
  for (std::uint64_t depth = 0; depth <= meta().maxDepth; ++depth) {
    for (std::uint64_t key : keys) {
      bool success = modifyLocal(depth, key, h.hash(key), isInsert);
      if (ADB_UNLIKELY(!success)) {
        // roll back the changes we already made, using best effort
        for (std::uint64_t d = 0; d <= depth; ++d) {
          for (std::uint64_t k : keys) {
            if (d == depth && k == key) {
              // we didn't make it all the way through at depth d, done
              break;
            }
            [[maybe_unused]] bool rolledBack =
                modifyLocal(d, k, h.hash(k), !isInsert);
          }
        }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        checkInternalConsistency();
#endif
        throw std::invalid_argument("Tried to remove key that is not present.");
      }
    }
  }
}

template <typename Hasher, std::uint64_t const BranchingBits>
bool MerkleTree<Hasher, BranchingBits>::modifyLocal(
    std::uint64_t depth, std::uint64_t key, std::uint64_t value, bool isInsert) {
  // only use via modify
  std::uint64_t index = this->index(key, depth);
  Node& node = this->node(index);
  
  if (isInsert) {
    ++node.count;
  } else {
    if (ADB_UNLIKELY(node.count == 0)) {
      return false;
    }
    --node.count;
  }
  node.hash ^= value;

  return true;
}

// The following method recomputes the tree from its bottommost level.
// This is needed after the new leftCombine2 and rightCombine2 methods.
template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::completeFromDeepest() noexcept {
  // not thread-safe, lock nodes from outside
  auto const maxDepth = meta().maxDepth;
  for (std::uint64_t d = maxDepth - 1; /* d >= 0 */; --d) {
    std::uint64_t offHere = nodeCountAboveDepth(d);  // works for d=0
    auto const n = nodeCountAtDepth(d);
    std::uint64_t offDown = nodeCountUpToDepth(d);
    for (std::uint64_t i = 0; i < n; ++i) {
      std::uint64_t count = 0;
      std::uint64_t hash = 0;
      for (std::uint64_t j = 0; j < BranchingFactor; ++j) {
        Node& src = this->node(offDown++);
        count += src.count;
        hash ^= src.hash;
      }
      Node& dst = this->node(offHere++);
      dst.count = count;
      dst.hash = hash;
    }
    if (d == 0) {
      break;
    }
  }
}

// The following method combines buckets for a growth operation to the
// right. It only does a factor of 2 and potentially a shift by one.
template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::leftCombine2(bool withShift) noexcept {
  // not thread-safe, lock nodes from outside

  // First to maxDepth:
  auto const maxDepth = meta().maxDepth;
  std::uint64_t offset = nodeCountAboveDepth(maxDepth);
  auto const n = nodeCountAtDepth(maxDepth);
  if (withShift) {
    // 0 -> 0
    // 1, 2 -> 1
    // 3, 4 -> 2
    // 5, 6 -> 3
    // ...
    // n-1 -> n/2
    // empty -> n/2+1
    // ...
    // empty -> n
    for (std::uint64_t i = 2; i < n; ++i) {
      Node& src = this->node(offset + i);
      Node& dst = this->node(offset + ((i + 1) / 2));

      TRI_ASSERT(&src != &dst);
      dst.count += src.count;
      dst.hash ^= src.hash;
      src = Node{0, 0};
    }
  } else {
    // 0, 1      -> 0
    // 2, 3      -> 1
    // 4, 5      -> 2
    // ...
    // n-2, n-1  -> n/2 - 1
    // empty     -> n/2
    // ...
    // empty     -> n - 1
    for (std::uint64_t i = 1; i < n; ++i) {
      Node& src = this->node(offset + i);
      Node& dst = this->node(offset + (i / 2));

      TRI_ASSERT(&src != &dst);
      dst.count += src.count;
      dst.hash ^= src.hash;
      src = Node{0, 0};
    }
  }
  completeFromDeepest();
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::growRight(std::uint64_t key) {
  std::unique_lock<std::shared_mutex> guard(_bufferLock);

  std::uint64_t maxDepth = meta().maxDepth;
  std::uint64_t rangeMin = meta().rangeMin;
  std::uint64_t rangeMax = meta().rangeMax;
  std::uint64_t initialRangeMin = meta().initialRangeMin;

  while (key >= rangeMax) {
    // someone else resized already while we were waiting for the lock
    // Furthermore, we can only grow by a factor of 2, so we might have
    // to grow multiple times

    std::uint64_t const width = rangeMax - rangeMin;
    std::uint64_t const keysPerBucket = width / nodeCountAtDepth(maxDepth);

    TRI_ASSERT(rangeMin < rangeMax);

    // First find out if we need to shift or not, this is for the
    // invariant 2:
    bool needToShift
      = (initialRangeMin - rangeMin) % (2 * keysPerBucket) != 0;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    std::uint64_t count = this->node(0).count;
#endif 

    leftCombine2(needToShift);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(count == this->node(0).count);
#endif 

    rangeMax += width;
    if (needToShift) {
      rangeMax -= keysPerBucket;
      rangeMin -= keysPerBucket;
    }
    meta().rangeMax = rangeMax;
    meta().rangeMin = rangeMin;

    TRI_ASSERT(meta().rangeMin < meta().rangeMax);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#ifdef PARANOID_TREE_CHECKS
    checkInternalConsistency();
#endif
#endif
  }
  TRI_ASSERT(key < meta().rangeMax);
}

// The following method combines buckets for a growth operation to the
// left. It only does a factor of 2 and potentially a shift by one.
template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::rightCombine2(bool withShift) noexcept {
  // not thread-safe, lock nodes from outside

  // First to maxDepth:
  auto const maxDepth = meta().maxDepth;
  std::uint64_t offset = nodeCountAboveDepth(maxDepth);
  auto const n = nodeCountAtDepth(maxDepth);
  if (withShift) {
    // empty     -> 0
    // ...
    // empty     -> n/2 - 2
    // 0         -> n/2 - 1
    // 1, 2      -> n/2
    // 3, 4      -> n/2 + 1
    // 5, 6      -> n/2 + 2
    // ...
    // n-3, n-2  -> n - 2
    // n-1       -> n - 1
    for (std::uint64_t i = n - 1; /* i >= 0 */ ; --i) {
      Node& src = this->node(offset + i);
      Node& dst = this->node(offset + (n + i - 1) / 2);

      TRI_ASSERT(&src != &dst);
      dst.count += src.count;
      dst.hash ^= src.hash;
      src = Node{0, 0};
      if (i == 0) {
        break;
      }
    }
  } else {
    // empty     -> 0
    // ...
    // empty     -> n/2 - 1
    // 0, 1      -> n/2
    // 2, 3      -> n/2 + 1
    // 4, 5      -> n/2 + 2
    // ...
    // n-2, n-1  -> n - 1
    for (std::uint64_t i = n - 2; /* i >= 0 */; --i) {
      Node& src = this->node(offset + (std::uint64_t) i);
      Node& dst = this->node(offset + (n + i) / 2);

      TRI_ASSERT(&src != &dst);
      dst.count += src.count;
      dst.hash ^= src.hash;
      src = Node{0, 0};
      if (i == 0) {
        break;
      }
    }
  }
  completeFromDeepest();
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::growLeft(std::uint64_t key) {
  std::unique_lock<std::shared_mutex> guard(_bufferLock);

  std::uint64_t maxDepth = meta().maxDepth;
  std::uint64_t rangeMin = meta().rangeMin;
  std::uint64_t rangeMax = meta().rangeMax;
  std::uint64_t initialRangeMin = meta().initialRangeMin;

  while (key < rangeMin) {
    // someone else resized already while we were waiting for the lock
    // Furthermore, we can only grow by a factor of 2, so we might have
    // to grow multiple times

    std::uint64_t const width = rangeMax - rangeMin;
    std::uint64_t const keysPerBucket = width / nodeCountAtDepth(maxDepth);

    TRI_ASSERT(rangeMin < rangeMax);

    // First find out if we need to shift or not, this is for the
    // invariant 2:
    bool needToShift
      = (initialRangeMin - rangeMin) % (2 * keysPerBucket) != 0;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    std::uint64_t count = this->node(0).count;
#endif 

    rightCombine2(needToShift);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(count == this->node(0).count);
#endif 

    rangeMin -= width;
    if (needToShift) {
      rangeMax += keysPerBucket;
      rangeMin += keysPerBucket;
    }
    meta().rangeMax = rangeMax;
    meta().rangeMin = rangeMin;

    TRI_ASSERT(meta().rangeMin < meta().rangeMax);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#ifdef PARANOID_TREE_CHECKS
    checkInternalConsistency();
#endif
#endif
  }
  TRI_ASSERT(key >= meta().rangeMin);
}

template <typename Hasher, std::uint64_t const BranchingBits>
bool MerkleTree<Hasher, BranchingBits>::equalAtIndex(
    MerkleTree<Hasher, BranchingBits> const& other, std::uint64_t index) const noexcept {
  // not fully thread-safe, lock nodes from outside
  return (this->node(index) == other.node(index));
}

template <typename Hasher, std::uint64_t const BranchingBits>
bool MerkleTree<Hasher, BranchingBits>::childrenAreLeaves(std::uint64_t index) const noexcept {
  // not thread-safe, lock buffer from outside
  std::uint64_t maxDepth = meta().maxDepth;
  return index >= nodeCountUpToDepth(maxDepth - 2) &&
         index < nodeCountUpToDepth(maxDepth - 1);
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::pair<std::uint64_t, std::uint64_t> MerkleTree<Hasher, BranchingBits>::chunkRange(
    std::uint64_t chunk, std::uint64_t depth) const {
  // not thread-safe, lock buffer from outside
  std::uint64_t rangeMin = meta().rangeMin;
  std::uint64_t rangeMax = meta().rangeMax;
  std::uint64_t chunkSizeAtDepth =
      (rangeMax - rangeMin) / (static_cast<std::uint64_t>(1) << (BranchingBits * depth));
  return std::make_pair(rangeMin + (chunkSizeAtDepth * chunk),
                        rangeMin + (chunkSizeAtDepth * (chunk + 1)) - 1);
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::checkInternalConsistency() const {
  // not thread-safe, lock buffer from outside
  if (!_buffer) {
    return;
  }

  // validate meta data
  std::uint64_t maxDepth = meta().maxDepth;
  std::uint64_t rangeMin = meta().rangeMin;
  std::uint64_t rangeMax = meta().rangeMax;
  std::uint64_t initialRangeMin = meta().initialRangeMin;
  
  if (maxDepth < 2) {
    throw std::invalid_argument("Invalid tree maxDepth");
  }

  if (rangeMin >= rangeMax) { // || rangeMin == 0) {
    throw std::invalid_argument("Invalid tree rangeMin / rangeMax");
  }
  if (!NumberUtils::isPowerOf2(rangeMax - rangeMin)) {
    throw std::invalid_argument("Expecting difference between min and max to be power of 2");
  }
  if ((initialRangeMin - rangeMin) % 
      ((rangeMax - rangeMin) / nodeCountAtDepth(maxDepth)) != 0) {
    throw std::invalid_argument("Expecting difference between initial min and min to be divisible by (max-min)/nodeCountAt(maxDepth)");
  }

  std::uint64_t previousCount = this->node(0).count;
  std::uint64_t previousHash = this->node(0).hash;

  for (std::uint64_t d = 1; d <= maxDepth; ++d) {
    std::uint64_t const offset = nodeCountUpToDepth(d - 1);
    std::uint64_t count = 0;
    std::uint64_t hash = 0;

    for (std::uint64_t i = 0; i < nodeCountAtDepth(d); ++i) {
      Node const& n = this->node(offset + i);
      count += n.count;
      hash ^= n.hash;

      TRI_ASSERT(n.count != 0 || n.hash == 0);
    }

    if (count != previousCount) {
      throw std::invalid_argument("Inconsistent count values in tree");
    }
    previousCount = count;
    
    if (hash != previousHash) {
      throw std::invalid_argument("Inconsistent hash values in tree");
    }
  }
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::storeBottomMostCompressed(std::string& output) const {
  // make a minimum allocation that will be enough for the meta data and a 
  // few notes. if we need more space, the string can grow as needed
  TRI_ASSERT(output.empty());
  output.reserve(64);

  // rangeMin / rangeMax / maxDepth
  {
    uint64_t value = basics::hostToLittle(meta().rangeMin);
    output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
    
    value = basics::hostToLittle(meta().rangeMax);
    output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
    
    value = basics::hostToLittle(meta().maxDepth);
    output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));

    value = basics::hostToLittle(meta().initialRangeMin);
    output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
  }
  
  std::uint64_t maxDepth = meta().maxDepth;
  std::uint64_t offset = nodeCountUpToDepth(maxDepth - 1);
  for (std::uint64_t index = 0; index < nodeCountAtDepth(maxDepth); ++index) {
    Node const& src = this->node(offset + index);
    TRI_ASSERT(src.count > 0 || src.hash == 0);

    // serialize only the non-zero buckets
    if (src.count > 0) {
      // index
      {
        uint32_t value = basics::hostToLittle(static_cast<uint32_t>(index));
        output.append(reinterpret_cast<const char*>(&value), sizeof(uint32_t));
      }
      // count / hash
      {
        uint64_t value = basics::hostToLittle(src.count);
        output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
        
        value = basics::hostToLittle(src.hash);
        output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
      }
    }
  }

  TRI_ASSERT(output.size() >= sizeof(Meta));
  TRI_ASSERT((output.size() - sizeof(Meta)) % (sizeof(uint32_t) + sizeof(Node)) == 0);
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::ostream& operator<<(std::ostream& stream,
                         MerkleTree<Hasher, BranchingBits> const& tree) {
  return stream << tree.toString(true);
}

/// INSTANTIATIONS
template class MerkleTree<FnvHashProvider, 3>;
template std::ostream& operator<<(std::ostream& stream, MerkleTree<FnvHashProvider, 3> const&);

}  // namespace containers
}  // namespace arangodb
