////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "AsyncExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/Query.h"
#include "Aql/ConstFetcher.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/QueryOptions.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SharedQueryState.h"
#include "Aql/Stats.h"

#include "Logger/LogMacros.h"

#include <algorithm>
#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

ExecutionBlockImpl<AsyncExecutor>::ExecutionBlockImpl(
    ExecutionEngine* engine, AsyncNode const* node, ExecutorInfos&& infos)
    : ExecutionBlock(engine, node),
      _infos(std::move(infos)),
      _query(*engine->getQuery()) {}

std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<AsyncExecutor>::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);
  auto result = getSomeWithoutTrace(atMost);
  return traceGetSomeEnd(result.first, std::move(result.second));
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<AsyncExecutor>::getSomeWithoutTrace(size_t atMost) {
  // silence tests -- we need to introduce new failure tests for fetchers
//  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome1") {
//    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
//  }
//  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome2") {
//    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
//  }
//  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome3") {
//    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
//  }

  if (getQuery().killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
  
  std::lock_guard<std::mutex> guard(_mutex);
  
  TRI_ASSERT(_dependencies.size() == 1);
  
  if (_internalState == AsyncState::InProgress) {
    return {ExecutionState::WAITING, SharedAqlItemBlockPtr()};
  } else if (_internalState == AsyncState::GotResult) {
    _internalState = AsyncState::Empty;
    return {_returnState, std::move(_returnBlock)};
  }
  TRI_ASSERT(_internalState == AsyncState::Empty);
  
  _internalState = AsyncState::InProgress;
  bool queued = _query.sharedState()->asyncExecuteAndWakeup([this, atMost](bool isAsync) {
    std::unique_lock<std::mutex> guard(_mutex, std::defer_lock);
    if (isAsync) {
      guard.lock();
    }
    std::tie(_returnState, _returnBlock) = _dependencies[0]->getSome(atMost);
    if (_returnBlock.get()) {
      LOG_DEVEL << "getSome rows " << _returnBlock->size();
    } else {
      LOG_DEVEL << "getSome rows " << 0;
    }
    
    _internalState = AsyncState::GotResult;
  });

  if (!queued) {
    _internalState = AsyncState::Empty;
    return {_returnState, std::move(_returnBlock)};
  }
  return {ExecutionState::WAITING, SharedAqlItemBlockPtr()};
}

std::pair<ExecutionState, size_t> ExecutionBlockImpl<AsyncExecutor>::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);
  auto result = skipSomeWithoutTrace(atMost);
  return traceSkipSomeEnd(result.first, result.second);
}

std::pair<ExecutionState, size_t> ExecutionBlockImpl<AsyncExecutor>::skipSomeWithoutTrace(size_t atMost) {

  if (getQuery().killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }

  std::lock_guard<std::mutex> guard(_mutex);

  if (_internalState == AsyncState::InProgress) {
    return {ExecutionState::WAITING, 0};
  } else if (_internalState == AsyncState::GotResult) {
    _internalState = AsyncState::Empty;
    return {_returnState, std::move(_returnSkipped)};
  }
  TRI_ASSERT(_internalState == AsyncState::Empty);

  _internalState = AsyncState::InProgress;
  bool queued = _query.sharedState()->asyncExecuteAndWakeup([this, atMost](bool isAsync) {
    std::unique_lock<std::mutex> guard(_mutex, std::defer_lock);
    if (isAsync) {
      guard.lock();
    }
    
    std::tie(_returnState, _returnSkipped) = _dependencies[0]->skipSome(atMost);
    
    _internalState = AsyncState::GotResult;
  });

  if (!queued) {
    _internalState = AsyncState::Empty;
    return {_returnState, _returnSkipped};
  }
  return {ExecutionState::WAITING, 0};
}

std::pair<ExecutionState, Result> ExecutionBlockImpl<AsyncExecutor>::initializeCursor(
    InputAqlItemRow const& input) {
//
//  if (getQuery().killed()) {
//    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
//  }
  
  TRI_ASSERT(_dependencies.size() == 1);
  
  LOG_DEVEL << "AsyncExecutor::initializeCursor";
  
  std::lock_guard<std::mutex> guard(_mutex);
  auto res = ExecutionBlock::initializeCursor(input);
  _returnBlock = nullptr;
  _returnState = ExecutionState::HASMORE;
  _internalState = AsyncState::Empty;
  return res;
}

/// @brief shutdown, will be called exactly once for the whole query
std::pair<ExecutionState, Result> ExecutionBlockImpl<AsyncExecutor>::shutdown(int errorCode) {
  std::lock_guard<std::mutex> guard(_mutex);
  auto res = ExecutionBlock::shutdown(errorCode);
  _returnBlock = nullptr;
  _returnState = ExecutionState::HASMORE;
  _internalState = AsyncState::Empty;
  return res;
}

