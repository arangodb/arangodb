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
#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Basics/debugging.h"

using namespace arangodb;
using namespace arangodb::aql;

UnsortedGatherExecutor::UnsortedGatherExecutor(Fetcher& fetcher, Infos& infos)
    : _fetcher(fetcher) {}

UnsortedGatherExecutor::~UnsortedGatherExecutor() = default;

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
