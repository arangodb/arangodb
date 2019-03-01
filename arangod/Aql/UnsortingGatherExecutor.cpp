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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "UnsortingGatherExecutor.h"

#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"

using namespace arangodb;
using namespace arangodb::aql;

UnsortingGatherExecutor::UnsortingGatherExecutor(Fetcher& fetcher, Infos&)
    : _fetcher(fetcher),
      _currentDependency(0),
      _numberDependencies(_fetcher.numberDependencies()) {}

UnsortingGatherExecutor::~UnsortingGatherExecutor() = default;

std::pair<ExecutionState, NoStats> UnsortingGatherExecutor::produceRow(OutputAqlItemRow& output) {
  if (_currentDependency >= _numberDependencies) {
    // We are done
    return {ExecutionState::DONE, NoStats{}};
  }
  while (true) {
    ExecutionState state;
    InputAqlItemRow input{CreateInvalidInputRowHint{}};
    std::tie(state, input) = _fetcher.fetchRowForDependency(_currentDependency);
    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(!input.isInitialized());
      return {state, NoStats{}};
    }
    if (state == ExecutionState::DONE) {
      // This dependency is finished.
      // Goto next
      _currentDependency++;
      if (_currentDependency < _numberDependencies) {
        // The next dependency most likely has more
        // We do not check to save lookups.
        state = ExecutionState::HASMORE;
      }
    }
    if (input.isInitialized()) {
      // We have an input, write it
      output.copyRow(input);
      return {state, NoStats{}};
    }
  }
}
