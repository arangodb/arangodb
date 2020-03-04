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

      if (state == ExecutorState::DONE) {
        advanceDependency();
      }
    } else {
      if (input.upstreamState(currentDependency()) == ExecutorState::DONE) {
        advanceDependency();
      } else {
        auto callSet = AqlCallSet{};
        // TODO shouldn't we use `output.getClientCall()` instead of `AqlCall{}` here?
        callSet.calls.emplace_back(AqlCallSet::DepCallPair{currentDependency(), AqlCall{}});
        return {input.upstreamState(currentDependency()), Stats{}, callSet};
      }
    }
  }

  while (!done() && input.upstreamState(currentDependency()) == ExecutorState::DONE) {
    advanceDependency();
  }

  if (done()) {
    TRI_ASSERT(!input.hasDataRow());
    return {ExecutorState::DONE, Stats{}, AqlCallSet{}};
  } else {
    auto callSet = AqlCallSet{};
    // TODO shouldn't we use `output.getClientCall()` instead of `AqlCall{}` here?
    callSet.calls.emplace_back(AqlCallSet::DepCallPair{currentDependency(), AqlCall{}});
    return {input.upstreamState(currentDependency()), Stats{}, callSet};
  }
}

auto UnsortedGatherExecutor::skipRowsRange(typename Fetcher::DataRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCallSet> {
  // TODO call input.skip()!
  // If we need to skip we can just tell our current dependency to skip
  // directly.

  // First try to skip upstream.
  auto skipped = size_t{0};

  while (call.needSkipMore()) {
    LOG_DEVEL << "need to skip more: " << call.getOffset();

    auto skippedHere = input.skipForDependency(currentDependency(), call.getOffset());
    call.didSkip(skippedHere);
    skipped += skippedHere;

    LOG_DEVEL << "skipped: " << skipped << " " << call.getOffset();

    // Skip over dependencies that are DONE, they cannot skip more
    while (!done() && input.upstreamState(currentDependency()) == ExecutorState::DONE) {
      LOG_DEVEL << " advance dep";
      advanceDependency();
    }

    if (done()) {
      return {ExecutorState::DONE, Stats{}, skipped, AqlCallSet{}};
    } else {
      // Ask for more skips
      if (call.needSkipMore()) {
        LOG_DEVEL << " skipping some more, asking dep " << call.getOffset();
        auto callSet = AqlCallSet{};
        callSet.calls.emplace_back(
            AqlCallSet::DepCallPair{currentDependency(), AqlCall{call.getOffset(), 0, 0}});
        return {ExecutorState::HASMORE, Stats{}, skipped, callSet};
      } else {
        return {ExecutorState::HASMORE, Stats{}, skipped, AqlCallSet{}};
      }
    }
  }
  TRI_ASSERT(false);
  return {ExecutorState::DONE, Stats{}, skipped, AqlCallSet{}};
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
