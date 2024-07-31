////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/Expression.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/QueryContext.h"
#include "Aql/QueryExpressionContext.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"

#include <absl/strings/str_cat.h>

#include <optional>

namespace arangodb::aql {

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

class EnumerateListExpressionContext final : public QueryExpressionContext {
 public:
  EnumerateListExpressionContext(
      transaction::Methods& trx, QueryContext& context,
      AqlFunctionsInternalCache& cache,
      std::vector<std::pair<VariableId, RegisterId>> const& varsToRegister,
      VariableId outputVariableId)
      : QueryExpressionContext(trx, context, cache),
        _varsToRegister(varsToRegister),
        _outputVariableId(outputVariableId),
        _currentValue{AqlValueHintNull{}} {}

  ~EnumerateListExpressionContext() override = default;

  AqlValue getVariableValue(Variable const* variable, bool doCopy,
                            bool& mustDestroy) const override {
    return QueryExpressionContext::getVariableValue(
        variable, doCopy, mustDestroy,
        [this](Variable const* variable, bool doCopy,
               bool& mustDestroy) -> AqlValue {
          mustDestroy = doCopy;
          auto const searchId = variable->id;
          if (searchId == _outputVariableId) {
            if (doCopy) {
              return _currentValue.clone();
            }
            return _currentValue;
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

  void adjustCurrentValue(AqlValue const& value) { _currentValue = value; }

  void adjustCurrentRow(InputAqlItemRow const& inputRow) {
    _inputRow = inputRow;
  }

 private:
  /// @brief temporary storage for expression data context
  std::optional<std::reference_wrapper<InputAqlItemRow const>> _inputRow;
  std::vector<std::pair<VariableId, RegisterId>> const& _varsToRegister;
  VariableId _outputVariableId;
  AqlValue _currentValue;
};

EnumerateListExecutorInfos::EnumerateListExecutorInfos(
    RegisterId inputRegister, RegisterId outputRegister, QueryContext& query,
    Expression* filter, VariableId outputVariableId,
    std::vector<std::pair<VariableId, RegisterId>>&& varsToRegs)
    : _query(query),
      _inputRegister(inputRegister),
      _outputRegister(outputRegister),
      _outputVariableId(outputVariableId),
      _filter(filter),
      _varsToRegs(std::move(varsToRegs)) {
  TRI_ASSERT(!hasFilter() ||
             _outputVariableId != std::numeric_limits<VariableId>::max());
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

VariableId EnumerateListExecutorInfos::getOutputVariableId() const noexcept {
  return _outputVariableId;
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
      _inputArrayLength(0) {
  if (_infos.hasFilter()) {
    _expressionContext = std::make_unique<EnumerateListExpressionContext>(
        _trx, _infos.getQuery(), _aqlFunctionsInternalCache,
        _infos.getVarsToRegs(), _infos.getOutputVariableId());
  }
}

EnumerateListExecutor::~EnumerateListExecutor() = default;

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

    _killCheckCounter = (_killCheckCounter + 1) % 1024;
    if (ADB_UNLIKELY(_killCheckCounter == 0 && _infos.getQuery().killed())) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
    }
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
    }

    _killCheckCounter = (_killCheckCounter + 1) % 1024;
    if (ADB_UNLIKELY(_killCheckCounter == 0 && _infos.getQuery().killed())) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
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

  bool mustDestroy;  // will get filled by execution
  AqlValue a =
      _infos.getFilter()->execute(_expressionContext.get(), mustDestroy);
  AqlValueGuard guard(a, mustDestroy);
  return a.toBoolean();
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

template class ExecutionBlockImpl<EnumerateListExecutor>;
}  // namespace arangodb::aql
