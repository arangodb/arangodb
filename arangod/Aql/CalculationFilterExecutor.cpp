////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "CalculationFilterExecutor.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlCallStack.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutorExpressionContext.h"
#include "Aql/Expression.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Basics/Exceptions.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

CalculationFilterExecutorInfos::CalculationFilterExecutorInfos(QueryContext& query, Expression& expression,
                                                               std::vector<Variable const*>&& expInVars,
                                                               std::vector<RegisterId>&& expInRegs)
    : _query(query),
      _expression(expression),
      _expInVars(std::move(expInVars)),
      _expInRegs(std::move(expInRegs)) {}

QueryContext& CalculationFilterExecutorInfos::getQuery() const noexcept { return _query; }

Expression& CalculationFilterExecutorInfos::getExpression() const noexcept {
  return _expression;
}

std::vector<Variable const*> const& CalculationFilterExecutorInfos::getExpInVars() const noexcept {
  return _expInVars;
}

std::vector<RegisterId> const& CalculationFilterExecutorInfos::getExpInRegs() const noexcept {
  return _expInRegs;
}

CalculationFilterExecutor::CalculationFilterExecutor(Fetcher& /*fetcher*/,
                                                     CalculationFilterExecutorInfos& infos)
    : _trx(infos.getQuery().newTrxContext()),
      _infos(infos) {}

CalculationFilterExecutor::~CalculationFilterExecutor() = default;

auto CalculationFilterExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  CalculationFilterStats stats{};
  
  while (inputRange.hasDataRow() && call.needSkipMore()) {
    auto const [unused, input] = inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input);
    if (doEvaluation(input)) {
      call.didSkip(1);
    } else {
      stats.incrFiltered();
    }
  }

  // Just fetch everything from above, allow overfetching
  return {inputRange.upstreamState(), stats, call.getSkipCount(), AqlCall{}};
}

auto CalculationFilterExecutor::produceRows(AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  TRI_IF_FAILURE("FilterExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  CalculationFilterStats stats{};

  while (inputRange.hasDataRow() && !output.isFull()) {
    auto const& [state, input] = inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input);
    TRI_ASSERT(input.isInitialized());
    if (doEvaluation(input)) {
      output.copyRow(input);
      output.advanceRow();
    } else {
      stats.incrFiltered();
    }
  }

  // Just fetch everything from above, allow overfetching
  return {inputRange.upstreamState(), stats, AqlCall{}};
}

[[nodiscard]] auto CalculationFilterExecutor::expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                                                      AqlCall const& call) const
    noexcept -> size_t {
  if (input.finalState() == ExecutorState::DONE) {
    return std::min(call.getLimit(), input.countDataRows());
  }
  // We do not know how many more rows will be returned from upstream.
  // So we can only overestimate
  return call.getLimit();
}

bool CalculationFilterExecutor::doEvaluation(InputAqlItemRow const& input) {
  // execute the expression
  ExecutorExpressionContext ctx(_trx, _infos.getQuery(), _aqlFunctionsInternalCache, input,
                                _infos.getExpInVars(), _infos.getExpInRegs());

  bool mustDestroy;  // will get filled by execution
  AqlValue a = _infos.getExpression().execute(&ctx, mustDestroy);
  AqlValueGuard guard(a, mustDestroy);

  return a.toBoolean();
}
