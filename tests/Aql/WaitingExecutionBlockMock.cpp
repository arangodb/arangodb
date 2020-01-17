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
                                                     std::deque<SharedAqlItemBlockPtr>&& data)
    : ExecutionBlock(engine, node),
      _data(std::move(data)),
      _resourceMonitor(),
      _inflight(0),
      _hasWaited(false) {}

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

// NOTE: Does not care for shadowrows!
std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr> WaitingExecutionBlockMock::execute(AqlCallStack stack) {
  auto myCall = stack.popCall();
  if (!_hasWaited) {
    _hasWaited = true;
    return {ExecutionState::WAITING, 0, nullptr};
  }
  size_t skipped = 0;
  while (!_data.empty()) {
    // Drop while skip
    if (myCall.getOffset() > 0) {
      size_t canSkip = _data.front()->size();
      // For simplicity we can only skip full blocks
      TRI_ASSERT(canSkip <= myCall.getOffset());
      _data.pop_front();
      myCall.didSkip(canSkip);
      skipped += canSkip;
      continue;
    }
    if (myCall.getLimit() > 0) {
      // For simplicity we can only request blocks of defined size.
      TRI_ASSERT(myCall.getLimit() >= _data.front()->size());
      auto result = std::move(_data.front());
      _data.pop_front();
      if (_data.empty()) {
        return {ExecutionState::DONE, skipped, result};
      } else {
        return {ExecutionState::HASMORE, skipped, result};
      }
    }
    TRI_ASSERT(myCall.needsFullCount());
    size_t counts = _data.front()->size();
    _data.pop_front();
    myCall.didSkip(counts);
    skipped += counts;
  }
  return {ExecutionState::DONE, skipped, nullptr};

  // TODO implement!
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}