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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_UNSORTEDGATHEREXECUTOR_H
#define ARANGOD_AQL_UNSORTEDGATHEREXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/types.h"

#include <string>
#include <unordered_set>

namespace arangodb::aql {

class NoStats;
class InputAqlItemRow;
class OutputAqlItemRow;
class IdExecutorInfos;
class SharedAqlItemBlockPtr;

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
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  [[nodiscard]] auto produceRows(OutputAqlItemRow& output)
      -> std::pair<ExecutionState, Stats>;

  [[nodiscard]] auto skipRows(size_t atMost) -> std::tuple<ExecutionState, NoStats, size_t>;

 private:
  [[nodiscard]] auto numDependencies() const
      noexcept(noexcept(static_cast<Fetcher*>(nullptr)->numberDependencies())) -> size_t;
  [[nodiscard]] auto fetcher() const noexcept -> Fetcher const&;
  [[nodiscard]] auto fetcher() noexcept -> Fetcher&;
  [[nodiscard]] auto done() const noexcept -> bool;
  [[nodiscard]] auto currentDependency() const noexcept -> size_t;
  [[nodiscard]] auto fetchNextRow(size_t atMost)
      -> std::pair<ExecutionState, InputAqlItemRow>;
  [[nodiscard]] auto skipNextRows(size_t atMost) -> std::pair<ExecutionState, size_t>;
  auto advanceDependency() noexcept -> void;

 private:
  Fetcher& _fetcher;
  size_t _currentDependency{0};
  size_t _skipped{0};
};

}  // namespace arangodb::aql

#endif  // ARANGOD_AQL_UNSORTEDGATHEREXECUTOR_H
