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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "NoResultsExecutor.h"

#include "Basics/Common.h"

#include "Aql/AllRowsFetcher.h"
#include "Aql/AqlItemMatrix.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SortRegister.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

NoResultsExecutor::NoResultsExecutor(Fetcher& fetcher, ExecutorInfos& infos)
    : _infos(infos), _fetcher(fetcher){};
NoResultsExecutor::~NoResultsExecutor() = default;

std::pair<ExecutionState, NoStats> NoResultsExecutor::produceRow(OutputAqlItemRow& output) {
  return {ExecutionState::DONE, NoStats{}};
}
