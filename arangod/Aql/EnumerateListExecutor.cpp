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
#include <Logger/LogMacros.h>

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/AqlValue.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {
void throwArrayExpectedException(AqlValue const& value) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_QUERY_ARRAY_EXPECTED,
      std::string("collection or ") + TRI_errno_string(TRI_ERROR_QUERY_ARRAY_EXPECTED) +
          std::string(
              " as operand to FOR loop; you provided a value of type '") +
          value.getTypeString() + std::string("'"));
}
}  // namespace

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

RegisterId EnumerateListExecutorInfos::getInputRegister() const noexcept {
  return _inputRegister;
}

RegisterId EnumerateListExecutorInfos::getOutputRegister() const noexcept {
  return _outputRegister;
}

EnumerateListExecutor::EnumerateListExecutor(Fetcher& fetcher, EnumerateListExecutorInfos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _currentRow{CreateInvalidInputRowHint{}},
      _rowState(ExecutionState::HASMORE),
      _inputArrayPosition(0),
      _inputArrayLength(0) {}

std::pair<ExecutionState, NoStats> EnumerateListExecutor::produceRows(OutputAqlItemRow& output) {
  while (true) {
    // HIT in first run, because pos and length are initialized
    // both with 0

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
        if (!inputList.isArray()) {
          throwArrayExpectedException(inputList);
        }
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

void EnumerateListExecutor::initializeNewRow(AqlItemBlockInputRange& inputRange) {
  if (_currentRow) {
    std::ignore = inputRange.nextDataRow();
  }
  std::tie(_currentRowState, _currentRow) = inputRange.peekDataRow();
  if (!_currentRow) {
    return;
  }

  // fetch new row, put it in local state
  AqlValue const& inputList = _currentRow.getValue(_infos.getInputRegister());

  // store the length into a local variable
  // so we don't need to calculate length every time
  if (inputList.isDocvec()) {
    _inputArrayLength = inputList.docvecSize();
  } else {
    if (!inputList.isArray()) {
      throwArrayExpectedException(inputList);
    }
    _inputArrayLength = inputList.length();
  }

  _inputArrayPosition = 0;
}

void EnumerateListExecutor::processArrayElement(OutputAqlItemRow& output) {
  bool mustDestroy;
  AqlValue const& inputList = _currentRow.getValue(_infos.getInputRegister());
  AqlValue innerValue = getAqlValue(inputList, _inputArrayPosition, mustDestroy);
  AqlValueGuard guard(innerValue, mustDestroy);

  TRI_IF_FAILURE("EnumerateListBlock::getSome") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  output.moveValueInto(_infos.getOutputRegister(), _currentRow, guard);
  output.advanceRow();

  // set position to +1 for next iteration after new fetchRow
  _inputArrayPosition++;
}

size_t EnumerateListExecutor::skipArrayElement() {
  // set position to +1 for next iteration after new fetchRow
  _inputArrayPosition++;
  return 1;
}

std::tuple<ExecutorState, NoStats, AqlCall> EnumerateListExecutor::produceRows(
    AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output) {
  AqlCall upstreamCall{};
  upstreamCall.fullCount = output.getClientCall().fullCount;

  if (!inputRange.hasDataRow()) {
    return {inputRange.upstreamState(), NoStats{}, upstreamCall};
  }

  while (inputRange.hasDataRow() && !output.isFull()) {
    if (_inputArrayLength == _inputArrayPosition) {
      // we reached either the end of an array
      // or are in our first loop iteration
      initializeNewRow(inputRange);
      continue;
    }

    TRI_ASSERT(_inputArrayPosition <= _inputArrayLength);
    processArrayElement(output);
  }

  if (_inputArrayLength == _inputArrayPosition) {
    // we reached either the end of an array
    // or are in our first loop iteration
    initializeNewRow(inputRange);
  }

  return {ExecutorState::DONE, NoStats{}, upstreamCall};
}

std::tuple<ExecutorState, size_t, AqlCall> EnumerateListExecutor::skipRowsRange(
    AqlItemBlockInputRange& inputRange, AqlCall& call) {
  AqlCall upstreamCall{};

  if (!inputRange.hasDataRow()) {
    return {inputRange.upstreamState(), 0, upstreamCall};
  }

  InputAqlItemRow input{CreateInvalidInputRowHint{}};
  size_t skipped = 0;
  bool offsetPhase = (call.getOffset() > 0);

  while (inputRange.hasDataRow() && call.shouldSkip()) {
    if (_inputArrayLength == _inputArrayPosition) {
      // we reached either the end of an array
      // or are in our first loop iteration
      initializeNewRow(inputRange);
      continue;
    }
    // auto const& [state, input] = inputRange.peekDataRow();

    TRI_ASSERT(_inputArrayPosition <= _inputArrayLength);
    // if offset is > 0, we're in offset skip phase
    if (offsetPhase) {
      if (skipped < call.getOffset()) {
        // we still need to skip offset entries
        skipped += skipArrayElement();
      } else {
        // we skipped enough in our offset phase
        break;
      }
    } else {
      // fullCount phase - skippen bis zum ende
      skipped += skipArrayElement();
    }
  }
  call.didSkip(skipped);

  upstreamCall.softLimit = call.getOffset();
  if (offsetPhase) {
    if (skipped < call.getOffset()) {
      return {inputRange.upstreamState(), skipped, upstreamCall};
    } else if (_inputArrayPosition < _inputArrayLength) {
      return {ExecutorState::HASMORE, skipped, upstreamCall};
    }
    return {_currentRowState, skipped, upstreamCall};
  }
  return {ExecutorState::DONE, skipped, upstreamCall};
}

void EnumerateListExecutor::initialize() {
  _skipped = 0;
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
