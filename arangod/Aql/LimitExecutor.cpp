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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "LimitExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Common.h"

#include <lib/Logger/LogMacros.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

LimitExecutorInfos::LimitExecutorInfos(RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                                       // cppcheck-suppress passedByValue
                                       std::unordered_set<RegisterId> registersToClear,
                                       // cppcheck-suppress passedByValue
                                       std::unordered_set<RegisterId> registersToKeep,
                                       size_t offset, size_t limit, bool fullCount)
    : ExecutorInfos(std::make_shared<std::unordered_set<RegisterId>>(),
                    std::make_shared<std::unordered_set<RegisterId>>(),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _offset(offset),
      _limit(limit),
      _fullCount(fullCount) {}

LimitExecutor::LimitExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos), _fetcher(fetcher), _stats() {};
LimitExecutor::~LimitExecutor() = default;

ExecutionState LimitExecutor::skipOffset(LimitStats& stats) {
  ExecutionState state;
  size_t skipped;
  std::tie(state, skipped) = _fetcher.skipRows(maxRowsLeftToSkip());

  if (state == ExecutionState::WAITING) {
    return state;
  }

  _counter += skipped;

  if (infos().isFullCountEnabled()) {
    stats.incrFullCountBy(skipped);
  }

  return state;
}

ExecutionState LimitExecutor::skipRestForFullCount(LimitStats& stats) {
  ExecutionState state;
  size_t skipped;
  // skip ALL the rows
  std::tie(state, skipped) = _fetcher.skipRows(std::numeric_limits<size_t>::max());

  if (state == ExecutionState::WAITING) {
    return state;
  }

  _counter += skipped;

  if (infos().isFullCountEnabled()) {
    stats.incrFullCountBy(skipped);
  }

  return state;
}

std::pair<ExecutionState, LimitStats> LimitExecutor::produceRows(OutputAqlItemRow& output) {
  TRI_IF_FAILURE("LimitExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  ExecutionState state;
  LimitState limitState;

  while (LimitState::SKIPPING == currentState()) {
    state = skipOffset(_stats);
    if (state == ExecutionState::WAITING || state == ExecutionState::DONE) {
      return {state, std::move(_stats)};
    }
  }

  while (LimitState::LIMIT_REACHED != (limitState = currentState()) && LimitState::COUNTING != limitState) {
    std::tie(state, input) = _fetcher.fetchRow(maxRowsLeftToFetch());

    if (state == ExecutionState::WAITING) {
      return {state, std::move(_stats)};
    }

    if (!input) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, std::move(_stats)};
    }
    TRI_ASSERT(input.isInitialized());

    // We've got one input row
    _counter++;

    if (infos().isFullCountEnabled()) {
      _stats.incrFullCount();
    }

    // Return one row
    if (limitState == LimitState::RETURNING) {
      output.copyRow(input);
      return {state, std::move(_stats)};
    }
    if (limitState == LimitState::RETURNING_LAST_ROW) {
      output.copyRow(input);
      return {ExecutionState::DONE, std::move(_stats)};
    }

    // Abort if upstream is done
    if (state == ExecutionState::DONE) {
      return {state, std::move(_stats)};
    }

    TRI_ASSERT(false);
  }

  while (LimitState::LIMIT_REACHED != currentState()) {
    state = skipRestForFullCount(_stats);

    if (state == ExecutionState::WAITING || state == ExecutionState::DONE) {
      return {state, std::move(_stats)};
    }
  }

  // When fullCount is enabled, the loop may only abort when upstream is done.
  TRI_ASSERT(!infos().isFullCountEnabled());

  return {ExecutionState::DONE, std::move(_stats)};
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> LimitExecutor::fetchBlockForPassthrough(size_t atMost) {
  // _fetcher.fetchBlockForPassthrough(atMost);
  switch (currentState()) {
    case LimitState::LIMIT_REACHED:
      // We are done with our rows!
      return {ExecutionState::DONE, nullptr};
    case LimitState::COUNTING:
      // This case must not happen!
      while (LimitState::LIMIT_REACHED != currentState()) {
        ExecutionState state;
        state = skipRestForFullCount( _stats);

        if (state == ExecutionState::WAITING || state == ExecutionState::DONE) {
          return {state, nullptr};
        }
      }
      TRI_ASSERT(false);
      return {ExecutionState::DONE, nullptr};
    case LimitState::SKIPPING: {
      while (LimitState::SKIPPING == currentState()) {
        ExecutionState state = skipOffset(_stats);
        if (state == ExecutionState::WAITING || state == ExecutionState::DONE) {
          return {state, nullptr};
        }
      }

      // We should have reached the next state now
      TRI_ASSERT(currentState() != LimitState::SKIPPING);
      // Now jump to the correct case
      return fetchBlockForPassthrough(atMost);
    }
    case LimitState::RETURNING_LAST_ROW:
    case LimitState::RETURNING:
      return _fetcher.fetchBlockForPassthrough(std::min(atMost, maxRowsLeftToFetch()));
  }
  TRI_ASSERT(false);
  // This should not be reached, (the switch case is covering all enum values)
  // Nevertheless if it is reached this will fall back to the non.optimal, but
  // working variant
  return {ExecutionState::DONE, nullptr};

}
