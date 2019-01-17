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

#include "Basics/Common.h"
#include "SingletonExecutor.h"
#include "Aql/AqlValue.h"
#include "Aql/OutputAqlItemRow.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

SingletonExecutor::SingletonExecutor(Fetcher& fetcher, ExecutorInfos& infos)
    : _infos(infos), _fetcher(fetcher), _done(false) {};
SingletonExecutor::~SingletonExecutor() = default;

std::pair<ExecutionState, SingletonExecutor::Stats> SingletonExecutor::produceRow(OutputAqlItemRow& output) {
  return {ExecutionState::DONE, Stats{}};

  // ExecutionState state;
  // SingletonExecutor::Stats stats;

  // if(_done){
  //   return {ExecutionState::DONE, std::move(stats)};
  // }

  // InputAqlItemRow inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
  // std::tie(state, inputRow) = _fetcher.fetchRow();

  // if (state == ExecutionState::WAITING) {
  //   TRI_ASSERT(!inputRow);
  //   return {state, std::move(stats)};
  // }

  // if (!inputRow) {
  //   TRI_ASSERT(state == ExecutionState::DONE);
  //   return {state, std::move(stats)};
  // }

  // TRI_ASSERT(state == ExecutionState::HASMORE || state == ExecutionState::DONE);
  // output.copyRow(inputRow);
  // _done = true;

  // return {ExecutionState::DONE, std::move(stats)};
}
