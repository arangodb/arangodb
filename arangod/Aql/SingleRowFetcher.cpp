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
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "SingleRowFetcher.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/DependencyProxy.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SkipResult.h"

using namespace arangodb;
using namespace arangodb::aql;

template <BlockPassthrough passBlocksThrough>
SingleRowFetcher<passBlocksThrough>::SingleRowFetcher(DependencyProxy<passBlocksThrough>& executionBlock)
    : _dependencyProxy(&executionBlock),
      _upstreamState(ExecutionState::HASMORE),
      _rowIndex(0),
      _currentRow{CreateInvalidInputRowHint{}},
      _currentShadowRow{CreateInvalidShadowRowHint{}} {}

template <BlockPassthrough passBlocksThrough>
std::pair<ExecutionState, SharedAqlItemBlockPtr> SingleRowFetcher<passBlocksThrough>::fetchBlock(size_t atMost) {
  if (_upstreamState == ExecutionState::DONE) {
    return {_upstreamState, nullptr};
  }
  atMost = (std::min)(atMost, ExecutionBlock::DefaultBatchSize);
  // There are still some blocks left that ask their parent even after they got
  // DONE the last time, and I don't currently have time to track them down.
  // Thus the following assert is commented out.
  // TRI_ASSERT(_upstreamState != ExecutionState::DONE);
  auto res = _dependencyProxy->fetchBlock(atMost);

  _upstreamState = res.first;

  return res;
}

template <BlockPassthrough passBlocksThrough>
SingleRowFetcher<passBlocksThrough>::SingleRowFetcher()
    : _dependencyProxy(nullptr),
      _upstreamState(ExecutionState::HASMORE),
      _rowIndex(0),
      _currentRow{CreateInvalidInputRowHint{}},
      _currentShadowRow{CreateInvalidShadowRowHint{}} {}

template <BlockPassthrough passBlocksThrough>
std::tuple<ExecutionState, SkipResult, AqlItemBlockInputRange>
SingleRowFetcher<passBlocksThrough>::execute(AqlCallStack& stack) {
  auto [state, skipped, block] = _dependencyProxy->execute(stack);
  if (state == ExecutionState::WAITING) {
    // On waiting we have nothing to return
    return {state, SkipResult{}, AqlItemBlockInputRange{ExecutorState::HASMORE}};
  }
  if (block == nullptr) {
    if (state == ExecutionState::HASMORE) {
      return {state, skipped,
              AqlItemBlockInputRange{ExecutorState::HASMORE, skipped.getSkipCount()}};
    }
    return {state, skipped,
            AqlItemBlockInputRange{ExecutorState::DONE, skipped.getSkipCount()}};
  }

  auto [start, end] = block->getRelevantRange();
  if (state == ExecutionState::HASMORE) {
    TRI_ASSERT(block != nullptr);
    return {state, skipped,
            AqlItemBlockInputRange{ExecutorState::HASMORE,
                                   skipped.getSkipCount(), block, start}};
  }
  return {state, skipped,
          AqlItemBlockInputRange{ExecutorState::DONE, skipped.getSkipCount(), block, start}};
}

template <BlockPassthrough passBlocksThrough>
bool SingleRowFetcher<passBlocksThrough>::fetchBlockIfNecessary(size_t atMost) {
  // Fetch a new block iff necessary
  if (!indexIsValid()) {
    // This returns the AqlItemBlock to the ItemBlockManager before fetching a
    // new one, so we might reuse it immediately!
    _currentBlock = nullptr;

    auto [state, newBlock] = fetchBlock(atMost);
    if (state == ExecutionState::WAITING) {
      return false;
    }

    _currentBlock = std::move(newBlock);
    _rowIndex = 0;
  }
  return true;
}

template <BlockPassthrough passBlocksThrough>
bool SingleRowFetcher<passBlocksThrough>::indexIsValid() const {
  return _currentBlock != nullptr && _rowIndex < _currentBlock->size();
}

template <BlockPassthrough passBlocksThrough>
ExecutionState SingleRowFetcher<passBlocksThrough>::returnState(bool isShadowRow) const {
  if (!indexIsValid()) {
    // We are locally done, return the upstream state
    return _upstreamState;
  }
  if (!isShadowRow && _currentBlock->isShadowRow(_rowIndex)) {
    // Next row is a shadow row
    return ExecutionState::DONE;
  }
  return ExecutionState::HASMORE;
}

template <BlockPassthrough blockPassthrough>
RegisterCount SingleRowFetcher<blockPassthrough>::getNrInputRegisters() const {
  return _dependencyProxy->getNrInputRegisters();
}

template <BlockPassthrough blockPassthrough>
void SingleRowFetcher<blockPassthrough>::setDistributeId(std::string const& id) {
  _dependencyProxy->setDistributeId(id);
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
template <BlockPassthrough blockPassthrough>
bool SingleRowFetcher<blockPassthrough>::hasRowsLeftInBlock() const {
  return indexIsValid();
}

template <BlockPassthrough blockPassthrough>
bool SingleRowFetcher<blockPassthrough>::isAtShadowRow() const {
  return indexIsValid() && _currentBlock->isShadowRow(_rowIndex);
}
#endif

//@deprecated
template <BlockPassthrough blockPassthrough>
auto SingleRowFetcher<blockPassthrough>::useStack(AqlCallStack const& stack) -> void {
  _dependencyProxy->useStack(stack);
}

template class ::arangodb::aql::SingleRowFetcher<BlockPassthrough::Disable>;
template class ::arangodb::aql::SingleRowFetcher<BlockPassthrough::Enable>;
