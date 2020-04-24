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

#include "TestEmptyExecutorHelper.h"

#include "Basics/Common.h"

#include "Aql/AqlValue.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SingleRowFetcher.h"
#include "Logger/LogMacros.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

TestEmptyExecutorHelper::TestEmptyExecutorHelper(Fetcher&, Infos&) {}

std::pair<ExecutionState, FilterStats> TestEmptyExecutorHelper::produceRows(OutputAqlItemRow& output) {
  TRI_IF_FAILURE("TestEmptyExecutorHelper::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  ExecutionState state = ExecutionState::DONE;
  FilterStats stats{};

  return {state, stats};
}
