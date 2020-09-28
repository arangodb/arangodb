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

#ifndef ARANGODB_CONTAINERS_MERKLE_TREE_H
#define ARANGODB_CONTAINERS_MERKLE_TREE_H 1

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include <velocypack/Builder.h>

namespace arangodb {
namespace containers {

template <std::size_t const BranchingBits = 3,  // 8 children per internal node,
          std::size_t const LockStripes = 64  // number number of buckets that can be accessed simultaneously
          >
class MerkleTree {
 protected:
  static constexpr std::size_t CacheLineSize =
      64;  // TODO replace with std::hardware_constructive_interference_size
           // once supported by all necessary compilers
  static constexpr std::size_t BranchingFactor = static_cast<std::size_t>(1) << BranchingBits;

  struct Node {
    std::uint64_t hash;
    std::size_t count;

    bool operator==(Node const& other);
  };
  static_assert(sizeof(Node) == 16, "Node size assumptions invalid.");
  static_assert(CacheLineSize % sizeof(Node) == 0,
                "Node size assumptions invalid.");
  static constexpr std::size_t NodeSize = sizeof(Node);

  struct Meta {
    std::uint64_t rangeMin;
    std::uint64_t rangeMax;
    std::size_t maxDepth;
  };
  static_assert(sizeof(Meta) == 24, "Meta size assumptions invalid.");
  static constexpr std::size_t MetaSize =
      (CacheLineSize * ((sizeof(Meta) + (CacheLineSize - 1)) / CacheLineSize));

  static constexpr std::size_t nodeCountUpToDepth(std::size_t maxDepth);
  static constexpr std::size_t allocationSize(std::size_t maxDepth);
  static constexpr std::uint64_t log2ceil(std::uint64_t n);
  static constexpr std::uint64_t minimumFactorFor(std::uint64_t current, std::uint64_t target);

 public:
  /**
   * @brief Calculates the number of nodes at the given depth
   *
   * @param maxDepth The same depth value used for the calculation
   */
  static constexpr std::size_t nodeCountAtDepth(std::size_t maxDepth) {
    return static_cast<std::size_t>(1) << (BranchingBits * maxDepth);
  }

  /**
   * @brief Chooses the default range width for a tree of a given depth.
   *
   * Most applications should use either this value or some power-of-two
   * mulitple of this value. The default is chosen so that each leaf bucket
   * initially covers a range of 64 keys.
   *
   * @param maxDepth The same depth value passed to the constructor
   */
  static std::uint64_t defaultRange(std::size_t maxDepth);

  /**
   * @brief Construct a tree from a buffer containing a serialized tree
   *
   * @param buffer      A buffer containing a serialized tree
   * @return A newly allocated tree constructed from the input
   */
  static std::unique_ptr<MerkleTree<BranchingBits, LockStripes>> fromBuffer(std::string_view buffer);

  /**
   * @brief Construct a tree from a portable serialized tree
   *
   * @param slice A slice containing a serialized tree
   * @return A newly allocated tree constructed from the input
   */
  static std::unique_ptr<MerkleTree<BranchingBits, LockStripes>> deserialize(velocypack::Slice slice);

  /**
   * @brief Construct a Merkle tree of a given depth with a given minimum key
   *
   * @param maxDepth The depth of the tree. This determines how much memory The
   *                 tree will consume, and how fine-grained the hash is.
   *                 Constructor will throw if a value less than 2 is specified.
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
   * @throws std::invalid_argument  If maxDepth is less than 2
   */
  MerkleTree(std::size_t maxDepth, std::uint64_t rangeMin, std::uint64_t rangeMax = 0);

  ~MerkleTree();

  /**
   * @brief Move assignment operator from pointer
   *
   * @param other Input tree, intended assignment
   */
  MerkleTree& operator=(std::unique_ptr<MerkleTree<BranchingBits, LockStripes>>&& other);

  /**
   * @brief Returns the number of hashed keys contained in the tree
   */
  std::size_t count() const;

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
  std::size_t maxDepth() const;

  /**
   * @brief Insert a value into the tree. May trigger a resize.
   *
   * @param key   The key for the item. If it is less than the minimum specified
   *              at construction, then it will trigger an exception. If it is
   *              greater than the current max, it will trigger a (potentially
   *              expensive) growth operation to ensure the key can be inserted.
   * @param value The hashed value associated with the key
   * @throws std::out_of_range  If key is less than rangeMin
   */
  void insert(std::uint64_t key, std::uint64_t value);

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
   * @param value The hashed value associated with the key
   * @throws std::out_of_range  If key is outside current range
   * @throws std::invalid_argument  If remove hits a node with 0 count
   */
  void remove(std::uint64_t key, std::uint64_t value);

  /**
   * @brief Remove a batch of keys (as values) from the tree.
   *
   * @param keys  The keys to be removed. Each key will be hashed to generate
   *              a value, then removed as if by the basic single removal
   *              method. This batch method is considerably more efficient.
   * @throws std::out_of_range  If key is less than rangeMin
   */
  void remove(std::vector<std::uint64_t> const& keys);

  /**
   * @brief Remove all values from the tree.
   */
  void clear();

  /**
   * @brief Clone the tree.
   */
  std::unique_ptr<MerkleTree<BranchingBits, LockStripes>> clone();

  /**
   * @brief Clone the tree with the given depth
   *
   * Will return a new copy of the tree, with the specified depth. If the new
   * depth is the same as the old, this is equivalent to clone(). If the new
   * depth is less than the old depth, this simply truncates the tree. If the
   * new depth is greater than the old depth, then the rangeMax will also be
   * increased.
   *
   * @param newDepth  Max depth to use for new tree
   */
  std::unique_ptr<MerkleTree<BranchingBits, LockStripes>> cloneWithDepth(std::size_t newDepth) const;

  /**
   * @brief Find the ranges of keys over which two trees differ.
   *
   * @param other The other tree to compare
   * @return  Vector of (inclusive) ranges of keys over which trees differ
   * @throws std::invalid_argument  If trees different rangeMin
   */
  std::vector<std::pair<std::uint64_t, std::uint64_t>> diff(MerkleTree<BranchingBits, LockStripes>& other);

  /**
   * @brief Convert to a human-readable string for printing
   *
   * @return String representing the tree
   */
  std::string toString() const;

  /**
   * @brief Serialize the tree for transport or storage in portable format
   *
   * @param output    VPackBuilder for output
   * @param depth     Maximum depth to serialize
   */
  void serialize(velocypack::Builder& output,
                 std::size_t maxDepth = std::numeric_limits<std::size_t>::max()) const;

  /**
   * @brief Serialize the tree for transport or storage in binary format
   *
   * @param output    String for output
   * @param compress  Whether or not to compress the output
   */
  void serializeBinary(std::string& output, bool compress) const;

 protected:
  explicit MerkleTree(std::string_view buffer);
  explicit MerkleTree(MerkleTree<BranchingBits, LockStripes> const& other);

  Meta& meta() const;
  Node& node(std::size_t index) const;
  std::mutex& lock(std::size_t index) const;
  std::size_t index(std::uint64_t key, std::size_t depth) const;
  void modify(std::uint64_t key, std::uint64_t value, bool isInsert);
  void modify(std::vector<std::uint64_t> const& keys, bool isInsert);
  void modifyLocal(std::size_t depth, std::uint64_t key, std::uint64_t value,
                   bool isInsert, bool doLock);
  void grow(std::uint64_t key);
  bool equalAtIndex(MerkleTree<BranchingBits, LockStripes> const& other,
                    std::size_t index) const;
  bool childrenAreLeaves(std::size_t index);
  std::pair<std::uint64_t, std::uint64_t> chunkRange(std::size_t chunk, std::size_t depth);

 private:
  std::unique_ptr<std::uint8_t[]> _buffer;
  mutable std::shared_mutex _bufferLock;
  mutable std::mutex _nodeLocks[LockStripes];
};

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::ostream& operator<<(std::ostream& stream,
                         MerkleTree<BranchingBits, LockStripes> const& tree);

using RevisionTree = MerkleTree<3, 64>;

}  // namespace containers
}  // namespace arangodb

#endif
