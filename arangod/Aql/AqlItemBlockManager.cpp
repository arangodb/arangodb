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

#include "AqlItemBlockManager.h"
#include "Aql/AqlItemBlock.h"

using namespace arangodb::aql;

/// @brief create the manager
AqlItemBlockManager::AqlItemBlockManager(ResourceMonitor* resourceMonitor) 
    : _resourceMonitor(resourceMonitor) {}

/// @brief destroy the manager
AqlItemBlockManager::~AqlItemBlockManager() { }

/// @brief request a block with the specified size
AqlItemBlock* AqlItemBlockManager::requestBlock(size_t nrItems,
                                                RegisterId nrRegs) {
  // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "requesting AqlItemBlock of " << nrItems << " x " << nrRegs;
  size_t const targetSize = nrItems * nrRegs;

  AqlItemBlock* block = nullptr;
  size_t i = Bucket::getId(targetSize);

  int tries = 0;
  while (tries++ < 2) {
    TRI_ASSERT(i < numBuckets);
    if (!_buckets[i].empty()) {
      block = _buckets[i].pop();
      TRI_ASSERT(block != nullptr);
      block->eraseAll();
      block->rescale(nrItems, nrRegs);
      // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "returned cached AqlItemBlock with dimensions " << block->size() << " x " << block->getNrRegs();
      break;
    }
    // try next (bigger) bucket
    if (++i >= numBuckets) {
      break;
    }
  }

  if (block == nullptr) {
    block = new AqlItemBlock(_resourceMonitor, nrItems, nrRegs);
    // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "created AqlItemBlock with dimensions " << block->size() << " x " << block->getNrRegs();
  }
 
  TRI_ASSERT(block != nullptr);
  TRI_ASSERT(block->size() == nrItems);   
  TRI_ASSERT(block->getNrRegs() == nrRegs);
  TRI_ASSERT(block->capacity() >= targetSize);
  return block;
}

/// @brief return a block to the manager
void AqlItemBlockManager::returnBlock(AqlItemBlock*& block) noexcept {
  TRI_ASSERT(block != nullptr);

  // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "returning AqlItemBlock of dimensions " << block->size() << " x " << block->getNrRegs();
  
  size_t const targetSize = block->size() * block->getNrRegs();
  size_t const i = Bucket::getId(targetSize);
  TRI_ASSERT(i < numBuckets);

  if (!_buckets[i].full()) {
    // recycle the block
    block->destroy();
    // store block in bucket (this will not fail)
    _buckets[i].push(block);
  } else {
    // bucket is full. simply delete the block
    delete block;
  }
  block = nullptr;
}

AqlItemBlockManager::Bucket::Bucket() 
    : numItems(0) {
  for (size_t i = 0; i < numBlocksPerBucket; ++i) {
    blocks[i] = nullptr;
  }
}

AqlItemBlockManager::Bucket::~Bucket() {
  while (!empty()) {
    delete pop();
  }
}
