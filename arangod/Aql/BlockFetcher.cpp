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
  SharedAqlItemBlockPtr block;
  do {
    // Note: upstreamBlock will return next dependency
    // if we need to lopp here
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
      if (!advanceDependency()) {
        return state;
      }
    }
  } while (block == nullptr);

  // Now we definitely have a block.
  TRI_ASSERT(block != nullptr);

  if (state == ExecutionState::DONE) {
    // We need to modify the state here s.t. on next call we fetch from
    // next dependency and do not return DONE on first dependency
    if (advanceDependency()) {
      state = ExecutionState::HASMORE;
    }
  }
  if /* constexpr */ (passBlocksThrough) {
    // Reposit block for pass-through executors.
    _blockPassThroughQueue.push({state, block});
  }
  _blockQueue.push({state, std::move(block)});

  return ExecutionState::HASMORE;
}

template <bool passBlocksThrough>
std::pair<ExecutionState, SharedAqlItemBlockPtr>
// NOLINTNEXTLINE google-default-arguments
BlockFetcher<passBlocksThrough>::fetchBlock(size_t atMost) {
  if (_blockQueue.empty()) {
    ExecutionState state = prefetchBlock(atMost);
    // prefetchBlock returns HASMORE iff it pushed a block onto _blockQueue.
    // If it didn't, it got either WAITING from upstream, or DONE + nullptr.
    if (state == ExecutionState::WAITING || state == ExecutionState::DONE) {
      return {state, nullptr};
    }
    TRI_ASSERT(state == ExecutionState::HASMORE);
  }

  TRI_ASSERT(!_blockQueue.empty());

  ExecutionState state;
  SharedAqlItemBlockPtr block;
  std::tie(state, block) = _blockQueue.front();
  _blockQueue.pop();

  return {state, std::move(block)};
}

template <bool passBlocksThrough>
std::pair<ExecutionState, SharedAqlItemBlockPtr>
// NOLINTNEXTLINE google-default-arguments
BlockFetcher<passBlocksThrough>::fetchBlockForDependency(size_t dependency, size_t atMost) {
  TRI_ASSERT(!passBlocksThrough);
  ExecutionBlock& upstream = upstreamBlockForDependency(dependency);

  TRI_ASSERT(atMost > 0);
  ExecutionState state;
  SharedAqlItemBlockPtr block;
  std::tie(state, block) = upstream.getSome(atMost);
  TRI_IF_FAILURE("ExecutionBlock::getBlock") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (state == ExecutionState::WAITING) {
    TRI_ASSERT(block == nullptr);
    return {state, nullptr};
  }

  if (block == nullptr) {
    // We're not waiting and didn't get a block, so we have to be done.
    TRI_ASSERT(state == ExecutionState::DONE);
    return {state, nullptr};
  }

  // Now we definitely have a block.
  TRI_ASSERT(block != nullptr);

  return {state, block};
}

template <bool allowBlockPassthrough>
std::pair<ExecutionState, SharedAqlItemBlockPtr>
BlockFetcher<allowBlockPassthrough>::fetchBlockForPassthrough(size_t atMost) {
  TRI_ASSERT(allowBlockPassthrough);  // TODO check this with enable_if in the header already

  if (_blockPassThroughQueue.empty()) {
    ExecutionState state = prefetchBlock(atMost);
    // prefetchBlock returns HASMORE iff it pushed a block onto _blockPassThroughQueue.
    // If it didn't, it got either WAITING from upstream, or DONE + nullptr.
    if (state == ExecutionState::WAITING || state == ExecutionState::DONE) {
      return {state, nullptr};
    }
    TRI_ASSERT(state == ExecutionState::HASMORE);
  }

  TRI_ASSERT(!_blockPassThroughQueue.empty());

  ExecutionState state;
  SharedAqlItemBlockPtr block;
  std::tie(state, block) = _blockPassThroughQueue.front();
  _blockPassThroughQueue.pop();

  return {state, std::move(block)};
}

template class ::arangodb::aql::BlockFetcher<true>;
template class ::arangodb::aql::BlockFetcher<false>;
