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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_COUNT_COLLECT_EXECUTOR_H
#define ARANGOD_AQL_COUNT_COLLECT_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/types.h"

#include <memory>
#include <unordered_set>

namespace arangodb {
namespace aql {

struct AqlCall;
class AqlItemBlockInputRange;
class InputAqlItemRow;
class NoStats;
class ExecutorInfos;
class OutputAqlItemRow;
template <BlockPassthrough>
class SingleRowFetcher;

class CountCollectExecutorInfos : public ExecutorInfos {
 public:
  CountCollectExecutorInfos(RegisterId collectRegister, RegisterId nrInputRegisters,
                            RegisterId nrOutputRegisters,
                            std::unordered_set<RegisterId> registersToClear,
                            std::unordered_set<RegisterId> registersToKeep);

  CountCollectExecutorInfos() = delete;
  CountCollectExecutorInfos(CountCollectExecutorInfos&&) = default;
  CountCollectExecutorInfos(CountCollectExecutorInfos const&) = delete;
  ~CountCollectExecutorInfos() = default;

 public:
  RegisterId getOutputRegisterId() const;

 private:
  RegisterId _collectRegister;
};

/**
 * @brief Implementation of Count Collect Executor
 */

class CountCollectExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = false;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = CountCollectExecutorInfos;
  using Stats = NoStats;

  CountCollectExecutor() = delete;
  CountCollectExecutor(CountCollectExecutor&&) = default;
  CountCollectExecutor(CountCollectExecutor const&) = delete;
  CountCollectExecutor(Fetcher& fetcher, Infos&);
  ~CountCollectExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */

  std::pair<ExecutionState, NoStats> produceRows(OutputAqlItemRow& output);

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  std::tuple<ExecutorState, Stats, AqlCall> produceRows(size_t atMost,
                                                        AqlItemBlockInputRange& input,
                                                        OutputAqlItemRow& output);

  void incrCountBy(size_t incr) noexcept;

  uint64_t getCount() noexcept;;

  std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t atMost) const;

 private:
  Infos const& infos() const noexcept;

 private:
  Infos const& _infos;
  Fetcher& _fetcher;
  ExecutionState _state;
  ExecutorState _executorState;
  uint64_t _count;
};

}  // namespace aql
}  // namespace arangodb

#endif
