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

UnsortedGatherExecutor::UnsortedGatherExecutor(Fetcher&, Infos&) {}

UnsortedGatherExecutor::~UnsortedGatherExecutor() = default;

auto UnsortedGatherExecutor::produceRows(typename Fetcher::DataRange& input,
                                         OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall, size_t> {
  initialize(input);
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
        return {input.upstreamState(currentDependency()), Stats{}, AqlCall{},
                currentDependency()};
      }
    }
  }

  while (!done() && input.upstreamState(currentDependency()) == ExecutorState::DONE) {
    advanceDependency();
  }

  if (done()) {
    // here currentDependency is invalid which will cause things to crash
    // if we ask upstream in ExecutionBlockImpl. yolo.
    TRI_ASSERT(!input.hasDataRow());
    return {ExecutorState::DONE, Stats{}, AqlCall{}, currentDependency()};
  } else {
    return {input.upstreamState(currentDependency()), Stats{}, AqlCall{},
            currentDependency()};
  }
}

auto UnsortedGatherExecutor::skipRowsRange(typename Fetcher::DataRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall, size_t> {
  initialize(input);
  auto skipped = size_t{0};
  while (call.needSkipMore() && !done() && input.hasDataRow(currentDependency())) {
    auto [state, inputRow] = input.nextDataRow(currentDependency());

    call.didSkip(1);
    skipped++;

    if (state == ExecutorState::DONE) {
      advanceDependency();
    }
  }

  if (done()) {
    // here currentDependency is invalid which will cause things to crash
    // if we ask upstream in ExecutionBlockImpl. yolo.
    return {ExecutorState::DONE, Stats{}, skipped, AqlCall{}, currentDependency()};
  } else {
    return {input.upstreamState(currentDependency()), Stats{}, skipped,
            AqlCall{}, currentDependency()};
  }
}

auto UnsortedGatherExecutor::initialize(typename Fetcher::DataRange const& input) -> void {
  // Dependencies can never change
  TRI_ASSERT(_numDependencies == 0 || _numDependencies == input.numberDependencies());
  _numDependencies = input.numberDependencies();
}

auto UnsortedGatherExecutor::numDependencies() const noexcept -> size_t {
  TRI_ASSERT(_numDependencies != 0);
  return _numDependencies;
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