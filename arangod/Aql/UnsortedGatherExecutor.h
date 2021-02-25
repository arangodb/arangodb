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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_UNSORTEDGATHEREXECUTOR_H
#define ARANGOD_AQL_UNSORTEDGATHEREXECUTOR_H

#include "Aql/AqlCallSet.h"
#include "Aql/ExecutionState.h"
#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/RegisterInfos.h"
#include "Aql/types.h"

#include <string>
#include <unordered_set>

namespace arangodb::aql {

class NoStats;
class InputAqlItemRow;
class OutputAqlItemRow;
class IdExecutorInfos;
class SharedAqlItemBlockPtr;
struct AqlCall;

/**
 * @brief Produces all rows from its dependencies, which may be more than one,
 * in some unspecified order. It is, purposefully, strictly synchronous, and
 * always waits for an answer before requesting the next row(s). This is as
 * opposed to the ParallelUnsortedGather, which already starts fetching the next
 * dependenci(es) while waiting for an answer.
 *
 * The actual implementation fetches all available rows from the first
 * dependency, then from the second, and so forth. But that is not guaranteed.
 */
class UnsortedGatherExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = false;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    // This (inputSizeRestrictsOutputSize) could be set to true, but its
    // usefulness would be limited.
    // We either can only use it for the last dependency, in which case it's
    // already too late to avoid a large allocation for a small result set; or
    // we'd have to prefetch all dependencies (at least until we got >=1000
    // rows) before answering hasExpectedNumberOfRows(). This might be okay,
    // but would increase the latency.
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = MultiDependencySingleRowFetcher;
  // TODO I should probably implement custom Infos, we don't need distributeId().
  using Infos = IdExecutorInfos;
  using Stats = NoStats;

  UnsortedGatherExecutor(Fetcher& fetcher, Infos&);
  ~UnsortedGatherExecutor();

  /**
   * @brief Produce rows
   *
   * @param input DataRange delivered by the fetcher
   * @param output place to write rows to
   * @return std::tuple<ExecutorState, Stats, AqlCall, size_t>
   *   ExecutorState: DONE or HASMORE (only within a subquery)
   *   Stats: Stats gerenated here
   *   AqlCallSet: Request to upstream
   */
  [[nodiscard]] auto produceRows(typename Fetcher::DataRange& input, OutputAqlItemRow& output)
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
   *   AqlCallSet: Request to upstream
   */
  [[nodiscard]] auto skipRowsRange(typename Fetcher::DataRange& input, AqlCall& call)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCallSet>;

 private:
  auto initialize(typename Fetcher::DataRange const& input) -> void;
  [[nodiscard]] auto numDependencies() const noexcept -> size_t;
  [[nodiscard]] auto done() const noexcept -> bool;
  [[nodiscard]] auto currentDependency() const noexcept -> size_t;
  auto advanceDependency() noexcept -> void;

 private:
  size_t _currentDependency{0};
  size_t _numDependencies{0};
};

}  // namespace arangodb::aql

#endif  // ARANGOD_AQL_UNSORTEDGATHEREXECUTOR_H
