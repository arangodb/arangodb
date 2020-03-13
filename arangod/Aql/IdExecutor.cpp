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

#include "Aql/AqlCall.h"
#include "Aql/AqlCallStack.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/AqlValue.h"
#include "Aql/ConstFetcher.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/QueryOptions.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"

#include <algorithm>
#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

IdExecutorInfos::IdExecutorInfos(RegisterId nrInOutRegisters,
                                 // cppcheck-suppress passedByValue
                                 std::unordered_set<RegisterId> registersToKeep,
                                 // cppcheck-suppress passedByValue
                                 std::unordered_set<RegisterId> registersToClear,
                                 bool doCount, RegisterId outputRegister,
                                 std::string distributeId, bool isResponsibleForInitializeCursor)
    : ExecutorInfos(make_shared_unordered_set(), make_shared_unordered_set(),
                    nrInOutRegisters, nrInOutRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _doCount(doCount),
      _outputRegister(outputRegister),
      _distributeId(std::move(distributeId)),
      _isResponsibleForInitializeCursor(isResponsibleForInitializeCursor) {
  // We can only doCount in the case where this executor is used as a Return.
  // And we can only have a distributeId if this executor is used as Gather.
  TRI_ASSERT(!_doCount || _distributeId.empty());
}

auto IdExecutorInfos::doCount() const noexcept -> bool { return _doCount; }

auto IdExecutorInfos::getOutputRegister() const noexcept -> RegisterId {
  return _outputRegister;
}

std::string const& IdExecutorInfos::distributeId() { return _distributeId; }

bool IdExecutorInfos::isResponsibleForInitializeCursor() const {
  return _isResponsibleForInitializeCursor;
}

template <class UsedFetcher>
IdExecutor<UsedFetcher>::IdExecutor(Fetcher& fetcher, IdExecutorInfos& infos)
    : _fetcher(fetcher), _infos(infos) {
  if (!infos.distributeId().empty()) {
    _fetcher.setDistributeId(infos.distributeId());
  }
}

template <class UsedFetcher>
IdExecutor<UsedFetcher>::~IdExecutor() = default;

template <class UsedFetcher>
auto IdExecutor<UsedFetcher>::produceRows(AqlItemBlockInputRange& inputRange,
                                          OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, CountStats, AqlCall> {
  CountStats stats;
  TRI_ASSERT(output.numRowsWritten() == 0);
  // TODO: We can implement a fastForward copy here.
  // We know that all rows we have will fit into the output
  while (!output.isFull() && inputRange.hasDataRow()) {
    auto const& [state, inputRow] = inputRange.nextDataRow();
    TRI_ASSERT(inputRow);

    TRI_IF_FAILURE("SingletonBlock::getOrSkipSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    /*Second parameter are to ignore registers that should be kept but are missing in the input row*/
    output.copyRow(inputRow, std::is_same_v<UsedFetcher, ConstFetcher>);
    TRI_ASSERT(output.produced());
    output.advanceRow();

    TRI_IF_FAILURE("SingletonBlock::getOrSkipSomeSet") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }
  if (_infos.doCount()) {
    stats.addCounted(output.numRowsWritten());
  }

  return {inputRange.upstreamState(), stats, output.getClientCall()};
}

template <class UsedFetcher>
auto IdExecutor<UsedFetcher>::skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
    -> std::tuple<ExecutorState, CountStats, size_t, AqlCall> {
  CountStats stats;
  size_t skipped = 0;
  if (call.getLimit() > 0) {
    // we can only account for offset
    skipped = inputRange.skip(call.getOffset());
  } else {
    skipped = inputRange.skipAll();
  }
  call.didSkip(skipped);
  // TODO: Do we need to do counting here?
  return {inputRange.upstreamState(), stats, skipped, call};
}

template <class UsedFetcher>
std::tuple<ExecutionState, typename IdExecutor<UsedFetcher>::Stats, SharedAqlItemBlockPtr>
IdExecutor<UsedFetcher>::fetchBlockForPassthrough(size_t atMost) {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

template class ::arangodb::aql::IdExecutor<ConstFetcher>;
// ID can always pass through
template class ::arangodb::aql::IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>;
