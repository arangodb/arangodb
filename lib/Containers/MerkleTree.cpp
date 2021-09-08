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
//#include <queue>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
//#include <type_traits>
#include <utility>
#include <vector>

#include <snappy.h>

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include "MerkleTree.h"

#include "Basics/Endian.h"
#include "Basics/HybridLogicalClock.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/debugging.h"
#include "Basics/hashes.h"
#include "Containers/MerkleTreeHelpers.h"

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
#include "Random/RandomGenerator.h"
#endif

// can turn this on for more aggressive and expensive consistency checks
// #define PARANOID_TREE_CHECKS

namespace {
static constexpr std::uint8_t CurrentVersion = 0x01;

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
  
static_assert(std::is_pod_v<MerkleTreeBase::Meta>);
    
void MerkleTreeBase::Data::ensureShard(std::uint64_t shard, std::uint64_t shardSize) {
  if (shards.size() <= shard) {
    // allocate a few shards at once
    std::size_t capacity = std::max(std::size_t(shard + 1), std::max(std::size_t(4), shards.size() * 2));
    shards.resize(capacity);
  }
  if (shards[shard] == nullptr) {
    shards[shard] = buildShard(shardSize);
  }
}

/*static*/ MerkleTreeBase::Data::ShardType MerkleTreeBase::Data::buildShard(std::uint64_t shardSize) {
  static_assert(std::is_pod_v<MerkleTreeBase::Node>);
  // note: shardSize is currently passed in bytes, not in number of nodes!
  TRI_ASSERT(shardSize % NodeSize == 0);
  auto p = std::make_unique<Node[]>(shardSize / NodeSize);
  memset(p.get(), 0, shardSize);
  return p;
}
  
/*static*/ MerkleTreeBase::Node const MerkleTreeBase::emptyNode{ 0, 0 };
    
void MerkleTreeBase::Node::toVelocyPack(arangodb::velocypack::Builder& output) const {
  velocypack::ObjectBuilder nodeGuard(&output);
  output.add(StaticStrings::RevisionTreeCount, velocypack::Value(count));
  output.add(StaticStrings::RevisionTreeHash, velocypack::Value(hash)); 
}

bool MerkleTreeBase::Node::operator==(Node const& other) const noexcept{
  return (this->count == other.count) && (this->hash == other.hash);
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::uint64_t MerkleTree<Hasher, BranchingBits>::defaultRange(std::uint64_t depth) {
  // start with 64 revisions per leaf; this is arbitrary, but the key is we want
  // to start with a relatively fine-grained tree so we can differentiate well,
  // but we don't want to go so small that we have to resize immediately
  return nodeCountAtDepth(depth) * 64;
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::uint64_t MerkleTree<Hasher, BranchingBits>::numberOfShards() const noexcept {
  TRI_ASSERT(allocationSize(meta().depth) - MetaSize > 0);
  // we must have at least one shard
  return std::max(std::uint64_t(1), ((allocationSize(meta().depth) - MetaSize) / ShardSize));
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::fromBuffer(std::string_view buffer) {
  if (buffer.size() < MetaSize - sizeof(Meta::Padding) + 2 /*for versioning*/) {
    throw std::invalid_argument(
        "Input buffer is too small");
  }

  TRI_ASSERT(buffer.size() > 2);
  std::uint8_t version = static_cast<uint8_t>(buffer[buffer.size() - 1]);

  if (version != ::CurrentVersion) {
    throw std::invalid_argument(
        "Buffer does not contain a properly versioned tree");
  }

  BinaryFormat format = static_cast<BinaryFormat>(buffer[buffer.size() - 2]);
  
  if (format == BinaryFormat::OnlyPopulated) {
    // Only populated buckets
    return fromOnlyPopulated(std::string_view(buffer.data(), buffer.size() - 2));
  } else if (format == BinaryFormat::CompressedSnappyFull) {
    // Snappy-compressed data (full 4MB)
    return fromSnappyCompressed(std::string_view(buffer.data(), buffer.size() - 2));
  } else if (format == BinaryFormat::Uncompressed) {
    // uncompressed data
    return fromUncompressed(std::string_view(buffer.data(), buffer.size() - 2));
  } else if (format == BinaryFormat::CompressedSnappyLazy) {
    // Snappy-compressed data (only populated shards)
    return fromSnappyLazyCompressed(std::string_view(buffer.data(), buffer.size() - 2));
  } 
  
  throw std::invalid_argument("unknown tree serialization type");
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::fromUncompressed(std::string_view buffer) {
  if (buffer.size() < MetaSize) {
    // not enough space to even store the meta info, can't proceed
    return nullptr;
  }

  Meta const& meta = *reinterpret_cast<Meta const*>(buffer.data());
  if (buffer.size() != MetaSize + (NodeSize * nodeCountAtDepth(meta.depth))) {
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
  if (!canUncompress || length < allocationSize(2)) {
    throw std::invalid_argument(
        "Cannot determine size of Snappy-compressed data.");
  }
  
  // create a buffer large enough to hold the entire uncompressed data.
  // note that this will allocate around 4 MB at once, so it is not very efficient.
  // the Snappy lazy format should be used instead, because it will only use as
  // much memory as is required.
  auto uncompressed = std::make_unique<uint8_t[]>(length);

  if (!snappy::RawUncompress(buffer.data(), buffer.size(), reinterpret_cast<char*>(uncompressed.get()))) {
    throw std::invalid_argument(
        "Cannot uncompress Snappy-compressed data.");
  }
  
  return std::unique_ptr<MerkleTree<Hasher, BranchingBits>>(
      new MerkleTree<Hasher, BranchingBits>(std::string_view(reinterpret_cast<char const*>(uncompressed.get()), length)));
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::fromSnappyLazyCompressed(std::string_view buffer) {
  char const* p = buffer.data();
  char const* e = p + buffer.size();

  if (p + MetaSize - sizeof(Meta::Padding) + sizeof(uint32_t) >= e) {
    throw std::invalid_argument("invalid compressed tree data in buffer (lazy compression)");
  }
  
  Data data;

  data.meta.rangeMin = readUInt<uint64_t>(p);
  data.meta.rangeMax = readUInt<uint64_t>(p);
  data.meta.depth = readUInt<uint64_t>(p);
  data.meta.initialRangeMin = readUInt<uint64_t>(p);

  std::uint64_t summaryCount = readUInt<uint64_t>(p);
  std::uint64_t summaryHash = readUInt<uint64_t>(p);
  data.meta.summary = { summaryCount, summaryHash };

  TRI_ASSERT(p - buffer.data() == MetaSize - sizeof(Meta::Padding));
 
  std::uint32_t numberOfShards = readUInt<uint32_t>(p);
  if (numberOfShards >= 1024) {
    throw std::invalid_argument("invalid compressed shard count in tree data");
  }
  // initialize all shards to nullptrs
  data.shards.resize(numberOfShards);

  char const* compressedData = p + numberOfShards * sizeof(uint32_t);
  for (std::uint64_t i = 0; i < numberOfShards; ++i) {
    TRI_ASSERT(p < e);
    std::uint32_t compressedLength = readUInt<uint32_t>(p);
    if (compressedLength != 0) {
      size_t shardLength = shardSize(data.meta.depth);
      TRI_ASSERT(shardLength % NodeSize == 0);
      // ShardSize may be more than we actually need (if we only have a single
      // shard), but it doesn't matter for correctness here.
      data.shards[i] = MerkleTreeBase::Data::buildShard(shardLength);

      size_t length;
      bool canUncompress = snappy::GetUncompressedLength(compressedData, compressedLength, &length);
      if (!canUncompress || length != shardLength) {
        throw std::invalid_argument(
            "Cannot determine size of Snappy-compressed data.");
      }
      if (!snappy::RawUncompress(compressedData, compressedLength, reinterpret_cast<char*>(data.shards[i].get()))) {
        throw std::invalid_argument(
          "Cannot uncompress Snappy-compressed data.");
      }
      compressedData += compressedLength;
    }
  }
  TRI_ASSERT(compressedData == e);
  
  return std::unique_ptr<MerkleTree<Hasher, BranchingBits>>(
      new MerkleTree<Hasher, BranchingBits>(std::move(data)));
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::fromOnlyPopulated(std::string_view buffer) {
  char const* p = buffer.data();
  char const* e = p + buffer.size();
  
  if (p + MetaSize - sizeof(Meta::Padding) > e) {
    throw std::invalid_argument("invalid compressed tree data in bottommost buffer");
  }

  std::uint64_t rangeMin = readUInt<uint64_t>(p);
  std::uint64_t rangeMax = readUInt<uint64_t>(p);
  std::uint64_t depth = readUInt<uint64_t>(p);
  std::uint64_t initialRangeMin = readUInt<uint64_t>(p);
  std::uint64_t summaryCount = readUInt<uint64_t>(p);
  std::uint64_t summaryHash = readUInt<uint64_t>(p);

  auto tree = std::make_unique<MerkleTree<Hasher, BranchingBits>>(depth,
      rangeMin, rangeMax, initialRangeMin);

  std::uint64_t totalCount = 0;
  std::uint64_t totalHash = 0;

  while (p + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t) <= e) {
    // enough data to read
    std::uint32_t pos = readUInt<uint32_t>(p);
    std::uint64_t count = readUInt<uint64_t>(p);
    std::uint64_t hash = readUInt<uint64_t>(p);

    if (count != 0 || hash != 0) {
      tree->node(pos) = { count, hash };

      totalCount += count;
      totalHash ^= hash;
    }
  }

  if (p != e) {
    throw std::invalid_argument("invalid compressed tree data with overflow values");
  }

  if (summaryCount != totalCount || summaryHash != totalHash) {
    throw std::invalid_argument("invalid compressed tree summary data");
  }

  // write summary node
  Node& summary = tree->meta().summary;
  summary = { totalCount, totalHash };
  
  return tree;
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::deserialize(velocypack::Slice slice) {
  if (!slice.isObject()) {
    return nullptr;
  }

  velocypack::Slice read = slice.get(StaticStrings::RevisionTreeVersion);
  if (!read.isNumber() || read.getNumber<std::uint8_t>() != ::CurrentVersion) {
    return nullptr;
  }

  read = slice.get(StaticStrings::RevisionTreeMaxDepth);
  if (!read.isNumber()) {
    return nullptr;
  }
  std::uint64_t depth = read.getNumber<std::uint64_t>();

  read = slice.get(StaticStrings::RevisionTreeRangeMax);
  if (!read.isString()) {
    return nullptr;
  }
  velocypack::ValueLength l;
  char const* p = read.getString(l);
  std::uint64_t rangeMax = basics::HybridLogicalClock::decodeTimeStamp(p, l);
  if (rangeMax == std::numeric_limits<std::uint64_t>::max()) {
    return nullptr;
  }

  read = slice.get(StaticStrings::RevisionTreeRangeMin);
  if (!read.isString()) {
    return nullptr;
  }
  p = read.getString(l);
  std::uint64_t rangeMin = basics::HybridLogicalClock::decodeTimeStamp(p, l);
  if (rangeMin == std::numeric_limits<std::uint64_t>::max()) {
    return nullptr;
  }

  read = slice.get(StaticStrings::RevisionTreeInitialRangeMin);
  std::uint64_t initialRangeMin = basics::HybridLogicalClock::decodeTimeStamp(read);
  if (initialRangeMin == std::numeric_limits<std::uint64_t>::max()) {
    return nullptr;
  }

  // summary count
  read = slice.get(StaticStrings::RevisionTreeCount);
  if (!read.isNumber()) {
    return nullptr;
  }
  std::uint64_t summaryCount = read.getNumber<std::uint64_t>();
  
  // summary hash
  read = slice.get(StaticStrings::RevisionTreeHash);
  if (!read.isNumber()) {
    return nullptr;
  }
  std::uint64_t summaryHash = read.getNumber<std::uint64_t>();

  velocypack::Slice nodes = slice.get(StaticStrings::RevisionTreeNodes);
  if (!nodes.isArray() || nodes.length() < nodeCountAtDepth(depth)) {
    return nullptr;
  }

  // allocate the tree
  TRI_ASSERT(rangeMin < rangeMax);
  auto tree = std::make_unique<MerkleTree<Hasher, BranchingBits>>(depth, rangeMin, rangeMax, initialRangeMin);

  std::uint64_t totalCount = 0;
  std::uint64_t totalHash = 0;
  std::uint64_t index = 0;
  for (velocypack::Slice nodeSlice : velocypack::ArrayIterator(nodes)) {
    TRI_ASSERT(nodeSlice.isObject());

    arangodb::velocypack::Slice countSlice = nodeSlice.get(StaticStrings::RevisionTreeCount);
    arangodb::velocypack::Slice hashSlice = nodeSlice.get(StaticStrings::RevisionTreeHash);

    // skip any nodes for which there is neither a count nor a hash value
    // present. such nodes will be returned by versions which support the
    // "onlyPopulated" URL parameter for improved efficiency.
    if (!countSlice.isNone() || !hashSlice.isNone()) {
      // if any attribute is present, both need to be numeric!
      if (!countSlice.isNumber() || !hashSlice.isNumber()) {
        return nullptr;
      }
      std::uint64_t count = countSlice.getNumber<std::uint64_t>();
      std::uint64_t hash = hashSlice.getNumber<std::uint64_t>();
      if (count != 0 || hash != 0) {
        tree->node(index) = { count, hash };

        totalCount += count;
        totalHash ^= hash;
      }
    }

    ++index;
  }

  if (totalCount != summaryCount || totalHash != summaryHash) {
    return nullptr;
  }

  tree->meta().summary = { totalCount, totalHash };

  return tree;
}

template <typename Hasher, std::uint64_t const BranchingBits>
MerkleTree<Hasher, BranchingBits>::MerkleTree(std::uint64_t depth,
                                              std::uint64_t rangeMin,
                                              std::uint64_t rangeMax,
                                              std::uint64_t initialRangeMin) {
  if (depth < 2) {
    throw std::invalid_argument("Must specify a depth >= 2");
  }
 
  TRI_ASSERT(rangeMax == 0 || rangeMax > rangeMin);
  if (initialRangeMin == 0) {
    initialRangeMin = rangeMin;
  }
  TRI_ASSERT(rangeMin <= initialRangeMin);

  if (rangeMax == 0) {
    // default value for rangeMax is 0
    rangeMax = rangeMin + defaultRange(depth);
    TRI_ASSERT(rangeMin < rangeMax);
  }

  if (rangeMax <= rangeMin) {
    throw std::invalid_argument("rangeMax must be larger than rangeMin");
  }
  if (!NumberUtils::isPowerOf2(rangeMax - rangeMin)) {
    throw std::invalid_argument("Expecting difference between min and max to be power of 2");
  }
  if (rangeMax - rangeMin < nodeCountAtDepth(depth)) {
    throw std::invalid_argument("Need at least one revision in each bucket in deepest layer");
  }
  TRI_ASSERT(nodeCountAtDepth(depth) > 0);
  TRI_ASSERT(rangeMax - rangeMin != 0);

  if ((initialRangeMin - rangeMin) % 
      ((rangeMax - rangeMin) / nodeCountAtDepth(depth)) != 0) {
    throw std::invalid_argument("Expecting difference between initial min and min to be divisible by (max-min)/nodeCountAt(depth)");
  }

  TRI_ASSERT(((rangeMax - rangeMin) / nodeCountAtDepth(depth)) * nodeCountAtDepth(depth) ==
             (rangeMax - rangeMin));
  
  // no lock necessary here
  _data.meta = Meta{rangeMin, rangeMax, depth, initialRangeMin, /*summary node*/ {0, 0}, /*padding*/ {0, 0}};
  
  TRI_ASSERT(this->meta().summary.count == 0);
  TRI_ASSERT(this->meta().summary.hash == 0);
}

template <typename Hasher, std::uint64_t const BranchingBits>
MerkleTree<Hasher, BranchingBits>::~MerkleTree() = default;

template <typename Hasher, std::uint64_t const BranchingBits>
MerkleTree<Hasher, BranchingBits>& MerkleTree<Hasher, BranchingBits>::
operator=(std::unique_ptr<MerkleTree<Hasher, BranchingBits>>&& other) {
  if (!other) {
    return *this;
  }

  std::unique_lock<std::shared_mutex> guard1(_dataLock);
  std::unique_lock<std::shared_mutex> guard2(other->_dataLock);

  TRI_ASSERT(this->meta().depth == other->meta().depth);

  _data = std::move(other->_data);

  return *this;
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::uint64_t MerkleTree<Hasher, BranchingBits>::count() const {
  std::shared_lock<std::shared_mutex> guard(_dataLock);
  return meta().summary.count;
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::uint64_t MerkleTree<Hasher, BranchingBits>::rootValue() const {
  std::shared_lock<std::shared_mutex> guard(_dataLock);
  return meta().summary.hash;
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::pair<std::uint64_t, std::uint64_t> MerkleTree<Hasher, BranchingBits>::range() const {
  std::shared_lock<std::shared_mutex> guard(_dataLock);
  return {meta().rangeMin, meta().rangeMax};
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::uint64_t MerkleTree<Hasher, BranchingBits>::depth() const {
  std::shared_lock<std::shared_mutex> guard(_dataLock);
  return meta().depth;
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::uint64_t MerkleTree<Hasher, BranchingBits>::byteSize() const {
  std::shared_lock<std::shared_mutex> guard(_dataLock);
  return allocationSize(meta().depth);
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::prepareInsertMinMax(std::unique_lock<std::shared_mutex>& guard,
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
  std::unique_lock<std::shared_mutex> guard(_dataLock);

  // may grow the tree so it can store key
  prepareInsertMinMax(guard, key, key);

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

  std::unique_lock<std::shared_mutex> guard(_dataLock);
  
  // may grow the tree so it can store minKey and MaxKey
  prepareInsertMinMax(guard, minKey, maxKey);

  modify(sortedKeys, /*isInsert*/ true);
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::remove(std::uint64_t key) {
  std::unique_lock<std::shared_mutex> guard(_dataLock);

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

  std::unique_lock<std::shared_mutex> guard(_dataLock);

  if (minKey < meta().rangeMin || maxKey >= meta().rangeMax) {
    throw std::out_of_range("Cannot remove, key out of current range.");
  }

  modify(sortedKeys, /*isInsert*/ false);
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::clear() {
  std::unique_lock<std::shared_mutex> guard(_dataLock);

  _data.clear();
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::checkConsistency() const {
  std::shared_lock<std::shared_mutex> guard(_dataLock);

  checkInternalConsistency();
}

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
// intentionally corrupts the tree. used for testing only
template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::corrupt(std::uint64_t count, std::uint64_t hash) {
  std::unique_lock<std::shared_mutex> guard(_dataLock);

  meta().summary = { count, hash };

  // we also need to corrupt the lowest level, simply because in case the bottom-most
  // level format is used, we will lose all corruption on upper levels

  for (std::uint64_t i = 0; i < 4; ++i) {
    std::uint32_t pos = arangodb::RandomGenerator::interval(0, static_cast<uint32_t>(nodeCountAtDepth(meta().depth))); 

    Node& node = this->node(pos);
    node.count = arangodb::RandomGenerator::interval(int64_t{0}, int64_t{UINT32_MAX});
    node.hash = arangodb::RandomGenerator::interval(int64_t{0}, int64_t{UINT32_MAX});
  }
}
#endif

template <typename Hasher, std::uint64_t const BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>> MerkleTree<Hasher, BranchingBits>::clone() {
  // acquire the read-lock here to protect ourselves from concurrent modifications
  std::shared_lock<std::shared_mutex> guard(_dataLock);

  // cannot use make_unique here as the called ctor is protected
  return std::unique_ptr<MerkleTree<Hasher, BranchingBits>>(new MerkleTree<Hasher, BranchingBits>(*this));
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::vector<std::pair<std::uint64_t, std::uint64_t>> MerkleTree<Hasher, BranchingBits>::diff(
    MerkleTree<Hasher, BranchingBits>& other) {
  std::shared_lock<std::shared_mutex> guard1(_dataLock);
  std::shared_lock<std::shared_mutex> guard2(other._dataLock);

  if (this->meta().depth != other.meta().depth) {
    throw std::invalid_argument("Expecting two trees with same depth.");
  }
  std::uint64_t depth = this->meta().depth;

  while (true) {  // left by break
    std::uint64_t width = this->meta().rangeMax - this->meta().rangeMin;
    std::uint64_t widthOther = other.meta().rangeMax - other.meta().rangeMin;
    if (width == widthOther) {
      break;
    }
    if (width < widthOther) {
      // grow this times 2:
      std::uint64_t rangeMax = this->meta().rangeMax;
      guard1.unlock();
      this->growRight(rangeMax);
      guard1.lock();
    } else {
      // grow other times 2:
      std::uint64_t rangeMax = other.meta().rangeMax;
      guard2.unlock();
      other.growRight(rangeMax);
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
    // swap trees so that tree1 always has an equal or lower rangeMin than tree2.
    auto dummy = tree1;
    tree1 = tree2;
    tree2 = dummy;
  }
  // Now the rangeMin of tree1 is <= the rangeMin of tree2.
  
  TRI_ASSERT(tree1->meta().rangeMin <= tree2->meta().rangeMin);

  std::vector<std::pair<std::uint64_t, std::uint64_t>> result;
  std::uint64_t n = nodeCountAtDepth(depth);

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
    Node const& node1 = tree1->node(index1);
    if (node1.count != 0) {
      addRange(pos, pos + keysPerBucket - 1);
    }
    ++index1;
  }
  // Now the buckets they both have:
  std::uint64_t index2 = 0;
  TRI_ASSERT(pos == tree2->meta().rangeMin);
  for ( ; pos < tree1->meta().rangeMax; pos += keysPerBucket) {
    Node const& node1 = tree1->node(index1);
    Node const& node2 = tree2->node(index2);
    if (node1.hash != node2.hash || node1.count != node2.count) {
      addRange(pos, pos + keysPerBucket - 1);
    }
    ++index1;
    ++index2;
  }
  // And finally the rest of tree2:
  for ( ; pos < tree2->meta().rangeMax; pos += keysPerBucket) {
    Node const& node2 = tree2->node(index2);
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

  std::shared_lock<std::shared_mutex> guard(_dataLock);

  if (full) {
    output.append("Merkle-tree ");
    output.append("- depth: ");
    output.append(std::to_string(meta().depth));
    output.append(", rangeMin: ");
    output.append(std::to_string(meta().rangeMin));
    output.append(", rangeMax: ");
    output.append(std::to_string(meta().rangeMax));
    output.append(", initialRangeMin: ");
    output.append(std::to_string(meta().initialRangeMin));
    output.append(", count: ");
    output.append(std::to_string(meta().summary.count));
    output.append(", hash: ");
    output.append(std::to_string(meta().summary.hash));
    output.append(" ");
  }
  output.append("[");
  
  std::uint64_t last = nodeCountAtDepth(meta().depth); 
  for (std::uint64_t chunk = 0; chunk < last; ++chunk) {
    Node const& node = this->node(chunk);
    output.append("[");
    output.append(std::to_string(node.count));
    output.append(",");
    output.append(std::to_string(node.hash));
    output.append("],");
  }
  output.append("]");
  return output;
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::serialize(velocypack::Builder& output,
                                                  bool onlyPopulated) const {
  // only a minimum allocation here. we may need a lot more
  output.reserve(onlyPopulated ? 1024 : 256 * 1024);
  char ridBuffer[arangodb::basics::maxUInt64StringSize];

  std::shared_lock<std::shared_mutex> guard(_dataLock);

  std::uint64_t depth = meta().depth;

  velocypack::ObjectBuilder topLevelGuard(&output);
  output.add(StaticStrings::RevisionTreeVersion, velocypack::Value(::CurrentVersion));
  output.add(StaticStrings::RevisionTreeMaxDepth, velocypack::Value(depth));
  output.add(StaticStrings::RevisionTreeRangeMax,
             basics::HybridLogicalClock::encodeTimeStampToValuePair(meta().rangeMax, ridBuffer));
  output.add(StaticStrings::RevisionTreeRangeMin,
             basics::HybridLogicalClock::encodeTimeStampToValuePair(meta().rangeMin, ridBuffer));
  output.add(StaticStrings::RevisionTreeInitialRangeMin,
             basics::HybridLogicalClock::encodeTimeStampToValuePair(meta().initialRangeMin, ridBuffer));
  output.add(StaticStrings::RevisionTreeCount, velocypack::Value(meta().summary.count));
  output.add(StaticStrings::RevisionTreeHash, velocypack::Value(meta().summary.hash));
  
  velocypack::ArrayBuilder nodeArrayGuard(&output, StaticStrings::RevisionTreeNodes);
  
  std::uint64_t last = nodeCountAtDepth(depth);
  for (std::uint64_t index = 0; index < last; ++index) {
    Node const& src = this->node(index);
    if (onlyPopulated && src.empty()) {
      // return an empty object. this is still worse than returning nothing at all,
      // but otherwise we would need to return the index number of each populaed node,
      // which is likely more data
      output.openObject();
      output.close();
    } else {
      src.toVelocyPack(output);
    }
  }
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::serializeBinary(std::string& output,
                                                        BinaryFormat format) const {
  TRI_ASSERT(output.empty());

  std::shared_lock<std::shared_mutex> guard(_dataLock);

  if (format == BinaryFormat::Optimal) {
    // 15'000 is an arbitrary cutoff value for when to move from
    // bottom-most level compression to full tree compression with Snappy
    if (meta().summary.count <= 15'000) {
      format = BinaryFormat::OnlyPopulated;
    } else {
      format = BinaryFormat::CompressedSnappyLazy;
    }
  }

  // the following format modifiers are only used for testing via JavaScript
  TRI_IF_FAILURE("MerkleTree::serializeUncompressed") {
    format = BinaryFormat::Uncompressed;
  }
  TRI_IF_FAILURE("MerkleTree::serializeOnlyPopulated") {
    format = BinaryFormat::OnlyPopulated;
  }
  TRI_IF_FAILURE("MerkleTree::serializeSnappy") {
    format = BinaryFormat::CompressedSnappyFull;
  }
  TRI_IF_FAILURE("MerkleTree::serializeSnappyLazy") {
    format = BinaryFormat::CompressedSnappyLazy;
  }

  TRI_ASSERT(format != BinaryFormat::Optimal);
   
  switch (format) {
    case BinaryFormat::OnlyPopulated: {
      // make a minimum allocation that will be enough for the meta data and a 
      // few notes. if we need more space, the string can grow as needed
      output.reserve(64);
      serializeMeta(output, /*usePadding*/ false);
      serializeNodes(output, /*all*/ false);
      break;
    }
    case BinaryFormat::CompressedSnappyFull: {
      output.reserve(16384);
      arangodb::containers::helpers::MerkleTreeSnappySource source(numberOfShards(), allocationSize(meta().depth), _data);
      arangodb::containers::helpers::SnappyStringAppendSink sink(output);
      snappy::Compress(&source, &sink);
      break;
    }
    case BinaryFormat::CompressedSnappyLazy: {
      output.reserve(4096);
      serializeMeta(output, /*usePadding*/ false);
      // number of shards
      uint32_t value = basics::hostToLittle(static_cast<uint32_t>(numberOfShards()));
      output.append(reinterpret_cast<const char*>(&value), sizeof(uint32_t));
      
      size_t const tableBase = output.size();
      TRI_ASSERT(tableBase == MetaSize - sizeof(Meta::Padding) + sizeof(uint32_t));

      // for each shard, write a placeholder for the compressed length.
      // the placeholders are initially filled with 0 bytes lengths for each shards.
      // these values will be overridden later for the shards that actually contain
      // data. for fully empty shards, the 0 values will remain in place.
      for (std::uint64_t i = 0; i < numberOfShards(); ++i) {
        uint32_t value = basics::hostToLittle(static_cast<uint32_t>(0));
        output.append(reinterpret_cast<const char*>(&value), sizeof(uint32_t));
      }

      TRI_ASSERT(output.size() == MetaSize - sizeof(Meta::Padding) + sizeof(uint32_t) + numberOfShards() * sizeof(uint32_t));
      arangodb::containers::helpers::SnappyStringAppendSink sink(output);
      for (std::uint64_t i = 0; i < numberOfShards(); ++i) {
        size_t compressedLength = 0;
        bool shardSet = (_data.shards.size() > i && _data.shards[i] != nullptr);
        if (shardSet) {
          snappy::ByteArraySource source(reinterpret_cast<char*>(_data.shards[i].get()), shardSize(meta().depth));
          size_t previousLength = output.size();
          snappy::Compress(&source, &sink);
          compressedLength = output.size() - previousLength;
        }
        uint32_t value = basics::hostToLittle(static_cast<uint32_t>(compressedLength));
        // patch offsets table with compressed length
        memcpy(&output[tableBase + i * sizeof(uint32_t)], &value, sizeof(value));
      }
      break;
    }
    case BinaryFormat::Uncompressed: {
      output.reserve(allocationSize(meta().depth) + 1);
      serializeMeta(output, /*usePadding*/ true);
      serializeNodes(output, /*all*/ true);
      break;
    }
    default: {
      TRI_ASSERT(false);
      throw std::invalid_argument("Invalid binary format");
    }
  }

  // append format and version
  output.push_back(static_cast<char>(format));
  output.push_back(static_cast<char>(::CurrentVersion));
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::vector<std::pair<std::uint64_t, std::uint64_t>>
MerkleTree<Hasher, BranchingBits>::partitionKeys(std::uint64_t count) const {
  std::vector<std::pair<std::uint64_t, std::uint64_t>> result;

  std::shared_lock<std::shared_mutex> guard(_dataLock);
  std::uint64_t remaining = meta().summary.count;

  if (count <= 1 || remaining == 0) {
    // special cases, just return full range
    result.emplace_back(meta().rangeMin, meta().rangeMax);
    return result;
  }

  std::uint64_t depth = meta().depth;
  std::uint64_t targetCount = std::max(static_cast<std::uint64_t>(1), remaining / count);
  std::uint64_t rangeStart = meta().rangeMin;
  std::uint64_t rangeCount = 0;
  std::uint64_t last = nodeCountAtDepth(depth);
  for (std::uint64_t chunk = 0; chunk < last; ++chunk) {
    if (result.size() == count - 1) {
      // if we are generating the last partition, just fast forward to the last
      // chunk, put everything in
      chunk = last - 1;
    }
    std::uint64_t index = chunk;
    Node const& node = this->node(index);
    rangeCount += node.count;
    if (rangeCount >= targetCount || chunk == last - 1) {
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
MerkleTree<Hasher, BranchingBits>::MerkleTree(std::string_view buffer) {
  if (buffer.size() < allocationSize(/*minDepth*/ 2)) {
    throw std::invalid_argument("Invalid (too small) buffer size for tree");
  }
  
  // no lock necessary here, as no other thread can see us yet
  Meta const* m = reinterpret_cast<Meta const*>(buffer.data());
  _data.meta = *m;
  // clear padded data
  _data.meta.padding = { 0, 0 }; 

  if (buffer.size() != allocationSize(meta().depth)) {
    throw std::invalid_argument("Unexpected buffer size for tree");
  }

  char const* p = buffer.data() + MetaSize;
  char const* e = buffer.data() + buffer.size();
  std::uint64_t i = 0;
  while (p < e) {
    Node const* n = reinterpret_cast<Node const*>(p);
    if (n->count != 0 || n->hash != 0) {
      // only store nodes that are non-empty! this is an optimization so
      // we only need to allocate backing memory for shards that are
      // actually populated!
      node(i) = *n;
    }
    p += NodeSize;
    ++i;
  }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#ifdef PARANOID_TREE_CHECKS
  checkInternalConsistency();
#endif
#endif
}

template <typename Hasher, std::uint64_t const BranchingBits>
MerkleTree<Hasher, BranchingBits>::MerkleTree(Data&& data) {
  _data = std::move(data);
}

template <typename Hasher, std::uint64_t const BranchingBits>
MerkleTree<Hasher, BranchingBits>::MerkleTree(MerkleTree<Hasher, BranchingBits> const& other) {
  // this is a protected constructor, and we get here only via clone().
  // in this case `other` is already properly locked
  
  // no lock necessary here for ourselves, as no other thread can see us yet
  _data.meta = Meta{other.meta().rangeMin, 
                    other.meta().rangeMax, 
                    other.meta().depth, 
                    other.meta().initialRangeMin, 
                    other.meta().summary, 
                    /*padding*/ {0, 0}};

  std::uint64_t nodesPerShard = shardSize(_data.meta.depth) / NodeSize;

  _data.shards.reserve(other._data.shards.size());
  std::uint64_t offset = 0;
  for (auto const& it : other._data.shards) {
    if (it != nullptr) {
      // only copy nodes from shards which are actually populated
      for (std::uint64_t i = 0; i < nodesPerShard; ++i) {
        if (!other.empty(offset + i)) {
          node(offset + i) = other.node(offset + i);
        }
      }
    }
    offset += nodesPerShard;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(meta().depth == other.meta().depth);
  TRI_ASSERT(meta().rangeMin == other.meta().rangeMin);
  TRI_ASSERT(meta().rangeMax == other.meta().rangeMax);
  TRI_ASSERT(meta().initialRangeMin == other.meta().initialRangeMin);
  TRI_ASSERT(meta().summary == other.meta().summary);
  
#ifdef PARANOID_TREE_CHECKS
  std::uint64_t last = nodeCountAtDepth(meta().depth);
  for (std::uint64_t i = 0; i < last; ++i) {
    TRI_ASSERT(this->node(i) == other.node(i));
  }
#endif
#endif
}

template <typename Hasher, std::uint64_t const BranchingBits>
typename MerkleTree<Hasher, BranchingBits>::Meta&
MerkleTree<Hasher, BranchingBits>::meta() noexcept {
  // not thread-safe, lock buffer from outside
  return _data.meta;
}

template <typename Hasher, std::uint64_t const BranchingBits>
MerkleTreeBase::Meta const& MerkleTree<Hasher, BranchingBits>::meta() const noexcept {
  // not thread-safe, lock buffer from outside
  return _data.meta;
}

template <typename Hasher, std::uint64_t const BranchingBits>
MerkleTreeBase::Node& MerkleTree<Hasher, BranchingBits>::node(std::uint64_t index) {
  // not thread-safe, lock buffer from outside
  
  // determine shard
  std::uint64_t shard = shardForIndex(index);
  TRI_ASSERT(shard < numberOfShards());

  // lazily allocate backing memory
  _data.ensureShard(shard, ShardSize);

  TRI_ASSERT(_data.shards[shard] != nullptr);
  TRI_ASSERT(index < nodeCountAtDepth(meta().depth));
  TRI_ASSERT(NodeSize * (index - shardBaseIndex(shard) + 1) <= ShardSize);
  return *(_data.shards[shard].get() + (index - shardBaseIndex(shard)));
}

template <typename Hasher, std::uint64_t const BranchingBits>
MerkleTreeBase::Node const& MerkleTree<Hasher, BranchingBits>::node(std::uint64_t index) const noexcept {
  // not thread-safe, lock buffer from outside
  TRI_ASSERT(index < nodeCountAtDepth(meta().depth));
  std::uint64_t shard = shardForIndex(index);
  if (_data.shards.size() <= shard || _data.shards[shard] == nullptr) {
    // return a shared empty node
    TRI_ASSERT(MerkleTreeBase::emptyNode.empty());
    return MerkleTreeBase::emptyNode;
  }
  return *(_data.shards[shard].get() + (index - shardBaseIndex(shard)));
}

void MerkleTreeBase::Meta::serialize(std::string& output, bool addPadding) const {
  // rangeMin / rangeMax / depth / initialRangeMin
  uint64_t value = basics::hostToLittle(rangeMin);
  output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
  
  value = basics::hostToLittle(rangeMax);
  output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
  
  value = basics::hostToLittle(depth);
  output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));

  value = basics::hostToLittle(initialRangeMin);
  output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
  
  // summary node count
  value = basics::hostToLittle(summary.count);
  output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
  
  // summary node hash
  value = basics::hostToLittle(summary.hash);
  output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));

  if (addPadding) {
    value = 0;
    output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
    output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
  }
}

template <typename Hasher, std::uint64_t const BranchingBits>
bool MerkleTree<Hasher, BranchingBits>::empty(std::uint64_t index) const noexcept {
  // not thread-safe, lock buffer from outside
  return this->node(index).empty();
}

template <typename Hasher, std::uint64_t const BranchingBits>
std::uint64_t MerkleTree<Hasher, BranchingBits>::index(std::uint64_t key) const noexcept {
  // not thread-safe, lock buffer from outside
  TRI_ASSERT(key >= meta().rangeMin);
  TRI_ASSERT(key < meta().rangeMax);

  std::uint64_t offset = key - meta().rangeMin;
  std::uint64_t chunkSizeAtDepth =
      (meta().rangeMax - meta().rangeMin) /
      (static_cast<std::uint64_t>(1) << (BranchingBits * meta().depth));
  std::uint64_t chunk = offset / chunkSizeAtDepth;

  return chunk;
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::modify(std::uint64_t key, bool isInsert) {
  // not thread-safe, shared-lock buffer from outside
  Hasher h;
  std::uint64_t const value = h.hash(key);
 
  // adjust bucket node
  bool success = modifyLocal(key, value, isInsert);
  if (ADB_UNLIKELY(!success)) {
    throw std::invalid_argument("Tried to remove key that is not present.");
  }
    
  // adjust summary node
  modifyLocal(meta().summary, 1, value, isInsert);
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::modify(std::vector<std::uint64_t> const& keys,
                                               bool isInsert) {
  // not thread-safe, unique-lock buffer from outside
  Hasher h;
  std::uint64_t totalCount = 0;
  std::uint64_t totalHash = 0;
  for (std::uint64_t key : keys) {
    std::uint64_t value = h.hash(key);
    bool success = modifyLocal(key, value, isInsert);
    if (ADB_UNLIKELY(!success)) {
      // roll back the changes we already made, using best effort
      for (std::uint64_t k : keys) {
        if (k == key) {
          break;
        }
        [[maybe_unused]] bool rolledBack = modifyLocal(k, h.hash(k), !isInsert);
      }
      throw std::invalid_argument("Tried to remove key that is not present.");
    }
    ++totalCount;
    totalHash ^= value;
  }
  
  // adjust summary node
  bool success = modifyLocal(meta().summary, totalCount, totalHash, isInsert);
  TRI_ASSERT(success);
}

template <typename Hasher, std::uint64_t const BranchingBits>
bool MerkleTree<Hasher, BranchingBits>::modifyLocal(
    Node& node, std::uint64_t count, std::uint64_t value, bool isInsert) noexcept {
  // only use via modify
  if (isInsert) {
    node.count += count;
  } else {
    if (ADB_UNLIKELY(node.count < count)) {
      return false;
    }
    node.count -= count;
  }
  node.hash ^= value;

  return true;
}

template <typename Hasher, std::uint64_t const BranchingBits>
bool MerkleTree<Hasher, BranchingBits>::modifyLocal(
    std::uint64_t key, std::uint64_t value, bool isInsert) {
  // only use via modify
  std::uint64_t index = this->index(key);
  return modifyLocal(this->node(index), 1, value, isInsert);
}

// The following method combines buckets for a growth operation to the
// right. It only does a factor of 2 and potentially a shift by one.
template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::leftCombine(bool withShift) {
  // not thread-safe, lock nodes from outside

  // First to depth:
  auto const depth = meta().depth;
  auto const n = nodeCountAtDepth(depth);
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
      if (!this->empty(i)) {
        // Please note: If both src Nodes are empty for some dst,
        // nothing is combined. This is OK, for the following reason:
        // The respective dst Node was a src in a previous loop
        // iteration. Either it was empty then, then it can stay empty
        // in this case. If it wasn't empty, then the assignmend
        // `src // = { 0, 0 };` at the end of the loop has erased it, so
        // it is empty now, also good.
        Node& src = this->node(i);
        Node& dst = this->node((i + 1) / 2);

        TRI_ASSERT(&src != &dst);
        dst.count += src.count;
        dst.hash ^= src.hash;
        // clear source node
        src = {0, 0};
      }
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
      if (!this->empty(i)) {
        // Please note: If both src Nodes are empty for some dst,
        // nothing is combined. This is OK, for the following reason:
        // The respective dst Node was a src in a previous loop
        // iteration. Either it was empty then, then it can stay empty
        // in this case. If it wasn't empty, then the assignmend
        // `src // = { 0, 0 };` at the end of the loop has erased it, so
        // it is empty now, also good.
        Node& src = this->node(i);
        Node& dst = this->node(i / 2);

        TRI_ASSERT(i % 2 != 0 || (dst.count == 0 && dst.hash == 0));
     
        TRI_ASSERT(&src != &dst);
        dst.count += src.count;
        dst.hash ^= src.hash;
        // clear source node
        src = {0, 0};
      }
    }
  }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#ifdef PARANOID_TREE_CHECKS
  checkInternalConsistency();
#endif
#endif
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::growRight(std::uint64_t key) {
  std::unique_lock<std::shared_mutex> guard(_dataLock);

  std::uint64_t depth = meta().depth;
  std::uint64_t rangeMin = meta().rangeMin;
  std::uint64_t rangeMax = meta().rangeMax;
  std::uint64_t initialRangeMin = meta().initialRangeMin;

  TRI_ASSERT(rangeMax > rangeMin);
  TRI_ASSERT(rangeMin <= initialRangeMin);

  while (key >= rangeMax) {
    // someone else resized already while we were waiting for the lock
    // Furthermore, we can only grow by a factor of 2, so we might have
    // to grow multiple times

    TRI_ASSERT(rangeMin < rangeMax);
    std::uint64_t const width = rangeMax - rangeMin;
    TRI_ASSERT(NumberUtils::isPowerOf2(width));

    if (width > std::numeric_limits<uint64_t>::max() - rangeMax) {
      // Oh dear, this would lead to overflow of uint64_t in rangeMax,
      // throw up our hands in despair:
      throw std::out_of_range("Cannot grow MerkleTree because of overflow in rangeMax.");
    }
    std::uint64_t const keysPerBucket = width / nodeCountAtDepth(depth);

    // First find out if we need to shift or not, this is for the
    // invariant 2:
    bool needToShift
      = (initialRangeMin - rangeMin) % (2 * keysPerBucket) != 0;
    
    leftCombine(needToShift);

    rangeMax += width;
    if (needToShift) {
      rangeMax -= keysPerBucket;
      rangeMin -= keysPerBucket;
    }
 
    TRI_ASSERT(rangeMax > rangeMin);
    TRI_ASSERT(NumberUtils::isPowerOf2(rangeMax - rangeMin));
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
void MerkleTree<Hasher, BranchingBits>::rightCombine(bool withShift) {
  // not thread-safe, lock nodes from outside

  // First to depth:
  auto const depth = meta().depth;
  auto const n = nodeCountAtDepth(depth);
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
    for (std::uint64_t i = n - 3; /* i >= 0 */ ; --i) {
      if (!this->empty(i)) {
        // Please note: If both src Nodes are empty for some dst,
        // nothing is combined. This is OK, for the following reason:
        // The respective dst Node was a src in a previous loop
        // iteration. Either it was empty then, then it can stay empty
        // in this case. If it wasn't empty, then the assignmend
        // `src // = { 0, 0 };` at the end of the loop has erased it, so
        // it is empty now, also good.
        Node& src = this->node(i);
        Node& dst = this->node((n + i - 1) / 2);

        TRI_ASSERT(&src != &dst);
        dst.count += src.count;
        dst.hash ^= src.hash;
        // clear source node
        src = { 0, 0 };
      }
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
      if (!this->empty(i)) {
        // Please note: If both src Nodes are empty for some dst,
        // nothing is combined. This is OK, for the following reason:
        // The respective dst Node was a src in a previous loop
        // iteration. Either it was empty then, then it can stay empty
        // in this case. If it wasn't empty, then the assignmend
        // `src // = { 0, 0 };` at the end of the loop has erased it, so
        // it is empty now, also good.
        Node& src = this->node(i);
        Node& dst = this->node((n + i) / 2);

        TRI_ASSERT(&src != &dst);
        dst.count += src.count;
        dst.hash ^= src.hash;
        // clear source node
        src = { 0, 0 };
      }
      if (i == 0) {
        break;
      }
    }
  }
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::growLeft(std::uint64_t key) {
  std::unique_lock<std::shared_mutex> guard(_dataLock);

  std::uint64_t depth = meta().depth;
  std::uint64_t rangeMin = meta().rangeMin;
  std::uint64_t rangeMax = meta().rangeMax;
  std::uint64_t initialRangeMin = meta().initialRangeMin;

  while (key < rangeMin) {
    // someone else resized already while we were waiting for the lock
    // Furthermore, we can only grow by a factor of 2, so we might have
    // to grow multiple times

    std::uint64_t const width = rangeMax - rangeMin;
    std::uint64_t const keysPerBucket = width / nodeCountAtDepth(depth);

    if (width > rangeMin) {
      // Oh dear, this would lead to underflow of uint64_t in rangeMin,
      // throw up our hands in despair:
      throw std::out_of_range("Cannot grow MerkleTree because of underflow in rangeMin.");
    }

    TRI_ASSERT(rangeMin < rangeMax);

    // First find out if we need to shift or not, this is for the
    // invariant 2:
    bool needToShift
      = (initialRangeMin - rangeMin) % (2 * keysPerBucket) != 0;

    rightCombine(needToShift);

    TRI_ASSERT(rangeMin >= width);

    rangeMin -= width;
    if (needToShift) {
      rangeMax += keysPerBucket;
      rangeMin += keysPerBucket;
    }
    TRI_ASSERT(rangeMax > rangeMin);
    TRI_ASSERT(NumberUtils::isPowerOf2(rangeMax - rangeMin));
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
  TRI_IF_FAILURE("MerkleTree::skipConsistencyCheck") {
    return;
  }

  // not thread-safe, lock buffer from outside

  // validate meta data
  std::uint64_t depth = meta().depth;
  std::uint64_t rangeMin = meta().rangeMin;
  std::uint64_t rangeMax = meta().rangeMax;
  std::uint64_t initialRangeMin = meta().initialRangeMin;
  
  if (depth < 2) {
    throw std::invalid_argument("Invalid tree depth");
  }
  if (rangeMin >= rangeMax) { 
    throw std::invalid_argument("Invalid tree rangeMin / rangeMax");
  }
  if (!NumberUtils::isPowerOf2(rangeMax - rangeMin)) {
    throw std::invalid_argument("Expecting difference between min and max to be power of 2");
  }
  if ((initialRangeMin - rangeMin) % 
      ((rangeMax - rangeMin) / nodeCountAtDepth(depth)) != 0) {
    throw std::invalid_argument("Expecting difference between initial min and min to be divisible by (max-min)/nodeCountAt(depth)");
  }

  std::uint64_t totalCount = 0;
  std::uint64_t totalHash = 0;
  std::uint64_t last = nodeCountAtDepth(depth);
  for (std::uint64_t i = 0; i < last; ++i) {
    Node const& n = this->node(i);
    TRI_ASSERT(n.count != 0 || n.hash == 0);
    totalCount += n.count;
    totalHash ^= n.hash;
  }

  if (totalCount != meta().summary.count) {
    throw std::invalid_argument("Inconsistent count values in tree");
  }
    
  if (totalHash != meta().summary.hash) {
    throw std::invalid_argument("Inconsistent hash values in tree");
  }
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::serializeMeta(std::string& output, bool addPadding) const {
  TRI_ASSERT(output.empty());
  meta().serialize(output, addPadding);
  TRI_ASSERT(output.size() == MetaSize - (addPadding ? 0 : sizeof(Meta::Padding)));
}

template <typename Hasher, std::uint64_t const BranchingBits>
void MerkleTree<Hasher, BranchingBits>::serializeNodes(std::string& output, bool all) const {
  std::uint64_t depth = meta().depth;
  std::uint64_t last = nodeCountAtDepth(depth);
  for (std::uint64_t index = 0; index < last; ++index) {
    Node const& src = this->node(index);
    TRI_ASSERT(src.count > 0 || src.hash == 0);

    // serialize only the non-zero buckets
    if (src.count == 0 && !all) {
      continue;
    }

    if (!all) {
      // index
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
