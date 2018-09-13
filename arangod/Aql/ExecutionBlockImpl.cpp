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
#include "Aql/FilterExecutor.h"
#include "Aql/ExecutionEngine.h"

using namespace arangodb;
using namespace arangodb::aql;

template <class Executor>
ExecutionBlockImpl<Executor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                 ExecutionNode const* node,
                                                 ExecutorInfos&& infos)
    : ExecutionBlock(engine, node),
      _infos(infos),
      _blockFetcher(this),
      _rowFetcher(_blockFetcher),
      _executor(_rowFetcher, _infos),
      _getSomeOutBlock(nullptr),
      _getSomeOutRowsAdded(0)
    {}

template<class Executor>
ExecutionBlockImpl<Executor>::~ExecutionBlockImpl() {
  if(_getSomeOutBlock){
    AqlItemBlock* block = _getSomeOutBlock.release();
    _engine->_itemBlockManager.returnBlock(block);
  }
}

template<class Executor>
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> ExecutionBlockImpl<Executor>::getSome(size_t atMost) {

  if(!_getSomeOutBlock) {
    auto block = this->requestBlock(atMost, _infos.numberOfRegisters());
    _getSomeOutBlock = std::unique_ptr<AqlItemBlock>(block);
    //auto deleter = [=](AqlItemBlock* b){_engine->_itemBlockManager.returnBlock(b); };
    //_getSomeOutBlock = std::unique_ptr<AqlItemBlock, decltype(deleter)>(block, deleter);
    _getSomeOutRowsAdded = 0;
  }

  // TODO It's not very obvious that `state` will be initialized, because
  // it's not obvious that the loop will run at least once (e.g. after a
  // WAITING). It should, but I'd like that to be clearer. Initializing here
  // won't help much because it's unclear whether the value will be correct.
  ExecutionState state;
  std::unique_ptr<OutputAqlItemRow> row;  // holds temporary rows

  TRI_ASSERT(atMost > 0);

  row = std::make_unique<OutputAqlItemRow>(_getSomeOutBlock.get(), _getSomeOutRowsAdded, _infos.registersToKeep());
  while (_getSomeOutRowsAdded < atMost) {
    row->changeRow(_getSomeOutRowsAdded);
    state = _executor.produceRow(*row);
    if (row && row->produced()) {
      ++_getSomeOutRowsAdded;
    }

    if (state == ExecutionState::WAITING) {
      return {state, nullptr};
    }

    if (state == ExecutionState::DONE) {
      if (_getSomeOutRowsAdded == 0) {
        // No results, just throw it away
        _getSomeOutBlock.reset();
      } else if (_getSomeOutRowsAdded < atMost) {
        _getSomeOutBlock->shrink(_getSomeOutRowsAdded);
      }
      // _getSomeOutBlock is guaranteed to be nullptr after move.
      // keep this invariant in case we switch to another type!!!!!
      return {state, std::move(_getSomeOutBlock)};
    }
  }

  TRI_ASSERT(state == ExecutionState::HASMORE);
  TRI_ASSERT(_getSomeOutRowsAdded == atMost);
  return {state, std::move(_getSomeOutBlock)};
}

template <class Executor>
std::pair<ExecutionState, size_t> ExecutionBlockImpl<Executor>::skipSome(
    size_t atMost) {
  // TODO IMPLEMENT ME, this is a stub!

  auto res = getSome(atMost);

  return {res.first, res.second->size()};
}

template class ::arangodb::aql::ExecutionBlockImpl<FilterExecutor>;
