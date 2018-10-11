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

#include "TraversalExecutor.h"
#include "Aql/SingleRowFetcher.h"

using namespace arangodb;
using namespace arangodb::aql;

TraversalExecutor::TraversalExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos), _fetcher(fetcher), _input{CreateInvalidInputRowHint{}} {}
TraversalExecutor::~TraversalExecutor() = default;

std::pair<ExecutionState, TraversalStats> TraversalExecutor::produceRow(OutputAqlItemRow& output) {
  TraversalStats s;

  if (!_input.isInitialized()) {
    std::tie(_rowState, _input) = _fetcher.fetchRow();
    if (_rowState == ExecutionState::WAITING) {
      TRI_ASSERT(!_input.isInitialized());
      return {_rowState, s};
    }
  }
  if (!_input.isInitialized()) {
    // We tried to fetch, but no upstream
    TRI_ASSERT(_rowState == ExecutionState::DONE);
    return {_rowState, s};
  }


  return {ExecutionState::DONE, s};
}

