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
  ~CalculationExecutor() = default;

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  inline std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output) {
    ExecutionState state;
    InputAqlItemRow row = InputAqlItemRow{CreateInvalidInputRowHint{}};
    std::tie(state, row) = _fetcher.fetchRow();

    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(!row);
      return {state, NoStats{}};
    }

    if (!row) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, NoStats{}};
    }

    doEvaluation(row, output);

    return {state, NoStats{}};
  }

  inline size_t numberOfRowsInFlight() const { return 0; }

 private:
  // specialized implementations
  inline void doEvaluation(InputAqlItemRow& input, OutputAqlItemRow& output);

 public:
  CalculationExecutorInfos& _infos;

 private:
  Fetcher& _fetcher;

  InputAqlItemRow _currentRow;
  ExecutionState _rowState;
};

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
  static const bool isRunningInCluster = ServerState::instance()->isRunningInCluster();
  const bool stream = _infos.getQuery().queryOptions().stream;
  auto cleanup = [&]() {
    if (isRunningInCluster || stream) {
      // must invalidate the expression now as we might be called from
      // different threads
      _infos.getExpression().invalidate();
      _infos.getQuery().exitContext();
    }
  };

  // must have a V8 context here to protect Expression::execute()
  _infos.getQuery().enterContext();
  TRI_DEFER(cleanup());

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
}

}  // namespace aql
}  // namespace arangodb

#endif
