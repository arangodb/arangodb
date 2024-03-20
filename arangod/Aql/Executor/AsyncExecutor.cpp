////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Aql/ConstFetcher.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/Query.h"
#include "Aql/QueryOptions.h"
#include "Aql/SharedQueryState.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Basics/ScopeGuard.h"

#include <algorithm>
#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

ExecutionBlockImpl<AsyncExecutor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                      ExecutionNode const* node)
    : ExecutionBlock(engine, node), _sharedState(engine->sharedState()) {}

ExecutionBlockImpl<AsyncExecutor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                      ExecutionNode const* node,
                                                      RegisterInfos,
                                                      AsyncExecutor::Infos)
    : ExecutionBlockImpl(engine, node) {}

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
    ++_numWakeupsQueued;
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

#ifdef ARANGODB_USE_GOOGLE_TESTS
          if (_postAsyncExecuteCallback) {
            _postAsyncExecuteCallback();
          }
#endif

          if (isAsync) {
            guard.lock();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
            bool old = false;
            TRI_ASSERT(_isBlockInUse.compare_exchange_strong(old, true));
            TRI_ASSERT(_isBlockInUse);
#endif
          }

          // If we got woken up while in progress, wake up our dependency now.
          // This is necessary so the query will always wake up from sleep. See
          // https://arangodb.atlassian.net/browse/BTS-1325
          // and
          // https://github.com/arangodb/arangodb/pull/18729
          // for details.
          while (state == ExecutionState::WAITING && _numWakeupsQueued > 0) {
            --_numWakeupsQueued;
            TRI_ASSERT(skip.nothingSkipped());
            TRI_ASSERT(block == nullptr);
            // isAsync => guard.owns_lock()
            TRI_ASSERT(!isAsync || guard.owns_lock());
            if (isAsync) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
              bool old = true;
              TRI_ASSERT(_isBlockInUse.compare_exchange_strong(old, false));
              TRI_ASSERT(!_isBlockInUse);
#endif
              guard.unlock();
            }
            std::tie(state, skip, block) = _dependencies[0]->execute(stack);
            if (isAsync) {
              TRI_ASSERT(!guard.owns_lock());
              TRI_ASSERT(guard.mutex() != nullptr);
              guard.lock();
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
              bool old = false;
              TRI_ASSERT(_isBlockInUse.compare_exchange_strong(old, true));
              TRI_ASSERT(_isBlockInUse);
#endif
            }
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
            TRI_ASSERT(!guard.owns_lock());
            TRI_ASSERT(guard.mutex() != nullptr);
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

#ifdef ARANGODB_USE_GOOGLE_TESTS
void ExecutionBlockImpl<AsyncExecutor>::setPostAsyncExecuteCallback(
    std::function<void()> cb) {
  _postAsyncExecuteCallback = std::move(cb);
}
#endif
