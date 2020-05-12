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

#include "MutexExecutor.h"

#include "Aql/ExecutionNode.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SharedQueryState.h"
#include "Aql/Stats.h"

#include <algorithm>
#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

ExecutionBlockImpl<MutexExecutor>::ExecutionBlockImpl(
    ExecutionEngine* engine, MutexNode const* node)
    : ExecutionBlock(engine, node), _isShutdown(false) {}

std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr> ExecutionBlockImpl<MutexExecutor>::execute(AqlCallStack stack) {
  
  std::unique_lock<std::mutex> guard(_mutex);
  
  traceExecuteBegin(stack);
  TRI_ASSERT(_dependencies.size() == 1);
  auto ret = _dependencies[0]->execute(stack);
  traceExecuteEnd(ret);
  guard.unlock();
  
  return ret;
}

std::pair<ExecutionState, Result> ExecutionBlockImpl<MutexExecutor>::initializeCursor(
    InputAqlItemRow const& input) {
  
  TRI_ASSERT(_dependencies.size() == 1);
  
  std::lock_guard<std::mutex> guard(_mutex);
  return ExecutionBlock::initializeCursor(input);
}

/// @brief shutdown, will be called exactly once for the whole query
std::pair<ExecutionState, Result> ExecutionBlockImpl<MutexExecutor>::shutdown(int errorCode) {
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
