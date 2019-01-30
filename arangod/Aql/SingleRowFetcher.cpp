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

#include "Aql/SingleRowFetcher.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/BlockFetcher.h"
#include "Aql/FilterExecutor.h"
#include "SingleRowFetcher.h"

using namespace arangodb;
using namespace arangodb::aql;

template <bool passBlocksThrough>
std::pair<ExecutionState, InputAqlItemRow> SingleRowFetcher<passBlocksThrough>::fetchRow() {
  // Fetch a new block iff necessary
  if (!indexIsValid()) {
    // This returns the AqlItemBlock to the ItemBlockManager before fetching a
    // new one, so we might reuse it immediately!
    _currentBlock = nullptr;

    ExecutionState state;
    std::shared_ptr<InputAqlItemBlockShell> newBlock;
    std::tie(state, newBlock) = fetchBlock();
    if (state == ExecutionState::WAITING) {
      return {ExecutionState::WAITING, InputAqlItemRow{CreateInvalidInputRowHint{}}};
    }

    _currentBlock = std::move(newBlock);
    _rowIndex = 0;
  }

  ExecutionState rowState;

  if (_currentBlock == nullptr) {
    TRI_ASSERT(_upstreamState == ExecutionState::DONE);
    _currentRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
    rowState = ExecutionState::DONE;
  } else {
    TRI_ASSERT(_currentBlock);
    _currentRow = InputAqlItemRow{_currentBlock, _rowIndex};

    TRI_ASSERT(_upstreamState != ExecutionState::WAITING);
    if (isLastRowInBlock() && _upstreamState == ExecutionState::DONE) {
      rowState = ExecutionState::DONE;
    } else {
      rowState = ExecutionState::HASMORE;
    }

    _rowIndex++;
  }

  return {rowState, _currentRow};
}

template <bool passBlocksThrough>
SingleRowFetcher<passBlocksThrough>::SingleRowFetcher(BlockFetcher<passBlocksThrough>& executionBlock)
    : _blockFetcher(&executionBlock), _currentRow{CreateInvalidInputRowHint{}} {}

template <bool passBlocksThrough>
std::pair<ExecutionState, std::shared_ptr<InputAqlItemBlockShell>> SingleRowFetcher<passBlocksThrough>::fetchBlock() {
  auto res = _blockFetcher->fetchBlock();

  _upstreamState = res.first;

  return res;
}

template <bool passBlocksThrough>
RegisterId SingleRowFetcher<passBlocksThrough>::getNrInputRegisters() const {
  return _blockFetcher->getNrInputRegisters();
}

template <bool passBlocksThrough>
bool SingleRowFetcher<passBlocksThrough>::indexIsValid() {
  // TODO Hopefully we can get rid of this distinction later. When there are no
  // more old blocks, we can replace the old getSome interface easily,
  // specifically replace std::unique_ptr<AqlItemBlock> with a shared_ptr
  // with a custom deleter (like AqlItemBlockShell), or a shared_ptr to an
  // AqlItemBlockShell. Then we will never lose the block.
  if /* constexpr */ (passBlocksThrough) {
    return _currentBlock != nullptr && _currentBlock->hasBlock() &&
           _rowIndex < _currentBlock->block().size();
  } else {
    // The current block must never become invalid (i.e. !hasBlock()), unless
    // it's passed through and therefore the output block steals it.
    TRI_ASSERT(_currentBlock == nullptr || _currentBlock->hasBlock());
    return _currentBlock != nullptr && _rowIndex < _currentBlock->block().size();
  }
}

template <bool passBlocksThrough>
bool SingleRowFetcher<passBlocksThrough>::isLastRowInBlock() {
  TRI_ASSERT(indexIsValid());
  return _rowIndex + 1 == _currentBlock->block().size();
}

template <bool passBlocksThrough>
size_t SingleRowFetcher<passBlocksThrough>::getRowIndex() {
  TRI_ASSERT(indexIsValid());
  return _rowIndex;
}

template <bool passBlocksThrough>
SingleRowFetcher<passBlocksThrough>::SingleRowFetcher()
    : _blockFetcher(nullptr), _currentRow{CreateInvalidInputRowHint{}} {}

template<bool passBlocksThrough>
std::pair<ExecutionState, std::shared_ptr<AqlItemBlockShell>>
SingleRowFetcher<passBlocksThrough>::fetchBlockForPassthrough() {
  return _blockFetcher->fetchBlockForPassthrough();
}

template class ::arangodb::aql::SingleRowFetcher<false>;
template class ::arangodb::aql::SingleRowFetcher<true>;
