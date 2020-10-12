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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "AqlItemBlockManager.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Basics/NumberUtils.h"
#include "Basics/VelocyPackHelper.h"

using namespace arangodb::aql;

using VelocyPackHelper = arangodb::basics::VelocyPackHelper;

/// @brief create the manager
AqlItemBlockManager::AqlItemBlockManager(ResourceMonitor* resourceMonitor,
                                         SerializationFormat format)
    : _resourceMonitor(resourceMonitor), _format(format) {
  TRI_ASSERT(resourceMonitor != nullptr);
}

/// @brief destroy the manager
AqlItemBlockManager::~AqlItemBlockManager() = default;

/// @brief request a block with the specified size
SharedAqlItemBlockPtr AqlItemBlockManager::requestBlock(size_t numRows, RegisterCount numRegisters) {
  size_t const targetSize = numRows * numRegisters;

  AqlItemBlock* block = nullptr;
  uint32_t i = Bucket::getId(targetSize);

  int tries = 0;
  do {
    TRI_ASSERT(i < numBuckets);
    
    std::lock_guard<std::mutex> guard(_buckets[i]._mutex);
    if (!_buckets[i].empty()) {
      block = _buckets[i].pop();
      TRI_ASSERT(block != nullptr);
      TRI_ASSERT(block->numEntries() == 0);
      break;
    }
    // try next (bigger) bucket
    ++i;
  } while (i < numBuckets && ++tries < 3);

  // perform potentially expensive allocation/cleanup tasks outside
  // of the locked section
  if (block == nullptr) {
    block = new AqlItemBlock(*this, numRows, numRegisters);
  } else {
    block->rescale(numRows, numRegisters);
  }

  TRI_ASSERT(block != nullptr);
  TRI_ASSERT(block->numRows() == numRows);
  TRI_ASSERT(block->numRegisters() == numRegisters);
  TRI_ASSERT(block->numEntries() == targetSize);
  TRI_ASSERT(block->numEffectiveEntries() == 0);
  TRI_ASSERT(block->getRefCount() == 0);
  TRI_ASSERT(block->hasShadowRows() == false);

  return SharedAqlItemBlockPtr{block};
}

/// @brief return a block to the manager
void AqlItemBlockManager::returnBlock(AqlItemBlock*& block) noexcept {
  TRI_ASSERT(block != nullptr);
  TRI_ASSERT(block->getRefCount() == 0);

  size_t const targetSize = block->capacity();
  uint32_t const i = Bucket::getId(targetSize);
  TRI_ASSERT(i < numBuckets);

  // Destroying the block releases the AqlValues. Which in turn may hold DocVecs
  // and can thus return AqlItemBlocks to this very Manager. So the destroy must
  // not happen between the check whether `_buckets[i].full()` and
  // `_buckets[i].push(block)`, because the destroy() can add blocks to the
  // buckets!
  block->destroy();

  {
    std::lock_guard<std::mutex> guard(_buckets[i]._mutex);

    if (!_buckets[i].full()) {
      // recycle the block
      TRI_ASSERT(block->numEntries() == 0);
      // store block in bucket (this will not fail)
      _buckets[i].push(block);
      block = nullptr;
    }
  }

  // call block destructor outside the lock
  if (block != nullptr) {
    // bucket is full. simply delete the block
    delete block;
    block = nullptr;
  }
}

SharedAqlItemBlockPtr AqlItemBlockManager::requestAndInitBlock(arangodb::velocypack::Slice slice) {
  auto const nrItemsSigned =
      VelocyPackHelper::getNumericValue<int64_t>(slice, "nrItems", 0);
  auto const nrRegs =
      VelocyPackHelper::getNumericValue<RegisterId>(slice, "nrRegs", 0);
  if (nrItemsSigned <= 0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "nrItems must be > 0");
  }
  auto const numRows = static_cast<size_t>(nrItemsSigned);

  SharedAqlItemBlockPtr block = requestBlock(numRows, nrRegs);
  block->initFromSlice(slice);

  TRI_ASSERT(block->numRows() == numRows);
  TRI_ASSERT(block->numRegisters() == nrRegs);

  return block;
}

ResourceMonitor* AqlItemBlockManager::resourceMonitor() const noexcept {
  return _resourceMonitor;
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
void AqlItemBlockManager::deleteBlock(AqlItemBlock* block) {
  delete block;
}
#endif

#ifdef ARANGODB_USE_GOOGLE_TESTS
uint32_t AqlItemBlockManager::getBucketId(size_t targetSize) noexcept {
  return Bucket::getId(targetSize);
}
#endif

AqlItemBlockManager::Bucket::Bucket() : numItems(0) {
  for (size_t i = 0; i < numBlocksPerBucket; ++i) {
    blocks[i] = nullptr;
  }
}

AqlItemBlockManager::Bucket::~Bucket() {
  while (!empty()) {
    delete pop();
  }
}

bool AqlItemBlockManager::Bucket::empty() const noexcept {
  return numItems == 0;
}

bool AqlItemBlockManager::Bucket::full() const noexcept {
  return (numItems == numBlocksPerBucket);
}

AqlItemBlock* AqlItemBlockManager::Bucket::pop() noexcept {
  TRI_ASSERT(!empty());
  AqlItemBlock* result = blocks[--numItems];
  TRI_ASSERT(result != nullptr);
  blocks[numItems] = nullptr;
  return result;
}

void AqlItemBlockManager::Bucket::push(AqlItemBlock* block) noexcept {
  TRI_ASSERT(!full());
  TRI_ASSERT(blocks[numItems] == nullptr);
  blocks[numItems++] = block;
  TRI_ASSERT(numItems <= numBlocksPerBucket);
}

uint32_t AqlItemBlockManager::Bucket::getId(size_t targetSize) noexcept {
  if (targetSize <= 1) {
    return 0;
  }

  if (ADB_UNLIKELY(targetSize >= (1ULL << numBuckets))) {
    return (numBuckets - 1);
  }
  
  uint32_t value = arangodb::NumberUtils::log2(static_cast<uint32_t>(targetSize));
  TRI_ASSERT(value < numBuckets);
  return value;
}
