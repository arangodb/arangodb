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
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/FilterExecutor.h"

using namespace arangodb;
using namespace arangodb::aql;

template <class Executor>
std::pair<ExecutionState, const AqlItemRow*>
SingleRowFetcher<Executor>::fetchRow() {
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

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _currentRow = std::make_unique<AqlItemRow>(_currentBlock.get(), _rowIndex,
                                             getNrInputRegisters());
#else
  _currentRow = std::make_unique<AqlItemRow>(_currentBlock.get(), _rowIndex);
#endif

  return {ExecutionState::DONE, _currentRow.get()};
}

template <class Executor>
SingleRowFetcher<Executor>::SingleRowFetcher(
    ExecutionBlockImpl<Executor>& executionBlock)
    : _executionBlock(&executionBlock) {}

template <class Executor>
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>>
SingleRowFetcher<Executor>::fetchBlock() {
  auto res = _executionBlock->fetchBlock();

  _upstreamState = res.first;

  return res;
}

template <class Executor>
RegisterId SingleRowFetcher<Executor>::getNrInputRegisters() const {
  return _executionBlock->getNrInputRegisters();
}

template class ::arangodb::aql::SingleRowFetcher<FilterExecutor>;
