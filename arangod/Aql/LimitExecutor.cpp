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

LimitExecutor::LimitExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _counter(0),
      _done(_counter == _infos.getLimit() && !_infos.isFullCountEnabled()),
      _doFullCount(_infos.isFullCountEnabled() && _infos.getQueryDepth() == 0){};
LimitExecutor::~LimitExecutor() = default;

ExecutionState LimitExecutor::handleSingleRow(OutputAqlItemRow& output,
                                              LimitStats& stats, bool skipOffset) {
  ExecutionState state;
  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  LOG_DEVEL << this << " fetch row "
            << std::boolalpha << skipOffset ;
  std::tie(state, input) = _fetcher.fetchRow();

  if (state == ExecutionState::DONE) {
    _done = true;
    if (!input.isInitialized()) {
      // no input given nothing to do!
      TRI_ASSERT(state != ExecutionState::HASMORE);
      return state;
    }
  }

  if (state == ExecutionState::WAITING) {
    TRI_ASSERT(!input);
    return state;
  }

  TRI_ASSERT(input.isInitialized());
  TRI_ASSERT(state == ExecutionState::HASMORE || state == ExecutionState::DONE);

  if (_doFullCount) {
    stats.incrFullCount();
  }

  if (skipOffset) {
    _infos.decrRemainingOffset();
  } else {
    // produce until we hit the limit
    if (_counter < _infos.getLimit()) {
      output.copyRow(input);
      ++_counter;
    }

    // if limit is hit and we do not full count then make sure the
    // executor will return immediatly on next call;
    if (_counter == _infos.getLimit() && !_doFullCount) {
      _done = true;
      return ExecutionState::DONE;
    }
  }

  return state;
}

std::pair<ExecutionState, LimitStats> LimitExecutor::produceRow(OutputAqlItemRow& output) {
  TRI_IF_FAILURE("LimitExecutor::skipOffsetRow") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  TRI_ASSERT(!output.produced());
  ExecutionState state;
  LimitStats stats{};

  if (_done) {
    return {ExecutionState::DONE, stats};
  }

  // skip offset
  while (_infos.getRemainingOffset() > 0) {
    state = handleSingleRow(output, stats, /*skipOffset*/ true);
    if (_done || state == ExecutionState::WAITING) {
      return {state, stats};
    }
  }

  // procuce and count remaining - break loop if DONE or WAITING or
  // whenever output has been produced
  do {
    state = handleSingleRow(output, stats, /*skipOffset*/ false);
  } while (!(_done || state == ExecutionState::WAITING || output.produced()));

  return {state, stats};
}

LimitExecutorInfos::LimitExecutorInfos(RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                                       std::unordered_set<RegisterId> registersToClear,
                                       size_t offset, size_t limit,
                                       bool fullCount, size_t queryDepth)
    : ExecutorInfos(std::make_shared<std::unordered_set<RegisterId>>(),
                    std::make_shared<std::unordered_set<RegisterId>>(), nrInputRegisters,
                    nrOutputRegisters, std::move(registersToClear)),
      _remainingOffset(offset),
      _limit(limit),
      _queryDepth(queryDepth),
      _fullCount(fullCount) {}
