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

#include "FilterExecutor.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlCallStack.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/AqlValue.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

FilterExecutorInfos::FilterExecutorInfos(RegisterId inputRegister, RegisterId nrInputRegisters,
                                         RegisterId nrOutputRegisters,
                                         // cppcheck-suppress passedByValue
                                         std::unordered_set<RegisterId> registersToClear,
                                         // cppcheck-suppress passedByValue
                                         std::unordered_set<RegisterId> registersToKeep)
    : ExecutorInfos(std::make_shared<std::unordered_set<RegisterId>>(inputRegister),
                    nullptr, nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _inputRegister(inputRegister) {}

RegisterId FilterExecutorInfos::getInputRegister() const noexcept {
  return _inputRegister;
}

FilterExecutor::FilterExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos), _fetcher(fetcher) {}

FilterExecutor::~FilterExecutor() = default;

std::pair<ExecutionState, FilterStats> FilterExecutor::produceRows(OutputAqlItemRow& output) {
  TRI_IF_FAILURE("FilterExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  ExecutionState state;
  FilterStats stats{};
  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  while (true) {
    std::tie(state, input) = _fetcher.fetchRow();

    if (state == ExecutionState::WAITING) {
      return {state, stats};
    }

    if (!input) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, stats};
    }
    TRI_ASSERT(input.isInitialized());

    if (input.getValue(_infos.getInputRegister()).toBoolean()) {
      output.copyRow(input);
      return {state, stats};
    } else {
      stats.incrFiltered();
    }

    if (state == ExecutionState::DONE) {
      return {state, stats};
    }
    TRI_ASSERT(state == ExecutionState::HASMORE);
  }
}

std::pair<ExecutionState, size_t> FilterExecutor::expectedNumberOfRows(size_t atMost) const {
  // This block cannot know how many elements will be returned exactly.
  // but it is upper bounded by the input.
  return _fetcher.preFetchNumberOfRows(atMost);
}

// TODO Remove me, we are using the getSome skip variant here.
std::tuple<ExecutorState, size_t, AqlCall> FilterExecutor::skipRowsRange(
    AqlItemBlockInputRange& inputRange, AqlCall& call) {
  ExecutorState state = ExecutorState::HASMORE;
  InputAqlItemRow input{CreateInvalidInputRowHint{}};
  size_t skipped = 0;
  while (inputRange.hasDataRow() && skipped < call.getOffset()) {
    std::tie(state, input) = inputRange.nextDataRow();
    if (!input) {
      TRI_ASSERT(!inputRange.hasDataRow());
      break;
    }
    if (input.getValue(_infos.getInputRegister()).toBoolean()) {
      skipped++;
    }
  }
  call.didSkip(skipped);

  AqlCall upstreamCall{};
  upstreamCall.softLimit = call.getOffset();
  return {state, skipped, upstreamCall};
}

std::tuple<ExecutorState, FilterStats, AqlCall> FilterExecutor::produceRows(
    AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output) {
  TRI_IF_FAILURE("FilterExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  FilterStats stats{};

  while (inputRange.hasDataRow() && !output.isFull()) {
    auto const& [state, input] = inputRange.nextDataRow();
    TRI_ASSERT(input.isInitialized());
    if (input.getValue(_infos.getInputRegister()).toBoolean()) {
      output.copyRow(input);
      output.advanceRow();
    } else {
      stats.incrFiltered();
    }
  }

  AqlCall upstreamCall{};
  auto const& clientCall = output.getClientCall();
  // This is a optimistic fetch. We do not do any overfetching here, only if we
  // pass through all rows this fetch is correct, otherwise we have too few rows.
  upstreamCall.softLimit = clientCall.getOffset() +
                           (std::min)(clientCall.softLimit, clientCall.hardLimit);
  return {inputRange.upstreamState(), stats, upstreamCall};
}
