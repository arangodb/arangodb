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
    : _fetcher(fetcher) {}

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
        callSet.calls.emplace_back(AqlCallSet::DepCallPair{currentDependency(),  AqlCall{}});
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
  while (call.needSkipMore() && !done()) {
    if (input.hasDataRow(currentDependency())) {
      auto [state, inputRow] = input.nextDataRow(currentDependency());
      call.didSkip(1);

      if (state == ExecutorState::DONE) {
        advanceDependency();
      }
    } else {
      if (input.upstreamState(currentDependency()) == ExecutorState::DONE) {
        advanceDependency();
      } else {
        // We need to fetch more first
        break;
      }
    }
  }

  while (!done() && input.upstreamState(currentDependency()) == ExecutorState::DONE) {
    advanceDependency();
  }

  if (done()) {
    return {ExecutorState::DONE, Stats{}, call.getSkipCount(), AqlCallSet{}};
  } else {
    // TODO Should we not generate a call that only skips, with a soft limit of 0,
    //      based off `call` rather than just asking for everything?
    //      In which case of course it is important to look into the skip count
    //      of the input ranges above!
    auto callSet = AqlCallSet{};
    callSet.calls.emplace_back(AqlCallSet::DepCallPair{currentDependency(), AqlCall{}});
    return {input.upstreamState(currentDependency()), Stats{}, call.getSkipCount(), callSet};
  }
}

auto UnsortedGatherExecutor::produceRows(OutputAqlItemRow& output)
    -> std::pair<ExecutionState, Stats> {
  while (!output.isFull() && !done()) {
    // Note that fetchNextRow may return DONE (because the current dependency is
    // DONE), and also return an unitialized row in that case, but we are not
    // DONE completely - that's what `done()` is for.
    auto [state, inputRow] = fetchNextRow(output.numRowsLeft());
    if (state == ExecutionState::WAITING) {
      return {state, {}};
    }
    // HASMORE => inputRow.isInitialized()
    TRI_ASSERT(state == ExecutionState::DONE || inputRow.isInitialized());
    if (inputRow.isInitialized()) {
      output.copyRow(inputRow);
      TRI_ASSERT(output.produced());
      output.advanceRow();
    }
  }

  auto state = done() ? ExecutionState::DONE : ExecutionState::HASMORE;
  return {state, {}};
}

auto UnsortedGatherExecutor::fetcher() const noexcept -> const Fetcher& {
  return _fetcher;
}

auto UnsortedGatherExecutor::fetcher() noexcept -> Fetcher& { return _fetcher; }

auto UnsortedGatherExecutor::numDependencies() const
    noexcept(noexcept(_fetcher.numberDependencies())) -> size_t {
  return _fetcher.numberDependencies();
}

auto UnsortedGatherExecutor::fetchNextRow(size_t atMost)
    -> std::pair<ExecutionState, InputAqlItemRow> {
  auto res = fetcher().fetchRowForDependency(currentDependency(), atMost);
  if (res.first == ExecutionState::DONE) {
    advanceDependency();
  }
  return res;
}

auto UnsortedGatherExecutor::skipNextRows(size_t atMost)
    -> std::pair<ExecutionState, size_t> {
  auto res = fetcher().skipRowsForDependency(currentDependency(), atMost);
  if (res.first == ExecutionState::DONE) {
    advanceDependency();
  }
  return res;
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

auto UnsortedGatherExecutor::skipRows(size_t const atMost)
    -> std::tuple<ExecutionState, UnsortedGatherExecutor::Stats, size_t> {
  auto const rowsLeftToSkip = [&atMost, &skipped = this->_skipped]() {
    TRI_ASSERT(atMost >= skipped);
    return atMost - skipped;
  };
  while (rowsLeftToSkip() > 0 && !done()) {
    // Note that skipNextRow may return DONE (because the current dependency is
    // DONE), and also return an unitialized row in that case, but we are not
    // DONE completely - that's what `done()` is for.
    auto [state, skipped] = skipNextRows(rowsLeftToSkip());
    _skipped += skipped;
    if (state == ExecutionState::WAITING) {
      return {state, {}, 0};
    }
  }

  auto state = done() ? ExecutionState::DONE : ExecutionState::HASMORE;
  auto skipped = size_t{0};
  std::swap(skipped, _skipped);
  return {state, {}, skipped};
}
