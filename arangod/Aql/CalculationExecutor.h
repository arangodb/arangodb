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
#include "Aql/OutputAqlItemRow.h"


namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

class ExecutorInfos;
class InputAqlItemRow;
class NoStats;
class SingleRowFetcher;
class Expression;
class Query;
struct Variable;

struct CalculationExecutorInfos : public ExecutorInfos {
  CalculationExecutorInfos(RegisterId outputRegister, RegisterId nrInputRegisters,
                           RegisterId nrOutputRegisters,
                           std::unordered_set<RegisterId> registersToClear, Query*,
                           Expression*, std::vector<Variable const*>&& expInVars,
                           std::vector<RegisterId>&& expInRegs);

  CalculationExecutorInfos() = delete;
  CalculationExecutorInfos(CalculationExecutorInfos&&) = default;
  CalculationExecutorInfos(CalculationExecutorInfos const&) = delete;
  ~CalculationExecutorInfos() = default;

  RegisterId _outputRegisterId;

  Query* _query;
  Expression* _expression;
  std::vector<Variable const*> _expInVars;  // input variables for expresseion
  std::vector<RegisterId> _expInRegs;       // input registers for expression?
  bool _isReference = false;
};

class CalculationExecutor {
 public:
  using Fetcher = SingleRowFetcher;
  using Infos = CalculationExecutorInfos;
  using Stats = NoStats;

  CalculationExecutor(Fetcher& fetcher, CalculationExecutorInfos&);
  ~CalculationExecutor() = default;

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

 public:
  CalculationExecutorInfos& _infos;

 private:
  AqlValue getAqlValue(AqlValue const& inVarReg, size_t const& pos, bool& mustDestroy);
  void initialize();

 private:
  Fetcher& _fetcher;

  InputAqlItemRow _currentRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
  ExecutionState _rowState;
};

}  // namespace aql
}  // namespace arangodb

#endif
