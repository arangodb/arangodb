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


#ifndef ARANGOD_AQL_RETURN_EXECUTOR_H
#define ARANGOD_AQL_RETURN_EXECUTOR_H

#include "Aql/ExecutionState.h"

#include "Aql/ExecutorInfos.h"
#include "Aql/SingleRowFetcher.h"

#include "Aql/Variable.h" //invar

#include <memory>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

class AllRowsFetcher;
class AqlItemMatrix;
class ExecutorInfos;
class NoStats;
class OutputAqlItemRow;
struct SortRegister;

class ReturnExecutorInfos : public ExecutorInfos {
 public:
  ReturnExecutorInfos(RegisterId inputRegister,
                RegisterId outputRegisters,
                RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                std::unordered_set<RegisterId> registersToClear, bool returnInheritedResults);

  ReturnExecutorInfos() = delete;
  ReturnExecutorInfos(ReturnExecutorInfos &&) = default;
  ReturnExecutorInfos(ReturnExecutorInfos const&) = delete;
  ~ReturnExecutorInfos() = default;

  /// @brief the variable produced by Return
  Variable const* _inVariable;
  bool _count;
  RegisterId _inputRegisterId;
  RegisterId _outputRegisterId;
  bool _returnInheritedResults;
};

/**
 * @brief Implementation of Return Node
 */
class ReturnExecutor {
 public:
  using Fetcher = SingleRowFetcher;
  using Infos = ReturnExecutorInfos;
  using Stats = NoStats;

  ReturnExecutor(Fetcher& fetcher, ReturnExecutorInfos&);
  ~ReturnExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

 private:
  ReturnExecutorInfos& _infos;
  Fetcher& _fetcher;

};
}  // namespace aql
}  // namespace arangodb

#endif
