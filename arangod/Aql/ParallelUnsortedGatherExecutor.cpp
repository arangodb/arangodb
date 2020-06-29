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

ParallelUnsortedGatherExecutor::ParallelUnsortedGatherExecutor(Fetcher&, Infos& infos) {}

ParallelUnsortedGatherExecutor::~ParallelUnsortedGatherExecutor() = default;

auto ParallelUnsortedGatherExecutor::upstreamCallSkip(AqlCall const& clientCall) const
    noexcept -> AqlCall {
  TRI_ASSERT(clientCall.needSkipMore());

  // Only skip, don't ask for rows
  if (clientCall.getOffset() > 0) {
    auto upstreamCall = clientCall;
    upstreamCall.softLimit = 0u;
    upstreamCall.hardLimit = AqlCall::Infinity{};
    upstreamCall.fullCount = false;
    return upstreamCall;
  }
  TRI_ASSERT(clientCall.getLimit() == 0 && clientCall.hasHardLimit());

  // This can onyl be fullCount or fastForward call.
  // Send it upstream.
  return clientCall;
}

auto ParallelUnsortedGatherExecutor::upstreamCallProduce(AqlCall const& clientCall) const
    noexcept -> AqlCall {
  TRI_ASSERT(clientCall.getOffset() == 0);
  return clientCall;
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
    -> std::tuple<ExecutorState, Stats, AqlCallSet> {
  auto const& clientCall = output.getClientCall();
  TRI_ASSERT(clientCall.getOffset() == 0);

  auto callSet = AqlCallSet{};

  for (size_t dep = 0; dep < input.numberDependencies(); ++dep) {
    auto& range = input.rangeForDependency(dep);
    while (!output.isFull() && range.hasDataRow()) {
      auto [state, row] = range.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
      TRI_ASSERT(row);
      output.copyRow(row);
      output.advanceRow();
    }
    // Produce a new call if necessary (we consumed all, and the state is still HASMORE)
    if (!range.hasDataRow() && range.upstreamState() == ExecutorState::HASMORE) {
      callSet.calls.emplace_back(
          AqlCallSet::DepCallPair{dep, AqlCallList{upstreamCallProduce(clientCall)}});
    }
  }

  // We cannot have one that we are waiting on, if we are done.
  TRI_ASSERT(!input.isDone() || callSet.empty());

  return {input.state(), NoStats{}, callSet};
}

auto ParallelUnsortedGatherExecutor::skipRowsRange(typename Fetcher::DataRange& input,
                                                   AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCallSet> {
  // TODO skipping is currently not parallelized, but should be
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
        // By guarantee we will only see data, if
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
  // We need to retain the skip,
  // but are not allowed to hand it over to
  // the callset.
  // so copy it out and reset
  size_t skipped = call.getSkipCount();
  call.resetSkipCount();
  if (input.isDone()) {
    // We cannot have one that we are waiting on, if we are done.
    TRI_ASSERT(waitingDep == input.numberDependencies());
    return {ExecutorState::DONE, NoStats{}, skipped, AqlCallSet{}};
  }
  auto callSet = AqlCallSet{};
  if (call.needSkipMore()) {
    // We are not done with skipping.
    // Prepare next call.
    callSet.calls.emplace_back(
        AqlCallSet::DepCallPair{waitingDep, AqlCallList{upstreamCallSkip(call)}});
  }
  return {ExecutorState::HASMORE, NoStats{}, skipped, callSet};
}
