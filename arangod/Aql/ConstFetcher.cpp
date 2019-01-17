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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ConstFetcher.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/BlockFetcher.h"
#include "Aql/FilterExecutor.h"
#include "ConstFetcher.h"


using namespace arangodb;
using namespace arangodb::aql;

std::pair<ExecutionState, InputAqlItemRow> ConstFetcher::fetchRow() {
  // Fetch a new block iff necessary
  if (_currentBlock == nullptr || !indexIsValid()) {
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

ConstFetcher::ConstFetcher(BlockFetcher& executionBlock)
    : _blockFetcher(&executionBlock),
      _currentRow{CreateInvalidInputRowHint{}} {}

std::pair<ExecutionState, std::shared_ptr<InputAqlItemBlockShell>>
ConstFetcher::fetchBlock() {
  auto res = _blockFetcher->fetchBlock();

  _upstreamState = res.first;

  return res;
}

RegisterId ConstFetcher::getNrInputRegisters() const {
  return _blockFetcher->getNrInputRegisters();
}

bool ConstFetcher::indexIsValid() {
  return _currentBlock != nullptr && _rowIndex + 1 <= _currentBlock->block().size();
}

bool ConstFetcher::isLastRowInBlock() {
  TRI_ASSERT(indexIsValid());
  return _rowIndex + 1 == _currentBlock->block().size();
}

size_t ConstFetcher::getRowIndex() {
  TRI_ASSERT(indexIsValid());
  return _rowIndex;
}


ConstFetcher::ConstFetcher()
    : _blockFetcher(nullptr), _currentRow{CreateInvalidInputRowHint{}} {}
