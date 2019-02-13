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
                                       std::unordered_set<RegisterId> registersToClear,
                                       size_t offset, size_t limit, bool fullCount)
    : ExecutorInfos(std::make_shared<std::unordered_set<RegisterId>>(),
                    std::make_shared<std::unordered_set<RegisterId>>(), nrInputRegisters,
                    nrOutputRegisters, std::move(registersToClear)),
      _offset(offset),
      _limit(limit),
      _fullCount(fullCount) {}

LimitExecutor::LimitExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos), _fetcher(fetcher){};
LimitExecutor::~LimitExecutor() = default;

std::pair<ExecutionState, LimitStats> LimitExecutor::produceRow(OutputAqlItemRow& output) {
  TRI_IF_FAILURE("LimitExecutor::produceRow") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  LimitStats stats{};
  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  ExecutionState state;
  LimitState limitState;
  while (LimitState::LIMIT_REACHED != (limitState = currentState())) {
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

    TRI_ASSERT(limitState == LimitState::SKIPPING || limitState == LimitState::COUNTING);
  }

  // When fullCount is enabled, the loop may only abort when upstream is done.
  TRI_ASSERT(!infos().isFullCountEnabled());

  return {ExecutionState::DONE, stats};
}
