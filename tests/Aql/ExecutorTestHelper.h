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

#ifndef TESTS_AQL_EXECUTORTESTHELPER_H
#define TESTS_AQL_EXECUTORTESTHELPER_H

#include "gtest/gtest.h"

#include "Aql/ExecutionState.h"
#include "Aql/ExecutionStats.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SharedAqlItemBlockPtr.h"

#include <tuple>

namespace arangodb {
namespace tests {
namespace aql {

template <class Executor>
std::tuple<arangodb::aql::SharedAqlItemBlockPtr, std::vector<arangodb::aql::ExecutionState>, arangodb::aql::ExecutionStats>
runExecutor(arangodb::aql::AqlItemBlockManager& manager, Executor& executor,
            arangodb::aql::OutputAqlItemRow& outputRow) {
  using namespace arangodb::aql;
  ExecutionState state = ExecutionState::HASMORE;
  std::vector<ExecutionState> states{};
  ExecutionStats stats{};

  // TODO rowsLeft assumes that Executor reads exactly one input row per written
  //  output row. So this only works for passThrough, and the assumption should
  //  be checked & asserted separately.
  size_t rowsLeft = 0;

  while (state != ExecutionState::DONE) {
    if (rowsLeft == 0) {
      ExecutionState fetchBlockState;
      typename Executor::Stats executorStats{};
      SharedAqlItemBlockPtr block{};
      // TODO: Don't do this at all for non-passThrough blocks
      // TODO: Should these states be returned as well?
      std::tie(fetchBlockState, executorStats, block) =
          executor.fetchBlockForPassthrough(1000);
      stats += executorStats;
      rowsLeft += block != nullptr ? block->size() : 0;
      if (fetchBlockState == ExecutionState::WAITING) {
        continue;
      }
    }

    EXPECT_GT(rowsLeft, 0);
    typename Executor::Stats executorStats{};
    std::tie(state, executorStats) = executor.produceRows(outputRow);
    states.emplace_back(state);
    stats += executorStats;

    if (outputRow.produced()) {
      outputRow.advanceRow();
      rowsLeft--;
    }
  }

  return {outputRow.stealBlock(), states, stats};
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb

#endif  // TESTS_AQL_EXECUTORTESTHELPER_H
