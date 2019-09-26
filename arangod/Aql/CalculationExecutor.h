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
#include "Aql/InputAqlItemRow.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Aql/Stats.h"
#include "Aql/types.h"

#include <unordered_set>
#include <vector>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

class Expression;
class OutputAqlItemRow;
class Query;
template<bool passBlocksThrough>
class SingleRowFetcher;
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

  RegisterId getOutputRegisterId() const noexcept;

  Query& getQuery() const noexcept;

  Expression& getExpression() const noexcept;

  std::vector<Variable const*> const& getExpInVars() const noexcept;

  std::vector<RegisterId> const& getExpInRegs() const noexcept;

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
  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

  std::tuple<ExecutionState, Stats, SharedAqlItemBlockPtr> fetchBlockForPassthrough(size_t atMost);

 private:
  // specialized implementations
  void doEvaluation(InputAqlItemRow& input, OutputAqlItemRow& output);

  // Only for V8Conditions
  template <CalculationType U = calculationType, typename = std::enable_if_t<U == CalculationType::V8Condition>>
  void enterContext();

  // Only for V8Conditions
  template <CalculationType U = calculationType, typename = std::enable_if_t<U == CalculationType::V8Condition>>
  void exitContext();

  bool shouldExitContextBetweenBlocks() const;

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


}  // namespace aql
}  // namespace arangodb

#endif
