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

#include "TakeWhileExecutor.h"

#include "Aql/AqlCall.h"
#include "Aql/Stats.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/AqlItemBlockInputRange.h"

namespace arangodb::aql {

TakeWhileExecutorInfos::TakeWhileExecutorInfos(RegisterId inputRegister,
                                               bool emitFirstFalseLine)
    : _inputRegister(inputRegister), _emitFirstFalseLine(emitFirstFalseLine) {}

auto TakeWhileExecutorInfos::getInputRegister() const noexcept -> RegisterId {
  return _inputRegister;
}

TakeWhileExecutor::TakeWhileExecutor(TakeWhileExecutor::Fetcher& fetcher,
                                     TakeWhileExecutor::Infos& infos)
    : _infos(infos) {}

[[nodiscard]] auto TakeWhileExecutor::produceRows(AqlItemBlockInputRange& input,
                                                  OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  TakeWhileStats stats;
  // TODO Implement correctly instead of pass-through
  if (input.hasDataRow()) {
    TRI_ASSERT(!output.isFull());
    TRI_IF_FAILURE("SingletonBlock::getOrSkipSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    auto const& [state, inputRow] = input.peekDataRow();

    size_t rows = input.countAndSkipAllRemainingDataRows();

    output.fastForwardAllRows(inputRow, rows);

    TRI_IF_FAILURE("SingletonBlock::getOrSkipSomeSet") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }
  TRI_ASSERT(!input.hasDataRow());
  // TODO increase stats
  // stats.incrCounted(output.numRowsWritten());

  return {input.upstreamState(), stats, output.getClientCall()};
}

auto TakeWhileExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange,
                                      AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  std::abort();  // TODO
}

}  // namespace arangodb::aql
