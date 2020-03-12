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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "UnsortedGatherExecutor.h"

#include "Aql/IdExecutor.h"  // for IdExecutorInfos
#include "Aql/MultiAqlItemBlockInputRange.h"
#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Basics/debugging.h"

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

struct Dependency {
  Dependency() : _number{0} {};

  size_t _number;
};

UnsortedGatherExecutor::UnsortedGatherExecutor(Fetcher& fetcher, Infos& infos)
    : _fetcher{fetcher} {}

UnsortedGatherExecutor::~UnsortedGatherExecutor() = default;

auto UnsortedGatherExecutor::produceRows(typename Fetcher::DataRange& input,
                                         OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCallSet> {
  while (!output.isFull() && !done()) {
    if (input.hasDataRow(currentDependency())) {
      auto [state, inputRow] = input.nextDataRow(currentDependency());
      output.copyRow(inputRow);
      TRI_ASSERT(output.produced());
      output.advanceRow();
    } else {
      if (input.upstreamState(currentDependency()) == ExecutorState::DONE) {
        TRI_ASSERT(input.rangeForDependency(currentDependency()).skippedInFlight() == 0);
        advanceDependency();
      } else {
        auto callSet = AqlCallSet{};
        callSet.calls.emplace_back(
            AqlCallSet::DepCallPair{currentDependency(), output.getClientCall()});
        return {input.upstreamState(currentDependency()), Stats{}, callSet};
      }
    }
  }

  while (!done() && input.upstreamState(currentDependency()) == ExecutorState::DONE) {
    auto range = input.rangeForDependency(currentDependency());
    if (range.upstreamState() == ExecutorState::HASMORE || range.skippedInFlight() > 0) {
      // skippedInFlight > 0 -> output.isFull()
      TRI_ASSERT(range.skippedInFlight() == 0 || output.isFull());
      break;
    }
    TRI_ASSERT(input.rangeForDependency(currentDependency()).skippedInFlight() == 0);
    advanceDependency();
  }

  if (done()) {
    TRI_ASSERT(!input.hasDataRow());
    return {ExecutorState::DONE, Stats{}, AqlCallSet{}};
  } else {
    auto callSet = AqlCallSet{};
    callSet.calls.emplace_back(
        AqlCallSet::DepCallPair{currentDependency(), output.getClientCall()});
    return {input.upstreamState(currentDependency()), Stats{}, callSet};
  }
}

auto UnsortedGatherExecutor::skipRowsRange(typename Fetcher::DataRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCallSet> {
  auto skipped = size_t{0};

  if (call.getOffset() > 0) {
    skipped = input.skipForDependency(currentDependency(), call.getOffset());
  } else {
    skipped = input.skipAllForDependency(currentDependency());
  }
  call.didSkip(skipped);

  // Skip over dependencies that are DONE, they cannot skip more
  while (!done() && input.upstreamState(currentDependency()) == ExecutorState::DONE) {
    advanceDependency();
  }

  // Here we are either done, or currentDependency() still could produce more
  if (done()) {
    return {ExecutorState::DONE, Stats{}, skipped, AqlCallSet{}};
  } else {
    // If we're not done skipping, we can just request the current clientcall
    // from upstream
    auto callSet = AqlCallSet{};
    if (call.needSkipMore()) {
      callSet.calls.emplace_back(AqlCallSet::DepCallPair{currentDependency(), call});
    }
    return {ExecutorState::HASMORE, Stats{}, skipped, callSet};
  }
}

auto UnsortedGatherExecutor::numDependencies() const
    noexcept(noexcept(_fetcher.numberDependencies())) -> size_t {
  return _fetcher.numberDependencies();
}

auto UnsortedGatherExecutor::done() const noexcept -> bool {
  return _currentDependency >= numDependencies();
}

auto UnsortedGatherExecutor::currentDependency() const noexcept -> size_t {
  return _currentDependency;
}

auto UnsortedGatherExecutor::advanceDependency() noexcept -> void {
  TRI_ASSERT(_currentDependency < numDependencies());
  ++_currentDependency;
}
