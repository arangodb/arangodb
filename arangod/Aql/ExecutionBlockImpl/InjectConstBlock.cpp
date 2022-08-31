////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include <Aql/ConstFetcher.h>
#include <Aql/IdExecutor.h>
#include <Aql/ExecutionEngine.h>

// Avoid duplicate symbols when linking: This file is directly included by
// ExecutionBlockImplTestInstances.cpp
#ifndef ARANGODB_INCLUDED_FROM_GTESTS

// Work around GCC bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56480
// Without the namespaces it fails with
// error: specialization of 'template<class Executor>
// std::pair<arangodb::aql::ExecutionState, arangodb::Result>
// arangodb::aql::ExecutionBlockImpl<Executor>::initializeCursor(arangodb::aql::AqlItemBlock*,
// size_t)' in different namespace
namespace arangodb::aql {
// TODO -- remove this specialization when cpp 17 becomes available

template<>
template<>
auto ExecutionBlockImpl<IdExecutor<ConstFetcher>>::injectConstantBlock<
    IdExecutor<ConstFetcher>>(SharedAqlItemBlockPtr block, SkipResult skipped)
    -> void {
  // reinitialize the DependencyProxy
  _dependencyProxy.reset();

  // destroy and re-create the Fetcher
  _rowFetcher.~Fetcher();
  new (&_rowFetcher) Fetcher(_dependencyProxy);

  TRI_ASSERT(_skipped.nothingSkipped());

  // Local skipped is either fresh (depth == 1)
  // Or exactly of the size handed in
  TRI_ASSERT(_skipped.subqueryDepth() == 1 ||
             _skipped.subqueryDepth() == skipped.subqueryDepth());

  TRI_ASSERT(_state == InternalState::DONE ||
             _state == InternalState::FETCH_DATA);

  _state = InternalState::FETCH_DATA;

  // Reset state of execute
  _lastRange = AqlItemBlockInputRange{MainQueryState::HASMORE};
  _hasUsedDataRangeBlock = false;
  _upstreamState = ExecutionState::HASMORE;

  _rowFetcher.injectBlock(std::move(block), std::move(skipped));

  resetExecutor();
}

// TODO -- remove this specialization when cpp 17 becomes available
template<>
std::pair<ExecutionState, Result>
ExecutionBlockImpl<IdExecutor<ConstFetcher>>::initializeCursor(
    InputAqlItemRow const& input) {
  SharedAqlItemBlockPtr block = input.cloneToBlock(
      _engine->itemBlockManager(), registerInfos().registersToKeep().back(),
      registerInfos().numberOfOutputRegisters());
  TRI_ASSERT(_skipped.nothingSkipped());
  _skipped.reset();
  // We inject an empty copy of our skipped here,
  // This is resetted, but will maintain the size
  injectConstantBlock(std::move(block), _skipped);

  // end of default initializeCursor
  return ExecutionBlock::initializeCursor(input);
}

}  // namespace arangodb::aql
#else
// Just predeclare the specializations for the tests.

namespace arangodb::aql {
template<>
template<>
auto ExecutionBlockImpl<IdExecutor<ConstFetcher>>::injectConstantBlock<
    IdExecutor<ConstFetcher>>(SharedAqlItemBlockPtr block, SkipResult skipped)
    -> void;

template<>
std::pair<ExecutionState, Result>
ExecutionBlockImpl<IdExecutor<ConstFetcher>>::initializeCursor(
    InputAqlItemRow const& input);

}  // namespace arangodb::aql

#endif
