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

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;

WaitingExecutionBlockMock::WaitingExecutionBlockMock(ExecutionEngine* engine,
                                                     ExecutionNode const* node,
                                                     std::deque<SharedAqlItemBlockPtr>&& data,
                                                     WaitingBehaviour variant)
    : ExecutionBlock(engine, node),
      _data(std::move(data)),
      _resourceMonitor(),
      _inflight(0),
      _hasWaited(false),
      _variant{variant} {}

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

std::pair<arangodb::aql::ExecutionState, SharedAqlItemBlockPtr> WaitingExecutionBlockMock::getSome(size_t atMost) {
  if (!_hasWaited) {
    _hasWaited = true;
    if (_returnedDone) {
      return {ExecutionState::DONE, nullptr};
    }
    return {ExecutionState::WAITING, nullptr};
  }
  _hasWaited = false;

  if (_data.empty()) {
    _returnedDone = true;
    return {ExecutionState::DONE, nullptr};
  }

  auto result = std::move(_data.front());
  _data.pop_front();

  if (_data.empty()) {
    _returnedDone = true;
    return {ExecutionState::DONE, std::move(result)};
  } else {
    return {ExecutionState::HASMORE, std::move(result)};
  }
}

std::pair<arangodb::aql::ExecutionState, size_t> WaitingExecutionBlockMock::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);
  if (!_hasWaited) {
    _hasWaited = true;
    return traceSkipSomeEnd(ExecutionState::WAITING, 0);
  }
  _hasWaited = false;

  if (_data.empty()) {
    return traceSkipSomeEnd(ExecutionState::DONE, 0);
  }

  size_t skipped = _data.front()->size();
  _data.pop_front();

  if (_data.empty()) {
    return traceSkipSomeEnd(ExecutionState::DONE, skipped);
  } else {
    return traceSkipSomeEnd(ExecutionState::HASMORE, skipped);
  }
}

std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr> WaitingExecutionBlockMock::execute(AqlCallStack stack) {
  traceExecuteBegin(stack);
  auto res = executeWithoutTrace(stack);
  traceExecuteEnd(res);
  return res;
}

// NOTE: Does not care for shadowrows!
std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr> WaitingExecutionBlockMock::executeWithoutTrace(
    AqlCallStack stack) {
  while (!stack.isRelevant()) {
    stack.pop();
  }
  auto myCall = stack.popCall();
  if (_variant != WaitingBehaviour::NEVER && !_hasWaited) {
    // If we orderd waiting check on _hasWaited and wait if not
    _hasWaited = true;
    return {ExecutionState::WAITING, 0, nullptr};
  }
  if (_variant == WaitingBehaviour::ALWAYS) {
    // If we allways wait, reset.
    _hasWaited = false;
  }
  size_t skipped = 0;
  SharedAqlItemBlockPtr result = nullptr;
  while (!_data.empty()) {
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
        return {ExecutionState::HASMORE, skipped, result};
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
      if (_data.empty()) {
        return {ExecutionState::DONE, skipped, result};
      } else {
        return {ExecutionState::HASMORE, skipped, result};
      }
    }
  }
  return {ExecutionState::DONE, skipped, result};
}

void WaitingExecutionBlockMock::dropBlock() {
  TRI_ASSERT(!_data.empty());
  _data.pop_front();
  _inflight = 0;
}