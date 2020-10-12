////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

IdExecutorInfos::IdExecutorInfos(bool doCount, RegisterId outputRegister,
                                 std::string distributeId, bool isResponsibleForInitializeCursor)
    : _doCount(doCount),
      _isResponsibleForInitializeCursor(isResponsibleForInitializeCursor),
      _outputRegister(outputRegister),
      _distributeId(std::move(distributeId)) {
  // We can only doCount in the case where this executor is used as a Return.
  // And we can only have a distributeId if this executor is used as Gather.
  TRI_ASSERT(!_doCount || _distributeId.empty());
}

auto IdExecutorInfos::doCount() const noexcept -> bool { return _doCount; }

auto IdExecutorInfos::getOutputRegister() const noexcept -> RegisterId {
  return _outputRegister;
}

std::string const& IdExecutorInfos::distributeId() const noexcept { return _distributeId; }

bool IdExecutorInfos::isResponsibleForInitializeCursor() const noexcept {
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
  if (inputRange.hasDataRow()) {
    TRI_ASSERT(!output.isFull());
    TRI_IF_FAILURE("SingletonBlock::getOrSkipSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    auto const& [state, inputRow] = inputRange.peekDataRow();

    output.fastForwardAllRows(inputRow, inputRange.countDataRows());

    std::ignore = inputRange.skipAllRemainingDataRows();

    TRI_IF_FAILURE("SingletonBlock::getOrSkipSomeSet") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }
  TRI_ASSERT(!inputRange.hasDataRow());
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
  return {inputRange.upstreamState(), stats, call.getSkipCount(), call};
}

template class ::arangodb::aql::IdExecutor<ConstFetcher>;
// ID can always pass through
template class ::arangodb::aql::IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>;
