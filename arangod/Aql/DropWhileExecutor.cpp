////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "DropWhileExecutor.h"

#include "Aql/AqlCall.h"
#include "Aql/Stats.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/AqlItemBlockInputRange.h"

namespace arangodb::aql {

DropWhileExecutorInfos::DropWhileExecutorInfos(RegisterId inputRegister)
    : _inputRegister(inputRegister) {}

auto DropWhileExecutorInfos::getInputRegister() const noexcept -> RegisterId {
  return _inputRegister;
}

DropWhileExecutor::DropWhileExecutor(DropWhileExecutor::Fetcher&,
                                     DropWhileExecutor::Infos& infos)
    : _infos(infos) {}

auto DropWhileExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                    OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  FilterStats stats{};

  while (inputRange.hasDataRow() && !output.isFull()) {
    auto const& [state, input] =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input);
    TRI_ASSERT(input.isInitialized());
    if (!_stopDropping) {
      if (input.getValue(_infos.getInputRegister()).toBoolean()) {
        stats.incrFiltered();
      } else {
        _stopDropping = true;
      }
    }
    if (_stopDropping) {
      output.copyRow(input);
      output.advanceRow();
    }
  }

  return {inputRange.upstreamState(), stats, AqlCall{}};
}

auto DropWhileExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange,
                                      AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  FilterStats stats{};

  while (inputRange.hasDataRow() && call.needSkipMore()) {
    auto const& [_, input] =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input);
    TRI_ASSERT(input.isInitialized());
    if (!_stopDropping) {
      if (input.getValue(_infos.getInputRegister()).toBoolean()) {
        stats.incrFiltered();
      } else {
        _stopDropping = true;
      }
    }
    if (_stopDropping) {
      call.didSkip(1);
    }
  }

  // Just fetch everything from above, allow overfetching
  return {inputRange.upstreamState(), stats, call.getSkipCount(), AqlCall{}};
}

auto DropWhileExecutor::expectedNumberOfRowsNew(
    AqlItemBlockInputRange const& input, AqlCall const& call) const noexcept
    -> size_t {
  if (input.finalState() == MainQueryState::DONE) {
    // This could be improved (if _stopDropping is not yet true) by looking
    // for the first false value (if any) in our input register.
    return std::min(call.getLimit(), input.countDataRows());
  }
  // We do not know how many more rows will be returned from upstream.
  // So we can only overestimate
  return call.getLimit();
}

}  // namespace arangodb::aql
