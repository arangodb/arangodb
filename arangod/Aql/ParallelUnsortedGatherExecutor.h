////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_PARALLEL_UNSORTED_GATHER_EXECUTOR_H
#define ARANGOD_AQL_PARALLEL_UNSORTED_GATHER_EXECUTOR_H

#include "Aql/ClusterNodes.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Containers/SmallVector.h"

namespace arangodb {

namespace transaction {
class Methods;
}

namespace aql {

class MultiDependencySingleRowFetcher;
class NoStats;
class OutputAqlItemRow;
struct SortRegister;

class ParallelUnsortedGatherExecutorInfos : public ExecutorInfos {
 public:
  ParallelUnsortedGatherExecutorInfos(RegisterId nrInOutRegisters,
                                      std::unordered_set<RegisterId> registersToKeep,
                                      std::unordered_set<RegisterId> registersToClear);
  ParallelUnsortedGatherExecutorInfos() = delete;
  ParallelUnsortedGatherExecutorInfos(ParallelUnsortedGatherExecutorInfos&&) = default;
  ParallelUnsortedGatherExecutorInfos(ParallelUnsortedGatherExecutorInfos const&) = delete;
  ~ParallelUnsortedGatherExecutorInfos() = default;
};

class ParallelUnsortedGatherExecutor {
 public:

 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };

  using Fetcher = MultiDependencySingleRowFetcher;
  using Infos = ParallelUnsortedGatherExecutorInfos;
  using Stats = NoStats;

  ParallelUnsortedGatherExecutor(Fetcher& fetcher, Infos& info);
  ~ParallelUnsortedGatherExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

  std::tuple<ExecutionState, Stats, size_t> skipRows(size_t atMost);
  
 private:
  
  void initDependencies();
  
 private:
  Fetcher& _fetcher;
  // 64: default size of buffer; 8: Alignment size; computed to 4 but breaks in windows debug build.
  ::arangodb::containers::SmallVector<ExecutionState, 64, 8>::allocator_type::arena_type _arena;
  ::arangodb::containers::SmallVector<ExecutionState, 64, 8> _upstream{_arena};

  // Total Number of dependencies
  size_t _numberDependencies;
  
  size_t _currentDependency;
  
  size_t _skipped;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_PARALLEL_UNSORTED_GATHER_EXECUTOR_H
