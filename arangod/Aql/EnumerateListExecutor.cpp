////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "EnumerateListExecutor.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/AqlValue.h"
#include "Aql/Expression.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/QueryContext.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Basics/Exceptions.h"

#include <absl/strings/str_cat.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

namespace {
void throwArrayExpectedException(AqlValue const& value) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_QUERY_ARRAY_EXPECTED,
      absl::StrCat("collection or ",
                   TRI_errno_string(TRI_ERROR_QUERY_ARRAY_EXPECTED),
                   " as operand to FOR loop; you provided a value of type '",
                   value.getTypeString(), "'"));
}

}  // namespace

EnumerateListExpressionContext::EnumerateListExpressionContext(
    transaction::Methods& trx, QueryContext& context,
    AqlFunctionsInternalCache& cache,
    std::vector<std::pair<VariableId, RegisterId>> const& varsToRegister,
    std::optional<VariableId> outputVariableId,
    std::optional<VariableId> keyVariableId)
    : QueryExpressionContext(trx, context, cache),
      _varsToRegister(varsToRegister),
      _outputVariableId(outputVariableId),
      _keyVariableId(keyVariableId),
      _currentRowIndex(0) {}

AqlValue EnumerateListExpressionContext::getVariableValue(
    Variable const* variable, bool doCopy, bool& mustDestroy) const {
  TRI_ASSERT(_currentValue.has_value());
  return QueryExpressionContext::getVariableValue(
      variable, doCopy, mustDestroy,
      [this](Variable const* variable, bool doCopy,
             bool& mustDestroy) -> AqlValue {
        mustDestroy = doCopy;
        auto const searchId = variable->id;
        if (_outputVariableId.has_value() && searchId == *_outputVariableId) {
          return *_currentValue;
        }
        if (_keyVariableId.has_value() && searchId == *_keyVariableId) {
          return AqlValue(AqlValueHintUInt(_currentRowIndex));
        }
        for (auto const& [varId, regId] : _varsToRegister) {
          if (varId == searchId) {
            TRI_ASSERT(_inputRow.has_value());
            if (doCopy) {
              return _inputRow->get().getValue(regId).clone();
            }
            return _inputRow->get().getValue(regId);
          }
        }
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            absl::StrCat("variable not found '", variable->name,
                         "' in EnumerateListExpressionContext"));
      });
}

void EnumerateListExpressionContext::adjustCurrentValue(AqlValue const& value) {
  _currentValue = value;
}

void EnumerateListExpressionContext::adjustCurrentRowIndex(uint64_t rowIndex) {
  _currentRowIndex = rowIndex;
}

void EnumerateListExpressionContext::adjustCurrentRow(
    InputAqlItemRow const& inputRow) {
  _inputRow = inputRow;
}

EnumerateListExecutorInfos::EnumerateListExecutorInfos(
    RegisterId inputRegister, RegisterId outputRegister, RegisterId keyRegister,
    QueryContext& query, Expression* filter,
    std::vector<std::pair<VariableId, RegisterId>>&& varsToRegs)
    : _query(query),
      _inputRegister(inputRegister),
      _outputRegister(outputRegister),
      _keyRegister(keyRegister),
      _filter(filter),
      _varsToRegs(std::move(varsToRegs)) {
  if (hasFilter()) {
    for (auto const& it : _varsToRegs) {
      if (it.second == _outputRegister) {
        _outputVariableId = it.first;
      } else if (it.second == _keyRegister) {
        _keyVariableId = it.first;
      }
    }
    TRI_ASSERT(_outputVariableId.has_value() || _keyVariableId.has_value());
  }
}

QueryContext& EnumerateListExecutorInfos::getQuery() const noexcept {
  return _query;
}

RegisterId EnumerateListExecutorInfos::getInputRegister() const noexcept {
  return _inputRegister;
}

RegisterId EnumerateListExecutorInfos::getOutputRegister() const noexcept {
  return _outputRegister;
}

std::optional<VariableId> EnumerateListExecutorInfos::getOutputVariableId()
    const noexcept {
  return _outputVariableId;
}

RegisterId EnumerateListExecutorInfos::getKeyRegister() const noexcept {
  return _keyRegister;
}

std::optional<VariableId> EnumerateListExecutorInfos::getKeyVariableId()
    const noexcept {
  return _keyVariableId;
}

bool EnumerateListExecutorInfos::hasFilter() const noexcept {
  return _filter != nullptr;
}

Expression* EnumerateListExecutorInfos::getFilter() const noexcept {
  return _filter;
}

std::vector<std::pair<VariableId, RegisterId>> const&
EnumerateListExecutorInfos::getVarsToRegs() const noexcept {
  return _varsToRegs;
}

EnumerateListExecutor::EnumerateListExecutor(Fetcher& fetcher,
                                             EnumerateListExecutorInfos& infos)
    : _infos(infos),
      _trx(_infos.getQuery().newTrxContext()),
      _currentRow{CreateInvalidInputRowHint{}},
      _inputArrayPosition(0),
      _inputArrayLength(0),
      _rowIndex(0) {
  if (_infos.hasFilter()) {
    _expressionContext = std::make_unique<EnumerateListExpressionContext>(
        _trx, _infos.getQuery(), _aqlFunctionsInternalCache,
        _infos.getVarsToRegs(), _infos.getOutputVariableId(),
        _infos.getKeyVariableId());
  }
}

void EnumerateListExecutor::initializeNewRow(
    AqlItemBlockInputRange& inputRange) {
  if (_currentRow) {
    inputRange.advanceDataRow();
  }
  std::tie(_currentRowState, _currentRow) = inputRange.peekDataRow();
  if (!_currentRow) {
    return;
  }

  // fetch new row, put it in local state
  AqlValue const& inputList = _currentRow.getValue(_infos.getInputRegister());

  // store the length into a local variable
  // so we don't need to calculate length every time
  if (!inputList.isArray()) {
    throwArrayExpectedException(inputList);
  }
  _inputArrayLength = inputList.length();
  _inputArrayPosition = 0;
}

bool EnumerateListExecutor::processArrayElement(OutputAqlItemRow& output) {
  AqlValue const& inputList = _currentRow.getValue(_infos.getInputRegister());
  bool mustDestroy;
  AqlValue innerValue =
      getAqlValue(inputList, _inputArrayPosition, mustDestroy);
  AqlValueGuard guard(innerValue, mustDestroy);

  TRI_IF_FAILURE("EnumerateListBlock::getSome") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (_infos.hasFilter() && !checkFilter(innerValue)) {
    return false;
  }

  output.moveValueInto(_infos.getOutputRegister(), _currentRow, &guard);
  if (_infos.getKeyRegister() != RegisterId::maxRegisterId) {
    // populate key register with row index
    bool mustDestroy = false;
    AqlValue key{AqlValueHintUInt{_rowIndex}};
    AqlValueGuard guard(key, mustDestroy);
    output.moveValueInto(_infos.getKeyRegister(), _currentRow, &guard);
  }
  output.advanceRow();

  return true;
}

size_t EnumerateListExecutor::skipArrayElement(size_t toSkip) {
  size_t skipped = 0;

  if (toSkip <= _inputArrayLength - _inputArrayPosition) {
    // if we're skipping less or exact the amount of elements we can skip with
    // toSkip
    _inputArrayPosition += toSkip;
    skipped = toSkip;
  } else if (toSkip > _inputArrayLength - _inputArrayPosition) {
    // we can only skip the max amount of values we've in our array
    skipped = _inputArrayLength - _inputArrayPosition;
    _inputArrayPosition = _inputArrayLength;
  }
  return skipped;
}

std::tuple<ExecutorState, FilterStats, AqlCall>
EnumerateListExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                   OutputAqlItemRow& output) {
  FilterStats stats{};

  AqlCall upstreamCall{};
  upstreamCall.fullCount = output.getClientCall().fullCount;

  while (inputRange.hasDataRow() && !output.isFull()) {
    if (_inputArrayLength == _inputArrayPosition) {
      // we reached either the end of an array
      // or are in our first loop iteration
      initializeNewRow(inputRange);
      continue;
    }

    TRI_ASSERT(_inputArrayPosition < _inputArrayLength);

    if (!processArrayElement(output)) {
      // item was filtered out
      stats.incrFiltered();
    }
    ++_inputArrayPosition;
    ++_rowIndex;
  }

  if (_inputArrayLength == _inputArrayPosition) {
    // we reached either the end of an array
    // or are in our first loop iteration
    initializeNewRow(inputRange);
  }

  return {inputRange.upstreamState(), stats, upstreamCall};
}

std::tuple<ExecutorState, FilterStats, size_t, AqlCall>
EnumerateListExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange,
                                     AqlCall& call) {
  FilterStats stats{};

  while (inputRange.hasDataRow() && call.needSkipMore()) {
    if (_inputArrayLength == _inputArrayPosition) {
      // we reached either the end of an array
      // or are in our first loop iteration
      initializeNewRow(inputRange);
      continue;
    }

    TRI_ASSERT(_inputArrayPosition < _inputArrayLength);

    if (_infos.hasFilter()) {
      // we have a filter. we need to apply the filter for each
      // document first, and only count those as skipped that
      // matched the filter.
      AqlValue const& inputList =
          _currentRow.getValue(_infos.getInputRegister());
      bool mustDestroy;
      AqlValue innerValue =
          getAqlValue(inputList, _inputArrayPosition, mustDestroy);
      AqlValueGuard guard(innerValue, mustDestroy);

      if (checkFilter(innerValue)) {
        call.didSkip(1);
      } else {
        stats.incrFiltered();
      }

      // always advance input position
      ++_inputArrayPosition;
      ++_rowIndex;
    } else {
      // no filter. we can skip many documents at once
      auto const skip = std::invoke([&] {
        // if offset is > 0, we're in offset skip phase
        if (call.getOffset() > 0) {
          // we still need to skip offset entries
          return call.getOffset();
        } else {
          TRI_ASSERT(call.needsFullCount());
          // fullCount phase - skippen bis zum ende
          return _inputArrayLength - _inputArrayPosition;
        }
      });
      auto const skipped = skipArrayElement(skip);
      // the call to skipArrayElement has advanced the input position already
      call.didSkip(skipped);
      _rowIndex += skipped;
    }
  }

  if (_inputArrayPosition < _inputArrayLength) {
    // fullCount will always skip the complete array
    return {ExecutorState::HASMORE, stats, call.getSkipCount(), AqlCall{}};
  }
  return {inputRange.upstreamState(), stats, call.getSkipCount(), AqlCall{}};
}

bool EnumerateListExecutor::checkFilter(AqlValue const& currentValue) {
  TRI_ASSERT(_infos.hasFilter());
  TRI_ASSERT(_expressionContext != nullptr);

  _expressionContext->adjustCurrentRow(_currentRow);
  _expressionContext->adjustCurrentValue(currentValue);
  _expressionContext->adjustCurrentRowIndex(_rowIndex);

  bool mustDestroy;  // will get filled by execution
  AqlValue a =
      _infos.getFilter()->execute(_expressionContext.get(), mustDestroy);
  AqlValueGuard guard(a, mustDestroy);
  return a.toBoolean();
}

void EnumerateListExecutor::initialize() {
  // TODO
  TRI_ASSERT(false);
  _inputArrayLength = 0;
  _inputArrayPosition = 0;
  _currentRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
}

/// @brief create an AqlValue from the inVariable using the current _index
AqlValue EnumerateListExecutor::getAqlValue(AqlValue const& inVarReg,
                                            size_t const& pos,
                                            bool& mustDestroy) {
  TRI_IF_FAILURE("EnumerateListBlock::getAqlValue") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  return inVarReg.at(pos, mustDestroy, true);
}
