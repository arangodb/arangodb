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

#include "IdExecutor.h"

#include "Aql/AqlCallStack.h"
#include "Aql/AqlValue.h"
#include "Aql/ConstFetcher.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/QueryOptions.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"

#include <algorithm>
#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

ExecutionBlockImpl<IdExecutor<void>>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                         ExecutionNode const* node,
                                                         RegisterId outputRegister, bool doCount)
    : ExecutionBlock(engine, node),
      _currentDependency(0),
      _outputRegister(outputRegister),
      _doCount(doCount) {
  // already insert ourselves into the statistics results
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    _engine->_stats.nodes.try_emplace(node->id(), ExecutionStats::Node());
  }
}

std::pair<ExecutionState, size_t> ExecutionBlockImpl<IdExecutor<void>>::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);
  if (isDone()) {
    return traceSkipSomeEnd(ExecutionState::DONE, 0);
  }

  ExecutionState state;
  size_t skipped;
  std::tie(state, skipped) = currentDependency().skipSome(atMost);

  if (state == ExecutionState::DONE) {
    nextDependency();
  }

  return traceSkipSomeEnd(state, skipped);
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<IdExecutor<void>>::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);
  if (isDone()) {
    return traceGetSomeEnd(ExecutionState::DONE, nullptr);
  }

  ExecutionState state;
  SharedAqlItemBlockPtr block;
  std::tie(state, block) = currentDependency().getSome(atMost);

  countStats(block);

  if (state == ExecutionState::DONE) {
    nextDependency();
  }

  return traceGetSomeEnd(state, block);
}

bool aql::ExecutionBlockImpl<IdExecutor<void>>::isDone() const noexcept {
  // I'd like to assert this in the constructor, but the dependencies are
  // added after construction.
  TRI_ASSERT(!_dependencies.empty());
  return _currentDependency >= _dependencies.size();
}

RegisterId ExecutionBlockImpl<IdExecutor<void>>::getOutputRegisterId() const noexcept {
  return _outputRegister;
}

std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr>
ExecutionBlockImpl<IdExecutor<void>>::execute(AqlCallStack stack) {
  // TODO Implement me
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

ExecutionBlock& ExecutionBlockImpl<IdExecutor<void>>::currentDependency() const {
  TRI_ASSERT(_currentDependency < _dependencies.size());
  TRI_ASSERT(_dependencies[_currentDependency] != nullptr);
  return *_dependencies[_currentDependency];
}

void ExecutionBlockImpl<IdExecutor<void>>::nextDependency() noexcept {
  ++_currentDependency;
}

bool ExecutionBlockImpl<IdExecutor<void>>::doCount() const noexcept {
  return _doCount;
}

void ExecutionBlockImpl<IdExecutor<void>>::countStats(SharedAqlItemBlockPtr& block) {
  if (doCount() && block != nullptr) {
    CountStats stats;
    stats.setCounted(block->size());
    _engine->_stats += stats;
  }
}

IdExecutorInfos::IdExecutorInfos(RegisterId nrInOutRegisters,
                                 // cppcheck-suppress passedByValue
                                 std::unordered_set<RegisterId> registersToKeep,
                                 // cppcheck-suppress passedByValue
                                 std::unordered_set<RegisterId> registersToClear,
                                 std::string distributeId, bool isResponsibleForInitializeCursor)
    : ExecutorInfos(make_shared_unordered_set(), make_shared_unordered_set(),
                    nrInOutRegisters, nrInOutRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _distributeId(std::move(distributeId)),
      _isResponsibleForInitializeCursor(isResponsibleForInitializeCursor) {}

std::string const& IdExecutorInfos::distributeId() { return _distributeId; }

bool IdExecutorInfos::isResponsibleForInitializeCursor() const {
  return _isResponsibleForInitializeCursor;
}

template <class UsedFetcher>
IdExecutor<UsedFetcher>::IdExecutor(Fetcher& fetcher, IdExecutorInfos& infos)
    : _fetcher(fetcher) {
  if (!infos.distributeId().empty()) {
    _fetcher.setDistributeId(infos.distributeId());
  }
}

template <class UsedFetcher>
IdExecutor<UsedFetcher>::~IdExecutor() = default;

template <class UsedFetcher>
std::pair<ExecutionState, NoStats> IdExecutor<UsedFetcher>::produceRows(OutputAqlItemRow& output) {
  ExecutionState state = ExecutionState::HASMORE;
  NoStats stats;
  InputAqlItemRow inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
  while (!output.isFull() && state != ExecutionState::DONE) {
    std::tie(state, inputRow) = _fetcher.fetchRow(output.numRowsLeft());

    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(!inputRow);
      return {state, stats};
    }

    if (!inputRow) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, stats};
    }

    TRI_IF_FAILURE("SingletonBlock::getOrSkipSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    TRI_ASSERT(state == ExecutionState::HASMORE || state == ExecutionState::DONE);
    /*Second parameter are to ignore registers that should be kept but are missing in the input row*/
    output.copyRow(inputRow, std::is_same<UsedFetcher, ConstFetcher>::value);
    TRI_ASSERT(output.produced());
    output.advanceRow();

    TRI_IF_FAILURE("SingletonBlock::getOrSkipSomeSet") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  return {state, stats};
}

template <class UsedFetcher>
std::tuple<ExecutionState, typename IdExecutor<UsedFetcher>::Stats, SharedAqlItemBlockPtr>
IdExecutor<UsedFetcher>::fetchBlockForPassthrough(size_t atMost) {
  auto rv = _fetcher.fetchBlockForPassthrough(atMost);
  return {rv.first, {}, std::move(rv.second)};
}

template class ::arangodb::aql::IdExecutor<ConstFetcher>;
// ID can always pass through
template class ::arangodb::aql::IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>;
