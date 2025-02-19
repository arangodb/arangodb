////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace containers {

class HashProvider {
 public:
  virtual ~HashProvider() = default;
  virtual std::uint64_t hash(std::uint64_t input) const = 0;
};

class FnvHashProvider : public HashProvider {
 public:
  std::uint64_t hash(std::uint64_t input) const override;
};

class MerkleTreeBase {
 public:
  enum class BinaryFormat : char {
    // Snappy-compressed full data (all buckets, even if empty)
    CompressedSnappyFull = '1',
    // Uncompressed data (all buckets, only use for testing!)
    Uncompressed = '2',
    // Only contains non-empty buckets (efficient for sparse trees)
    OnlyPopulated = '3',
    // Snappy-compressed data of populated buckets
    CompressedSnappyLazy = '4',

    // placeholder for optimal format, will determine the "best"
    // format automatically, based on heuristics
    Optimal = 'z',
  };

  struct Node {
    std::uint64_t count;
    std::uint64_t hash;

    void toVelocyPack(arangodb::velocypack::Builder& output) const;

    bool empty() const noexcept { return count == 0 && hash == 0; }

    bool operator==(Node const& other) const noexcept;
  };
  static constexpr std::uint64_t NodeSize = sizeof(Node);
  static_assert(NodeSize == 16, "Node size assumptions invalid.");

  // an empty dummy node with count=0, hash=0, shared and read-only
  static Node const emptyNode;

  struct alignas(64) Meta {
    std::uint64_t rangeMin;
    std::uint64_t rangeMax;
    std::uint64_t depth;
    std::uint64_t initialRangeMin;
    Node summary;

    // used for older versions. unfortunately needed
    struct Padding {
      std::uint64_t p0;
      std::uint64_t p1;
    } padding;

    void serialize(std::string& output, bool addPadding) const;
  };

  static_assert(sizeof(Meta) == 64, "Meta size assumptions invalid.");
  static_assert(sizeof(Meta::Padding) == 16,
                "Meta padding size assumptions invalid.");
  static constexpr std::uint64_t MetaSize = sizeof(Meta);

  // size of each shard, in bytes.
  // note: trees with a small depth may only have a single shard which is
  // smaller than this value
  static constexpr std::uint64_t ShardSize = (1 << 16);

  struct Data {
    using ShardType = std::unique_ptr<Node[]>;

    Meta meta;
    std::vector<ShardType> shards;
    std::size_t memoryUsage = 0;

    void clear() {
      shards.clear();
      memoryUsage = 0;
      meta.summary = {0, 0};
    }

    void ensureShard(std::uint64_t shard, std::uint64_t shardSize);

    static ShardType buildShard(std::uint64_t shardSize);
  };
};

template<typename Hasher,
         std::uint64_t const BranchingBits = 3  // 8 children per internal node,
         >
class MerkleTree : public MerkleTreeBase {
  // A MerkleTree has three parameters which define its semantics:
  //  - rangeMin: lower bound (inclusive) for _rev values it can take
  //  - rangeMax: upper bound (exclusive) for _rev values it can take
  //  - depth: depth of the tree (root plus so many levels below)
  // We call rangeMin-rangeMax the "width" of the tree.
  //
  // We do no longer grow trees in depth, since this would mean a complete
  // rehash or a large growth of width, which can lead to integer overflow,
  // which we must avoid.
  //
  // However, we do grow the width of a tree as needed. Originally, we
  // only grew to the right and rangeMin was constant over the lifetime
  // of the tree. This turned out to be not good enough, since we cannot
  // estimate the smallest _rev value a collection will ever receive
  // (for example, in DC2DC we might need to replicate old data into a
  // newly created collection). Therefore, we can now also grow the width
  // to the left by decreasing rangeMin.
  //
  // Unfortunately, we can only compare two different MerkleTrees, if
  // the difference of their rangeMin values is a multiple of the number
  // of _rev values in a leaf node, which is
  //   (rangeMax-rangeMin)/(1ULL << (BranchingBits*depth)),
  // since (1ULL << (BranchingBits*depth)) is the number of leaves. Therefore
  // we must ensure that trees of replicas of shards which we must be
  // able to compare, remain compatible. Therefore, we pick a magic
  // constant M as the initial value of rangeMin, which is the same for
  // all replicas of a shard, and then maintain the following invariants
  // at all times, for all changes to rangeMin and rangeMax we ever do:
  //
  // 1. rangeMax-rangeMin is a power of two and is a multiple of
  //    the number of leaves in the tree, which is
  //      1ULL << (BranchingBits*depth)
  //    That is, we can only ever grow the width by factors of 2.
  // 2. M - rangeMin is divisible by
  //      (rangeMax-rangeMin)/(1ULL << (BranchingBits*depth))
  //
  // Condition 1. ensures that each leaf is responsible for the same
  // number of _rev values and that we can always grow rangeMax-rangeMin
  // by a factor of 2 without having to rehash everything.
  // Condition 2. ensures that two trees which have started with the same
  // magic M and have the same width are comparable, since the difference
  // of their rangeMin values will always be divisible by the number
  // given in Condition 2.
  //
  // See methods growLeft and growRight for an explanation how we keep
  // these invariants in place on growth.
 public:
  static constexpr std::uint64_t allocationSize(std::uint64_t depth) noexcept {
    // summary node is included in MetaSize
    return MetaSize + (NodeSize * nodeCountAtDepth(depth));
  }

  static constexpr std::uint64_t shardSize(std::uint64_t depth) noexcept {
    std::uint64_t shardSize = allocationSize(depth) - MetaSize;
    return std::min(shardSize, ShardSize);
  }

  /**
   * @brief Calculates the number of nodes at the given depth
   *
   * @param depth The same depth value used for the calculation
   */
  static constexpr std::uint64_t nodeCountAtDepth(
      std::uint64_t depth) noexcept {
    return static_cast<std::uint64_t>(1) << (BranchingBits * depth);
  }

  static constexpr std::uint64_t shardForIndex(std::uint64_t index) noexcept {
    return NodeSize * index / ShardSize;
  }

  static constexpr std::uint64_t shardBaseIndex(std::uint64_t shard) noexcept {
    return shard * ShardSize / NodeSize;
  }

  /**
   * @brief Chooses the default range width for a tree of a given depth.
   *
   * Most applications should use either this value or some power-of-two
   * mulitple of this value. The default is chosen so that each leaf bucket
   * initially covers a range of 64 keys.
   *
   * @param depth The same depth value passed to the constructor
   */
  static std::uint64_t defaultRange(std::uint64_t depth);

  /**
   * @brief Construct a tree from a buffer containing a serialized tree
   *
   * @param buffer      A buffer containing a serialized tree
   * @return A newly allocated tree constructed from the input
   */
  static std::unique_ptr<MerkleTree<Hasher, BranchingBits>> fromBuffer(
      std::string_view buffer);

  /**
   * @brief Construct a tree from a buffer containing an uncompressed tree
   *
   * @param buffer      A buffer containing an uncompressed tree
   * @return A newly allocated tree constructed from the input
   */
  static std::unique_ptr<MerkleTree<Hasher, BranchingBits>> fromUncompressed(
      std::string_view buffer);

  /**
   * @brief Construct a tree from a buffer containing a Snappy-compressed tree
   *
   * @param buffer      A buffer containing a Snappy compressed tree
   * @return A newly allocated tree constructed from the input
   */
  static std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
  fromSnappyCompressed(std::string_view buffer);

  static std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
  fromSnappyLazyCompressed(std::string_view buffer);

  /**
   * @brief Construct a tree from a buffer containing only the populated buckets
   *
   * @param buffer      A buffer containing a series of populated buckets
   * @return A newly allocated tree constructed from the input
   */
  static std::unique_ptr<MerkleTree<Hasher, BranchingBits>> fromOnlyPopulated(
      std::string_view buffer);

  /**
   * @brief Construct a tree from a portable serialized tree
   *
   * @param slice A slice containing a serialized tree
   * @return A newly allocated tree constructed from the input
   */
  static std::unique_ptr<MerkleTree<Hasher, BranchingBits>> deserialize(
      velocypack::Slice slice);

  /**
   * @brief Construct a Merkle tree of a given depth with a given minimum key
   *
   * @param depth    The depth of the tree. This determines how much memory The
   *                 tree will consume, and how fine-grained the hash is.
   *                 Constructor will throw if a value less than 2 is specified.
   * @param rangeMin The minimum key that can be stored in the tree.
   *                 An attempt to insert a smaller key will result
   *                 in a growLeft. See above (magic constant M) for a
   *                 sensible choice of initial rangeMin.
   * @param rangeMax Must be an offset from rangeMin of a multiple of the
   *                 number of leaf nodes. If 0, it will be  chosen using the
   *                 defaultRange method. This is just an initial value to
   *                 prevent immediate resizing; if a key larger than rangeMax
   *                 is inserted into the tree, it will be dynamically resized
   *                 so that a larger rangeMax is chosen, and adjacent nodes
   *                 merged as necessary (growRight).
   * @param initialRangeMin The initial value of rangeMin when the tree was
   *                 first created and was still empty.
   * @throws std::invalid_argument  If depth is less than 2
   */
  MerkleTree(std::uint64_t depth, std::uint64_t rangeMin,
             std::uint64_t rangeMax = 0, std::uint64_t initialRangeMin = 0);

  ~MerkleTree();

  /**
   * @brief Move assignment operator from pointer
   *
   * @param other Input tree, intended assignment
   */
  MerkleTree& operator=(
      std::unique_ptr<MerkleTree<Hasher, BranchingBits>>&& other);

  /// @brief base memory usage for struct + dynamic memory usage
  std::uint64_t memoryUsage() const;

  /// @brief only dynamic memory usage (excluding base memory usage)
  std::uint64_t dynamicMemoryUsage() const;

  /**
   * @brief Returns the number of hashed keys contained in the tree
   */
  std::uint64_t count() const;

  /**
   * @brief Returns the hash of all values in the tree, equivalently the root
   *        value
   */
  std::uint64_t rootValue() const;

  /**
   * @brief Returns the current range of the tree
   */
  std::pair<std::uint64_t, std::uint64_t> range() const;

  /**
   * @brief Returns the maximum depth of the tree
   */
  std::uint64_t depth() const;

  /**
   * @brief Returns the number of bytes allocated for the tree
   */
  std::uint64_t byteSize() const;

  /**
   * @brief Insert a value into the tree. May trigger a resize.
   *
   * @param key   The key for the item. If it is less than the minimum specified
   *              at construction, then it will trigger an exception. If it is
   *              greater than the current max, it will trigger a (potentially
   *              expensive) growth operation to ensure the key can be inserted.
   * @throws std::out_of_range  If key is less than rangeMin
   */
  void insert(std::uint64_t key);

  /**
   * @brief Insert a batch of keys (as values) into the tree. May trigger a
   * resize.
   *
   * @param keys  The keys to be inserted. Each key will be hashed to generate
   *              a value, then inserted as if by the basic single insertion
   *              method. This batch method is considerably more efficient.
   * @throws std::out_of_range  If key is less than rangeMin
   */
  void insert(std::vector<std::uint64_t> const& keys);

  /**
   * @brief Remove a value from the tree.
   *
   * @param key   The key for the item. If it is outside the current tree range,
   *              then it will trigger an exception.
   * @throws std::out_of_range  If key is outside current range
   */
  void remove(std::uint64_t key);

  /**
   * @brief Remove a batch of keys (as values) from the tree.
   *
   * @param keys  The keys to be removed. Each key will be hashed to generate
   *              a value, then removed as if by the basic single removal
   *              method. This batch method is considerably more efficient.
   * @throws std::out_of_range  If key is less than rangeMin
   * @throws std::invalid_argument  If remove hits a node with 0 count
   */
  void remove(std::vector<std::uint64_t> const& keys);

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  /**
   * @brief Remove a batch of keys (as values) from the tree.
   *        This is a special version of remove that processes the keys in
   *        the specified order, without sorting them first.
   *
   * @param keys  The keys to be removed. Each key will be hashed to generate
   *              a value, then removed as if by the basic single removal
   *              method. This batch method is considerably more efficient.
   * @throws std::invalid_argument  If remove hits a node with 0 count
   */
  void removeUnsorted(std::vector<std::uint64_t> const& keys);
#endif

  /**
   * @brief Remove all values from the tree.
   */
  void clear();

  /**
   * @brief Clone the tree.
   */
  std::unique_ptr<MerkleTree<Hasher, BranchingBits>> clone();

  /**
   * @brief Find the ranges of keys over which two trees differ. Currently,
   * only trees of the same depth can be diffed.
   *
   * @param other The other tree to compare
   * @return  Vector of (inclusive) ranges of keys over which trees differ
   * @throws std::invalid_argument  If trees different rangeMin
   */
  std::vector<std::pair<std::uint64_t, std::uint64_t>> diff(
      MerkleTree<Hasher, BranchingBits>& other);

  /**
   * @brief Convert to a human-readable string for printing
   *
   * @param full Whether or not to include meta data
   * @return String representing the tree
   */
  std::string toString(bool full) const;

  /**
   * @brief Serialize the tree for transport or storage in portable format
   *
   * @param output    VPackBuilder for output
   * @param onlyPopulated  Only return populated buckets
   */
  void serialize(velocypack::Builder& output, bool onlyPopulated) const;

  /**
   * @brief Provides a partition of the keyspace
   *
   * Makes best effort attempt to ensure the partitions are as even as possible.
   * That is, to the granularity allowed, it will try to ensure that the number
   * of keys in each partition is roughly the same.
   *
   * @param count The number of partitions to return
   * @return Vector of (inclusive) ranges that partiion the keyspace
   */
  std::vector<std::pair<std::uint64_t, std::uint64_t>> partitionKeys(
      std::uint64_t count) const;

  /**
   * @brief Serialize the tree for transport or storage in binary format
   */
  void serializeBinary(std::string& output, BinaryFormat format) const;

  /**
   * @brief Checks the consistency of the tree
   *
   * If any inconsistency is found, this function will throw
   */
  void checkConsistency() const;

  std::uint64_t numberOfShards() const noexcept;

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  // intentionally corrupts the tree. used for testing only
  void corrupt(std::uint64_t count, std::uint64_t hash);
#endif

 protected:
  explicit MerkleTree(std::string_view buffer);
  explicit MerkleTree(Data&& data);
  explicit MerkleTree(MerkleTree<Hasher, BranchingBits> const& other);

  Meta& meta() noexcept;
  Meta const& meta() const noexcept;

  Node& node(std::uint64_t index);
  Node const& node(std::uint64_t index) const noexcept;
  bool empty(std::uint64_t index) const noexcept;

  std::uint64_t index(std::uint64_t key) const noexcept;
  void modify(std::uint64_t key, bool isInsert);
  void modify(std::vector<std::uint64_t> const& keys, bool isInsert);
  bool modifyLocal(Node& node, std::uint64_t count, std::uint64_t value,
                   bool isInsert) noexcept;
  bool modifyLocal(std::uint64_t key, std::uint64_t value, bool isInsert);
  void leftCombine(bool withShift);
  void rightCombine(bool withShift);
  void growLeft(std::uint64_t key);
  void growRight(std::uint64_t key);
  bool equalAtIndex(MerkleTree<Hasher, BranchingBits> const& other,
                    std::uint64_t index) const noexcept;
  std::pair<std::uint64_t, std::uint64_t> chunkRange(std::uint64_t chunk,
                                                     std::uint64_t depth) const;
  void serializeMeta(std::string& output, bool addPadding) const;
  void serializeNodes(std::string& output, bool all) const;

 private:
  /**
   * @brief Checks the min and max keys for an insert, and grows
   * the tree as necessary
   *
   * If minKey < rangeMin, this will grow the tree to the left
   * If maxKey >= rangeMax, this will grow the tree to the right
   *
   * @param guard Lock guard (already locked)
   * @param minKey Minimum key to insert
   * @param maxKey Maximum key to insert
   */
  void prepareInsertMinMax(std::unique_lock<std::shared_mutex>& guard,
                           std::uint64_t minKey, std::uint64_t maxKey);

  /**
   * @brief Checks the consistency of the tree
   *
   * If any inconsistency is found, this function will throw
   */
  void checkInternalConsistency() const;

 private:
  mutable std::shared_mutex _dataLock;
  Data _data;
};

template<typename Hasher, std::uint64_t const BranchingBits>
std::ostream& operator<<(std::ostream& stream,
                         MerkleTree<Hasher, BranchingBits> const& tree);

using RevisionTree = MerkleTree<FnvHashProvider, 3>;

}  // namespace containers
}  // namespace arangodb
