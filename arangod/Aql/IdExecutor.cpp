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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "IdExecutor.h"
#include "Aql/AqlValue.h"
#include "Aql/ConstFetcher.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Common.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

IdExecutorInfos::IdExecutorInfos(RegisterId nrInOutRegisters,
                                 // cppcheck-suppress passedByValue
                                 std::unordered_set<RegisterId> registersToKeep,
                                 // cppcheck-suppress passedByValue
                                 std::unordered_set<RegisterId> registersToClear,
                                 std::string const& distributeId)
    : ExecutorInfos(make_shared_unordered_set(), make_shared_unordered_set(),
                    nrInOutRegisters, nrInOutRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _distributeId(distributeId) {}

template <bool usePassThrough, class UsedFetcher>
IdExecutor<usePassThrough, UsedFetcher>::IdExecutor(Fetcher& fetcher, IdExecutorInfos& infos)
    : _fetcher(fetcher) {
  if (!infos.distributeId().empty()) {
    _fetcher.setDistributeId(infos.distributeId());
  }
}

template <bool usePassThrough, class UsedFetcher>
IdExecutor<usePassThrough, UsedFetcher>::~IdExecutor() = default;

template <bool usePassThrough, class UsedFetcher>
std::pair<ExecutionState, NoStats> IdExecutor<usePassThrough, UsedFetcher>::produceRows(OutputAqlItemRow& output) {
  ExecutionState state = ExecutionState::HASMORE;
  NoStats stats;
  InputAqlItemRow inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
  while (!output.isFull() && state != ExecutionState::DONE) {
    std::tie(state, inputRow) = _fetcher.fetchRow(output.numRowsLeft());

    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(!inputRow);
      return {state, std::move(stats)};
    }

    if (!inputRow) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, std::move(stats)};
    }

    TRI_IF_FAILURE("SingletonBlock::getOrSkipSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_ASSERT(state == ExecutionState::HASMORE || state == ExecutionState::DONE);
    /*Second parameter are to ignore registers that should be kept but are missing in the input row*/
    output.copyRow(inputRow, std::is_same<UsedFetcher, ConstFetcher>::value);
    TRI_ASSERT(output.produced());
    output.advanceRow();

    TRI_IF_FAILURE("SingletonBlock::getOrSkipSomeSet") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  return {state, std::move(stats)};
}

template class ::arangodb::aql::IdExecutor<true, ConstFetcher>;
// ID can always pass through
template class ::arangodb::aql::IdExecutor<true, SingleRowFetcher<true>>;
// Local gather does NOT want to passThrough
template class ::arangodb::aql::IdExecutor<false, SingleRowFetcher<false>>;
