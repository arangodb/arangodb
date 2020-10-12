////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_CALACULATION_EXECUTOR_H
#define ARANGOD_AQL_CALACULATION_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/RegisterInfos.h"
#include "Aql/Stats.h"
#include "Aql/types.h"
#include "Transaction/Methods.h"

#include <unordered_set>
#include <vector>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

struct AqlCall;
class AqlItemBlockInputRange;
class Expression;
class OutputAqlItemRow;
class QueryContext;
template <BlockPassthrough>
class SingleRowFetcher;
struct Variable;

struct CalculationExecutorInfos {
  CalculationExecutorInfos(RegisterId outputRegister, QueryContext& query, Expression& expression,
                           std::vector<Variable const*>&& expInVars,
                           std::vector<RegisterId>&& expInRegs);

  CalculationExecutorInfos() = delete;
  CalculationExecutorInfos(CalculationExecutorInfos&&) = default;
  CalculationExecutorInfos(CalculationExecutorInfos const&) = delete;
  ~CalculationExecutorInfos() = default;

  RegisterId getOutputRegisterId() const noexcept;

  QueryContext& getQuery() const noexcept;
  transaction::Methods* getTrx() const noexcept;

  Expression& getExpression() const noexcept;

  std::vector<Variable const*> const& getExpInVars() const noexcept;

  std::vector<RegisterId> const& getExpInRegs() const noexcept;

 private:
  RegisterId _outputRegisterId;

  QueryContext& _query;
  Expression& _expression;
  std::vector<Variable const*> _expInVars;  // input variables for expression
  std::vector<RegisterId> _expInRegs;       // input registers for expression
};

enum class CalculationType { Condition, V8Condition, Reference };

template <CalculationType calculationType>
class CalculationExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Enable;
    /* This could be set to true after some investigation/fixes */
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = CalculationExecutorInfos;
  using Stats = NoStats;

  CalculationExecutor(Fetcher& fetcher, CalculationExecutorInfos&);
  ~CalculationExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output);

 private:
  // specialized implementations
  void doEvaluation(InputAqlItemRow& input, OutputAqlItemRow& output);

  // Only for V8Conditions
  template <CalculationType U = calculationType, typename = std::enable_if_t<U == CalculationType::V8Condition>>
  void enterContext();

  // Only for V8Conditions
  template <CalculationType U = calculationType, typename = std::enable_if_t<U == CalculationType::V8Condition>>
  void exitContext();

  [[nodiscard]] bool shouldExitContextBetweenBlocks() const;

 private:
  transaction::Methods _trx;
  aql::AqlFunctionsInternalCache _aqlFunctionsInternalCache;
  CalculationExecutorInfos& _infos;

  Fetcher& _fetcher;

  InputAqlItemRow _currentRow;
  ExecutionState _rowState;

  // true iff we entered a V8 context and didn't exit it yet.
  // Necessary for owned contexts, which will not be exited when we call
  // exitContext; but only for assertions in maintainer mode.
  bool _hasEnteredContext;
};

}  // namespace aql
}  // namespace arangodb

#endif
