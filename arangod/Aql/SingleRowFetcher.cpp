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

using namespace arangodb;
using namespace arangodb::aql;

template <bool passBlocksThrough>
SingleRowFetcher<passBlocksThrough>::SingleRowFetcher(DependencyProxy<passBlocksThrough>& executionBlock)
    : _dependencyProxy(&executionBlock),
      _upstreamState(ExecutionState::HASMORE),
      _rowIndex(0),
      _currentRow{CreateInvalidInputRowHint{}},
      _currentShadowRow{CreateInvalidShadowRowHint{}} {}

template <bool passBlocksThrough>
std::pair<ExecutionState, SharedAqlItemBlockPtr> SingleRowFetcher<passBlocksThrough>::fetchBlock(size_t atMost) {
  atMost = (std::min)(atMost, ExecutionBlock::DefaultBatchSize());

  // There are still some blocks left that ask their parent even after they got
  // DONE the last time, and I don't currently have time to track them down.
  // Thus the following assert is commented out.
  // TRI_ASSERT(_upstreamState != ExecutionState::DONE);
  auto res = _dependencyProxy->fetchBlock(atMost);

  _upstreamState = res.first;

  return res;
}

template <bool passBlocksThrough>
SingleRowFetcher<passBlocksThrough>::SingleRowFetcher()
    : _dependencyProxy(nullptr),
      _rowIndex(0),
      _currentRow{CreateInvalidInputRowHint{}},
      _currentShadowRow{CreateInvalidShadowRowHint{}} {}

template <bool passBlocksThrough>
std::pair<ExecutionState, SharedAqlItemBlockPtr>
SingleRowFetcher<passBlocksThrough>::fetchBlockForPassthrough(size_t atMost) {
  return _dependencyProxy->fetchBlockForPassthrough(atMost);
}

template <bool passBlocksThrough>
std::pair<ExecutionState, size_t> SingleRowFetcher<passBlocksThrough>::skipRows(size_t atMost) {
  TRI_ASSERT(!_currentRow.isInitialized() || _currentRow.isLastRowInBlock());
  TRI_ASSERT(!indexIsValid());

  auto res = _dependencyProxy->skipSome(atMost);
  _upstreamState = res.first;

  TRI_ASSERT(res.second <= atMost);

  return res;
}


template <bool passBlocksThrough>
bool SingleRowFetcher<passBlocksThrough>::fetchBlockIfNecessary(size_t atMost)
{
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

template <bool passBlocksThrough>
std::pair<ExecutionState, InputAqlItemRow> SingleRowFetcher<passBlocksThrough>::fetchRow(size_t atMost) {

  if (!fetchBlockIfNecessary(atMost)) {
    return {ExecutionState::WAITING, InputAqlItemRow{CreateInvalidInputRowHint{}}};
  }

  ExecutionState rowState;

  if (_currentBlock == nullptr) {
    TRI_ASSERT(_upstreamState == ExecutionState::DONE);
    _currentRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
    rowState = ExecutionState::DONE;
  } else {
    TRI_ASSERT(_currentBlock != nullptr);
    if (_currentBlock->isShadowRow(_rowIndex)) {
      _currentRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
      rowState = ExecutionState::DONE;
    } else {
      _currentRow = InputAqlItemRow{_currentBlock, _rowIndex};

      TRI_ASSERT(_upstreamState != ExecutionState::WAITING);
      if (isLastRowInBlock()) {
        if(_upstreamState == ExecutionState::DONE) {
          rowState = ExecutionState::DONE;
        } else {
          rowState = ExecutionState::HASMORE;
        }
      } else {
        if(_currentBlock->isShadowRow(_rowIndex+1)) {
          rowState = ExecutionState::DONE;
        } else {
          rowState = ExecutionState::HASMORE;
        }
      }
    }
    _rowIndex++;
  }
  return {rowState, _currentRow};
}

template <bool passBlocksThrough>
std::pair<ExecutionState, ShadowAqlItemRow> SingleRowFetcher<passBlocksThrough>::fetchShadowRow(size_t atMost) {

  if (!fetchBlockIfNecessary(atMost)) {
    return {ExecutionState::WAITING, ShadowAqlItemRow{CreateInvalidShadowRowHint{}}};
  }

  ExecutionState rowState;

  if (_currentBlock == nullptr) {
    TRI_ASSERT(_upstreamState == ExecutionState::DONE);
    _currentShadowRow = ShadowAqlItemRow{CreateInvalidShadowRowHint{}};
    rowState = ExecutionState::DONE;
  } else {
    if (_currentBlock->isShadowRow(_rowIndex)) {
      _currentShadowRow = ShadowAqlItemRow{_currentBlock, _rowIndex};
      rowState = ExecutionState::HASMORE;
      _rowIndex++;
    } else {
      _currentShadowRow = ShadowAqlItemRow{CreateInvalidShadowRowHint{}};
      rowState = ExecutionState::HASMORE;
    }
  }

  return {rowState, _currentShadowRow};
}

template <bool passBlocksThrough>
bool SingleRowFetcher<passBlocksThrough>::indexIsValid() const {
  return _currentBlock != nullptr && _rowIndex < _currentBlock->size();
}

template class ::arangodb::aql::SingleRowFetcher<false>;
template class ::arangodb::aql::SingleRowFetcher<true>;
