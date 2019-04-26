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

#include "EnumerateListExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Common.h"
#include "Basics/Exceptions.h"

#include <lib/Logger/LogMacros.h>

using namespace arangodb;
using namespace arangodb::aql;

EnumerateListExecutorInfos::EnumerateListExecutorInfos(
    RegisterId inputRegister, RegisterId outputRegister,
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToClear,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToKeep)
    : ExecutorInfos(make_shared_unordered_set({inputRegister}),
                    make_shared_unordered_set({outputRegister}),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _inputRegister(inputRegister),
      _outputRegister(outputRegister) {}

EnumerateListExecutor::EnumerateListExecutor(Fetcher& fetcher, EnumerateListExecutorInfos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _currentRow{CreateInvalidInputRowHint{}},
      _rowState(ExecutionState::HASMORE),
      _inputArrayPosition(0),
      _inputArrayLength(0){};

std::pair<ExecutionState, NoStats> EnumerateListExecutor::produceRows(OutputAqlItemRow& output) {
  while (true) {
    // HIT in first run, because pos and length are initiliazed
    // both with 0

    // if (_inputArrayPosition == _inputArrayLength || _inputArrayPosition == _inputArrayLength - 1) {
    if (_inputArrayPosition == _inputArrayLength) {
      // we need to set position back to zero
      // because we finished iterating over existing array
      // element and need to refetch another row
      // _inputArrayPosition = 0;
      if (_rowState == ExecutionState::DONE) {
        return {_rowState, NoStats{}};
      }
      initialize();
      std::tie(_rowState, _currentRow) = _fetcher.fetchRow();
      if (_rowState == ExecutionState::WAITING) {
        return {_rowState, NoStats{}};
      }
    }

    if (!_currentRow.isInitialized()) {
      TRI_ASSERT(_rowState == ExecutionState::DONE);
      return {_rowState, NoStats{}};
    }

    AqlValue const& inputList = _currentRow.getValue(_infos.getInputRegister());

    if (_inputArrayPosition == 0) {
      // store the length into a local variable
      // so we don't need to calculate length every time
      if (inputList.isDocvec()) {
        _inputArrayLength = inputList.docvecSize();
      } else {
        _inputArrayLength = inputList.length();
      }
    }

    if (_inputArrayLength == 0) {
      continue;
    } else if (_inputArrayLength == _inputArrayPosition) {
      // we reached the end, forget all state
      initialize();

      if (_rowState == ExecutionState::HASMORE) {
        continue;
      } else {
        return {_rowState, NoStats{}};
      }
    } else {
      bool mustDestroy;
      AqlValue innerValue = getAqlValue(inputList, _inputArrayPosition, mustDestroy);
      AqlValueGuard guard(innerValue, mustDestroy);

      TRI_IF_FAILURE("EnumerateListBlock::getSome") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      output.moveValueInto(_infos.getOutputRegister(), _currentRow, guard);

      // set position to +1 for next iteration after new fetchRow
      _inputArrayPosition++;

      if (_inputArrayPosition < _inputArrayLength || _rowState == ExecutionState::HASMORE) {
        return {ExecutionState::HASMORE, NoStats{}};
      }
      return {ExecutionState::DONE, NoStats{}};
    }
  }
}

void EnumerateListExecutor::initialize() {
  _inputArrayLength = 0;
  _inputArrayPosition = 0;
  _currentRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
}

/// @brief create an AqlValue from the inVariable using the current _index
AqlValue EnumerateListExecutor::getAqlValue(AqlValue const& inVarReg,
                                            size_t const& pos, bool& mustDestroy) {
  TRI_IF_FAILURE("EnumerateListBlock::getAqlValue") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return inVarReg.at(pos, mustDestroy, true);
}
