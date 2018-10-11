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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_TRAVERSAL_EXECUTOR_H
#define ARANGOD_AQL_TRAVERSAL_EXECUTOR_H

#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/TraversalStats.h"

namespace arangodb {
namespace aql {

class InputAqlItemRow;
class OutputAqlItemRow;
class ExecutorInfos;
class SingleRowFetcher;

class TraversalExecutorInfos : public ExecutorInfos {
  public:
  TraversalExecutorInfos(std::unordered_set<RegisterId> inputRegisters,
                         std::unordered_set<RegisterId> outputRegisters,
                         RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                         std::unordered_set<RegisterId> registersToClear) :
    ExecutorInfos(inputRegisters, outputRegisters, nrInputRegisters, nrOutputRegisters, registersToClear) {}
 
  TraversalExecutorInfos() = delete;

  TraversalExecutorInfos(TraversalExecutorInfos &&) = default;
  TraversalExecutorInfos(TraversalExecutorInfos const&) = delete;
  ~TraversalExecutorInfos() = default;
};

/**
 * @brief Implementation of Traversal Node
 */
class TraversalExecutor {
  public:
    using Fetcher = SingleRowFetcher;
    using Infos = TraversalExecutorInfos;
    using Stats = TraversalStats;

    TraversalExecutor() = delete;
    TraversalExecutor(TraversalExecutor&&) = default;
    TraversalExecutor(TraversalExecutor const&) = default;
    TraversalExecutor(Fetcher& fetcher, Infos&);
    ~TraversalExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

 private:
  Infos& _infos;
  Fetcher& _fetcher;
  InputAqlItemRow _input;

};

}  // namespace aql
}  // namespace arangodb


#endif
