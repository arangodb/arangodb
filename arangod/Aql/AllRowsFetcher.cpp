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

#include "AllRowsFetcher.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/DependencyProxy.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SortExecutor.h"

using namespace arangodb;
using namespace arangodb::aql;

std::pair<ExecutionState, AqlItemMatrix const*> AllRowsFetcher::fetchAllRows() {
  // Avoid unnecessary upstream calls
  if (_upstreamState == ExecutionState::DONE) {
    TRI_ASSERT(_aqlItemMatrix != nullptr);
    return {ExecutionState::DONE, _aqlItemMatrix.get()};
  }
  if (fetchUntilDone() == ExecutionState::WAITING) {
    return {ExecutionState::WAITING, nullptr};
  }
  TRI_ASSERT(_aqlItemMatrix != nullptr);
  return {ExecutionState::DONE, _aqlItemMatrix.get()};
}

ExecutionState AllRowsFetcher::fetchUntilDone() {
  if (_aqlItemMatrix == nullptr) {
    _aqlItemMatrix = std::make_unique<AqlItemMatrix>(getNrInputRegisters());
  }

  ExecutionState state = ExecutionState::HASMORE;
  SharedAqlItemBlockPtr block;

  while (state == ExecutionState::HASMORE) {
    std::tie(state, block) = fetchBlock();
    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(block == nullptr);
      return state;
    }
    if (block == nullptr) {
      TRI_ASSERT(state == ExecutionState::DONE);
    } else {
      _aqlItemMatrix->addBlock(std::move(block));
    }
  }

  TRI_ASSERT(_aqlItemMatrix != nullptr);
  TRI_ASSERT(state == ExecutionState::DONE);
  return state;
}

std::pair<ExecutionState, size_t> AllRowsFetcher::preFetchNumberOfRows(size_t) {
  if (_upstreamState == ExecutionState::DONE) {
    TRI_ASSERT(_aqlItemMatrix != nullptr);
    return {ExecutionState::DONE, _aqlItemMatrix->size()};
  }
  if (fetchUntilDone() == ExecutionState::WAITING) {
    return {ExecutionState::WAITING, 0};
  }
  TRI_ASSERT(_aqlItemMatrix != nullptr);
  return {ExecutionState::DONE, _aqlItemMatrix->size()};
}

AllRowsFetcher::AllRowsFetcher(DependencyProxy<false>& executionBlock)
    : _dependencyProxy(&executionBlock),
      _aqlItemMatrix(nullptr),
      _upstreamState(ExecutionState::HASMORE),
      _blockToReturnNext(0) {}

RegisterId AllRowsFetcher::getNrInputRegisters() const {
  return _dependencyProxy->getNrInputRegisters();
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> AllRowsFetcher::fetchBlock() {
  auto res = _dependencyProxy->fetchBlock();

  _upstreamState = res.first;

  return res;
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> AllRowsFetcher::fetchBlockForModificationExecutor(
    std::size_t limit = ExecutionBlock::DefaultBatchSize()) {
  while (_upstreamState != ExecutionState::DONE) {
    auto state = fetchUntilDone();
    if (state == ExecutionState::WAITING) {
      return {state, nullptr};
    }
  }
  TRI_ASSERT(_aqlItemMatrix != nullptr);
  auto size = _aqlItemMatrix->numberOfBlocks();
  if (_blockToReturnNext >= size) {
    return {ExecutionState::DONE, nullptr};
  }
  auto blk = _aqlItemMatrix->getBlock(_blockToReturnNext);
  ++_blockToReturnNext;
  return {(_blockToReturnNext < size ? ExecutionState::HASMORE : ExecutionState::DONE),
          std::move(blk)};
}

ExecutionState AllRowsFetcher::upstreamState() {
  if (_aqlItemMatrix == nullptr) {
    // We have not pulled anything yet!
    return ExecutionState::HASMORE;
  }
  if (_upstreamState == ExecutionState::WAITING) {
    return ExecutionState::WAITING;
  }
  if (_blockToReturnNext >= _aqlItemMatrix->numberOfBlocks()) {
    return ExecutionState::DONE;
  }
  return ExecutionState::HASMORE;
}
