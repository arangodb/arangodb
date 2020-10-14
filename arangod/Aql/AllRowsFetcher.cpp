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
#include <Logger/LogMacros.h>

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemMatrix.h"
#include "Aql/DependencyProxy.h"

using namespace arangodb;
using namespace arangodb::aql;

std::tuple<ExecutionState, SkipResult, AqlItemBlockInputMatrix> AllRowsFetcher::execute(AqlCallStack& stack) {
  TRI_ASSERT(stack.peek().getOffset() == 0);
  TRI_ASSERT(!stack.peek().needsFullCount());
  // We allow a 0 hardLimit for bypassing
  // bot otherwise we do not allow any limit
  TRI_ASSERT(!stack.peek().hasHardLimit() || stack.peek().getLimit() == 0);
  TRI_ASSERT(!stack.peek().hasSoftLimit());

  if (_aqlItemMatrix == nullptr) {
    _aqlItemMatrix = std::make_unique<AqlItemMatrix>(getNrInputRegisters());
  }
  // We can only execute More if we are not Stopped yet.
  TRI_ASSERT(!_aqlItemMatrix->stoppedOnShadowRow());
  while (true) {
    auto [state, skipped, block] = _dependencyProxy->execute(stack);
    TRI_ASSERT(skipped.getSkipCount() == 0);

    // we will either build a complete fetched AqlItemBlockInputMatrix or return an empty one
    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(skipped.nothingSkipped());
      TRI_ASSERT(block == nullptr);
      // On waiting we have nothing to return
      return {state, SkipResult{}, AqlItemBlockInputMatrix{ExecutorState::HASMORE}};
    }
    TRI_ASSERT(block != nullptr || state == ExecutionState::DONE);

    if (block != nullptr) {
      // we need to store the block for later creation of AqlItemBlockInputMatrix
      _aqlItemMatrix->addBlock(std::move(block));
    }

    // If we find a ShadowRow or ExecutionState == Done, we're done fetching.
    if (_aqlItemMatrix->stoppedOnShadowRow() || state == ExecutionState::DONE) {
      if (state == ExecutionState::HASMORE) {
        return {state, skipped,
                AqlItemBlockInputMatrix{ExecutorState::HASMORE, _aqlItemMatrix.get()}};
      }
      return {state, skipped,
              AqlItemBlockInputMatrix{ExecutorState::DONE, _aqlItemMatrix.get()}};
    }
  }
}

ExecutionState AllRowsFetcher::fetchData() {
  if (_upstreamState == ExecutionState::DONE) {
    TRI_ASSERT(_aqlItemMatrix != nullptr);
    return ExecutionState::DONE;
  }
  if (fetchUntilDone() == ExecutionState::WAITING) {
    return ExecutionState::WAITING;
  }
  TRI_ASSERT(_aqlItemMatrix != nullptr);
  return ExecutionState::DONE;
}

ExecutionState AllRowsFetcher::fetchUntilDone() {
  if (_aqlItemMatrix == nullptr) {
    _aqlItemMatrix = std::make_unique<AqlItemMatrix>(getNrInputRegisters());
  }

  ExecutionState state = ExecutionState::HASMORE;
  SharedAqlItemBlockPtr block;

  while (state == ExecutionState::HASMORE && !_aqlItemMatrix->stoppedOnShadowRow()) {
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
  return state;
}

AllRowsFetcher::AllRowsFetcher(DependencyProxy<BlockPassthrough::Disable>& executionBlock)
    : _dependencyProxy(&executionBlock),
      _aqlItemMatrix(nullptr),
      _upstreamState(ExecutionState::HASMORE),
      _blockToReturnNext(0) {}

RegisterCount AllRowsFetcher::getNrInputRegisters() const {
  return _dependencyProxy->getNrInputRegisters();
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> AllRowsFetcher::fetchBlock() {
  auto res = _dependencyProxy->fetchBlock();

  _upstreamState = res.first;

  return res;
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

//@deprecated
auto AllRowsFetcher::useStack(AqlCallStack const& stack) -> void {
  _dependencyProxy->useStack(stack);
}
