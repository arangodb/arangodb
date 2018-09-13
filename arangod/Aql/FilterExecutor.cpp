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

#include <lib/Logger/LogMacros.h>
#include "FilterExecutor.h"

#include "Basics/Common.h"

#include "Aql/InputAqlItemRow.h"
#include "Aql/AqlValue.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/SingleRowFetcher.h"

using namespace arangodb;
using namespace arangodb::aql;

FilterExecutor::FilterExecutor(Fetcher& fetcher, ExecutorInfos& infos) : _fetcher(fetcher), _infos(infos) {};
FilterExecutor::~FilterExecutor() = default;

ExecutionState FilterExecutor::produceRow(InputAqlItemRow& output) {
  ExecutionState state;
  InputAqlItemRow const* input = nullptr;

  while (true) {
    std::tie(state, input) = _fetcher.fetchRow();

    if (state == ExecutionState::WAITING) {
      return state;
    }

    if (input == nullptr) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return state;
    }

    if (input->getValue(_infos.getInput()).toBoolean()) {
      output.copyRow(*input);
      return state;
    }

    if (state == ExecutionState::DONE) {
      return state;
    }
    TRI_ASSERT(state == ExecutionState::HASMORE);
  }
}
