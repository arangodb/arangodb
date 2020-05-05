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

#include "ParallelStartExecutor.h"

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

ExecutionBlockImpl<ParallelStartExecutor>::ExecutionBlockImpl(
    ExecutionEngine* engine, ParallelStartNode const* node)
    : ExecutionBlock(engine, node), _isShutdown(false) {}

//std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<ParallelStartExecutor>::getSome(size_t atMost) {
//  traceGetSomeBegin(atMost);
//  auto result = getSomeWithoutTrace(atMost);
//  traceGetSomeEnd(result.first, result.second);
//  return result;
//}
//
//std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<ParallelStartExecutor>::getSomeWithoutTrace(size_t atMost) {
//    
//  
//  std::unique_lock<std::mutex> guard(_mutex);
//  
//  TRI_ASSERT(_dependencies.size() == 1);
//
//  auto ret = _dependencies[0]->getSome(atMost);
//  
//  return ret;
//}
//
//std::pair<ExecutionState, size_t> ExecutionBlockImpl<ParallelStartExecutor>::skipSome(size_t atMost) {
//  traceSkipSomeBegin(atMost);
//  auto result = skipSomeWithoutTrace(atMost);
//  traceSkipSomeEnd(result.first, result.second);
//  return result;
//}
//
//std::pair<ExecutionState, size_t> ExecutionBlockImpl<ParallelStartExecutor>::skipSomeWithoutTrace(size_t atMost) {
//  std::lock_guard<std::mutex> guard(_mutex);
//
//  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
//                                 "skipping is not supported in parallel blocks");
//  return {ExecutionState::DONE, 0};
//}

std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> ExecutionBlockImpl<ParallelStartExecutor>::execute(AqlCallStack stack) {
  TRI_ASSERT(false);
  return {ExecutionState::DONE, SkipResult(), nullptr};
}

std::pair<ExecutionState, Result> ExecutionBlockImpl<ParallelStartExecutor>::initializeCursor(
    InputAqlItemRow const& input) {
  
  TRI_ASSERT(_dependencies.size() == 1);
  
  LOG_DEVEL << "ParallelStartExecutor::initializeCursor";
  
  std::lock_guard<std::mutex> guard(_mutex);
  auto ret = ExecutionBlock::initializeCursor(input);
  return ret;
}

/// @brief shutdown, will be called exactly once for the whole query
std::pair<ExecutionState, Result> ExecutionBlockImpl<ParallelStartExecutor>::shutdown(int errorCode) {
  std::lock_guard<std::mutex> guard(_mutex);
  if (_isShutdown) {
    return {ExecutionState::DONE, Result()};
  }
  auto ret = ExecutionBlock::shutdown(errorCode);
  if (ret.first == ExecutionState::DONE) {
    _isShutdown = true;
  }
  return ret;
}

