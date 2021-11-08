////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#include "FixedOutputExecutionBlockMock.h"

#include "Aql/AqlCallStack.h"
#include "Basics/voc-errors.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests::aql;

namespace {
// NOTE copy pasted from Waiting ExecutionBlock mock
static auto blocksToInfos(std::deque<SharedAqlItemBlockPtr> const& blocks) -> RegisterInfos {
  // If there are no blocks injected, we have nothing to analyze.
  // This Mock does only work with predefined data output.
  TRI_ASSERT(!blocks.empty());
  auto readInput = RegIdSet{};
  auto writeOutput = RegIdSet{};
  RegIdSet toClear{};
  RegIdSetStack toKeep{{}};
  RegisterCount regs = 1;
  for (auto const& b : blocks) {
    if (b != nullptr) {
      // Find the first non-nullptr block
      regs = b->numRegisters();

      break;
    }
  }

  for (RegisterId::value_t r = 0; r < regs; ++r) {
    toKeep.back().emplace(r);
  }
  return {readInput, writeOutput, regs, regs, toClear, toKeep};
}
}  // namespace

FixedOutputExecutionBlockMock::FixedOutputExecutionBlockMock(ExecutionEngine* engine,
                                                             ExecutionNode const* node,
                                                             std::deque<SharedAqlItemBlockPtr>&& data)
    : ExecutionBlock(engine, node), _infos{::blocksToInfos(data)}, _blockData{std::move(data)} {}

std::pair<ExecutionState, arangodb::Result> FixedOutputExecutionBlockMock::initializeCursor(
    InputAqlItemRow const& input) {
  // Nothing to do
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> FixedOutputExecutionBlockMock::execute(
    AqlCallStack const& stack) {
  SkipResult skipped{};
  for (size_t i = 1; i < stack.subqueryLevel(); ++i) {
    // For every additional subquery level we need to increase the skipped subquery level
    skipped.incrementSubquery();
  }
  if (_blockData.empty()) {
    return {ExecutionState::DONE, skipped, nullptr};
  }
  // This Block is very dump, it does NOT care what you ask it for. it will just deliver what it has in the queue
  auto block = _blockData.front();
  _blockData.pop_front();
  ExecutionState state = _blockData.empty() ? ExecutionState::DONE : ExecutionState::HASMORE;
  return {state, skipped, block};
}
