////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Michael Hackstein
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ExecutionBlock.h"
#include "Aql/Ast.h"
#include "Aql/BlockCollector.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::aql;

ExecutionBlock::ExecutionBlock(ExecutionEngine* engine, ExecutionNode const* ep)
    : _engine(engine),
      _trx(engine->getQuery()->trx()),
      _shutdownResult(TRI_ERROR_NO_ERROR),
      _done(false),
      _exeNode(ep),
      _dependencyPos(_dependencies.end()),
      _profile(engine->getQuery()->queryOptions().getProfileLevel()),
      _getSomeBegin(0.0),
      _upstreamState(ExecutionState::HASMORE),
      _pos(0),
      _collector(&engine->itemBlockManager()) {}

ExecutionBlock::~ExecutionBlock() {
  for (auto& it : _buffer) {
    delete it;
  }
}

void ExecutionBlock::throwIfKilled() {  // TODO Scatter & DistributeExecutor still using
  if (_engine->getQuery()->killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
}

std::pair<ExecutionState, Result> ExecutionBlock::initializeCursor(InputAqlItemRow const& input) {
  if (_dependencyPos == _dependencies.end()) {
    // We need to start again.
    _dependencyPos = _dependencies.begin();
  }
  for (; _dependencyPos != _dependencies.end(); ++_dependencyPos) {
    auto res = (*_dependencyPos)->initializeCursor(input);
    if (res.first == ExecutionState::WAITING || !res.second.ok()) {
      // If we need to wait or get an error we return as is.
      return res;
    }
  }

  for (auto& it : _buffer) {
    returnBlock(it);
  }
  _buffer.clear();

  _done = false;
  _upstreamState = ExecutionState::HASMORE;
  _pos = 0;
  _collector.clear();

  TRI_ASSERT(getHasMoreState() == ExecutionState::HASMORE);
  TRI_ASSERT(_dependencyPos == _dependencies.end());
  return {ExecutionState::DONE, TRI_ERROR_NO_ERROR};
}

std::pair<ExecutionState, Result> ExecutionBlock::shutdown(int errorCode) {
  if (_dependencyPos == _dependencies.end()) {
    _shutdownResult.reset(TRI_ERROR_NO_ERROR);
    _dependencyPos = _dependencies.begin();
  }

  for (; _dependencyPos != _dependencies.end(); ++_dependencyPos) {
    Result res;
    ExecutionState state;
    try {
      std::tie(state, res) = (*_dependencyPos)->shutdown(errorCode);
      if (state == ExecutionState::WAITING) {
        return {state, TRI_ERROR_NO_ERROR};
      }
    } catch (...) {
      _shutdownResult.reset(TRI_ERROR_INTERNAL);
    }

    if (res.fail()) {
      _shutdownResult = res;
    }
  }

  if (!_buffer.empty()) {
    for (auto& it : _buffer) {
      delete it;
    }
    _buffer.clear();
  }

  return {ExecutionState::DONE, _shutdownResult};
}

