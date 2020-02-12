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
      : _expectedSkip{0},
        _expectedState{ExecutionState::HASMORE},
        _testStats{false},
        _query(query),
        _dummyNode{std::make_unique<SingletonNode>(_query.plan(), 42)} {}

  auto setCall(AqlCall c) -> ExecutorTestHelper& {
    _call = c;
    return *this;
  }

  auto setInputValue(MatrixBuilder<inputColumns> in) -> ExecutorTestHelper& {
    _input = std::move(in);
    return *this;
  }

  template <typename... Ts>
  auto setInputValueList(Ts&&... ts) -> ExecutorTestHelper& {
    _input = MatrixBuilder<inputColumns>{{ts}...};
    return *this;
  }

  auto setInputFromRowNum(size_t rows) -> ExecutorTestHelper& {
    static_assert(inputColumns == 1);
    _input.clear();
    for (auto i = size_t{0}; i < rows; ++i) {
      _input.emplace_back(RowBuilder<1>{i});
    }
    return *this;
  }

  auto setInputSplit(std::vector<std::size_t> const& list) -> ExecutorTestHelper& {
    _inputSplit = list;
    return *this;
  }

  auto setInputSplitStep(std::size_t step) -> ExecutorTestHelper& {
    _inputSplit = step;
    return *this;
  }

  auto setInputSplitType(SplitType split) -> ExecutorTestHelper& {
    _inputSplit = split;
    return *this;
  }

  template <typename T>
  auto setOutputSplit(T&& list) -> ExecutorTestHelper& {
    ASSERT_FALSE(true);
    _outputSplit = std::forward<T>(list);
    return *this;
  }

  auto expectOutput(std::array<std::size_t, outputColumns> const& regs,
                    MatrixBuilder<outputColumns> const& out) -> ExecutorTestHelper& {
    _outputRegisters = regs;
    _output = out;
    return *this;
  }

  template <typename... Ts>
  auto expectOutputValueList(Ts&&... ts) -> ExecutorTestHelper& {
    static_assert(outputColumns == 1);
    _outputRegisters[0] = 1;
    _output = MatrixBuilder<outputColumns>{{ts}...};
    return *this;
  }

  auto expectSkipped(std::size_t skip) -> ExecutorTestHelper& {
    _expectedSkip = skip;
    return *this;
  }

  auto expectedState(ExecutionState state) -> ExecutorTestHelper& {
    _expectedState = state;
    return *this;
  }

  auto expectedStats(ExecutionStats stats) -> ExecutorTestHelper& {
    _expectedStats = stats;
    _testStats = true;
    return *this;
  };

  auto run(typename E::Infos infos) -> void {
    ResourceMonitor monitor;
    AqlItemBlockManager itemBlockManager(&monitor, SerializationFormat::SHADOWROWS);

    auto inputBlock = generateInputRanges(itemBlockManager);

    auto testeeNode = std::make_unique<SingletonNode>(_query.plan(), 1);

    ExecutionBlockImpl<E> testee{_query.engine(), testeeNode.get(), std::move(infos)};
    testee.addDependency(inputBlock.get());

    AqlCallStack stack{_call};
    auto const [state, skipped, result] = testee.execute(stack);
    EXPECT_EQ(skipped, _expectedSkip);

    EXPECT_EQ(state, _expectedState);

    SharedAqlItemBlockPtr expectedOutputBlock =
        buildBlock<outputColumns>(itemBlockManager, std::move(_output));
    testOutputBlock(result, expectedOutputBlock);
    if (_testStats) {
      auto actualStats = _query.engine()->getStats();
      EXPECT_EQ(actualStats, _expectedStats);
    }
  };

 private:
  void testOutputBlock(SharedAqlItemBlockPtr const& outputBlock,
                       SharedAqlItemBlockPtr const& expectedOutputBlock) {
    velocypack::Options vpackOptions;

    EXPECT_EQ(outputBlock == nullptr, expectedOutputBlock == nullptr);
    if (expectedOutputBlock == nullptr) {
      return;
    }
    EXPECT_EQ(outputBlock->size(), expectedOutputBlock->size());
    for (size_t i = 0; i < outputBlock->size(); i++) {
      for (size_t j = 0; j < outputColumns; j++) {
        AqlValue const& x = outputBlock->getValueReference(i, _outputRegisters[j]);
        AqlValue const& y = expectedOutputBlock->getValueReference(i, j);

        EXPECT_TRUE(AqlValue::Compare(&vpackOptions, x, y, true) == 0)
            << "Row " << i << " Column " << j << " (Reg " << _outputRegisters[j]
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

    if (std::holds_alternative<VectorSizeT>(_inputSplit)) {
      iter = std::get<VectorSizeT>(_inputSplit).begin();
      end = std::get<VectorSizeT>(_inputSplit).end();
    }

    for (auto const& value : _input) {
      matrix.push_back(value);

      TRI_ASSERT(!_inputSplit.valueless_by_exception());
      TRI_ASSERT(!std::holds_alternative<std::monostate>(_inputSplit));

      bool openNewBlock =
          std::visit(overload{[&](VectorSizeT& list) {
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
                     _inputSplit);
      if (openNewBlock) {
        SharedAqlItemBlockPtr inputBlock =
            buildBlock<inputColumns>(itemBlockManager, std::move(matrix));
        blockDeque.emplace_back(inputBlock);
        matrix.clear();
      }
    }

    if (!matrix.empty()) {
      SharedAqlItemBlockPtr inputBlock =
          buildBlock<inputColumns>(itemBlockManager, std::move(matrix));
      blockDeque.emplace_back(inputBlock);
    }

    return std::make_unique<WaitingExecutionBlockMock>(
        _query.engine(), _dummyNode.get(), std::move(blockDeque),
        WaitingExecutionBlockMock::WaitingBehaviour::NEVER);
  }

  AqlCall _call;
  MatrixBuilder<inputColumns> _input;
  MatrixBuilder<outputColumns> _output;
  std::array<std::size_t, outputColumns> _outputRegisters;
  size_t _expectedSkip;
  ExecutionState _expectedState;
  ExecutionStats _expectedStats;
  bool _testStats;

  SplitType _inputSplit = {std::monostate()};
  SplitType _outputSplit = {std::monostate()};

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
