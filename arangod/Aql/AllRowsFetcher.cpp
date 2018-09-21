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
#include "Aql/BlockFetcher.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SortExecutor.h"

using namespace arangodb;
using namespace arangodb::aql;

std::pair<ExecutionState, AqlItemMatrix const*> AllRowsFetcher::fetchAllRows() {
  // Avoid unnecessary upstream calls
  if (_upstreamState == ExecutionState::DONE) {
    return {ExecutionState::DONE, nullptr};
  }

  if (_aqlItemMatrix == nullptr) {
    _aqlItemMatrix = std::make_unique<AqlItemMatrix>(getNrInputRegisters());
  }

  ExecutionState state = ExecutionState::HASMORE;
  std::unique_ptr<AqlItemBlock> block;

  while(state == ExecutionState::HASMORE) {
    std::tie(state, block) = fetchBlock();
    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(block == nullptr);
      return {ExecutionState::WAITING, nullptr};
    }
    if (block == nullptr) {
      TRI_ASSERT(state == ExecutionState::DONE);
    } else {
      _aqlItemMatrix->addBlock(std::move(block));
    }
  }

  TRI_ASSERT(state == ExecutionState::DONE);

  return {ExecutionState::DONE, _aqlItemMatrix.get()};
}

AllRowsFetcher::AllRowsFetcher(BlockFetcher& executionBlock)
    : _blockFetcher(&executionBlock),
      _aqlItemMatrix(nullptr),
      _upstreamState(ExecutionState::HASMORE) {}

RegisterId AllRowsFetcher::getNrInputRegisters() const {
  return _blockFetcher->getNrInputRegisters();
}

AllRowsFetcher::~AllRowsFetcher() {
  if (_aqlItemMatrix != nullptr) {
    std::vector<std::unique_ptr<AqlItemBlock>> blocks{
        _aqlItemMatrix->stealBlocks()};
    for (auto& it : blocks) {
      _blockFetcher->returnBlock(std::move(it));
    }
  }
}

std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>>
AllRowsFetcher::fetchBlock() {
  auto res = _blockFetcher->fetchBlock();

  _upstreamState = res.first;

  return res;
}
