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

#include "MerkleTree.h"

#include "Basics/debugging.h"
#include "Basics/hashes.h"

#include "Logger/LogMacros.h"

namespace arangodb {
namespace containers {

template <std::size_t const BranchingBits, std::size_t const LockStripes>
bool MerkleTree<BranchingBits, LockStripes>::Node::operator==(Node const& other) {
  return ((this->count == other.count) && (this->hash == other.hash));
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
constexpr std::size_t MerkleTree<BranchingBits, LockStripes>::nodeCountAtDepth(std::size_t maxDepth) {
  return static_cast<std::size_t>(1) << (BranchingBits * maxDepth);
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
constexpr std::size_t MerkleTree<BranchingBits, LockStripes>::nodeCountUpToDepth(std::size_t maxDepth) {
  return ((static_cast<std::size_t>(1) << (BranchingBits * (maxDepth + 1))) - 1) /
         (BranchingFactor - 1);
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
constexpr std::size_t MerkleTree<BranchingBits, LockStripes>::allocationSize(std::size_t maxDepth) {
  return MetaSize + (NodeSize * nodeCountUpToDepth(maxDepth));
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
constexpr bool MerkleTree<BranchingBits, LockStripes>::isPowerOf2(std::size_t n) {
  return n > 0 && (n & (n - 1)) == 0;
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
constexpr std::size_t MerkleTree<BranchingBits, LockStripes>::log2ceil(std::size_t n) {
  if (n > (std::numeric_limits<std::size_t>::max() / 2)) {
    return 8 * sizeof(std::size_t) - 1;
  }
  std::size_t i = 1;
  for (; (static_cast<std::size_t>(1) << i) < n; ++i) {
  }
  return i;
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
constexpr std::size_t MerkleTree<BranchingBits, LockStripes>::minimumFactorFor(
    std::size_t current, std::size_t target) {
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

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::size_t MerkleTree<BranchingBits, LockStripes>::defaultRange(std::size_t maxDepth) {
  return nodeCountAtDepth(maxDepth) * 64;
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::unique_ptr<MerkleTree<BranchingBits, LockStripes>>
MerkleTree<BranchingBits, LockStripes>::fromBuffer(std::string_view buffer) {
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
MerkleTree<BranchingBits, LockStripes>::MerkleTree(std::size_t maxDepth,
                                                   std::size_t rangeMin, std::size_t rangeMax)
    : _buffer(new uint8_t[allocationSize(maxDepth)]) {
  if (maxDepth < 2) {
    throw std::invalid_argument("Must specify a maxDepth >= 2.");
  }
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

  std::size_t const last = nodeCountUpToDepth(meta().maxDepth);
  for (std::size_t i = 0; i < last; ++i) {
    node(i).~Node();
  }

  meta().~Meta();
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::size_t MerkleTree<BranchingBits, LockStripes>::count() const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  std::unique_lock<std::mutex> lock(this->lock(0));
  return node(0).count;
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::pair<std::size_t, std::size_t> MerkleTree<BranchingBits, LockStripes>::range() const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  return {meta().rangeMin, meta().rangeMax};
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::size_t MerkleTree<BranchingBits, LockStripes>::maxDepth() const {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  return meta().maxDepth;
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
void MerkleTree<BranchingBits, LockStripes>::insert(std::size_t key, std::size_t value) {
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

template <std::size_t const BranchingBits, std::size_t const LockStripes>
void MerkleTree<BranchingBits, LockStripes>::remove(std::size_t key, std::size_t value) {
  std::shared_lock<std::shared_mutex> guard(_bufferLock);
  if (key < meta().rangeMin || key >= meta().rangeMax) {
    throw std::out_of_range("Key out of current range.");
  }

  modify(key, value, /* isInsert */ false);
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
std::vector<std::pair<std::size_t, std::size_t>> MerkleTree<BranchingBits, LockStripes>::diff(
    MerkleTree<BranchingBits, LockStripes>& other) {
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

  std::vector<std::pair<std::size_t, std::size_t>> result;
  std::queue<std::size_t> candidates;
  candidates.emplace(0);

  while (!candidates.empty()) {
    std::size_t index = candidates.front();
    candidates.pop();
    if (!equalAtIndex(other, index)) {
      bool leaves = childrenAreLeaves(index);
      for (std::size_t child = (BranchingFactor * index) + 1;
           child < (BranchingFactor * (index + 1) + 1); ++child) {
        if (!leaves) {
          // internal children, queue them all up for further investigation
          candidates.emplace(child);
        } else {
          if (!equalAtIndex(other, child)) {
            // actually work with key ranges now
            std::size_t chunk = child - nodeCountUpToDepth(meta().maxDepth - 1);
            std::pair<std::size_t, std::size_t> range = chunkRange(chunk, meta().maxDepth);
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
std::string MerkleTree<BranchingBits, LockStripes>::serialize() const {
  std::unique_lock<std::shared_mutex> guard(_bufferLock);
  return std::string(reinterpret_cast<char*>(_buffer.get()),
                     allocationSize(meta().maxDepth));
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
MerkleTree<BranchingBits, LockStripes>::MerkleTree(std::string_view buffer)
    : _buffer(new uint8_t[buffer.size()]) {
  std::memcpy(_buffer.get(), buffer.data(), buffer.size());
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
typename MerkleTree<BranchingBits, LockStripes>::Meta& MerkleTree<BranchingBits, LockStripes>::meta() const {
  // not thread-safe, lock buffer from outside
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
std::size_t MerkleTree<BranchingBits, LockStripes>::index(std::size_t key,
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
  std::size_t chunkSizeAtDepth =
      (meta().rangeMax - meta().rangeMin) / (static_cast<std::size_t>(1) << (BranchingBits * depth));
  std::size_t chunk = offset / chunkSizeAtDepth;

  return chunk + nodeCountUpToDepth(depth - 1);
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
void MerkleTree<BranchingBits, LockStripes>::modify(std::size_t key, std::size_t value,
                                                    bool isInsert) {
  // not thread-safe, lock buffer from outside
  for (std::size_t depth = 0; depth <= meta().maxDepth; ++depth) {
    std::size_t index = this->index(key, depth);
    Node& node = this->node(index);
    std::unique_lock<std::mutex> lock(this->lock(index));
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
}

template <std::size_t const BranchingBits, std::size_t const LockStripes>
void MerkleTree<BranchingBits, LockStripes>::grow(std::size_t key) {
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
std::pair<std::size_t, std::size_t> MerkleTree<BranchingBits, LockStripes>::chunkRange(
    std::size_t chunk, std::size_t depth) {
  // not thread-safe, lock buffer from outside
  std::size_t rangeMin = meta().rangeMin;
  std::size_t rangeMax = meta().rangeMax;
  std::size_t chunkSizeAtDepth = (rangeMax - rangeMin) / (static_cast<std::size_t>(1) << (BranchingBits * depth));
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
