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

#include "BlockFetcherMock.h"

#include <lib/Basics/Common.h>

#include "catch.hpp"

namespace arangodb {
namespace tests {
namespace aql {

using namespace arangodb::aql;

/* * * * *
 * Mocks
 * * * * */

BlockFetcherMock::BlockFetcherMock(RegisterId nrRegisters)
    : BlockFetcher(nullptr, std::shared_ptr<std::unordered_set<RegisterId>>()),
      _itemsToReturn(),
      _fetchedBlocks(),
      _returnedBlocks(),
      _numFetchBlockCalls(0),
      _nrRegs(nrRegisters),
      _monitor(),
      _itemBlockManager(&_monitor) {}

std::pair<ExecutionState, std::shared_ptr<InputAqlItemBlockShell>>
BlockFetcherMock::fetchBlock() {
  _numFetchBlockCalls++;

  if (_itemsToReturn.empty()) {
    return {ExecutionState::DONE, nullptr};
  }

  std::pair<ExecutionState, std::shared_ptr<InputAqlItemBlockShell>>
      returnValue = std::move(_itemsToReturn.front());
  _itemsToReturn.pop_front();

  if (returnValue.second != nullptr) {
    auto blockPtr = reinterpret_cast<AqlItemBlockPtr>(returnValue.second.get());
    bool didInsert;
    std::tie(std::ignore, didInsert) = _fetchedBlocks.insert(blockPtr);
    // BlockFetcherMock::fetchBlock() should not return the same block twice:
    REQUIRE(didInsert);
  }

  return returnValue;
}

void BlockFetcherMock::returnBlock(
    std::unique_ptr<AqlItemBlock> block) noexcept {
  if (block == nullptr) {
    return;
  }

  auto blockPtr = reinterpret_cast<AqlItemBlockPtr>(block.get());

  bool didInsert;
  std::tie(std::ignore, didInsert) = _returnedBlocks.insert(blockPtr);
}

RegisterId BlockFetcherMock::getNrInputRegisters() {
  return _nrRegs;
}

/* * * * * * * * * * * * *
 * Test helper functions
 * * * * * * * * * * * * */

BlockFetcherMock& BlockFetcherMock::shouldReturn(
    ExecutionState state, std::unique_ptr<AqlItemBlock> block) {
  // Should only be called once on each instance
  TRI_ASSERT(_itemsToReturn.empty());

  return andThenReturn(state, std::move(block));
}

BlockFetcherMock& BlockFetcherMock::shouldReturn(
    std::pair<ExecutionState, std::shared_ptr<InputAqlItemBlockShell>>
        firstReturnValue) {
  // Should only be called once on each instance
  TRI_ASSERT(_itemsToReturn.empty());

  return andThenReturn(std::move(firstReturnValue));
}

BlockFetcherMock& BlockFetcherMock::shouldReturn(
    std::vector<
        std::pair<ExecutionState, std::shared_ptr<InputAqlItemBlockShell>>>
        firstReturnValues) {
  // Should only be called once on each instance
  TRI_ASSERT(_itemsToReturn.empty());

  return andThenReturn(std::move(firstReturnValues));
}

BlockFetcherMock& BlockFetcherMock::andThenReturn(
  ExecutionState state, std::unique_ptr<AqlItemBlock> block) {
  auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>();
  // add all registers as input
  for(RegisterId i = 0; i < getNrInputRegisters(); i++) {
    inputRegisters->emplace(i);
  }
  std::shared_ptr<InputAqlItemBlockShell> blockShell;
  if (block != nullptr) {
    blockShell = std::make_shared<InputAqlItemBlockShell>(
      _itemBlockManager, std::move(block), inputRegisters, _blockId
    );
    _blockId++;
  }
  return andThenReturn({state, std::move(blockShell)});
}

BlockFetcherMock& BlockFetcherMock::andThenReturn(
    std::pair<ExecutionState, std::shared_ptr<InputAqlItemBlockShell>>
        additionalReturnValue) {
  _itemsToReturn.emplace_back(std::move(additionalReturnValue));

  return *this;
}

BlockFetcherMock& BlockFetcherMock::andThenReturn(
    std::vector<
        std::pair<ExecutionState, std::shared_ptr<InputAqlItemBlockShell>>>
        additionalReturnValues) {
  for (auto& it : additionalReturnValues) {
    andThenReturn(std::move(it));
  }

  return *this;
}

bool BlockFetcherMock::allBlocksFetched() const {
  return _itemsToReturn.empty();
}

bool BlockFetcherMock::allFetchedBlocksReturned() const {
  for (auto const it : _fetchedBlocks) {
    if (_returnedBlocks.find(it) == _returnedBlocks.end()) {
      return false;
    }
  }

  return true;
}

size_t BlockFetcherMock::numFetchBlockCalls() const {
  return _numFetchBlockCalls;
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
