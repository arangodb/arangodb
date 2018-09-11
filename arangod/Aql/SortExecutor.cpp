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


#include "SortExecutor.h"

#include "Basics/Common.h"

#include "Aql/AllRowsFetcher.h"
#include "Aql/AqlItemMatrix.h"
#include "Aql/AqlItemRow.h"
#include "Aql/ExecutorInfos.h"

using namespace arangodb;
using namespace arangodb::aql;

SortExecutor::SortExecutor(Fetcher& fetcher, ExecutorInfos& infos) : _fetcher(fetcher), _infos(infos) {};
SortExecutor::~SortExecutor() = default;

ExecutionState SortExecutor::produceRow(AqlItemRow& output) {
  ExecutionState state;
  AqlItemMatrix const* input = nullptr;
  while (true) {
    // TODO implement me!
    std::tie(state, input) = _fetcher.fetchAllRows();
    if (state == ExecutionState::WAITING) {
      return state;
    }
    if (input == nullptr) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return state;
    }
  }
}
