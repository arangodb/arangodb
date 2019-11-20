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
#include <Logger/LogMacros.h>

#include "Aql/AqlItemBlock.h"
#include "Aql/DependencyProxy.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"

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
  atMost = (std::min)(atMost, ExecutionBlock::DefaultBatchSize());
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
std::pair<ExecutionState, SharedAqlItemBlockPtr>
SingleRowFetcher<passBlocksThrough>::fetchBlockForPassthrough(size_t atMost) {
  return _dependencyProxy->fetchBlockForPassthrough(atMost);
}

template <BlockPassthrough passBlocksThrough>
std::tuple<ExecutionState, size_t, AqlItemBlockInputRange>
SingleRowFetcher<passBlocksThrough>::execute(AqlCallStack& stack) {
  auto [state, skipped, block] = _dependencyProxy->execute(stack);
  if (state == ExecutionState::WAITING) {
    // On waiting we have nothing to return
    return {state, 0, AqlItemBlockInputRange{ExecutorState::HASMORE}};
  }
  if (block == nullptr) {
    return {state, skipped, AqlItemBlockInputRange{ExecutorState::DONE}};
  }

  auto [start, end] = block->getRelevantRange();
  if (state == ExecutionState::HASMORE) {
    TRI_ASSERT(block != nullptr);
    return {state, skipped,
            AqlItemBlockInputRange{ExecutorState::HASMORE, block, start, end}};
  }
  return {state, skipped, AqlItemBlockInputRange{ExecutorState::DONE, block, start, end}};
}

template <BlockPassthrough passBlocksThrough>
std::pair<ExecutionState, size_t> SingleRowFetcher<passBlocksThrough>::skipRows(size_t atMost) {
  TRI_ASSERT(!_currentRow.isInitialized() || _currentRow.isLastRowInBlock());
  TRI_ASSERT(!indexIsValid());

  auto res = _dependencyProxy->skipSome(atMost);
  _upstreamState = res.first;

  TRI_ASSERT(res.second <= atMost);

  return res;
}

template <BlockPassthrough passBlocksThrough>
bool SingleRowFetcher<passBlocksThrough>::fetchBlockIfNecessary(size_t atMost) {
  // Fetch a new block iff necessary
  if (!indexIsValid()) {
    // This returns the AqlItemBlock to the ItemBlockManager before fetching a
    // new one, so we might reuse it immediately!
    _currentBlock = nullptr;

    ExecutionState state;
    SharedAqlItemBlockPtr newBlock;
    std::tie(state, newBlock) = fetchBlock(atMost);
    if (state == ExecutionState::WAITING) {
      return false;
    }

    _currentBlock = std::move(newBlock);
    _rowIndex = 0;
  }
  return true;
}

template <BlockPassthrough passBlocksThrough>
std::pair<ExecutionState, InputAqlItemRow> SingleRowFetcher<passBlocksThrough>::fetchRow(size_t atMost) {
  if (!fetchBlockIfNecessary(atMost)) {
    return {ExecutionState::WAITING, InputAqlItemRow{CreateInvalidInputRowHint{}}};
  }
  if (_currentShadowRow.isInitialized()) {
    // Reset shadow rows as soon as we ask for data.
    _currentShadowRow = ShadowAqlItemRow{CreateInvalidShadowRowHint{}};
  }

  if (_currentBlock == nullptr) {
    TRI_ASSERT(_upstreamState == ExecutionState::DONE);
    _currentRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
  } else {
    TRI_ASSERT(_currentBlock != nullptr);
    TRI_ASSERT(_upstreamState != ExecutionState::WAITING);
    if (_currentBlock->isShadowRow(_rowIndex)) {
      _currentRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
    } else {
      _currentRow = InputAqlItemRow{_currentBlock, _rowIndex};
      _rowIndex++;
    }
  }
  return {returnState(false), _currentRow};
}

template <BlockPassthrough passBlocksThrough>
std::pair<ExecutionState, ShadowAqlItemRow> SingleRowFetcher<passBlocksThrough>::fetchShadowRow(size_t atMost) {
  // TODO We should never fetch from upstream here, as we can't know atMost - only the executor does.
  if (!fetchBlockIfNecessary(atMost)) {
    return {ExecutionState::WAITING, ShadowAqlItemRow{CreateInvalidShadowRowHint{}}};
  }

  if (_currentBlock == nullptr) {
    TRI_ASSERT(_upstreamState == ExecutionState::DONE);
    _currentShadowRow = ShadowAqlItemRow{CreateInvalidShadowRowHint{}};
  } else {
    if (_currentBlock->isShadowRow(_rowIndex)) {
      auto next = ShadowAqlItemRow{_currentBlock, _rowIndex};
      if (_currentShadowRow.isInitialized() && next.isRelevant()) {
        // Special case, we are in the return shadow row case, but the next row
        // is relevant We are required that we call fetchRow in between
        return {returnState(true), ShadowAqlItemRow{CreateInvalidShadowRowHint{}}};
      }
      _currentShadowRow = ShadowAqlItemRow{_currentBlock, _rowIndex};
      _rowIndex++;
    } else {
      _currentShadowRow = ShadowAqlItemRow{CreateInvalidShadowRowHint{}};
    }
  }

  return {returnState(true), _currentShadowRow};
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

template class ::arangodb::aql::SingleRowFetcher<BlockPassthrough::Disable>;
template class ::arangodb::aql::SingleRowFetcher<BlockPassthrough::Enable>;
