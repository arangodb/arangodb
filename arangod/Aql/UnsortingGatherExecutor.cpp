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

#include "UnsortingGatherExecutor.h"
#include <Logger/LogMacros.h>

#include "Aql/IdExecutor.h"  // for IdExecutorInfos
#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"

using namespace arangodb;
using namespace arangodb::aql;

UnsortingGatherExecutor::UnsortingGatherExecutor(Fetcher& fetcher, Infos& infos)
    : _fetcher(fetcher) {}

UnsortingGatherExecutor::~UnsortingGatherExecutor() = default;

auto UnsortingGatherExecutor::produceRows(OutputAqlItemRow& output)
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

auto UnsortingGatherExecutor::fetcher() const noexcept -> const Fetcher& {
  return _fetcher;
}

auto UnsortingGatherExecutor::fetcher() noexcept -> Fetcher& {
  return _fetcher;
}

auto UnsortingGatherExecutor::numDependencies() const
    noexcept(noexcept(_fetcher.numberDependencies())) -> size_t {
  return _fetcher.numberDependencies();
}

auto UnsortingGatherExecutor::fetchNextRow(size_t atMost)
    -> std::pair<ExecutionState, InputAqlItemRow> {
  auto res = fetcher().fetchRowForDependency(currentDependency(), atMost);
  if (res.first == ExecutionState::DONE) {
    advanceDependency();
  }
  return res;
}

auto UnsortingGatherExecutor::skipNextRows(size_t atMost)
    -> std::pair<ExecutionState, size_t> {
  auto res = fetcher().skipRowsForDependency(currentDependency(), atMost);
  if (res.first == ExecutionState::DONE) {
    advanceDependency();
  }
  return res;
}

auto UnsortingGatherExecutor::done() const noexcept -> bool {
  return _currentDependency >= numDependencies();
}

auto UnsortingGatherExecutor::currentDependency() const noexcept -> size_t {
  return _currentDependency;
}

auto UnsortingGatherExecutor::advanceDependency() noexcept -> void {
  TRI_ASSERT(_currentDependency < numDependencies());
  ++_currentDependency;
}

auto UnsortingGatherExecutor::skipRows(size_t const atMost)
    -> std::tuple<ExecutionState, UnsortingGatherExecutor::Stats, size_t> {
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
