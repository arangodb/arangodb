////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_AQL_ITEM_BLOCK_MANAGER_H
#define ARANGOD_AQL_AQL_ITEM_BLOCK_MANAGER_H 1

#include "Aql/types.h"
#include "Basics/Common.h"

#include <array>

namespace arangodb {
namespace aql {

class AqlItemBlock;
struct ResourceMonitor;

class AqlItemBlockManager {
 public:
  /// @brief create the manager
  explicit AqlItemBlockManager(ResourceMonitor*);

  /// @brief destroy the manager
  TEST_VIRTUAL ~AqlItemBlockManager();

 public:
  /// @brief request a block with the specified size
  TEST_VIRTUAL AqlItemBlock* requestBlock(size_t nrItems, RegisterId nrRegs);

  /// @brief return a block to the manager
  TEST_VIRTUAL void returnBlock(AqlItemBlock*& block) noexcept;

  /// @brief return a block to the manager
  void returnBlock(std::unique_ptr<AqlItemBlock> block) noexcept {
    AqlItemBlock* b = block.release();
    returnBlock(b);
  }

  ResourceMonitor* resourceMonitor() const { return _resourceMonitor; }

 private:
  ResourceMonitor* _resourceMonitor;

  static constexpr size_t numBuckets = 12;
  static constexpr size_t numBlocksPerBucket = 7;

  struct Bucket {
    std::array<AqlItemBlock*, numBlocksPerBucket> blocks;
    size_t numItems;

    Bucket();
    ~Bucket();

    bool empty() const noexcept { return numItems == 0; }

    bool full() const noexcept { return (numItems == numBlocksPerBucket); }

    AqlItemBlock* pop() noexcept {
      TRI_ASSERT(!empty());
      TRI_ASSERT(numItems > 0);
      AqlItemBlock* result = blocks[--numItems];
      TRI_ASSERT(result != nullptr);
      blocks[numItems] = nullptr;
      return result;
    }

    void push(AqlItemBlock* block) noexcept {
      TRI_ASSERT(!full());
      TRI_ASSERT(blocks[numItems] == nullptr);
      blocks[numItems++] = block;
      TRI_ASSERT(numItems <= numBlocksPerBucket);
    }

    static size_t getId(size_t targetSize) noexcept {
      if (targetSize <= 1) {
        return 0;
      }
      if (targetSize <= 10) {
        return 1;
      }
      if (targetSize <= 20) {
        return 2;
      }
      if (targetSize <= 40) {
        return 3;
      }
      if (targetSize <= 100) {
        return 4;
      }
      if (targetSize <= 200) {
        return 5;
      }
      if (targetSize <= 400) {
        return 6;
      }
      if (targetSize <= 1000) {
        return 7;
      }
      if (targetSize <= 2000) {
        return 8;
      }
      if (targetSize <= 4000) {
        return 9;
      }
      if (targetSize <= 10000) {
        return 10;
      }
      return 11;
    }
  };

  Bucket _buckets[numBuckets];
};

}  // namespace aql
}  // namespace arangodb

#endif
