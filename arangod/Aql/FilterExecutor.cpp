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
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

FilterExecutorInfos::FilterExecutorInfos(RegisterId inputRegister)
    : _inputRegister(inputRegister) {}

RegisterId FilterExecutorInfos::getInputRegister() const noexcept {
  return _inputRegister;
}

FilterExecutor::FilterExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos) {}

FilterExecutor::~FilterExecutor() = default;

auto FilterExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  FilterStats stats{};
  while (inputRange.hasDataRow() && call.needSkipMore()) {
    auto const [unused, input] = inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    if (!input) {
      TRI_ASSERT(!inputRange.hasDataRow());
      break;
    }
    if (input.getValue(_infos.getInputRegister()).toBoolean()) {
      call.didSkip(1);
    } else {
      stats.incrFiltered();
    }
  }

  // Just fetch everything from above, allow overfetching
  AqlCall upstreamCall{};
  return {inputRange.upstreamState(), stats, call.getSkipCount(), upstreamCall};
}

auto FilterExecutor::produceRows(AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  TRI_IF_FAILURE("FilterExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  FilterStats stats{};

  while (inputRange.hasDataRow() && !output.isFull()) {
    auto const& [state, input] = inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input.isInitialized());
    if (input.getValue(_infos.getInputRegister()).toBoolean()) {
      output.copyRow(input);
      output.advanceRow();
    } else {
      stats.incrFiltered();
    }
  }

  // Just fetch everything from above, allow overfetching
  AqlCall upstreamCall{};
  return {inputRange.upstreamState(), stats, upstreamCall};
}

[[nodiscard]] auto FilterExecutor::expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                                           AqlCall const& call) const
    noexcept -> size_t {
  if (input.finalState() == ExecutorState::DONE) {
    return std::min(call.getLimit(), input.countDataRows());
  }
  // We do not know how many more rows will be returned from upstream.
  // So we can only overestimate
  return call.getLimit();
}
