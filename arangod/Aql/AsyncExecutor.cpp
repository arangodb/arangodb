////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/ScopeGuard.h"

#include "Logger/LogMacros.h"

#include <algorithm>
#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

ExecutionBlockImpl<AsyncExecutor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                      AsyncNode const* node)
    : ExecutionBlock(engine, node), _sharedState(engine->sharedState()) {}

std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr>
ExecutionBlockImpl<AsyncExecutor>::execute(AqlCallStack const& stack) {
  traceExecuteBegin(stack);
  auto res = executeWithoutTrace(stack);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto const& [state, skipped, block] = res;
  if (block != nullptr) {
    block->validateShadowRowConsistency();
  }
#endif
  traceExecuteEnd(res);
  return res;
}

std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr>
ExecutionBlockImpl<AsyncExecutor>::executeWithoutTrace(
    AqlCallStack const& stack) {
  //  if (getQuery().killed()) {
  //    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  //  }

  std::lock_guard<std::mutex> guard(_mutex);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool old = false;
  TRI_ASSERT(_isBlockInUse.compare_exchange_strong(old, true));
  TRI_ASSERT(_isBlockInUse);

  auto blockInUseGuard = scopeGuard([&]() noexcept {
    bool old = true;
    TRI_ASSERT(_isBlockInUse.compare_exchange_strong(old, false));
    TRI_ASSERT(!_isBlockInUse);
  });
#endif

  TRI_ASSERT(_dependencies.size() == 1);

  if (_internalState == AsyncState::InProgress) {
    return {ExecutionState::WAITING, SkipResult{}, SharedAqlItemBlockPtr()};
  } else if (_internalState == AsyncState::GotResult) {
    if (_returnState != ExecutionState::DONE) {
      // we may not return WAITING if upstream returned DONE
      _internalState = AsyncState::Empty;
    }
    return {_returnState, std::move(_returnSkip), std::move(_returnBlock)};
  } else if (_internalState == AsyncState::GotException) {
    TRI_ASSERT(_returnException != nullptr);
    std::rethrow_exception(_returnException);
    TRI_ASSERT(false);
    return {ExecutionState::DONE, SkipResult(), SharedAqlItemBlockPtr()};
  }
  TRI_ASSERT(_internalState == AsyncState::Empty);

  _internalState = AsyncState::InProgress;
  bool queued =
      _sharedState->asyncExecuteAndWakeup([this, stack](bool isAsync) {
        std::unique_lock<std::mutex> guard(_mutex, std::defer_lock);

        try {
          auto [state, skip, block] = _dependencies[0]->execute(stack);
          if (isAsync) {
            guard.lock();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
            bool old = false;
            TRI_ASSERT(_isBlockInUse.compare_exchange_strong(old, true));
            TRI_ASSERT(_isBlockInUse);
#endif
          }
          _returnState = state;
          _returnSkip = std::move(skip);
          _returnBlock = std::move(block);

          _internalState = AsyncState::GotResult;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          if (isAsync) {
            bool old = true;
            TRI_ASSERT(_isBlockInUse.compare_exchange_strong(old, false));
            TRI_ASSERT(!_isBlockInUse);
          }
#endif
        } catch (...) {
          if (isAsync) {
            guard.lock();
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
            bool old = false;
            TRI_ASSERT(_isBlockInUse.compare_exchange_strong(old, true));
            TRI_ASSERT(_isBlockInUse);
#endif
          }
          _returnException = std::current_exception();
          _internalState = AsyncState::GotException;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          if (isAsync) {
            bool old = true;
            TRI_ASSERT(_isBlockInUse.compare_exchange_strong(old, false));
            TRI_ASSERT(!_isBlockInUse);
          }
#endif
        }
      });

  if (!queued) {
    if (_internalState == AsyncState::GotException) {
      TRI_ASSERT(_returnException != nullptr);
      std::rethrow_exception(_returnException);
    } else {
      // The callback can only end up with GotResult or GetException
      TRI_ASSERT(_internalState == AsyncState::GotResult);
      _internalState = AsyncState::Empty;
      return {_returnState, std::move(_returnSkip), std::move(_returnBlock)};
    }
  }
  return {ExecutionState::WAITING, SkipResult{}, SharedAqlItemBlockPtr()};
}

std::pair<ExecutionState, Result> ExecutionBlockImpl<
    AsyncExecutor>::initializeCursor(InputAqlItemRow const& input) {
  //
  //  if (getQuery().killed()) {
  //    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  //  }

  TRI_ASSERT(_dependencies.size() == 1);

  std::lock_guard<std::mutex> guard(_mutex);
  auto res = ExecutionBlock::initializeCursor(input);
  _returnBlock = nullptr;
  _returnState = ExecutionState::HASMORE;
  _internalState = AsyncState::Empty;
  return res;
}
