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
    : _infos(infos), _fetcher(fetcher){};
LimitExecutor::~LimitExecutor() = default;

std::pair<ExecutionState, LimitStats> LimitExecutor::produceRows(OutputAqlItemRow& output) {
  TRI_IF_FAILURE("LimitExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  LimitStats stats{};
  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  ExecutionState state;
  LimitState limitState;

  while (LimitState::SKIPPING == currentState()) {
    size_t skipped;
    std::tie(state, skipped) = _fetcher.skipRows(maxRowsLeftToSkip());

    if (state == ExecutionState::WAITING) {
      return {state, stats};
    }

    _counter += skipped;

    if (infos().isFullCountEnabled()) {
      stats.incrFullCountBy(skipped);
    }

    // Abort if upstream is done
    if (state == ExecutionState::DONE) {
      return {state, stats};
    }
  }

  while (LimitState::LIMIT_REACHED != (limitState = currentState()) && LimitState::COUNTING != limitState) {
    std::tie(state, input) = _fetcher.fetchRow(maxRowsLeftToFetch());

    if (state == ExecutionState::WAITING) {
      return {state, stats};
    }

    if (!input) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, stats};
    }
    TRI_ASSERT(input.isInitialized());

    // We've got one input row
    _counter++;

    if (infos().isFullCountEnabled()) {
      stats.incrFullCount();
    }

    // Return one row
    if (limitState == LimitState::RETURNING) {
      output.copyRow(input);
      return {state, stats};
    }
    if (limitState == LimitState::RETURNING_LAST_ROW) {
      output.copyRow(input);
      return {ExecutionState::DONE, stats};
    }

    // Abort if upstream is done
    if (state == ExecutionState::DONE) {
      return {state, stats};
    }

    TRI_ASSERT(false);
  }

  while (LimitState::LIMIT_REACHED != currentState()) {
    size_t skipped;
    // TODO: skip ALL the rows
    std::tie(state, skipped) = _fetcher.skipRows(ExecutionBlock::DefaultBatchSize());

    if (state == ExecutionState::WAITING) {
      return {state, stats};
    }

    _counter += skipped;

    if (infos().isFullCountEnabled()) {
      stats.incrFullCountBy(skipped);
    }

    // Abort if upstream is done
    if (state == ExecutionState::DONE) {
      return {state, stats};
    }
  }

  // When fullCount is enabled, the loop may only abort when upstream is done.
  TRI_ASSERT(!infos().isFullCountEnabled());

  return {ExecutionState::DONE, stats};
}

std::pair<ExecutionState, size_t> LimitExecutor::expectedNumberOfRows(size_t atMost) const {
  switch (currentState()) {
    case LimitState::LIMIT_REACHED:
      // We are done with our rows!
      return {ExecutionState::DONE, 0};
    case LimitState::COUNTING:
      // We are actually done with our rows,
      // BUt we need to make sure that we get asked more
      return {ExecutionState::DONE, 1};
    case LimitState::SKIPPING: {
      // This is the best guess we can make without calling
      // preFetchNumberOfRows(), which, however, would prevent skipping.
      // The problem is not here, but in ExecutionBlockImpl which calls this to
      // allocate a block before we had a chance to skip here.
      // There is a corresponding todo note on
      // LimitExecutor::Properties::inputSizeRestrictsOutputSize.

      TRI_ASSERT(_counter < infos().getOffset());

      // Note on fullCount we might get more lines from upstream then required.
      size_t leftOverIncludingSkip = infos().getLimitPlusOffset() - _counter;
      size_t leftOver = infos().getLimit();
      if (_infos.isFullCountEnabled()) {
        // Add one for the fullcount.
        if (leftOverIncludingSkip < atMost) {
          leftOverIncludingSkip++;
        }
        if (leftOver < atMost) {
          leftOver++;
        }
      }

      ExecutionState const state =
          leftOverIncludingSkip > 0 ? ExecutionState::HASMORE : ExecutionState::DONE;

      if (state != ExecutionState::DONE) {
        // unless we're DONE, never return 0.
        leftOver = (std::max)(std::size_t{1}, leftOver);
      }

      return {state, leftOver};
    }
    case LimitState::RETURNING_LAST_ROW:
    case LimitState::RETURNING: {
      auto res = _fetcher.preFetchNumberOfRows(maxRowsLeftToFetch());
      if (res.first == ExecutionState::WAITING) {
        return res;
      }
      // Note on fullCount we might get more lines from upstream then required.
      size_t leftOver = (std::min)(infos().getLimitPlusOffset() - _counter, res.second);
      if (_infos.isFullCountEnabled() && leftOver < atMost) {
        // Add one for the fullcount.
        leftOver++;
      }
      if (leftOver > 0) {
        return {ExecutionState::HASMORE, leftOver};
      }
      return {ExecutionState::DONE, 0};
    }
  }
  TRI_ASSERT(false);
  // This should not be reached, (the switch case is covering all enum values)
  // Nevertheless if it is reached this will fall back to the non.optimal, but
  // working variant
  return {ExecutionState::DONE, atMost};
}
