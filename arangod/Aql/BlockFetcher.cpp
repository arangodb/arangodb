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

#include "BlockFetcher.h"

using namespace arangodb::aql;

template <bool passBlocksThrough>
ExecutionState BlockFetcher<passBlocksThrough>::prefetchBlock(size_t atMost) {
  TRI_ASSERT(atMost > 0);
  ExecutionState state;
  std::unique_ptr<AqlItemBlock> block;
  std::tie(state, block) = upstreamBlock().getSome(atMost);
    TRI_IF_FAILURE("ExecutionBlock::getBlock") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

  if (state == ExecutionState::WAITING) {
    TRI_ASSERT(block == nullptr);
    return state;
  }

  if (block == nullptr) {
    // We're not waiting and didn't get a block, so we have to be done.
    TRI_ASSERT(state == ExecutionState::DONE);
    return state;
  }

  // Now we definitely have a block.
  TRI_ASSERT(block != nullptr);

  auto blockShell =
      std::make_shared<AqlItemBlockShell>(itemBlockManager(), std::move(block));

  if /* constexpr */ (passBlocksThrough) {
    // Reposit blockShell for pass-through executors.
    _blockShellPassThroughQueue.push({state, blockShell});
  }

  _blockShellQueue.push({state, blockShell});
  return ExecutionState::HASMORE;
}

template <bool passBlocksThrough>
std::pair<ExecutionState, std::shared_ptr<AqlItemBlockShell>>
// NOLINTNEXTLINE google-default-arguments
BlockFetcher<passBlocksThrough>::fetchBlock(size_t atMost) {
  if (_blockShellQueue.empty()) {
    ExecutionState state = prefetchBlock(atMost);
    // prefetchBlock returns HASMORE iff it pushed a block onto _blockShellQueue.
    // If it didn't, it got either WAITING from upstream, or DONE + nullptr.
    if (state == ExecutionState::WAITING || state == ExecutionState::DONE) {
      return {state, nullptr};
    }
    TRI_ASSERT(state == ExecutionState::HASMORE);
  }

  ExecutionState state;
  std::shared_ptr<AqlItemBlockShell> blockShell;
  std::tie(state, blockShell) = _blockShellQueue.front();
  _blockShellQueue.pop();

  //auto inputBlockShell =
  //    std::make_shared<InputAqlItemBlockShell>(blockShell, _inputRegisters);
  return {state, blockShell};
}

template <bool allowBlockPassthrough>
std::pair<ExecutionState, std::shared_ptr<AqlItemBlockShell>>
BlockFetcher<allowBlockPassthrough>::fetchBlockForPassthrough(size_t atMost) {
  TRI_ASSERT(allowBlockPassthrough);  // TODO check this with enable_if in the header already

  if (_blockShellPassThroughQueue.empty()) {
    ExecutionState state = prefetchBlock(atMost);
    // prefetchBlock returns HASMORE iff it pushed a block onto _blockShellPassThroughQueue.
    // If it didn't, it got either WAITING from upstream, or DONE + nullptr.
    if (state == ExecutionState::WAITING || state == ExecutionState::DONE) {
      return {state, nullptr};
    }
    TRI_ASSERT(state == ExecutionState::HASMORE);
  }

  ExecutionState state;
  std::shared_ptr<AqlItemBlockShell> blockShell;
  std::tie(state, blockShell) = _blockShellPassThroughQueue.front();
  _blockShellPassThroughQueue.pop();

  return {state, std::move(blockShell)};
}

template class ::arangodb::aql::BlockFetcher<true>;
template class ::arangodb::aql::BlockFetcher<false>;
