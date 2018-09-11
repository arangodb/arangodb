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
#include "Aql/AqlItemRow.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/ExecutionState.h"

using namespace arangodb;
using namespace arangodb::aql;

template<class Executor>
ExecutionBlockImpl<Executor>::ExecutionBlockImpl(ExecutionEngine* engine, ExecutionNode const* node) :
  ExecutionBlock(engine, node), SingleRowFetcher(), _infos(0, 0), _executor(*this, _infos)
{
}

template<class Executor>
ExecutionBlockImpl<Executor>::~ExecutionBlockImpl() = default;


struct RegInfo {
  std::size_t numInRegs;
  std::size_t numOutRegs;
  std::unordered_set<RegisterId> _regsToClear;
};

RegInfo getRegisterInfo( ExecutionBlock* thisBlock
                       , ExecutionNode* thisPlanNode
                       , ExecutionNode* previousPlanNode
                       ){
  // find out exact in-regs
  return { thisBlock->getNrInputRegisters()
         , thisBlock->getNrOutputRegisters()
         , thisPlanNode->getRegsToClear()
         };
}

template<class Executor>
std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> ExecutionBlockImpl<Executor>::getSome(size_t atMost) {

  auto regInfo = getRegisterInfo(this, this->getPlanNode(), this->getPlanNode()->getFirstDependency());

  //auto resultBlockManges = this->requestBlock(atMost, this->getNrOutputRegisters());
  auto resultBlock = std::make_unique<AqlItemBlock>(nullptr, atMost, regInfo.numOutRegs);
  std::size_t rowsAdded = 0;

  ExecutionState state;
  std::unique_ptr<AqlItemRow> row; //holds temporary rows

  while (rowsAdded < atMost) {
    row = std::make_unique<AqlItemRow>(resultBlock.get(),rowsAdded);
    state = _executor.produceRow(row.get()); // adds row to output
    if( row && row->hasValue() ) {
      // copy from input
      ++rowsAdded;
    }

    if( state == ExecutionState::WAITING || state == ExecutionState::DONE ) {
      break;
    }
  }

  if (rowsAdded){
    return {state, std::move(resultBlock)};
  } else {
    return {state, nullptr};
  }
}

template<class Executor>
std::pair<ExecutionState, size_t> ExecutionBlockImpl<Executor>::skipSome(size_t atMost) {
  // TODO IMPLEMENT ME
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  return {ExecutionState::DONE, 0};
}

template<class Executor>
std::pair<ExecutionState, AqlItemRow const*> ExecutionBlockImpl<Executor>::fetchRow() {
  // TODO IMPLEMENT ME
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  return {ExecutionState::DONE, nullptr};
}

template<class Executor>
ExecutionState ExecutionBlockImpl<Executor>::fetchBlock() {
  // TODO IMPLEMENT ME
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  return ExecutionState::DONE;
}
