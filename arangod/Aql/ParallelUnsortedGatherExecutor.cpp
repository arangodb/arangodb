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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ParallelUnsortedGatherExecutor.h"

#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Transaction/Methods.h"

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

ParallelUnsortedGatherExecutorInfos::ParallelUnsortedGatherExecutorInfos(
    RegisterId nrInOutRegisters, std::unordered_set<RegisterId> registersToKeep,
    std::unordered_set<RegisterId> registersToClear)
    : ExecutorInfos(make_shared_unordered_set(), make_shared_unordered_set(),
                    nrInOutRegisters, nrInOutRegisters,
                    std::move(registersToClear), std::move(registersToKeep)) {}

ParallelUnsortedGatherExecutor::ParallelUnsortedGatherExecutor(Fetcher& fetcher, Infos& infos)
    : _fetcher(fetcher), _numberDependencies(0), _currentDependency(0), _skipped(0) {}

ParallelUnsortedGatherExecutor::~ParallelUnsortedGatherExecutor() = default;

auto ParallelUnsortedGatherExecutor::upstreamCall() const noexcept -> AqlCall {
  // TODO: Implement me
  return AqlCall{};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Guarantees requiredby this this block:
///        1) For every dependency the input is sorted, according to the same strategy.
///
///        What this block does:
///        InitPhase:
///          Fetch 1 Block for every dependency.
///        ExecPhase:
///          Fetch row of scheduled block.
///          Pick the next (sorted) element (by strategy)
///          Schedule this block to fetch Row
///
////////////////////////////////////////////////////////////////////////////////

auto ParallelUnsortedGatherExecutor::produceRows(typename Fetcher::DataRange& input,
                                                 OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall, size_t> {
  // Illegal dependency, on purpose to trigger asserts
  size_t waitingDep = input.numberDependencies();
  for (size_t dep = 0; dep < input.numberDependencies(); ++dep) {
    while (!output.isFull()) {
      auto [state, row] = input.nextDataRow(dep);
      if (row) {
        output.copyRow(row);
        output.advanceRow();
      } else {
        // This output did not produce anything
        if (state == ExecutorState::HASMORE) {
          waitingDep = dep;
        }
        break;
      }
    }
  }
  if (input.isDone()) {
    // We cannot have one that we are waiting on, if we are done.
    TRI_ASSERT(waitingDep == input.numberDependencies());
    return {ExecutorState::DONE, NoStats{}, AqlCall{}, waitingDep};
  }
  return {ExecutorState::HASMORE, NoStats{}, upstreamCall(), waitingDep};
}

auto ParallelUnsortedGatherExecutor::skipRowsRange(typename Fetcher::DataRange& input,
                                                   AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall, size_t> {
  size_t waitingDep = input.numberDependencies();
  for (size_t dep = 0; dep < input.numberDependencies(); ++dep) {
    auto& range = input.rangeForDependency(dep);
    while (call.needSkipMore()) {
      if (!range.hasDataRow() && range.skippedInFlight() == 0) {
        // Consumed this range,
        // consume the next one

        // Guarantee:
        // While in offsetPhase, we will only send requests to the first
        // NON-DONE dependency.
        if (range.upstreamState() == ExecutorState::HASMORE &&
            waitingDep == input.numberDependencies()) {
          waitingDep = dep;
        }
        break;
      }
      if (range.hasDataRow()) {
        // We overfetched, skipLocally
        // By gurantee we will only see data, if
        // we are past the offset phase.
        TRI_ASSERT(call.getOffset() == 0);
      } else {
        if (call.getOffset() > 0) {
          call.didSkip(range.skip(call.getOffset()));
        } else {
          // Fullcount Case
          call.didSkip(range.skipAll());
        }
      }
    }
  }
  if (input.isDone()) {
    // We cannot have one that we are waiting on, if we are done.
    TRI_ASSERT(waitingDep == input.numberDependencies());
    return {ExecutorState::DONE, NoStats{}, call.getSkipCount(), AqlCall{}, waitingDep};
  }
  return {ExecutorState::HASMORE, NoStats{}, call.getSkipCount(), upstreamCall(), waitingDep};
}
