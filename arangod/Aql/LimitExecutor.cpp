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
    : _infos(infos),
      _fetcher(fetcher),
      _lastRowToOutput(CreateInvalidInputRowHint{}),
      _stateOfLastRowToOutput(ExecutionState::HASMORE),
      _stats() {}
LimitExecutor::~LimitExecutor() = default;

ExecutionState LimitExecutor::skipOffset() {
  ExecutionState state;
  size_t skipped;
  std::tie(state, skipped) = _fetcher.skipRows(maxRowsLeftToSkip());

  if (state == ExecutionState::WAITING) {
    return state;
  }

  _counter += skipped;

  if (infos().isFullCountEnabled()) {
    _stats.incrFullCountBy(skipped);
  }

  return state;
}

ExecutionState LimitExecutor::skipRestForFullCount() {
  ExecutionState state;
  size_t skipped;
  // skip ALL the rows
  std::tie(state, skipped) = _fetcher.skipRows(std::numeric_limits<size_t>::max());

  if (state == ExecutionState::WAITING) {
    return state;
  }

  // We must not update _counter here. It is only used to count until offset+limit
  // is reached.

  if (infos().isFullCountEnabled()) {
    _stats.incrFullCountBy(skipped);
  }

  return state;
}

std::pair<ExecutionState, LimitStats> LimitExecutor::produceRows(OutputAqlItemRow& output) {
  TRI_IF_FAILURE("LimitExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  ExecutionState state;

  while (LimitState::SKIPPING == currentState()) {
    state = skipOffset();
    if (state == ExecutionState::WAITING || state == ExecutionState::DONE) {
      return {state, std::move(_stats)};
    }
  }

  while (LimitState::RETURNING == currentState()) {
    std::tie(state, input) = _fetcher.fetchRow(maxRowsLeftToFetch());

    if (state == ExecutionState::WAITING) {
      return {state, std::move(_stats)};
    }

    // This executor is pass-through. Thus we will never get asked to write an
    // output row for which there is no input, as in- and output rows have a
    // 1:1 correspondence.
    TRI_ASSERT(input.isInitialized());

    // We've got one input row
    _counter++;

    if (infos().isFullCountEnabled()) {
      _stats.incrFullCount();
    }

    // Return one row
    output.copyRow(input);
    return {state, std::move(_stats)};
  }

  // This case is special for two reasons.
  // First, after this we want to return DONE, regardless of the upstream's
  // state.
  // Second, when fullCount is enabled, we need to get the fullCount before
  // returning the last row, as the count is returned with the stats (and we
  // would not be asked again by ExecutionBlockImpl in any case).
  if (LimitState::RETURNING_LAST_ROW == currentState()) {
    if (_lastRowToOutput.isInitialized()) {
      // Use previously saved row iff there is one. We can get here only if
      // fullCount is enabled. If it is, we can get here multiple times (until
      // we consumed the whole upstream, which might return WAITING repeatedly).
      TRI_ASSERT(infos().isFullCountEnabled());
      state = _stateOfLastRowToOutput;
      TRI_ASSERT(state != ExecutionState::WAITING);
      input = std::move(_lastRowToOutput);
      TRI_ASSERT(!_lastRowToOutput.isInitialized()); // rely on the move
    } else {
      std::tie(state, input) = _fetcher.fetchRow(maxRowsLeftToFetch());

      if (state == ExecutionState::WAITING) {
        return {state, std::move(_stats)};
      }
    }

    // This executor is pass-through. Thus we will never get asked to write an
    // output row for which there is no input, as in- and output rows have a
    // 1:1 correspondence.
    TRI_ASSERT(input.isInitialized());

    if (infos().isFullCountEnabled()) {
      // Save the state now. The _stateOfLastRowToOutput will not be used unless
      // _lastRowToOutput gets set.
      _stateOfLastRowToOutput = state;
      state = skipRestForFullCount();
      if (state == ExecutionState::WAITING) {
        // Save the row
        _lastRowToOutput = std::move(input);
        return {state, std::move(_stats)};
      }
    }

    // It's important to increase the counter for the last row only *after*
    // skipRestForFullCount() is done, because we need currentState() to stay
    // at RETURNING_LAST_ROW until we've actually returned the last row.
    _counter++;

    // Count this not before here as well so we count the row only once.
    if (infos().isFullCountEnabled()) {
      _stats.incrFullCount();
    }
    output.copyRow(input);
    return {ExecutionState::DONE, std::move(_stats)};
  }

  // We should never be COUNTING, this must already be done in the
  // RETURNING_LAST_ROW-handler.
  TRI_ASSERT(LimitState::LIMIT_REACHED == currentState());
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
        state = skipRestForFullCount();

        if (state == ExecutionState::WAITING || state == ExecutionState::DONE) {
          return {state, nullptr};
        }
      }
      TRI_ASSERT(false);
      return {ExecutionState::DONE, nullptr};
    case LimitState::SKIPPING: {
      while (LimitState::SKIPPING == currentState()) {
        ExecutionState state = skipOffset();
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
