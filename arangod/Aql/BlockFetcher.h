////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_BLOCK_FETCHER_H
#define ARANGOD_AQL_BLOCK_FETCHER_H

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockShell.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionState.h"
#include "Basics/Exceptions.h"
#include "ExecutionEngine.h"
#include "AqlItemBlockShell.h"

#include <memory>
#include <utility>

namespace arangodb {
namespace aql {

/**
 * @brief Thin interface to access the methods of ExecutionBlock that are
 * necessary for the row Fetchers. Makes it easier to test the Fetchers.
 */
class BlockFetcher {
 public:
  explicit BlockFetcher(
      ExecutionBlock* executionBlock_,
      std::shared_ptr<const std::unordered_set<RegisterId>> inputRegisters_)
      : _executionBlock(executionBlock_),
        _inputRegisters(std::move(inputRegisters_)),
        _blockId(-1){};

  TEST_VIRTUAL ~BlockFetcher() = default;

  TEST_VIRTUAL
      std::pair<ExecutionState, std::shared_ptr<InputAqlItemBlockShell>>
      fetchBlock();

  // TODO this should no longer be needed with AqlItemBlockShell. Maybe we
  // can get rid of the BlockFetcher now.
  TEST_VIRTUAL inline void returnBlock(
      std::unique_ptr<AqlItemBlock> block) noexcept {
    AqlItemBlock* blockPtr = block.get();
    _executionBlock->returnBlockUnlessNull(blockPtr);
    TRI_ASSERT(blockPtr == nullptr);
    block.release();
  };

  TEST_VIRTUAL inline RegisterId getNrInputRegisters() {
    return _executionBlock->getNrInputRegisters();
  }

 private:
  ExecutionBlock* _executionBlock;
  std::shared_ptr<const std::unordered_set<RegisterId>> _inputRegisters;

  // Holds the ID of the block returned *last*. Before the first block is
  // returned, this is invalid (-1).
  AqlItemBlockShell::AqlItemBlockId _blockId;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_BLOCK_FETCHER_H
