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

#include "CountCollectExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/Stats.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

CountCollectExecutorInfos::CountCollectExecutorInfos(RegisterId collectRegister)
    : _collectRegister(collectRegister) {}

RegisterId CountCollectExecutorInfos::getOutputRegisterId() const {
  return _collectRegister;
}

CountCollectExecutor::CountCollectExecutor(Fetcher&, Infos& infos)
    : _infos(infos) {}

auto CountCollectExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                       OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  TRI_IF_FAILURE("CountCollectExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  // skipped > 0 -> done
  // We have the guarantee that every thing is skipped
  // within a single call
  TRI_ASSERT(inputRange.skippedInFlight() == 0 ||
             inputRange.upstreamState() == ExecutorState::DONE);
  if (inputRange.upstreamState() == ExecutorState::DONE) {
    // We have skipped all, report it.
    // In general, we do not have an input row. In fact, we never fetch one.
    output.setAllowSourceRowUninitialized();

    // We must produce exactly one output row.
    output.cloneValueInto(_infos.getOutputRegisterId(),
                          InputAqlItemRow{CreateInvalidInputRowHint{}},
                          AqlValue(AqlValueHintUInt(inputRange.skipAll())));
    output.advanceRow();
  }
  // We always send a hardLimit with fullcount to upstream
  return {inputRange.upstreamState(), NoStats{},
          AqlCall{0, true, 0, AqlCall::LimitType::HARD}};
}

auto CountCollectExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  TRI_IF_FAILURE("CountCollectExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  // skipped > 0 -> done
  // We have the guarantee that every thing is skipped
  // within a single call
  TRI_ASSERT(inputRange.skippedInFlight() == 0 ||
             inputRange.upstreamState() == ExecutorState::DONE);
  if (inputRange.upstreamState() == ExecutorState::DONE) {
    // We have skipped all, report it.
    auto skipped = inputRange.skipAll();
    // We request to just forward, we do not even care how much
    TRI_ASSERT(skipped == 0);
    call.didSkip(1);
  }
  // We always send a hardLimit with fullcount to upstream
  return {inputRange.upstreamState(), NoStats{}, call.getSkipCount(),
          AqlCall{0, false, 0, AqlCall::LimitType::HARD}};
}

auto CountCollectExecutor::expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                                   AqlCall const& call) const
    noexcept -> size_t {
  auto subqueries = input.countShadowRows();
  if (subqueries > 0) {
    // We will return 1 row for every subquery execution.
    // We do overallocate here if we have nested subqueries
    return subqueries;
  }
  // No subqueries, we are at the end of the execution.
  // We either return 1, or the callLimit.
  return std::min<size_t>(1, call.getLimit());
}

const CountCollectExecutor::Infos& CountCollectExecutor::infos() const noexcept {
  return _infos;
}

CountCollectExecutor::~CountCollectExecutor() = default;
