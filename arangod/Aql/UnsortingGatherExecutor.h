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

#ifndef ARANGOD_AQL_UNSORTINGGATHEREXECUTOR_H
#define ARANGOD_AQL_UNSORTINGGATHEREXECUTOR_H

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
* always waits for an answer before requesting the next row(s).
*
* The actual implementation fetches all available rows from the first
* dependency, then from the second, and so forth. But that is not guaranteed.
*/
class UnsortingGatherExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = false;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    // TODO I think we can set this to true (but needs to implement
    //  hasExpectedNumberOfRows for that)
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = MultiDependencySingleRowFetcher;
  // TODO I should probably implement custom Infos, we don't need distributeId().
  using Infos = IdExecutorInfos;
  using Stats = NoStats;

  UnsortingGatherExecutor(Fetcher& fetcher, Infos&);
  ~UnsortingGatherExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  [[nodiscard]] auto produceRows(OutputAqlItemRow& output)
      -> std::pair<ExecutionState, Stats>;

  // TODO implement this
  // std::tuple<ExecutionState, NoStats, size_t> skipRows(size_t atMost);

 private:
  [[nodiscard]] auto numDependencies() const
      noexcept(noexcept(static_cast<Fetcher*>(nullptr)->numberDependencies())) -> size_t;
  [[nodiscard]] auto fetcher() const noexcept -> Fetcher const&;
  [[nodiscard]] auto fetcher() noexcept -> Fetcher&;
  [[nodiscard]] auto done() const noexcept -> bool;
  [[nodiscard]] auto currentDependency() const noexcept -> size_t;
  [[nodiscard]] auto fetchNextRow(size_t atMost)
      -> std::pair<ExecutionState, InputAqlItemRow>;
  auto advanceDependency() noexcept -> void;

 private:
  Fetcher& _fetcher;
  size_t _currentDependency{0};
};

}  // namespace arangodb::aql

#endif  // ARANGOD_AQL_UNSORTINGGATHEREXECUTOR_H
