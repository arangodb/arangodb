////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "ReadAllExecutionBlock.h"

#include "Aql/AqlCallStack.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/QueryContext.h"
#include "Basics/Exceptions.h"

using namespace arangodb::aql;

ReadAllExecutionBlock::ReadAllExecutionBlock(ExecutionEngine* engine, ExecutionNode const* en)
    : ExecutionBlock(engine, en), _query(engine->getQuery()) {}

ReadAllExecutionBlock::~ReadAllExecutionBlock() = default;

[[nodiscard]] QueryContext const& ReadAllExecutionBlock::getQuery() const {
  return _query;
}

std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> ReadAllExecutionBlock::execute(AqlCallStack stack) {
  if (getQuery().killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
  traceExecuteBegin(stack);
  // silence tests -- we need to introduce new failure tests for fetchers
  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome1") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome2") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  auto res = executeWithoutTrace(std::move(stack));
  traceExecuteEnd(res);
  return res;
}

auto ReadAllExecutionBlock::executeWithoutTrace(AqlCallStack stack)
    -> std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> {
  // This block only works with a single dependency
  TRI_ASSERT(_dependencies.size() == 1);

  // Invariant can only work if all calls of the stack are fetchAll
  TRI_ASSERT(stack.validateAllCallsAreFetchAll());


  // This block fetches until the upstream returns done, before it returns anything
  while (_upstreamState != ExecutionState::DONE) {
    auto [state, skipResult, block] =  _dependencies.at(0)->execute(stack);
    if (state == ExecutionState::WAITING) {
      return {state, SkipResult{}, nullptr};
    }
    _upstreamState = state;
    // We can never skip anything here.
    TRI_ASSERT(skipResult.nothingSkipped());
    _skipped = skipResult;
    _blocks.emplace_back(std::move(block));
  }
  // We can never skip anything here.
  TRI_ASSERT(_skipped.nothingSkipped());

  if (!_blocks.empty()) {
    auto block = _blocks.front();
    _blocks.pop_front();
    if (_blocks.empty()) {
      return {ExecutionState::DONE, _skipped, std::move(block)};
    }
    return {ExecutionState::HASMORE, _skipped, std::move(block)};
  }
  return {ExecutionState::DONE, _skipped, nullptr};
}
