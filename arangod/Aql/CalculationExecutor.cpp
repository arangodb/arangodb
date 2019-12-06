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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "CalculationExecutor.h"

#include "Aql/ExecutorExpressionContext.h"
#include "Aql/Expression.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/ServerState.h"
#include "V8/v8-globals.h"

using namespace arangodb;
using namespace arangodb::aql;

CalculationExecutorInfos::CalculationExecutorInfos(
    RegisterId outputRegister, RegisterId nrInputRegisters,
    RegisterId nrOutputRegisters, std::unordered_set<RegisterId> registersToClear,
    std::unordered_set<RegisterId> registersToKeep, Query& query, Expression& expression,
    std::vector<Variable const*>&& expInVars, std::vector<RegisterId>&& expInRegs)
    : ExecutorInfos(make_shared_unordered_set(expInRegs.begin(), expInRegs.end()),
                    make_shared_unordered_set({outputRegister}),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _outputRegisterId(outputRegister),
      _query(query),
      _expression(expression),
      _expInVars(std::move(expInVars)),
      _expInRegs(std::move(expInRegs)) {
  TRI_ASSERT(_query.trx() != nullptr);
}

template <CalculationType calculationType>
CalculationExecutor<calculationType>::CalculationExecutor(Fetcher& fetcher,
                                                          CalculationExecutorInfos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _currentRow(InputAqlItemRow{CreateInvalidInputRowHint{}}),
      _rowState(ExecutionState::HASMORE),
      _hasEnteredContext(false) {}

template <CalculationType calculationType>
CalculationExecutor<calculationType>::~CalculationExecutor() = default;

RegisterId CalculationExecutorInfos::getOutputRegisterId() const noexcept {
  return _outputRegisterId;
}

Query& CalculationExecutorInfos::getQuery() const noexcept { return _query; }

Expression& CalculationExecutorInfos::getExpression() const noexcept {
  return _expression;
}

std::vector<Variable const*> const& CalculationExecutorInfos::getExpInVars() const noexcept {
  return _expInVars;
}

std::vector<RegisterId> const& CalculationExecutorInfos::getExpInRegs() const noexcept {
  return _expInRegs;
}

template <CalculationType calculationType>
std::pair<ExecutionState, typename CalculationExecutor<calculationType>::Stats>
CalculationExecutor<calculationType>::produceRows(OutputAqlItemRow& output) {
  ExecutionState state;
  InputAqlItemRow row = InputAqlItemRow{CreateInvalidInputRowHint{}};
  std::tie(state, row) = _fetcher.fetchRow();

  if (state == ExecutionState::WAITING) {
    TRI_ASSERT(!row);
    TRI_ASSERT(!_infos.getQuery().hasEnteredContext());
    return {state, NoStats{}};
  }

  if (!row) {
    TRI_ASSERT(state == ExecutionState::DONE);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_fetcher.isAtShadowRow() || (!_fetcher.hasRowsLeftInBlock() &&
                                            !_infos.getQuery().hasEnteredContext()));
#endif
    return {state, NoStats{}};
  }

  doEvaluation(row, output);

  // _hasEnteredContext implies the query has entered the context, but not
  // the other way round because it may be owned by exterior.
  TRI_ASSERT(!_hasEnteredContext || _infos.getQuery().hasEnteredContext());

  // The following only affects V8Conditions. If we should exit the V8 context
  // between blocks, because we might have to wait for client or upstream, then
  //   hasEnteredContext => state == HASMORE,
  // as we only leave the context open when there are rows left in the current
  // block.
  // Note that _infos.getQuery().hasEnteredContext() may be true, even if
  // _hasEnteredContext is false, if (and only if) the query context is owned
  // by exterior.
  TRI_ASSERT(!shouldExitContextBetweenBlocks() || !_hasEnteredContext ||
             state == ExecutionState::HASMORE);

  return {state, NoStats{}};
}

template <CalculationType calculationType>
std::tuple<ExecutionState, typename CalculationExecutor<calculationType>::Stats, SharedAqlItemBlockPtr>
CalculationExecutor<calculationType>::fetchBlockForPassthrough(size_t atMost) {
  auto rv = _fetcher.fetchBlockForPassthrough(atMost);
  return {rv.first, {}, std::move(rv.second)};
}

template <CalculationType calculationType>
template <CalculationType U, typename>
void CalculationExecutor<calculationType>::enterContext() {
  _infos.getQuery().enterContext();
  _hasEnteredContext = true;
}

template <CalculationType calculationType>
template <CalculationType U, typename>
void CalculationExecutor<calculationType>::exitContext() {
  if (shouldExitContextBetweenBlocks()) {
    // must invalidate the expression now as we might be called from
    // different threads
    _infos.getExpression().invalidate();
    _infos.getQuery().exitContext();
    _hasEnteredContext = false;
  }
}

template <CalculationType calculationType>
bool CalculationExecutor<calculationType>::shouldExitContextBetweenBlocks() const {
  static const bool isRunningInCluster = ServerState::instance()->isRunningInCluster();
  const bool stream = _infos.getQuery().queryOptions().stream;

  return isRunningInCluster || stream;
}

template <>
void CalculationExecutor<CalculationType::Reference>::doEvaluation(InputAqlItemRow& input,
                                                                   OutputAqlItemRow& output) {
  auto const& inRegs = _infos.getExpInRegs();
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
  output.copyBlockInternalRegister(input, inRegs[0], _infos.getOutputRegisterId());
}

template <>
void CalculationExecutor<CalculationType::Condition>::doEvaluation(InputAqlItemRow& input,
                                                                   OutputAqlItemRow& output) {
  // execute the expression
  ExecutorExpressionContext ctx(&_infos.getQuery(), input,
                                _infos.getExpInVars(), _infos.getExpInRegs());

  bool mustDestroy;  // will get filled by execution
  AqlValue a = _infos.getExpression().execute(_infos.getQuery().trx(), &ctx, mustDestroy);
  AqlValueGuard guard(a, mustDestroy);

  TRI_IF_FAILURE("CalculationBlock::executeExpression") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  output.moveValueInto(_infos.getOutputRegisterId(), input, guard);
}

template <>
void CalculationExecutor<CalculationType::V8Condition>::doEvaluation(InputAqlItemRow& input,
                                                                     OutputAqlItemRow& output) {
  // must have a V8 context here to protect Expression::execute().

  // enterContext is safe to call even if we've already entered.

  // If we should exit the context between two blocks, because client or
  // upstream might send us to sleep, it is expected that we enter the context
  // exactly on the first row of every block.
  TRI_ASSERT(!shouldExitContextBetweenBlocks() ||
             _hasEnteredContext == !input.isFirstDataRowInBlock());

  enterContext();
  auto contextGuard = scopeGuard([this]() { exitContext(); });

  ISOLATE;
  v8::HandleScope scope(isolate);  // do not delete this!
  // execute the expression
  ExecutorExpressionContext ctx(&_infos.getQuery(), input,
                                _infos.getExpInVars(), _infos.getExpInRegs());

  bool mustDestroy;  // will get filled by execution
  AqlValue a = _infos.getExpression().execute(_infos.getQuery().trx(), &ctx, mustDestroy);
  AqlValueGuard guard(a, mustDestroy);

  TRI_IF_FAILURE("CalculationBlock::executeExpression") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  output.moveValueInto(_infos.getOutputRegisterId(), input, guard);

  if (input.blockHasMoreDataRowsAfterThis()) {
    // We will be called again before the fetcher needs to get a new block.
    // Thus we won't wait for upstream, nor will get a WAITING on the next
    // fetchRow().
    // So we keep the context open.
    // This works because this block allows pass through, i.e. produces exactly
    // one output row per input row.
    contextGuard.cancel();
  }
}

template class ::arangodb::aql::CalculationExecutor<CalculationType::Condition>;
template class ::arangodb::aql::CalculationExecutor<CalculationType::V8Condition>;
template class ::arangodb::aql::CalculationExecutor<CalculationType::Reference>;
