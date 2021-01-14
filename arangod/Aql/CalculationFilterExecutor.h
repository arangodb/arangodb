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

#ifndef ARANGOD_AQL_CALCULATION_FILTER_EXECUTOR_H
#define ARANGOD_AQL_CALCULATION_FILTER_EXECUTOR_H

#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/ExecutionState.h"
#include "Aql/RegisterInfos.h"
#include "Aql/Stats.h"
#include "Aql/types.h"
#include "Transaction/Methods.h"

namespace arangodb::aql {

struct AqlCall;
class AqlItemBlockInputRange;
class Expression;
class InputAqlItemRow;
class OutputAqlItemRow;
class QueryContext;
class RegisterInfos;
class CalculationFilterStats;
template <BlockPassthrough>
class SingleRowFetcher;
struct Variable;

struct CalculationFilterExecutorInfos {
  CalculationFilterExecutorInfos(QueryContext& query, Expression& expression,
                                 std::vector<Variable const*>&& expInVars,
                                 std::vector<RegisterId>&& expInRegs);

  CalculationFilterExecutorInfos() = delete;
  CalculationFilterExecutorInfos(CalculationFilterExecutorInfos&&) = default;
  CalculationFilterExecutorInfos(CalculationFilterExecutorInfos const&) = delete;
  ~CalculationFilterExecutorInfos() = default;

  QueryContext& getQuery() const noexcept;
  transaction::Methods* getTrx() const noexcept;

  Expression& getExpression() const noexcept;

  std::vector<Variable const*> const& getExpInVars() const noexcept;

  std::vector<RegisterId> const& getExpInRegs() const noexcept;

 private:
  QueryContext& _query;
  Expression& _expression;
  std::vector<Variable const*> _expInVars;  // input variables for expression
  std::vector<RegisterId> _expInRegs;       // input registers for expression
};

/**
 * @brief Implementation of CalculationFilter Node
 */
class CalculationFilterExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = CalculationFilterExecutorInfos;
  using Stats = CalculationFilterStats;

  CalculationFilterExecutor() = delete;
  CalculationFilterExecutor(CalculationFilterExecutor&&) = default;
  CalculationFilterExecutor(CalculationFilterExecutor const&) = delete;
  CalculationFilterExecutor(Fetcher& fetcher, Infos&);
  ~CalculationFilterExecutor();

  /**
   * @brief produce the next Rows of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputRange& input, OutputAqlItemRow& output);

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall> skipRowsRange(
      AqlItemBlockInputRange& inputRange, AqlCall& call);
  
  [[nodiscard]] auto expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                             AqlCall const& call) const noexcept -> size_t;

 private:
  bool doEvaluation(InputAqlItemRow const& input);

 private:
  transaction::Methods _trx;
  aql::AqlFunctionsInternalCache _aqlFunctionsInternalCache;
  Infos& _infos;
};

}  // namespace arangodb::aql

#endif
