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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "WaitingExecutionBlockMock.h"

#include "Aql/AqlCallStack.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionState.h"
#include "Aql/QueryOptions.h"
#include "Aql/SkipResult.h"

#include "Logger/LogMacros.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;

namespace {
static auto blocksToInfos(std::deque<SharedAqlItemBlockPtr> const& blocks) -> ExecutorInfos {
  auto readInput = make_shared_unordered_set();
  auto writeOutput = make_shared_unordered_set();
  std::unordered_set<RegisterId> toClear{};
  std::unordered_set<RegisterId> toKeep{};
  RegisterId regs = 1;
  for (auto const& b : blocks) {
    if (b != nullptr) {
      // Find the first non-nullptr block
      regs = b->getNrRegs();

      break;
    }
  }
  // if non found Sorry blind guess here.
  // Consider adding data first if the test fails

  for (RegisterId r = 0; r < regs; ++r) {
    toKeep.emplace(r);
  }
  return {readInput, writeOutput, regs, regs, toClear, toKeep};
}
}  // namespace
WaitingExecutionBlockMock::WaitingExecutionBlockMock(ExecutionEngine* engine,
                                                     ExecutionNode const* node,
                                                     std::deque<SharedAqlItemBlockPtr>&& data,
                                                     WaitingBehaviour variant)
    : ExecutionBlock(engine, node),
      _data(std::move(data)),
      _inflight(0),
      _hasWaited(false),
      _variant{variant},
      _infos{::blocksToInfos(_data)},
      _blockData{*engine, node, _infos} {
  SkipResult s;
  for (auto const& b : _data) {
    if (b != nullptr) {
      TRI_ASSERT(s.nothingSkipped());
      _blockData.addBlock(b, s);
      if (b->hasShadowRows()) {
        _doesContainShadowRows = true;
      }
    }
  }

  if (!_data.empty() && _data.back() == nullptr) {
    // If the last block in _data is explicitly a nullptr
    // we will lie on the last row
    _shouldLieOnLastRow = true;
  }
}

std::pair<arangodb::aql::ExecutionState, arangodb::Result> WaitingExecutionBlockMock::initializeCursor(
    arangodb::aql::InputAqlItemRow const& input) {
  if (!_hasWaited) {
    _hasWaited = true;
    return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
  }
  _hasWaited = false;
  _inflight = 0;
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

std::pair<arangodb::aql::ExecutionState, Result> WaitingExecutionBlockMock::shutdown(int errorCode) {
  ExecutionState state;
  Result res;
  return std::make_pair(state, res);
}

std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> WaitingExecutionBlockMock::execute(AqlCallStack stack) {
  traceExecuteBegin(stack);
  auto res = executeWithoutTrace(stack);
  traceExecuteEnd(res);
  return res;
}

#if true

std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> WaitingExecutionBlockMock::executeWithoutTrace(
    AqlCallStack stack) {
  while (!stack.isRelevant()) {
    stack.pop();
  }
  auto myCall = stack.peek();
  TRI_ASSERT(!(myCall.getOffset() == 0 && myCall.softLimit == AqlCall::Limit{0}));
  TRI_ASSERT(!(myCall.hasSoftLimit() && myCall.fullCount));
  TRI_ASSERT(!(myCall.hasSoftLimit() && myCall.hasHardLimit()));
  if (_variant != WaitingBehaviour::NEVER && !_hasWaited) {
    // If we ordered waiting check on _hasWaited and wait if not
    _hasWaited = true;
    return {ExecutionState::WAITING, SkipResult{}, nullptr};
  }
  if (_variant == WaitingBehaviour::ALWAYS) {
    // If we always wait, reset.
    _hasWaited = false;
  }
  if (!_blockData.hasDataFor(myCall)) {
    return {ExecutionState::DONE, {}, nullptr};
  }
  SkipResult localSkipped;
  while (true) {
    auto [state, skipped, result] = _blockData.execute(stack, ExecutionState::DONE);
    // We loop here if we only skip
    localSkipped.merge(skipped, false);
    bool shouldReturn = state == ExecutionState::DONE || result != nullptr;

    if (result != nullptr && !result->hasShadowRows()) {
      // Count produced rows
      auto modCall = stack.popCall();
      modCall.didProduce(result->size());
      stack.pushCall(std::move(modCall));
    }

    if (!skipped.nothingSkipped()) {
      auto modCall = stack.popCall();
      modCall.didSkip(skipped.getSkipCount());
      // Reset the internal counter.
      // We reuse the call to upstream
      // this inturn uses this counter to report nrRowsSkipped
      modCall.skippedRows = 0;
      if (!modCall.needSkipMore() && modCall.getLimit() == 0) {
        // We do not have anything to do for this call
        shouldReturn = true;
      }
      stack.pushCall(std::move(modCall));
    }
    if (shouldReturn) {
      if (!_doesContainShadowRows && state == ExecutionState::HASMORE) {
        // FullCount phase, let us loop until we are done
        // We do not have anything to do for this call
        // But let us only do this on top-level queries

        auto call = stack.peek();
        if (call.hasHardLimit() && call.getLimit() == 0) {
          // We are in fullCount/fastForward phase now.
          while (state == ExecutionState::HASMORE) {
            auto [nextState, nextSkipped, nextResult] =
                _blockData.execute(stack, ExecutionState::DONE);
            state = nextState;
            // We are disallowed to have any result here.
            TRI_ASSERT(nextResult == nullptr);
            localSkipped.merge(nextSkipped, false);
          }
        }
      }
      // We want to "lie" on upstream if we have hit a softLimit exactly on the last row
      if (state == ExecutionState::DONE && _shouldLieOnLastRow) {
        auto const& call = stack.peek();
        if (call.hasSoftLimit() && call.getLimit() == 0 && call.getOffset() == 0) {
          state = ExecutionState::HASMORE;
        }
      }

      // We have a valid result.
      return {state, localSkipped, result};
    }
  }
}

#else
// NOTE: Does not care for shadowrows!
std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> WaitingExecutionBlockMock::executeWithoutTrace(
    AqlCallStack stack) {
  while (!stack.isRelevant()) {
    stack.pop();
  }
  auto myCall = stack.popCall();

  TRI_ASSERT(!(myCall.getOffset() == 0 && myCall.softLimit == AqlCall::Limit{0}));
  TRI_ASSERT(!(myCall.hasSoftLimit() && myCall.fullCount));
  TRI_ASSERT(!(myCall.hasSoftLimit() && myCall.hasHardLimit()));

  if (_variant != WaitingBehaviour::NEVER && !_hasWaited) {
    // If we ordered waiting check on _hasWaited and wait if not
    _hasWaited = true;
    return {ExecutionState::WAITING, SkipResult{}, nullptr};
  }
  if (_variant == WaitingBehaviour::ALWAYS) {
    // If we always wait, reset.
    _hasWaited = false;
  }
  size_t skipped = 0;
  SharedAqlItemBlockPtr result = nullptr;
  if (!_data.empty() && _data.front() == nullptr) {
    dropBlock();
  }
  while (!_data.empty()) {
    if (_data.front() == nullptr) {
      if ((skipped > 0 || result != nullptr) &&
          !(myCall.hasHardLimit() && myCall.getLimit() == 0)) {
        // This is a specific break point return now.
        // Sorry we can only return one block.
        // This means we have prepared the first block.
        // But still need more data.
        SkipResult skipRes{};
        skipRes.didSkip(skipped);
        return {ExecutionState::HASMORE, skipRes, result};
      } else {
        dropBlock();
        continue;
      }
    }
    if (_data.front()->size() <= _inflight) {
      dropBlock();
      continue;
    }
    TRI_ASSERT(_data.front()->size() > _inflight);
    // Drop while skip
    if (myCall.getOffset() > 0) {
      size_t canSkip = (std::min)(_data.front()->size() - _inflight, myCall.getOffset());
      _inflight += canSkip;
      myCall.didSkip(canSkip);
      skipped += canSkip;
      continue;
    } else if (myCall.getLimit() > 0) {
      if (result != nullptr) {
        // Sorry we can only return one block.
        // This means we have prepared the first block.
        // But still need more data.
        SkipResult skipRes{};
        skipRes.didSkip(skipped);
        return {ExecutionState::HASMORE, skipRes, result};
      }

      size_t canReturn = _data.front()->size() - _inflight;

      if (canReturn <= myCall.getLimit()) {
        // We can return the remainder of this block
        if (_inflight == 0) {
          // use full block
          result = std::move(_data.front());
        } else {
          // Slice out the last part
          result = _data.front()->slice(_inflight, _data.front()->size());
        }
        dropBlock();
      } else {
        // Slice out limit many rows starting at _inflight
        result = _data.front()->slice(_inflight, _inflight + myCall.getLimit());
        // adjust _inflight to the fist non-returned row.
        _inflight += myCall.getLimit();
      }
      TRI_ASSERT(result != nullptr);
      myCall.didProduce(result->size());
    } else if (myCall.needsFullCount()) {
      size_t counts = _data.front()->size() - _inflight;
      dropBlock();
      myCall.didSkip(counts);
      skipped += counts;
    } else {
      if (myCall.getLimit() == 0 && !myCall.needsFullCount() && myCall.hasHardLimit()) {
        while (!_data.empty()) {
          // Drop data we are in fastForward phase
          dropBlock();
        }
      }
      SkipResult skipRes{};
      skipRes.didSkip(skipped);
      if (!_data.empty()) {
        return {ExecutionState::HASMORE, skipRes, result};
      } else if (result != nullptr && result->size() < myCall.hardLimit) {
        return {ExecutionState::HASMORE, skipRes, result};
      } else {
        return {ExecutionState::DONE, skipRes, result};
      }
    }
  }
  SkipResult skipRes{};
  skipRes.didSkip(skipped);
  return {ExecutionState::DONE, skipRes, result};
}
#endif

void WaitingExecutionBlockMock::dropBlock() {
  TRI_ASSERT(!_data.empty());
  _data.pop_front();
  _inflight = 0;
}