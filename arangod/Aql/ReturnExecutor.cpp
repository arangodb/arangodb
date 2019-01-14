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

#include "ReturnExecutor.h"

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

ReturnExecutorInfos::ReturnExecutorInfos(RegisterId inputRegister, RegisterId outputRegister,
                                         RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                                         std::unordered_set<RegisterId> registersToClear,
                                         bool returnInheritedResults)
    : ExecutorInfos(make_shared_unordered_set({inputRegister}),
                    make_shared_unordered_set({outputRegister}),
                    nrInputRegisters, nrOutputRegisters,
                    std::unordered_set<RegisterId>{} /*to clear*/, // std::move(registersToClear),
                    std::unordered_set<RegisterId>{} /*to keep*/
                    ),
      _inputRegisterId(inputRegister),
      _outputRegisterId(outputRegister),
      _returnInheritedResults(returnInheritedResults) {

      }

ReturnExecutor::ReturnExecutor(Fetcher& fetcher, ReturnExecutorInfos& infos)
    : _infos(infos), _fetcher(fetcher){};
ReturnExecutor::~ReturnExecutor() = default;

std::pair<ExecutionState, NoStats> ReturnExecutor::produceRow(OutputAqlItemRow& output) {
  ExecutionState state;
  InputAqlItemRow inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
  std::tie(state, inputRow) = _fetcher.fetchRow();

  if (state == ExecutionState::WAITING) {
    TRI_ASSERT(!inputRow);
    return {state, NoStats{}};
  }

  if (!inputRow) {
    TRI_ASSERT(state == ExecutionState::DONE);
    return {state, NoStats{}};
  }

  if (_infos._returnInheritedResults) {
    output.copyRow(inputRow);
  } else {
    AqlValue val;
    val = inputRow.getValue(_infos._inputRegisterId);
    AqlValueGuard guard(val, true);
    //LOG_DEVEL << "writing to ouputReg: " << _infos._outputRegisterId;
    output.setValue(_infos._outputRegisterId, inputRow, val);
    guard.steal();
  }

  return {state, NoStats{}};
}
