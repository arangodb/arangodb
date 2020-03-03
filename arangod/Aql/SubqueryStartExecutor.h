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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SUBQUERY_START_EXECUTOR_H
#define ARANGOD_AQL_SUBQUERY_START_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"

#include <utility>

namespace arangodb {
namespace aql {

template <BlockPassthrough allowsPassThrough>
class SingleRowFetcher;
class NoStats;
class ExecutorInfos;
class OutputAqlItemRow;

class SubqueryStartExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };

  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = ExecutorInfos;
  using Stats = NoStats;
  SubqueryStartExecutor(Fetcher& fetcher, Infos& infos);
  ~SubqueryStartExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

  /**
   * @brief Estimate of expected number of rows.
   *
   * @return ExecutionState, merely taken from upstream,
   *         size_t number of rows we are likely to produce (at most)
   *
   */
  std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t atMost) const;

 private:
  enum class State {
    READ_DATA_ROW,
    PRODUCE_DATA_ROW,
    PRODUCE_SHADOW_ROW,
    PASS_SHADOW_ROW
  };

  auto static stateToString(State state) -> std::string;

 private:
  // Fetcher to get data.
  Fetcher& _fetcher;

  // Upstream state, used to determine if we are done with all subqueries
  ExecutionState _upstreamState{ExecutionState::HASMORE};

  // Cache for the input row we are currently working on
  InputAqlItemRow _input{CreateInvalidInputRowHint{}};

  // Internal state
  State _internalState{State::READ_DATA_ROW};
};
}  // namespace aql
}  // namespace arangodb
#endif
