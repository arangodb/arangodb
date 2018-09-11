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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "FilterExecutor.h"

#include "Basics/Common.h"

#include "Aql/AqlItemRow.h"
#include "Aql/BlockFetcherInterfaces.h"
#include "Aql/ExecutorInfos.h"

using namespace arangodb;
using namespace arangodb::aql;

FilterExecutor::FilterExecutor(SingleRowFetcher& fetcher, ExecutorInfos& infos) : _fetcher(fetcher), _infos(infos) {};
FilterExecutor::~FilterExecutor() = default;

std::pair<ExecutionState, std::unique_ptr<AqlItemRow>> FilterExecutor::produceRow() {
  ExecutionState state;
  AqlItemRow const* input = nullptr;
  while (true) {
    // TODO implement me!
    std::tie(state, input) = _fetcher.fetchRow();
    if (state == ExecutionState::WAITING) {
      return {state, nullptr};
    }
    if (input == nullptr) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, nullptr};
    }
  }
}
