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

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutionStats.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/QueryOptions.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;

WaitingExecutionBlockMock::WaitingExecutionBlockMock(ExecutionEngine* engine,
                                                     ExecutionNode const* node,
                                                     std::deque<std::unique_ptr<AqlItemBlock>>&& data)
    : ExecutionBlock(engine, node),
      _data(std::move(data)),
      _resourceMonitor(),
      _inflight(0),
      _hasWaited(false),
      _engine(engine) {}

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

std::pair<arangodb::aql::ExecutionState, std::unique_ptr<arangodb::aql::AqlItemBlock>>
WaitingExecutionBlockMock::getSome(size_t atMost) {
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
    traceSkipSomeEnd(0, ExecutionState::WAITING);
    return {ExecutionState::WAITING, 0};
  }
  _hasWaited = false;

  if (_data.empty()) {
    traceSkipSomeEnd(0, ExecutionState::DONE);
    return {ExecutionState::DONE, 0};
  }

  size_t skipped = _data.front()->size();
  _data.pop_front();

  if (_data.empty()) {
    traceSkipSomeEnd(skipped, ExecutionState::DONE);
    return {ExecutionState::DONE, skipped};
  } else {
    traceSkipSomeEnd(skipped, ExecutionState::HASMORE);
    return {ExecutionState::HASMORE, skipped};
  }
}
