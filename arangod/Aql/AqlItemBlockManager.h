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

#include "Aql/AqlItemBlockSerializationFormat.h"
#include "Aql/types.h"
#include "Basics/Common.h"

#include <array>
#include <cstdint>
#include <mutex>

namespace arangodb {
struct ResourceMonitor;

namespace velocypack {
class Slice;
}

namespace aql {

class AqlItemBlock;
class SharedAqlItemBlockPtr;

class AqlItemBlockManager {
  friend class SharedAqlItemBlockPtr;

 public:
  /// @brief create the manager
  explicit AqlItemBlockManager(arangodb::ResourceMonitor&, SerializationFormat format);

  /// @brief destroy the manager
  TEST_VIRTUAL ~AqlItemBlockManager();

 public:
  /// @brief request a block with the specified size
  TEST_VIRTUAL SharedAqlItemBlockPtr requestBlock(size_t nrItems, RegisterCount nrRegs);

  /// @brief request a block and initialize it from the slice
  TEST_VIRTUAL SharedAqlItemBlockPtr requestAndInitBlock(velocypack::Slice slice);

  TEST_VIRTUAL arangodb::ResourceMonitor& resourceMonitor() const noexcept;

  SerializationFormat getFormatType() const { return _format; }

#ifdef ARANGODB_USE_GOOGLE_TESTS
  // Only used for the mocks in the catch tests. Other code should always use
  // SharedAqlItemBlockPtr which in turn call returnBlock()!
  static void deleteBlock(AqlItemBlock* block);

  static uint32_t getBucketId(size_t targetSize) noexcept;
#endif

#ifndef ARANGODB_USE_GOOGLE_TESTS
 protected:
#else
  // make returnBlock public for tests so it can be mocked
 public:
#endif
  /// @brief return a block to the manager
  /// Should only be called by SharedAqlItemBlockPtr!
  TEST_VIRTUAL void returnBlock(AqlItemBlock*& block) noexcept;

 private:
  arangodb::ResourceMonitor& _resourceMonitor;
  SerializationFormat const _format;

  static constexpr uint32_t numBuckets = 12;
  static constexpr size_t numBlocksPerBucket = 7;

  struct Bucket {
    std::array<AqlItemBlock*, numBlocksPerBucket> blocks;
    size_t numItems;
    mutable std::mutex _mutex;

    Bucket();
    ~Bucket();

    bool empty() const noexcept;

    bool full() const noexcept;

    AqlItemBlock* pop() noexcept;

    void push(AqlItemBlock* block) noexcept;

    static uint32_t getId(size_t targetSize) noexcept;
  };

  Bucket _buckets[numBuckets];
};

}  // namespace aql
}  // namespace arangodb

#endif
