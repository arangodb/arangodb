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

std::pair<ExecutionState, NoStats> ParallelUnsortedGatherExecutor::produceRows(OutputAqlItemRow& output) {
  initDependencies();
  
  ExecutionState state;
  InputAqlItemRow inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
  
  size_t x;
  for (x = 0; x < _numberDependencies; ++x) {
    size_t i = (_currentDependency + x) % _numberDependencies;
  
    if (_upstream[i] == ExecutionState::DONE) {
      continue;
    }
    
    size_t tmp = 0;
    
    state = ExecutionState::HASMORE;
    while (!output.isFull() && state == ExecutionState::HASMORE) {
      std::tie(state, inputRow) = _fetcher.fetchRowForDependency(i, output.numRowsLeft() /*atMost*/);
      if (inputRow) {
        output.copyRow(inputRow);
        TRI_ASSERT(output.produced());
        output.advanceRow();
        tmp++;
      }
    }
    
    _upstream[i] = state;
    if (output.isFull()) {
      break;
    }
  }
  _currentDependency = x;
  
  NoStats stats;
  
  // fix assert in ExecutionBlockImpl<E>::getSomeWithoutTrace
  if (output.isFull()) {
    return {ExecutionState::HASMORE, stats};
  }
  
  size_t numWaiting = 0;
  for (x = 0; x < _numberDependencies; ++x) {
    if (_upstream[x] == ExecutionState::HASMORE) {
      return {ExecutionState::HASMORE, stats};
    } else if (_upstream[x] == ExecutionState::WAITING) {
      numWaiting++;
    }
  }
  if (numWaiting > 0) {
    return {ExecutionState::WAITING, stats};
  }
  
  TRI_ASSERT(std::all_of(_upstream.begin(), _upstream.end(), [](auto const& s) { return s == ExecutionState::DONE; } ));
  return {ExecutionState::DONE, stats};
}

std::tuple<ExecutionState, ParallelUnsortedGatherExecutor::Stats, size_t>
ParallelUnsortedGatherExecutor::skipRows(size_t const toSkip) {
  initDependencies();
  TRI_ASSERT(_skipped <= toSkip);
  
  ExecutionState state = ExecutionState::HASMORE;
  while (_skipped < toSkip) {
    
    const size_t i = _currentDependency;
    if (_upstream[i] == ExecutionState::DONE) {
      if (std::all_of(_upstream.begin(), _upstream.end(),
                      [](auto s) { return s == ExecutionState::DONE; })) {
         state = ExecutionState::DONE;
         break;
       }
      _currentDependency = (i + 1) % _numberDependencies;
       continue;
     }
    
    TRI_ASSERT(_skipped <= toSkip);

    size_t skippedNow;
    std::tie(state, skippedNow) = _fetcher.skipRowsForDependency(i, toSkip - _skipped);
    _upstream[i] = state;
    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(skippedNow == 0);
      return {ExecutionState::WAITING, NoStats{}, 0};
    }
    _skipped += skippedNow;
    
    if (_upstream[i] == ExecutionState::DONE) {
      if (std::all_of(_upstream.begin(), _upstream.end(),
                      [](auto s) { return s == ExecutionState::DONE; })) {
        break;
      }
       _currentDependency = (i + 1) % _numberDependencies;
       continue;
     }
  }
  
  size_t skipped = _skipped;
  _skipped = 0;

  TRI_ASSERT(skipped <= toSkip);
  return {state, NoStats{}, skipped};
}

void ParallelUnsortedGatherExecutor::initDependencies() {
  if (_numberDependencies == 0) {
    // We need to initialize the dependencies once, they are injected
    // after the fetcher is created.
    _numberDependencies = _fetcher.numberDependencies();
    TRI_ASSERT(_numberDependencies > 0);
    _upstream.resize(_numberDependencies, ExecutionState::HASMORE);
    TRI_ASSERT(std::all_of(_upstream.begin(), _upstream.end(), [](auto const& s) { return s == ExecutionState::HASMORE; } ));
  }
}
