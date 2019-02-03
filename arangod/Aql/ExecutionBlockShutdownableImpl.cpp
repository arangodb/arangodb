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

#include "ExecutionBlockShutdownableImpl.h"
#include "Aql/TraversalExecutor.h"
#include "ExecutionBlockImpl.h"

using namespace arangodb;
using namespace arangodb::aql;

template <class Executor>
ExecutionBlockShutdownableImpl<Executor>::ExecutionBlockShutdownableImpl(
    ExecutionEngine* engine, ExecutionNode const* node, typename Executor::Infos&& infos)
    : ExecutionBlockImpl<Executor>(engine, node, std::move(infos)) {}

template <class Executor>
ExecutionBlockShutdownableImpl<Executor>::~ExecutionBlockShutdownableImpl() {}

template <class Executor>
std::pair<ExecutionState, Result> ExecutionBlockShutdownableImpl<Executor>::shutdown(int errorCode) {
  ExecutionState state;
  Result result;

  std::tie(state, result) = ExecutionBlock::shutdown(errorCode);
  if (state == ExecutionState::WAITING) {
    return {state, result};
  }
  return this->executor().shutdown(errorCode);
}

template class ::arangodb::aql::ExecutionBlockShutdownableImpl<TraversalExecutor>;
