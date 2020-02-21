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
  // We must not have a row pending to be written as a
  // shadow row
  TRI_ASSERT(!_inputRow.isInitialized());
  if (!output.isFull()) {
    if (input.hasDataRow()) {
      std::tie(_upstreamState, _inputRow) = input.nextDataRow();
      output.copyRow(_inputRow);
      output.advanceRow();
      return {ExecutorState::DONE, NoStats{}, AqlCall{}};
    }
  } else {
    return {ExecutorState::HASMORE, NoStats{}, AqlCall{}};
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

auto SubqueryStartExecutor::produceShadowRow(OutputAqlItemRow& output) -> void {
  // TRI_ASSERT(!output.isFull());
  if (_inputRow.isInitialized()) {
    output.createShadowRow(_inputRow);
    // We do not advanceRow here to make cleaner code in ExecutionBlockImpl
    _inputRow = InputAqlItemRow(CreateInvalidInputRowHint{});
  }
}

// TODO: remove me
auto SubqueryStartExecutor::expectedNumberOfRows(size_t atMost) const
    -> std::pair<ExecutionState, size_t> {
  TRI_ASSERT(false);
  return {ExecutionState::DONE, 0};
}
