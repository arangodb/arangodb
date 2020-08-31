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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_PARALLEL_UNSORTED_GATHER_EXECUTOR_H
#define ARANGOD_AQL_PARALLEL_UNSORTED_GATHER_EXECUTOR_H

#include "Aql/AqlCallSet.h"
#include "Aql/ClusterNodes.h"
#include "Aql/EmptyExecutorInfos.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Containers/SmallVector.h"

namespace arangodb {

namespace transaction {
class Methods;
}

namespace aql {

struct AqlCall;
class MultiAqlItemBlockInputRange;
class MultiDependencySingleRowFetcher;
class NoStats;
class OutputAqlItemRow;

class ParallelUnsortedGatherExecutor {
 public:
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };

  using Fetcher = MultiDependencySingleRowFetcher;
  using Infos = EmptyExecutorInfos;
  using Stats = NoStats;

  ParallelUnsortedGatherExecutor(Fetcher& fetcher, Infos& info);
  ~ParallelUnsortedGatherExecutor();

  /**
   * @brief Produce rows
   *
   * @param input DataRange delivered by the fetcher
   * @param output place to write rows to
   * @return std::tuple<ExecutorState, Stats, AqlCall, size_t>
   *   ExecutorState: DONE or HASMORE (only within a subquery)
   *   Stats: Stats gerenated here
   *   AqlCall: Request to upstream
   *   size:t: Dependency to request
   */
  [[nodiscard]] auto produceRows(MultiAqlItemBlockInputRange& input, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCallSet>;

  /**
   * @brief Skip rows
   *
   * @param input DataRange delivered by the fetcher
   * @param call skip request form consumer
   * @return std::tuple<ExecutorState, Stats, AqlCall, size_t>
   *   ExecutorState: DONE or HASMORE (only within a subquery)
   *   Stats: Stats gerenated here
   *   size_t: Number of rows skipped
   *   AqlCall: Request to upstream
   *   size:t: Dependency to request
   */
  [[nodiscard]] auto skipRowsRange(MultiAqlItemBlockInputRange& input, AqlCall& call)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCallSet>;

 private:
  auto upstreamCallSkip(AqlCall const& clientCall) const noexcept -> AqlCall;
  auto upstreamCallProduce(AqlCall const& clientCall) const noexcept -> AqlCall;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_PARALLEL_UNSORTED_GATHER_EXECUTOR_H
