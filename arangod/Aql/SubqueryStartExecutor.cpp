////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "SubqueryStartExecutor.h"

#include "Aql/ExecutionState.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

SubqueryStartExecutor::SubqueryStartExecutor(Fetcher& fetcher, Infos& infos) {}

std::pair<ExecutionState, NoStats> SubqueryStartExecutor::produceRows(OutputAqlItemRow& output) {
  TRI_ASSERT(false);
}

auto SubqueryStartExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  if (_inputRow.isInitialized()) {
    // We have not been able to report the ShadowRow.
    // Simply return DONE to trigger Impl to fetch shadow row instead.
    return {ExecutorState::DONE, NoStats{}, AqlCall{}};
  }
  TRI_ASSERT(!_inputRow.isInitialized());
  TRI_ASSERT(!output.isFull());
  if (input.hasDataRow()) {
    std::tie(_upstreamState, _inputRow) = input.peekDataRow();
    output.copyRow(_inputRow);
    output.advanceRow();
    return {ExecutorState::DONE, NoStats{}, AqlCall{}};
  }
  return {input.upstreamState(), NoStats{}, AqlCall{}};
}

auto SubqueryStartExecutor::skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  // We must not have a row pending to be written as a
  // shadow row
  TRI_ASSERT(!_inputRow.isInitialized());

  if (call.shouldSkip() && input.hasDataRow()) {
    std::tie(_upstreamState, _inputRow) = input.nextDataRow();
    call.didSkip(1);
  }
  return {ExecutorState::DONE, NoStats{}, 1, AqlCall{}};
}

auto SubqueryStartExecutor::produceShadowRow(AqlItemBlockInputRange& input,
                                             OutputAqlItemRow& output) -> bool {
  TRI_ASSERT(!output.isFull());
  if (_inputRow.isInitialized()) {
    // Actually consume the input row now.
    auto const [upstreamState, inputRow] = input.nextDataRow();
    // We are only supposed to report the inputRow we
    // have seen in produce as a ShadowRow
    TRI_ASSERT(inputRow.isSameBlockAndIndex(_inputRow));
    output.createShadowRow(_inputRow);
    output.advanceRow();
    // Reset local input row
    _inputRow = InputAqlItemRow(CreateInvalidInputRowHint{});
    return true;
  }
  return false;
}

// TODO: remove me
auto SubqueryStartExecutor::expectedNumberOfRows(size_t atMost) const
    -> std::pair<ExecutionState, size_t> {
  TRI_ASSERT(false);
  return {ExecutionState::DONE, 0};
}
