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

#include "AqlItemBlockHelper.h"
#include "WaitingExecutionBlockMock.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlCallStack.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutionStats.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SharedAqlItemBlockPtr.h"

#include <tuple>

namespace arangodb {
namespace tests {
namespace aql {

template <typename E, std::size_t inputColumns = 1, std::size_t outputColumns = 1>
struct ExecutorTestHelper {
  using SplitType = std::variant<std::vector<std::size_t>, std::size_t, std::monostate>;

  ExecutorTestHelper(ExecutorTestHelper const&) = delete;
  ExecutorTestHelper(ExecutorTestHelper&&) = delete;
  explicit ExecutorTestHelper(arangodb::aql::Query& query)
      : _query(query), _dummyNode{std::make_unique<SingletonNode>(_query.plan(), 42)} {}

  auto setCall(AqlCall c) -> ExecutorTestHelper& {
    call = c;
    return *this;
  }

  auto setInputValue(MatrixBuilder<inputColumns> in) -> ExecutorTestHelper& {
    input = std::move(in);
    return *this;
  }

  template <typename... Ts>
  auto setInputValueList(Ts&&... ts) -> ExecutorTestHelper& {
    input = MatrixBuilder<inputColumns>{{ts}...};
    return *this;
  }

  auto setInputSplit(std::vector<std::size_t> const& list) -> ExecutorTestHelper& {
    inputSplit = list;
    return *this;
  }

  auto setInputSplitStep(std::size_t step) -> ExecutorTestHelper& {
    inputSplit = step;
    return *this;
  }

  template <typename T>
  auto setOutputSplit(T&& list) -> ExecutorTestHelper& {
    ASSERT_FALSE(true);
    outputSplit = std::forward<T>(list);
    return *this;
  }

  auto expectOutput(std::array<std::size_t, outputColumns> const& regs,
                    MatrixBuilder<outputColumns> const& out) -> ExecutorTestHelper& {
    outputRegisters = regs;
    output = out;
    return *this;
  }

  template <typename... Ts>
  auto expectOutputValueList(Ts&&... ts) -> ExecutorTestHelper& {
    static_assert(outputColumns == 1);
    outputRegisters[0] = 1;
    output = MatrixBuilder<outputColumns>{{ts}...};
    return *this;
  }

  auto expectFullCount(std::size_t count) -> ExecutorTestHelper& {
    fullCount = count;
    return *this;
  }

  auto run(typename E::Infos infos) -> void {
    ResourceMonitor monitor;
    AqlItemBlockManager itemBlockManager(&monitor, SerializationFormat::SHADOWROWS);

    auto inputBlock = generateInputRanges(itemBlockManager);

    auto testeeNode = std::make_unique<SingletonNode>(_query.plan(), 1);

    ExecutionBlockImpl<E> testee{_query.engine(), testeeNode.get(), std::move(infos)};
    testee.addDependency(inputBlock.get());

    AqlCallStack stack{call};
    auto const [state, skipped, result] = testee.execute(stack);

    SharedAqlItemBlockPtr expectedOutputBlock =
        buildBlock<outputColumns>(itemBlockManager, std::move(output));
    testOutputBlock(result, expectedOutputBlock);
  };

 private:
  void testOutputBlock(SharedAqlItemBlockPtr const& outputBlock,
                       SharedAqlItemBlockPtr const& expectedOutputBlock) {
    velocypack::Options vpackOptions;

    EXPECT_EQ(outputBlock->size(), expectedOutputBlock->size());
    for (size_t i = 0; i < outputBlock->size(); i++) {
      for (size_t j = 0; j < outputColumns; j++) {
        AqlValue const& x = outputBlock->getValueReference(i, outputRegisters[j]);
        AqlValue const& y = expectedOutputBlock->getValueReference(i, j);

        EXPECT_TRUE(AqlValue::Compare(&vpackOptions, x, y, true) == 0)
            << "Row " << i << " Column " << j << " (Reg " << outputRegisters[j]
            << ") do not agree";
      }
    }
  }

  auto generateInputRanges(AqlItemBlockManager& itemBlockManager)
      -> std::unique_ptr<ExecutionBlock> {
    using VectorSizeT = std::vector<std::size_t>;

    MatrixBuilder<inputColumns> matrix;

    std::deque<SharedAqlItemBlockPtr> blockDeque;

    std::optional<VectorSizeT::iterator> iter, end;

    if (std::holds_alternative<VectorSizeT>(inputSplit)) {
      iter = std::get<VectorSizeT>(inputSplit).begin();
      end = std::get<VectorSizeT>(inputSplit).end();
    }

    for (auto const& value : input) {
      matrix.push_back(value);

      bool openNewBlock =
          std::visit(overload{[&](std::vector<std::size_t>& list) {
                                if (*iter != *end && matrix.size() == **iter) {
                                  iter->operator++();
                                  return true;
                                }

                                return false;
                              },
                              [&](std::size_t size) {
                                return matrix.size() == size;
                              },
                              [](auto) { return false; }},
                     inputSplit);
      if (openNewBlock) {
        SharedAqlItemBlockPtr inputBlock =
            buildBlock<inputColumns>(itemBlockManager, std::move(matrix));
        blockDeque.emplace_back(inputBlock);
      }
    }

    SharedAqlItemBlockPtr inputBlock =
        buildBlock<inputColumns>(itemBlockManager, std::move(matrix));
    blockDeque.emplace_back(inputBlock);
    return std::make_unique<WaitingExecutionBlockMock>(
        _query.engine(), _dummyNode.get(), std::move(blockDeque),
        WaitingExecutionBlockMock::WaitingBehaviour::NEVER);
  }

  AqlCall call;
  MatrixBuilder<inputColumns> input;
  MatrixBuilder<outputColumns> output;
  std::array<std::size_t, outputColumns> outputRegisters;
  std::optional<std::size_t> fullCount;

  SplitType inputSplit = {std::monostate()};
  SplitType outputSplit = {std::monostate()};

  arangodb::aql::Query& _query;
  std::unique_ptr<arangodb::aql::ExecutionNode> _dummyNode;
};

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
            arangodb::aql::OutputAqlItemRow& outputRow, size_t const numSkip,
            size_t const numProduce, bool const skipRest) {
  using namespace arangodb::aql;
  ExecutionState state = ExecutionState::HASMORE;
  std::vector<ExecutorStepResult> results{};
  ExecutionStats stats{};

  uint64_t rowsLeft = 0;
  size_t skippedTotal = 0;
  size_t producedTotal = 0;

  enum class RunState {
    SKIP_OFFSET,
    FETCH_FOR_PASSTHROUGH,
    PRODUCE,
    SKIP_REST,
    BREAK
  };

  while (state != ExecutionState::DONE) {
    RunState const runState = [&]() {
      if (skippedTotal < numSkip) {
        return RunState::SKIP_OFFSET;
      }
      if (rowsLeft == 0 && (producedTotal < numProduce || numProduce == 0)) {
        return RunState::FETCH_FOR_PASSTHROUGH;
      }
      if (producedTotal < numProduce || !skipRest) {
        return RunState::PRODUCE;
      }
      if (skipRest) {
        return RunState::SKIP_REST;
      }
      return RunState::BREAK;
    }();

    switch (runState) {
      // Skip first
      // TODO don't do this for executors which don't have skipRows
      case RunState::SKIP_OFFSET: {
        size_t skipped;
        typename Executor::Stats executorStats{};
        std::tie(state, executorStats, skipped) = executor.skipRows(numSkip);
        results.emplace_back(std::make_tuple(ExecutorCall::SKIP_ROWS, state, skipped));
        stats += executorStats;
        skippedTotal += skipped;
      } break;
      // Get a new block for pass-through if we still need to produce rows and
      // the current (imagined, via rowsLeft) block is "empty".
      // TODO: Don't do this at all for non-passThrough blocks
      case RunState::FETCH_FOR_PASSTHROUGH: {
        ExecutionState fetchBlockState;
        typename Executor::Stats executorStats{};
        SharedAqlItemBlockPtr block{};
        std::tie(fetchBlockState, executorStats, block) =
            executor.fetchBlockForPassthrough(1000);
        size_t const blockSize = block != nullptr ? block->size() : 0;
        results.emplace_back(std::make_tuple(ExecutorCall::FETCH_FOR_PASSTHROUGH,
                                             fetchBlockState, blockSize));
        stats += executorStats;
        rowsLeft = blockSize;
        if (fetchBlockState != ExecutionState::WAITING &&
            fetchBlockState != ExecutionState::DONE) {
          EXPECT_GT(rowsLeft, 0);
        }
        if (fetchBlockState != ExecutionState::WAITING && block == nullptr) {
          EXPECT_EQ(ExecutionState::DONE, fetchBlockState);
          // Abort
          state = ExecutionState::DONE;
        }
      } break;
      // Produce rows
      case RunState::PRODUCE: {
        EXPECT_GT(rowsLeft, 0);
        typename Executor::Stats executorStats{};
        size_t const rowsBefore = outputRow.numRowsWritten();
        std::tie(state, executorStats) = executor.produceRows(outputRow);
        size_t const rowsAfter = outputRow.numRowsWritten();
        size_t const rowsProduced = rowsAfter - rowsBefore;
        results.emplace_back(std::make_tuple(ExecutorCall::PRODUCE_ROWS, state, rowsProduced));
        stats += executorStats;
        EXPECT_LE(rowsProduced, rowsLeft);
        rowsLeft -= rowsProduced;
        producedTotal += rowsProduced;

        if (outputRow.produced()) {
          outputRow.advanceRow();
        }
      } break;
      // TODO don't do this for executors which don't have skipRows
      case RunState::SKIP_REST: {
        size_t skipped;
        typename Executor::Stats executorStats{};
        std::tie(state, executorStats, skipped) =
            executor.skipRows(ExecutionBlock::SkipAllSize());
        results.emplace_back(std::make_tuple(ExecutorCall::SKIP_ROWS, state, skipped));
        stats += executorStats;
      } break;
      // We're done
      case RunState::BREAK: {
        state = ExecutionState::DONE;
      } break;
    }
  }

  return {outputRow.stealBlock(), results, stats};
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb

#endif  // TESTS_AQL_EXECUTORTESTHELPER_H
