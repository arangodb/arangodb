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

using namespace arangodb;
using namespace arangodb::aql;

std::pair<ExecutionState, const AqlItemRow*> SingleRowFetcher::fetchRow() {
  // Fetch a new block iff necessary
  if (_currentBlock == nullptr || _rowIndex > _currentBlock->size()) {
    ExecutionState state;
    std::unique_ptr<AqlItemBlock> newBlock;
    std::tie(state, newBlock) = fetchBlock();
    if (state == ExecutionState::WAITING) {
      return {ExecutionState::WAITING, nullptr};
    }

    _currentBlock.swap(newBlock);
    _rowIndex = 0;
  }

  if (_currentBlock == nullptr) {
    _currentRow = nullptr;
  } else {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    _currentRow = std::make_unique<AqlItemRow>(_currentBlock.get(), _rowIndex,
                                               getNrInputRegisters());
#else
    _currentRow = std::make_unique<AqlItemRow>(_currentBlock.get(), _rowIndex);
#endif
  }

  return {ExecutionState::DONE, _currentRow.get()};
}

SingleRowFetcher::SingleRowFetcher(BlockFetcher& executionBlock)
    : _blockFetcher(&executionBlock) {}

std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>>
SingleRowFetcher::fetchBlock() {
  auto res = _blockFetcher->fetchBlock();

  _upstreamState = res.first;

  return res;
}

RegisterId SingleRowFetcher::getNrInputRegisters() const {
  return _blockFetcher->getNrInputRegisters();
}
