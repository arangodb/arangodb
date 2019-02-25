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

// Note that _itemBlockManager gets passed first to the parent constructor,
// and only then gets instantiated. That is okay, however, because the
// constructor will not access it.
template <bool passBlocksThrough>
BlockFetcherMock<passBlocksThrough>::BlockFetcherMock(arangodb::aql::ResourceMonitor& monitor,
                                                      ::arangodb::aql::RegisterId nrRegisters)
    : BlockFetcher<passBlocksThrough>({}, _itemBlockManager,
                                      std::shared_ptr<std::unordered_set<RegisterId>>(),
                                      nrRegisters),
      _itemsToReturn(),
      _fetchedBlocks(),
      _numFetchBlockCalls(0),
      _monitor(monitor),
      _itemBlockManager(&_monitor) {}

template <bool passBlocksThrough>
std::pair<ExecutionState, std::shared_ptr<AqlItemBlockShell>>
// NOLINTNEXTLINE google-default-arguments
BlockFetcherMock<passBlocksThrough>::fetchBlock(size_t) {
  _numFetchBlockCalls++;

  if (_itemsToReturn.empty()) {
    return {ExecutionState::DONE, nullptr};
  }

  std::pair<ExecutionState, std::shared_ptr<AqlItemBlockShell>> returnValue =
      std::move(_itemsToReturn.front());
  _itemsToReturn.pop();

  if (returnValue.second != nullptr) {
    auto blockPtr = reinterpret_cast<AqlItemBlockPtr>(&returnValue.second->block());
    bool didInsert;
    std::tie(std::ignore, didInsert) = _fetchedBlocks.insert(blockPtr);
    // BlockFetcherMock<passBlocksThrough>::fetchBlock() should not return the same block twice:
    REQUIRE(didInsert);
  }

  return returnValue;
}

/* * * * * * * * * * * * *
 * Test helper functions
 * * * * * * * * * * * * */

template <bool passBlocksThrough>
BlockFetcherMock<passBlocksThrough>& BlockFetcherMock<passBlocksThrough>::shouldReturn(
    ExecutionState state, std::unique_ptr<AqlItemBlock> block) {
  // Should only be called once on each instance
  TRI_ASSERT(_itemsToReturn.empty());

  return andThenReturn(state, std::move(block));
}

template <bool passBlocksThrough>
BlockFetcherMock<passBlocksThrough>& BlockFetcherMock<passBlocksThrough>::shouldReturn(
    std::pair<ExecutionState, std::shared_ptr<AqlItemBlockShell>> firstReturnValue) {
  // Should only be called once on each instance
  TRI_ASSERT(_itemsToReturn.empty());

  return andThenReturn(std::move(firstReturnValue));
}

template <bool passBlocksThrough>
BlockFetcherMock<passBlocksThrough>& BlockFetcherMock<passBlocksThrough>::shouldReturn(
    std::vector<std::pair<ExecutionState, std::shared_ptr<AqlItemBlockShell>>> firstReturnValues) {
  // Should only be called once on each instance
  TRI_ASSERT(_itemsToReturn.empty());

  return andThenReturn(std::move(firstReturnValues));
}

template <bool passBlocksThrough>
BlockFetcherMock<passBlocksThrough>& BlockFetcherMock<passBlocksThrough>::andThenReturn(
    ExecutionState state, std::unique_ptr<AqlItemBlock> block) {
  auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>();
  // add all registers as input
  /* Note: without the `this->` in the following line, I'm getting this error:
  /home/tobias/Documents/ArangoDB/arangodb/arangodb/tests/Aql/BlockFetcherMock.cpp:112:30: error: use of undeclared identifier 'getNrInputRegisters'
    for (RegisterId i = 0; i < getNrInputRegisters(); i++) {
                               ^
   * wth? with clang version 6.0.0-1ubuntu2
   */
  for (RegisterId i = 0; i < this->getNrInputRegisters(); i++) {
    inputRegisters->emplace(i);
  }
  std::shared_ptr<AqlItemBlockShell> blockShell;
  if (block != nullptr) {
    blockShell =
        std::make_shared<AqlItemBlockShell>(_itemBlockManager, std::move(block));
  }
  return andThenReturn({state, std::move(blockShell)});
}

template <bool passBlocksThrough>
BlockFetcherMock<passBlocksThrough>& BlockFetcherMock<passBlocksThrough>::andThenReturn(
    std::pair<ExecutionState, std::shared_ptr<AqlItemBlockShell>> additionalReturnValue) {
  _itemsToReturn.push(std::move(additionalReturnValue));

  return *this;
}

template <bool passBlocksThrough>
BlockFetcherMock<passBlocksThrough>& BlockFetcherMock<passBlocksThrough>::andThenReturn(
    std::vector<std::pair<ExecutionState, std::shared_ptr<AqlItemBlockShell>>> additionalReturnValues) {
  for (auto& it : additionalReturnValues) {
    andThenReturn(std::move(it));
  }

  return *this;
}

template <bool passBlocksThrough>
bool BlockFetcherMock<passBlocksThrough>::allBlocksFetched() const {
  return _itemsToReturn.empty();
}

template <bool passBlocksThrough>
size_t BlockFetcherMock<passBlocksThrough>::numFetchBlockCalls() const {
  return _numFetchBlockCalls;
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb

template class ::arangodb::tests::aql::BlockFetcherMock<true>;
template class ::arangodb::tests::aql::BlockFetcherMock<false>;
