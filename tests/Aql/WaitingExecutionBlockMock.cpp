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
#include "Aql/ExecutionState.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::aql::tests;

WaitingExecutionBlockMock::WaitingExecutionBlockMock(
    ExecutionEngine* engine, ExecutionNode const* node,
    std::shared_ptr<VPackBuilder> data)
    : ExecutionBlock(engine, node),
      _data(data),
      _resourceMonitor(),
      _inflight(0),
      _hasWaited(false) {}

std::pair<arangodb::aql::ExecutionState, arangodb::Result> WaitingExecutionBlockMock::initializeCursor(
      arangodb::aql::AqlItemBlock* items, size_t pos) {
  if (!_hasWaited) {
    _hasWaited = true;
    return {ExecutionState::WAITING, TRI_ERROR_NO_ERROR};
  }
  _hasWaited = false;
  _inflight = 0;
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

std::pair<arangodb::aql::ExecutionState,
          std::unique_ptr<arangodb::aql::AqlItemBlock>>
WaitingExecutionBlockMock::getSome(size_t atMost) {
  if (!_hasWaited) {
    _hasWaited = true;
    return {ExecutionState::WAITING, nullptr};
  }
  _hasWaited = false;
  VPackSlice source = _data->slice();
  size_t max = source.length();
  TRI_ASSERT(_inflight <= max);
  size_t toReturn = (std::min)(max - _inflight, atMost);
  _inflight += toReturn;
  VPackBuilder data;
  data.openArray();
  for (size_t i = 0; i < toReturn; ++i) {
    data.add(source.at(i));
  }
  data.close();
  auto result = std::make_unique<AqlItemBlock>(&_resourceMonitor, data.slice());
  if (_inflight < max) {
    return {ExecutionState::HASMORE, std::move(result)};
  }
  return {ExecutionState::DONE, std::move(result)};

}

std::pair<arangodb::aql::ExecutionState, size_t> WaitingExecutionBlockMock::skipSome(
  size_t atMost
) {
  traceSkipSomeBegin(atMost);
  if (!_hasWaited) {
    _hasWaited = true;
    traceSkipSomeEnd(0, ExecutionState::WAITING);
    return {ExecutionState::WAITING, 0};
  }
  _hasWaited = false;
  size_t max = _data->slice().length();
  TRI_ASSERT(_inflight <= max);
  size_t skipped = (std::min)(max - _inflight, atMost);
  _inflight += skipped;
  if (_inflight < max) {
    traceSkipSomeEnd(skipped, ExecutionState::HASMORE);
    return {ExecutionState::HASMORE, skipped};
  }
  traceSkipSomeEnd(skipped, ExecutionState::DONE);
  return {ExecutionState::DONE, skipped};
}
