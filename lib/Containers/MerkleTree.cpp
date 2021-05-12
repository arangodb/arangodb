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
constexpr std::uint64_t MerkleTree<Hasher, BranchingBits>::log2ceil(std::uint64_t n) noexcept {
  if (n > (std::numeric_limits<std::uint64_t>::max() / 2)) {
    return 8 * sizeof(std::uint64_t) - 1;
  }
  std::uint64_t i = 1;
  for (; (static_cast<std::uint64_t>(1) << i) < n; ++i) {
  }
  return i;
}

class TestLog2Ceil : public MerkleTree<FnvHashProvider, 3> {
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

template <typename Hasher, std::uint64_t const BranchingBits>
constexpr std::uint64_t MerkleTree<Hasher, BranchingBits>::minimumFactorFor(
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
  
  TRI_ASSERT(current != 0);

  std::uint64_t rawFactor = target / current;
  std::uint64_t correctedFactor = static_cast<std::uint64_t>(1)
                                  << log2ceil(rawFactor + 1);  // force power of 2
  TRI_ASSERT(NumberUtils::isPowerOf2(correctedFactor));
  TRI_ASSERT(target >= (current * correctedFactor / 2));
  return correctedFactor;
}

class TestMinimumFactorFor : public MerkleTree<FnvHashProvider, 3> {
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

  if (p + 3 * sizeof(uint64_t) > e) {
    throw std::invalid_argument("invalid compressed tree data");
  }

  std::uint64_t rangeMin = readUInt<uint64_t>(p);
  std::uint64_t rangeMax = readUInt<uint64_t>(p);
  std::uint64_t maxDepth = readUInt<uint64_t>(p);

  auto tree = std::make_unique<MerkleTree<Hasher, BranchingBits>>(maxDepth,
                                                                  rangeMin,
                                                                  rangeMax);
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
  
  // rebuild all levels above from bottom-most data
  for (std::size_t d = maxDepth - 1; /*d >= 0*/; --d) {
    std::uint64_t prevOffset = (d == 0 ? 0 : nodeCountUpToDepth(d - 1));
    std::uint64_t ourOffset = nodeCountUpToDepth(d);
    std::uint64_t const ourEnd = nodeCountUpToDepth(d + 1);

    while (ourOffset < ourEnd) {
      std::uint64_t count = 0;
      std::uint64_t hash = 0;
      for (std::size_t index = 0; index < (1 << BranchingBits); ++index) {
        Node const& src = tree->node(ourOffset++);
        count += src.count;
        hash ^= src.hash;
      }
      Node& dst = tree->node(prevOffset);
      dst.count = count;
      dst.hash = hash;

      ++prevOffset;
    }
    if (d == 0) { 
      break;
    }
  }

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

  velocypack::Slice nodes = slice.get(StaticStrings::RevisionTreeNodes);
  if (!nodes.isArray() || nodes.length() < nodeCountUpToDepth(maxDepth)) {
    return tree;
  }

  // allocate the tree
  TRI_ASSERT(rangeMin < rangeMax);

  tree.reset(new MerkleTree<Hasher, BranchingBits>(maxDepth, rangeMin, rangeMax));

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
                                              std::uint64_t rangeMax) {
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
 
  TRI_ASSERT(((rangeMax - rangeMin) / nodeCountAtDepth(maxDepth)) * nodeCountAtDepth(maxDepth) ==
             (rangeMax - rangeMin));
  
  // no lock necessary here
  _buffer.reset(new uint8_t[allocationSize(maxDepth)]);

  new (&meta()) Meta{rangeMin, rangeMax, maxDepth};
  
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
  std::unique_lock<std::shared_mutex> guard(_bufferLock);

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
      std::uint32_t pos = arangodb::RandomGenerator::interval(0, nodeCountAtDepth(d)); 

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
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::cloneWithDepth(std::uint64_t newDepth) const {
  // arangod does not call this method! That is, because it is rather dangerous:
  // To grow the depth, it has to modify `rangeMax` by multiplying `rangeMax-rangeMin` by
  // the `branchingFactor`. This can quickly lead to integer overflow! Do not use without
  // knowing exactly what you are doing!

  // acquire the read-lock here to protect ourselves from concurrent modifications
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  
  TRI_ASSERT(meta().rangeMin < meta().rangeMax);

  if (newDepth == meta().maxDepth) {
    // exact copy
    return std::unique_ptr<MerkleTree<Hasher, BranchingBits>>(
        new MerkleTree<Hasher, BranchingBits>(*this));
  }

  if (newDepth < meta().maxDepth) {
    // shrinking
    std::unique_ptr<MerkleTree<Hasher, BranchingBits>> newTree =
        std::make_unique<MerkleTree<Hasher, BranchingBits>>(newDepth,
                                                            meta().rangeMin,
                                                            meta().rangeMax);
    for (std::uint64_t index = 0; index < nodeCountUpToDepth(newDepth); ++index) {
      Node const& n = this->node(index);
      Node& m = newTree->node(index);
      m = n;
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    newTree->checkInternalConsistency();
#endif
    return newTree;
  }

  // Otherwise let's grow the tree deeper. We're going to grow one level at a
  // time, recursively. Typically we'll only be requesting to grow one level
  // deeper at a time anyway, and we should very, very rarely be growing more
  // than a couple levels at a time.
  std::uint64_t newRangeMax =
      meta().rangeMin + ((meta().rangeMax - meta().rangeMin) * BranchingFactor);
 
  TRI_ASSERT(newRangeMax > meta().rangeMin);
  TRI_ASSERT(newRangeMax > meta().rangeMax);

  auto newTree = 
      std::make_unique<MerkleTree<Hasher, BranchingBits>>(newDepth,
                                                          meta().rangeMin,
                                                          newRangeMax);
  
  {
    for (std::uint64_t d = 0; d <= meta().maxDepth; ++d) {
      // copy each cell into the same index at the next level of the deeper tree
      std::uint64_t const offset = (d == 0 ? 0 : nodeCountUpToDepth(d - 1));
      std::uint64_t const offsetNew = nodeCountUpToDepth(d);
      for (std::uint64_t i = 0; i < nodeCountAtDepth(d); ++i) {
        Node const& n = this->node(offset + i);
        Node& m = newTree->node(offsetNew + i);
        m = n;
      }
    }
      
    // now copy the root into the root, special case
    Node const& n = this->node(0);
    Node& m = newTree->node(0);
    m = n;
  }

  if (newDepth == meta().maxDepth + 1) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    newTree->checkInternalConsistency();
#endif
    return newTree;
  }

  return newTree->cloneWithDepth(newDepth);
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::vector<std::pair<std::uint64_t, std::uint64_t>> MerkleTree<Hasher, BranchingBits>::diff(
    MerkleTree<Hasher, BranchingBits>& other) {
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

  std::uint64_t const maxDepth = std::min(meta().maxDepth, other.meta().maxDepth);
  std::vector<std::pair<std::uint64_t, std::uint64_t>> result;
  std::queue<std::uint64_t> candidates;
  candidates.emplace(0);

  while (!candidates.empty()) {
    std::uint64_t index = candidates.front();
    candidates.pop();
    if (!equalAtIndex(other, index)) {
      bool leaves = childrenAreLeaves(index) || other.childrenAreLeaves(index);
      for (std::uint64_t child = (BranchingFactor * index) + 1;
           child < (BranchingFactor * (index + 1) + 1); ++child) {
        if (!leaves) {
          // internal children, queue them all up for further investigation
          candidates.emplace(child);
        } else {
          if (!equalAtIndex(other, child)) {
            // actually work with key ranges now
            std::uint64_t chunk = child - nodeCountUpToDepth(maxDepth - 1);
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
std::vector<std::pair<std::uint64_t, std::uint64_t>>
MerkleTree<Hasher, BranchingBits>::partitionKeys(std::uint64_t count) const {
  std::vector<std::pair<std::uint64_t, std::uint64_t>> result;

  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  std::uint64_t remaining = node(0).count;

  if (count <= 1 || remaining == 0) {
    // special cases, just return full range
    result.emplace_back(meta().rangeMin, meta().rangeMax);
    return result;
  }

  std::uint64_t depth = meta().maxDepth;
  std::uint64_t offset = (depth == 0) ? 0 : nodeCountUpToDepth(depth - 1);
  std::uint64_t targetCount = std::max(static_cast<std::uint64_t>(1), remaining / count);
  std::uint64_t rangeStart = meta().rangeMin;
  std::uint64_t rangeCount = 0;
  for (std::uint64_t chunk = 0; chunk < nodeCountAtDepth(depth); ++chunk) {
    if (result.size() == count - 1) {
      // if we are generating the last partition, just fast forward to the last
      // chunk, put everything in
      chunk = nodeCountAtDepth(depth) - 1;
    }
    std::uint64_t index = offset + chunk;
    Node const& node = this->node(index);
    rangeCount += node.count;
    if (rangeCount >= targetCount || chunk == nodeCountAtDepth(depth) - 1) {
      auto [_, rangeEnd] = chunkRange(chunk, depth);
      result.emplace_back(rangeStart, rangeEnd);
      remaining -= rangeCount;
      if (remaining == 0 || result.size() == count) {
        // if we just finished the last partiion, shortcut out
        break;
      }
      rangeCount = 0;
      rangeStart = rangeEnd + 1;
      targetCount = std::max(static_cast<std::uint64_t>(1),
                             remaining / (count - result.size()));
    }
  }

  TRI_ASSERT(result.size() <= count);

  return result;
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
  // this is a protected constructor, and we get here only via clone() or cloneWithDepth().
  // in this case  other  is already properly locked

  // no lock necessary here for ourselves, as no other thread can see us yet
  new (&meta()) Meta{other.meta().rangeMin, other.meta().rangeMax, other.meta().maxDepth};

  std::uint64_t const last = nodeCountUpToDepth(meta().maxDepth);
  for (std::uint64_t i = 0; i < last; ++i) {
    new (&this->node(i)) Node{other.node(i).count, other.node(i).hash};
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(meta().maxDepth == other.meta().maxDepth);
  TRI_ASSERT(meta().rangeMin == other.meta().rangeMin);
  TRI_ASSERT(meta().rangeMax == other.meta().rangeMax);
  
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

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::leftCombine(std::uint64_t factor) noexcept {
  // not fully thread-safe, lock nodes from outside
  for (std::uint64_t depth = 1; depth <= meta().maxDepth; ++depth) {
    // iterate over all nodes and left-combine, (skipping the first, identity)
    std::uint64_t offset = nodeCountUpToDepth(depth - 1);
    for (std::uint64_t index = 1; index < nodeCountAtDepth(depth); ++index) {
      Node& src = this->node(offset + index);
      Node& dst = this->node(offset + (index / factor));

      TRI_ASSERT(&src != &dst);
      dst.count += src.count;
      dst.hash ^= src.hash;
      src = Node{0, 0};
    }
  }
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::grow(std::uint64_t key) {
  std::unique_lock<std::shared_mutex> guard(_bufferLock);

  std::uint64_t rangeMin = meta().rangeMin;
  std::uint64_t rangeMax = meta().rangeMax;

  TRI_ASSERT(rangeMin < rangeMax);

  if (key < rangeMax) {
    // someone else resized already while we were waiting for the lock
    return;
  }

  std::uint64_t factor = minimumFactorFor(rangeMax - rangeMin, key - rangeMin);
  TRI_ASSERT(factor != 0);
  TRI_ASSERT(NumberUtils::isPowerOf2(factor));
  
  if (!NumberUtils::isPowerOf2(factor)) {
    throw std::invalid_argument("Expecting factor to be power of 2");
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  std::uint64_t count = this->node(0).count;
#endif 

  leftCombine(factor);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(count == this->node(0).count);
#endif 

  meta().rangeMax = rangeMin + ((rangeMax - rangeMin) * factor);

  TRI_ASSERT(meta().rangeMin < meta().rangeMax);
  TRI_ASSERT(key < meta().rangeMax);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#ifdef PARANOID_TREE_CHECKS
  checkInternalConsistency();
#endif
#endif
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
  
  if (maxDepth < 2) {
    throw std::invalid_argument("Invalid tree maxDepth");
  }

  if (rangeMin >= rangeMax) { // || rangeMin == 0) {
    throw std::invalid_argument("Invalid tree rangeMin / rangeMax");
  }
  if (!NumberUtils::isPowerOf2(rangeMax - rangeMin)) {
    throw std::invalid_argument("Expecting difference between min and max to be power of 2");
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

  TRI_ASSERT(output.size() >= 3 * sizeof(uint64_t));
  TRI_ASSERT((output.size() - 3 * sizeof(uint64_t)) % (sizeof(uint32_t) + 2 * sizeof(uint64_t)) == 0);
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
