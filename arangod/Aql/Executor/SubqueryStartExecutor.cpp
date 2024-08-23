////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutionState.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"

namespace arangodb::aql {

SubqueryStartExecutor::SubqueryStartExecutor(Fetcher&, Infos&) {}

auto SubqueryStartExecutor::produceRows(AqlItemBlockInputRange& input,
                                        OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  if (_inputRow.isInitialized()) {
    // We have not been able to report the ShadowRow.
    // Simply return DONE to trigger Impl to fetch shadow row instead.
    return {ExecutorState::DONE, NoStats{}, AqlCall{}};
  }
  TRI_ASSERT(!_inputRow.isInitialized());
  if (input.hasDataRow()) {
    TRI_ASSERT(!output.isFull());
    std::tie(_upstreamState, _inputRow) =
        input.peekDataRow(AqlItemBlockInputRange::HasDataRow{});
    output.copyRow(_inputRow);
    output.advanceRow();
    return {ExecutorState::DONE, NoStats{}, AqlCall{}};
  }
  return {input.upstreamState(), NoStats{}, AqlCall{}};
}

auto SubqueryStartExecutor::skipRowsRange(AqlItemBlockInputRange& input,
                                          AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  TRI_ASSERT(call.needSkipMore());
  if (_inputRow.isInitialized()) {
    // We have not been able to report the ShadowRow.
    // Simply return DONE to trigger Impl to fetch shadow row instead.
    return {ExecutorState::DONE, NoStats{}, 0, AqlCall{}};
  }

  if (input.hasDataRow()) {
    // Do not consume the row.
    // It needs to be reported in Produce.
    std::tie(_upstreamState, _inputRow) =
        input.peekDataRow(AqlItemBlockInputRange::HasDataRow{});
    call.didSkip(1);
    return {ExecutorState::DONE, NoStats{}, call.getSkipCount(), AqlCall{}};
  }
  return {input.upstreamState(), NoStats{}, 0, AqlCall{}};
}

auto SubqueryStartExecutor::produceShadowRow(AqlItemBlockInputRange& input,
                                             OutputAqlItemRow& output) -> bool {
  TRI_ASSERT(!output.allRowsUsed());
  if (_inputRow.isInitialized()) {
    // Actually consume the input row now.
    TRI_ASSERT(input.hasDataRow());
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

[[nodiscard]] auto SubqueryStartExecutor::expectedNumberOfRows(
    AqlItemBlockInputRange const& input, AqlCall const& call) const noexcept
    -> size_t {
  // The DataRow is consumed after a shadowRow is produced.
  // So if there is no datarow in the input we will not create a data or a
  // shadowRow, we might be off by one, if we get asked this and have written
  // the last dataRow. However, as we only overallocate a single row then, this
  // is not too bad.
  if (input.countDataRows() > 0) {
    // We will write one ShadowRow
    if (call.getLimit() > 0) {
      // We will write one DataRow
      return 2 * input.countDataRows();
    }
    return input.countDataRows();
  }
  // Nothing to create here.
  return 0;
}

template class ExecutionBlockImpl<SubqueryStartExecutor>;

}  // namespace arangodb::aql
