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

#ifndef ARANGOD_AQL_CALACULATION_EXECUTOR_H
#define ARANGOD_AQL_CALACULATION_EXECUTOR_H

#include "Basics/Common.h"
#include "Basics/ScopeGuard.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/Expression.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Cluster/ServerState.h"
#include "ExecutorExpressionContext.h"
#include "V8/v8-globals.h"

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

class ExecutorInfos;
class InputAqlItemRow;
class Expression;
class Query;
struct Variable;

struct CalculationExecutorInfos : public ExecutorInfos {
  CalculationExecutorInfos(RegisterId outputRegister, RegisterId nrInputRegisters,
                           RegisterId nrOutputRegisters,
                           std::unordered_set<RegisterId> registersToClear,
                           std::unordered_set<RegisterId> registersToKeep, Query& query,
                           Expression& expression, std::vector<Variable const*>&& expInVars,
                           std::vector<RegisterId>&& expInRegs);

  CalculationExecutorInfos() = delete;
  CalculationExecutorInfos(CalculationExecutorInfos&&) = default;
  CalculationExecutorInfos(CalculationExecutorInfos const&) = delete;
  ~CalculationExecutorInfos() = default;

  RegisterId getOutputRegisterId() const { return _outputRegisterId; }

  Query& getQuery() const { return _query; }

  Expression& getExpression() const { return _expression; }

  std::vector<Variable const*> const& getExpInVars() const {
    return _expInVars;
  }

  std::vector<RegisterId> const& getExpInRegs() const { return _expInRegs; }

 private:
  RegisterId _outputRegisterId;

  Query& _query;
  Expression& _expression;
  std::vector<Variable const*> _expInVars;  // input variables for expresseion
  std::vector<RegisterId> _expInRegs;       // input registers for expression
};

enum class CalculationType { Condition, V8Condition, Reference };

template <CalculationType calculationType>
class CalculationExecutor {
 public:
  struct Properties {
    static const bool preservesOrder = true;
    static const bool allowsBlockPassthrough = true;
    /* This could be set to true after some investigation/fixes */
    static const bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = CalculationExecutorInfos;
  using Stats = NoStats;

  CalculationExecutor(Fetcher& fetcher, CalculationExecutorInfos&);
  ~CalculationExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  inline std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

  inline std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t) const {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Logic_error, prefetching number fo rows not supported");
  }

 private:
  // specialized implementations
  inline void doEvaluation(InputAqlItemRow& input, OutputAqlItemRow& output);

  // Only for V8Conditions
  template <CalculationType U = calculationType, typename = std::enable_if_t<U == CalculationType::V8Condition>>
  inline void enterContext();

  // Only for V8Conditions
  template <CalculationType U = calculationType, typename = std::enable_if_t<U == CalculationType::V8Condition>>
  inline void exitContext();

  inline bool shouldExitContextBetweenBlocks() const;

 public:
  CalculationExecutorInfos& _infos;

 private:
  Fetcher& _fetcher;

  InputAqlItemRow _currentRow;
  ExecutionState _rowState;

  // true iff we entered a V8 context and didn't exit it yet.
  // Necessary for owned contexts, which will not be exited when we call
  // exitContext; but only for assertions in maintainer mode.
  bool _hasEnteredContext;
};

template<CalculationType calculationType>
template<CalculationType U, typename>
inline void CalculationExecutor<calculationType>::enterContext() {
  _infos.getQuery().enterContext();
  _hasEnteredContext = true;
}

template<CalculationType calculationType>
template<CalculationType U, typename>
inline void CalculationExecutor<calculationType>::exitContext() {
  if (shouldExitContextBetweenBlocks()) {
    // must invalidate the expression now as we might be called from
    // different threads
    _infos.getExpression().invalidate();
    _infos.getQuery().exitContext();
    _hasEnteredContext = false;
  }
}

template<CalculationType calculationType>
bool CalculationExecutor<calculationType>::shouldExitContextBetweenBlocks() const {
  static const bool isRunningInCluster = ServerState::instance()->isRunningInCluster();
  const bool stream = _infos.getQuery().queryOptions().stream;

  return isRunningInCluster || stream;
}

template <>
inline void CalculationExecutor<CalculationType::Reference>::doEvaluation(
    InputAqlItemRow& input, OutputAqlItemRow& output) {
  auto const& inRegs = _infos.getExpInRegs();
  TRI_ASSERT(inRegs.size() == 1);

  TRI_IF_FAILURE("CalculationBlock::executeExpression") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("CalculationBlock::fillBlockWithReference") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  output.cloneValueInto(_infos.getOutputRegisterId(), input, input.getValue(inRegs[0]));
}

template <CalculationType calculationType>
inline std::pair<ExecutionState, typename CalculationExecutor<calculationType>::Stats>
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
    TRI_ASSERT(!_infos.getQuery().hasEnteredContext());
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

template <>
inline void CalculationExecutor<CalculationType::Condition>::doEvaluation(
    InputAqlItemRow& input, OutputAqlItemRow& output) {
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
inline void CalculationExecutor<CalculationType::V8Condition>::doEvaluation(
    InputAqlItemRow& input, OutputAqlItemRow& output) {
  // must have a V8 context here to protect Expression::execute().

  // enterContext is safe to call even if we've already entered.

  // If we should exit the context between two blocks, because client or
  // upstream might send us to sleep, it is expected that we enter the context
  // exactly on the first row of every block.
  TRI_ASSERT(!shouldExitContextBetweenBlocks() ||
             _hasEnteredContext == !input.isFirstRowInBlock());

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

  if (input.blockHasMoreRows()) {
    // We will be called again before the fetcher needs to get a new block.
    // Thus we won't wait for upstream, nor will get a WAITING on the next
    // fetchRow().
    // So we keep the context open.
    // This works because this block allows pass through, i.e. produces exactly
    // one output row per input row.
    contextGuard.cancel();
  }
}

}  // namespace aql
}  // namespace arangodb

#endif
