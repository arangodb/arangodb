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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "CalculationExecutor.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlCallStack.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/ExecutorExpressionContext.h"
#include "Aql/Expression.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/ServerState.h"

#ifdef USE_V8
#include "V8/v8-globals.h"
#endif

namespace arangodb::aql {

CalculationExecutorInfos::CalculationExecutorInfos(
    RegisterId outputRegister, QueryContext& query, Expression& expression,
    std::vector<std::pair<VariableId, RegisterId>>&& expInVarToRegs)
    : _outputRegisterId(outputRegister),
      _query(query),
      _expression(expression),
      _expVarToRegs(std::move(expInVarToRegs)) {}

template<CalculationType calculationType>
CalculationExecutor<calculationType>::CalculationExecutor(
    Fetcher& fetcher, CalculationExecutorInfos& infos)
    : _infos(infos),
      _trx(_infos.getQuery().newTrxContext()),
      _fetcher(fetcher),
      _currentRow(InputAqlItemRow{CreateInvalidInputRowHint{}}),
      _rowState(ExecutionState::HASMORE),
      _hasEnteredExecutor(false) {}

template<CalculationType calculationType>
CalculationExecutor<calculationType>::~CalculationExecutor() = default;

RegisterId CalculationExecutorInfos::getOutputRegisterId() const noexcept {
  return _outputRegisterId;
}

QueryContext& CalculationExecutorInfos::getQuery() const noexcept {
  return _query;
}

Expression& CalculationExecutorInfos::getExpression() const noexcept {
  return _expression;
}

std::vector<std::pair<VariableId, RegisterId>> const&
CalculationExecutorInfos::getVarToRegs() const noexcept {
  return _expVarToRegs;
}

template<CalculationType calculationType>
std::tuple<ExecutorState, typename CalculationExecutor<calculationType>::Stats,
           AqlCall>
CalculationExecutor<calculationType>::produceRows(
    AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output) {
  TRI_IF_FAILURE("CalculationExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  while (inputRange.hasDataRow()) {
    // This executor is passthrough. it has enough place to write.
    TRI_ASSERT(!output.isFull());
    auto [state, input] =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input.isInitialized());

    doEvaluation(input, output);
    output.advanceRow();

    // _hasEnteredExecutor implies the query has entered the context, but not
    // the other way round because it may be owned by exterior.
    TRI_ASSERT(!_hasEnteredExecutor ||
               _infos.getQuery().hasEnteredV8Executor());

    // The following only affects V8Conditions. If we should exit the V8 context
    // between blocks, because we might have to wait for client or upstream,
    // then
    //   hasEnteredContext => state == HASMORE,
    // as we only leave the context open when there are rows left in the current
    // block.
    // Note that _infos.getQuery().hasEnteredContext() may be true, even if
    // _hasEnteredExecutor is false, if (and only if) the query context is owned
    // by exterior.
    TRI_ASSERT(!shouldExitContextBetweenBlocks() || !_hasEnteredExecutor ||
               state == ExecutorState::HASMORE);

    _killCheckCounter = (_killCheckCounter + 1) % 1024;
    if (ADB_UNLIKELY(_killCheckCounter == 0 && _infos.getQuery().killed())) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
    }
  }

  return {inputRange.upstreamState(), NoStats{}, output.getClientCall()};
}

#ifdef USE_V8
template<CalculationType calculationType>
template<CalculationType U, typename>
void CalculationExecutor<calculationType>::enterContext() {
  _infos.getQuery().enterV8Executor();
  _hasEnteredExecutor = true;
}

template<CalculationType calculationType>
template<CalculationType U, typename>
void CalculationExecutor<calculationType>::exitContext() noexcept {
  if (shouldExitContextBetweenBlocks()) {
    // must invalidate the expression now as we might be called from
    // different threads
    _infos.getQuery().exitV8Executor();
    _hasEnteredExecutor = false;
  }
}
#endif

template<CalculationType calculationType>
bool CalculationExecutor<calculationType>::shouldExitContextBetweenBlocks()
    const noexcept {
  static bool const isRunningInCluster =
      ServerState::instance()->isRunningInCluster();
  bool const stream = _infos.getQuery().queryOptions().stream;

  return isRunningInCluster || stream;
}

template<>
void CalculationExecutor<CalculationType::Reference>::doEvaluation(
    InputAqlItemRow& input, OutputAqlItemRow& output) {
  auto const& inRegs = _infos.getVarToRegs();
  TRI_ASSERT(inRegs.size() == 1);

  TRI_IF_FAILURE("CalculationBlock::executeExpression") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("CalculationBlock::fillBlockWithReference") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // We assume here that the output block (which must be the same as the input
  // block) is already responsible for this value.
  // Thus we do not want to clone it.
  output.copyBlockInternalRegister(input, inRegs[0].second,
                                   _infos.getOutputRegisterId());
}

template<>
void CalculationExecutor<CalculationType::Condition>::doEvaluation(
    InputAqlItemRow& input, OutputAqlItemRow& output) {
  // execute the expression
  ExecutorExpressionContext ctx(_trx, _infos.getQuery(),
                                _aqlFunctionsInternalCache, input,
                                _infos.getVarToRegs());

  bool mustDestroy;  // will get filled by execution
  AqlValue a = _infos.getExpression().execute(&ctx, mustDestroy);
  AqlValueGuard guard(a, mustDestroy);

  TRI_IF_FAILURE("CalculationBlock::executeExpression") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  output.moveValueInto(_infos.getOutputRegisterId(), input, &guard);
}

#ifdef USE_V8
template<>
void CalculationExecutor<CalculationType::V8Condition>::doEvaluation(
    InputAqlItemRow& input, OutputAqlItemRow& output) {
  // must have a V8 context here to protect Expression::execute().

  // enterContext is safe to call even if we've already entered.

  // If we should exit the context between two blocks, because client or
  // upstream might send us to sleep, it is expected that we enter the context
  // exactly on the first row of every block.
  TRI_ASSERT(!shouldExitContextBetweenBlocks() ||
             _hasEnteredExecutor == !input.isFirstDataRowInBlock());

  enterContext();
  auto contextGuard = scopeGuard([this]() noexcept { exitContext(); });

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope scope(isolate);  // do not delete this!
  // execute the expression
  ExecutorExpressionContext ctx(_trx, _infos.getQuery(),
                                _aqlFunctionsInternalCache, input,
                                _infos.getVarToRegs());

  bool mustDestroy;  // will get filled by execution
  AqlValue a = _infos.getExpression().execute(&ctx, mustDestroy);
  AqlValueGuard guard(a, mustDestroy);

  TRI_IF_FAILURE("CalculationBlock::executeExpression") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  output.moveValueInto(_infos.getOutputRegisterId(), input, &guard);

  if (input.blockHasMoreDataRowsAfterThis()) {
    // We will be called again before the fetcher needs to get a new block.
    // So we keep the context open.
    // This works because this block allows pass through, i.e. produces exactly
    // one output row per input row.
    contextGuard.cancel();
  }
}
#endif

template class CalculationExecutor<CalculationType::Condition>;
template class ExecutionBlockImpl<
    CalculationExecutor<CalculationType::Reference>>;
#ifdef USE_V8
template class CalculationExecutor<CalculationType::V8Condition>;
template class ExecutionBlockImpl<
    CalculationExecutor<CalculationType::V8Condition>>;
#endif
template class CalculationExecutor<CalculationType::Reference>;
template class ExecutionBlockImpl<
    CalculationExecutor<CalculationType::Condition>>;
}  // namespace arangodb::aql
