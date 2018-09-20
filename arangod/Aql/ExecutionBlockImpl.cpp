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

#include "ExecutionBlockImpl.h"

#include "Basics/Common.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/ExecutionEngine.h"

#include "Aql/EnumerateListExecutor.h"
#include "Aql/FilterExecutor.h"
#include "Aql/SortExecutor.h"

#include "Aql/SortRegister.h"

using namespace arangodb;
using namespace arangodb::aql;

template <class Executor>
ExecutionBlockImpl<Executor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                 ExecutionNode const* node,
                                                 typename Executor::Infos&& infos)
    : ExecutionBlock(engine, node),
      _blockFetcher(this, infos.getInputRegisters()),
      _rowFetcher(_blockFetcher),
      _infos(std::move(infos)),
      _executor(_rowFetcher, _infos) {}

template <class Executor>
ExecutionBlockImpl<Executor>::~ExecutionBlockImpl() {
  if (_outputItemRow) {
    std::unique_ptr<AqlItemBlock> block = _outputItemRow->stealBlock();
    if (block != nullptr) {
      _engine->_itemBlockManager.returnBlock(std::move(block));
    }
  }
}

template <class Executor>
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>>
ExecutionBlockImpl<Executor>::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);
  auto result = getSomeWithoutTrace(atMost);
  return traceGetSomeEnd(result.first, std::move(result.second));
}

template<class Executor>
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>>
ExecutionBlockImpl<Executor>::getSomeWithoutTrace(size_t atMost) {
  if (!_outputItemRow) {
    auto newBlock =
        requestWrappedBlock(atMost, _infos.numberOfOutputRegisters());
    _outputItemRow =
        std::make_unique<OutputAqlItemRow>(std::move(newBlock), _infos);
  }

  // TODO It's not very obvious that `state` will be initialized, because
  // it's not obvious that the loop will run at least once (e.g. after a
  // WAITING). It should, but I'd like that to be clearer. Initializing here
  // won't help much because it's unclear whether the value will be correct.
  ExecutionState state;
  ExecutorStats executorStats{};
  std::unique_ptr<OutputAqlItemRow> row;  // holds temporary rows

  TRI_ASSERT(atMost > 0);

  while (!_outputItemRow->isFull()) {
    std::tie(state, executorStats) = _executor.produceRow(*_outputItemRow);
    // Count global but executor-specific statistics, like number of filtered
    // rows.
    _engine->_stats += executorStats;
    if (_outputItemRow && _outputItemRow->produced()) {
      _outputItemRow->advanceRow();
    }

    if (state == ExecutionState::WAITING) {
      return {state, nullptr};
    }

    if (state == ExecutionState::DONE) {
      // TODO Does this work as expected when there was no row produced, or
      // we were DONE already, so we didn't build a single row?
      // We must return nullptr then, because empty AqlItemBlocks are not
      // allowed!
      auto outputBlock = _outputItemRow->stealBlock();
      // This is not strictly necessary here, as we shouldn't be called again
      // after DONE.
      _outputItemRow.reset(nullptr);

      return {state, std::move(outputBlock)};
    }
  }

  TRI_ASSERT(state == ExecutionState::HASMORE);
  TRI_ASSERT(_outputItemRow->numRowsWritten() == atMost);

  auto outputBlock = _outputItemRow->stealBlock();
  // TODO OutputAqlItemRow could get "reset" and "isValid" methods and be reused
  _outputItemRow.reset(nullptr);

  return {state, std::move(outputBlock)};
}

template <class Executor>
std::pair<ExecutionState, size_t> ExecutionBlockImpl<Executor>::skipSome(
    size_t atMost) {
  // TODO IMPLEMENT ME, this is a stub!

  traceSkipSomeBegin(atMost);

  auto res = getSomeWithoutTrace(atMost);

  size_t skipped = 0;
  if (res.second != nullptr) {
    skipped = res.second->size();
    AqlItemBlock* resultPtr = res.second.get();
    returnBlock(resultPtr);
    res.second.release();
  }

  return traceSkipSomeEnd(res.first, skipped);
}

template <class Executor>
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>>
ExecutionBlockImpl<Executor>::traceGetSomeEnd(
    ExecutionState state, std::unique_ptr<AqlItemBlock> result) {
  ExecutionBlock::traceGetSomeEnd(result.get(), state);
  return {state, std::move(result)};
}

template <class Executor>
std::pair<ExecutionState, size_t>
ExecutionBlockImpl<Executor>::traceSkipSomeEnd(ExecutionState state,
                                               size_t skipped) {
  ExecutionBlock::traceSkipSomeEnd(skipped, state);
  return {state, skipped};
}

template <class Executor>
std::pair<ExecutionState, Result>
ExecutionBlockImpl<Executor>::initializeCursor(AqlItemBlock* items,
                                               size_t pos) {
  // re-initialize BlockFetcher
  _blockFetcher = BlockFetcher(this, _infos.getInputRegisters());

  // destroy and re-create the Fetcher
  _rowFetcher.~Fetcher();
  new (&_rowFetcher) Fetcher(_blockFetcher);

  // destroy and re-create the Executor
  _executor.~Executor();
  new (&_executor) Executor(_rowFetcher, _infos);

  return ExecutionBlock::initializeCursor(items, pos);
}

template <class Executor>
std::unique_ptr<AqlItemBlockShell>
ExecutionBlockImpl<Executor>::requestWrappedBlock(size_t nrItems,
                                                  RegisterId nrRegs) {
  AqlItemBlock* block = requestBlock(nrItems, nrRegs);
  std::unique_ptr<AqlItemBlockShell> blockShell =
      std::make_unique<AqlItemBlockShell>(
          _engine->itemBlockManager(), std::unique_ptr<AqlItemBlock>{block},
          nullptr, _infos.getOutputRegisters(), -1);
  return blockShell;
}

template class ::arangodb::aql::ExecutionBlockImpl<EnumerateListExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<FilterExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<SortExecutor>;
