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

enum class ExecutorCall {
  SKIP_ROWS,
  PRODUCE_ROWS,
  FETCH_FOR_PASSTHROUGH,
  EXPECTED_NR_ROWS,
};

std::ostream& operator<<(std::ostream& stream, ExecutorCall call);

using ExecutorStepResult = std::tuple<ExecutorCall, arangodb::aql::ExecutionState, size_t>;

// TODO Add skipRows by passing 3 additional integers i, j, k, saying we should
//  - skip i rows
//  - produce j rows
//  - skip k rows
// TODO Make the calls to skipRows, fetchBlockForPassthrough and (later) expectedNumberOfRows
//  somehow optional. e.g. call a templated function or so.
// TODO Add calls to expectedNumberOfRows

template <class Executor>
std::tuple<arangodb::aql::SharedAqlItemBlockPtr, std::vector<ExecutorStepResult>, arangodb::aql::ExecutionStats>
runExecutor(arangodb::aql::AqlItemBlockManager& manager, Executor& executor,
            arangodb::aql::OutputAqlItemRow& outputRow) {
  using namespace arangodb::aql;
  ExecutionState state = ExecutionState::HASMORE;
  std::vector<ExecutorStepResult> results{};
  ExecutionStats stats{};

  uint64_t rowsLeft = 0;

  while (state != ExecutionState::DONE) {
    if (rowsLeft == 0) {
      ExecutionState fetchBlockState;
      typename Executor::Stats executorStats{};
      SharedAqlItemBlockPtr block{};
      // TODO: Don't do this at all for non-passThrough blocks
      // TODO: Should these results be returned as well?
      std::tie(fetchBlockState, executorStats, block) =
          executor.fetchBlockForPassthrough(1000);
      size_t const blockSize = block != nullptr ? block->size() : 0;
      results.emplace_back(std::make_tuple(ExecutorCall::FETCH_FOR_PASSTHROUGH, fetchBlockState, blockSize));
      stats += executorStats;
      rowsLeft = blockSize;
      if (fetchBlockState == ExecutionState::WAITING) {
        continue;
      }
      if (block == nullptr) {
        EXPECT_EQ(ExecutionState::DONE, fetchBlockState);
        break;
      }
    }

    EXPECT_GT(rowsLeft, 0);
    typename Executor::Stats executorStats{};
    size_t const rowsBefore = outputRow.numRowsWritten();
    std::tie(state, executorStats) = executor.produceRows(outputRow);
    size_t const rowsAfter = outputRow.numRowsWritten();
    size_t const rowsProduced = rowsAfter-rowsBefore;
    results.emplace_back(std::make_tuple(ExecutorCall::PRODUCE_ROWS, state, rowsProduced));
    stats += executorStats;
    EXPECT_LE(rowsProduced, rowsLeft);
    rowsLeft -= rowsProduced;

    if (outputRow.produced()) {
      outputRow.advanceRow();
    }
  }

  return {outputRow.stealBlock(), results, stats};
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb

#endif  // TESTS_AQL_EXECUTORTESTHELPER_H
