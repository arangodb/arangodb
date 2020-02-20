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
#include "Mocks/Servers.h"
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
/**
 * @brief Static helper class just offers helper methods
 * Do never instantiate
 *
 */
class asserthelper {
 private:
  asserthelper() {}

 public:
  static auto AqlValuesAreIdentical(AqlValue const& lhs, AqlValue const& rhs) -> bool;

  static auto RowsAreIdentical(SharedAqlItemBlockPtr actual, size_t actualRow,
                               SharedAqlItemBlockPtr expected, size_t expectedRow,
                               std::optional<std::vector<RegisterId>> const& onlyCompareRegisters = std::nullopt)
      -> bool;

  static auto ValidateAqlValuesAreEqual(SharedAqlItemBlockPtr actual,
                                        size_t actualRow, RegisterId actualRegister,
                                        SharedAqlItemBlockPtr expected, size_t expectedRow,
                                        RegisterId expectedRegister) -> void;

  static auto ValidateBlocksAreEqual(
      SharedAqlItemBlockPtr actual, SharedAqlItemBlockPtr expected,
      std::optional<std::vector<RegisterId>> const& onlyCompareRegisters = std::nullopt)
      -> void;

  static auto ValidateBlocksAreEqualUnordered(
      SharedAqlItemBlockPtr actual, SharedAqlItemBlockPtr expected,
      std::size_t numRowsNotContained = 0,
      std::optional<std::vector<RegisterId>> const& onlyCompareRegisters = std::nullopt)
      -> void;

  static auto ValidateBlocksAreEqualUnordered(
      SharedAqlItemBlockPtr actual, SharedAqlItemBlockPtr expected,
      std::unordered_set<size_t>& matchedRows, std::size_t numRowsNotContained = 0,
      std::optional<std::vector<RegisterId>> const& onlyCompareRegisters = std::nullopt)
      -> void;
};

/**
 * @brief Base class for ExecutorTests in Aql.
 *        It will provide a test server, including
 *        an AqlQuery, as well as the ability to generate
 *        Dummy ExecutionNodes.
 *
 * @tparam enableQueryTrace Enable Aql Profile Trace logging
 */
template <bool enableQueryTrace = false>
class AqlExecutorTestCase : public ::testing::Test {
 public:
  // Creating a server instance costs a lot of time, so do it only once.
  // Note that newer version of gtest call these SetUpTestSuite/TearDownTestSuite
  static void SetUpTestCase() {
    _server = std::make_unique<mocks::MockAqlServer>();
  }

  static void TearDownTestCase() { _server.reset(); }

 protected:
  AqlExecutorTestCase();
  virtual ~AqlExecutorTestCase() = default;

  /**
   * @brief Creates and manages a ExecutionNode.
   *        These nodes can be used to create the Executors
   *        Caller does not need to manage the memory.
   *
   * @return ExecutionNode* Pointer to a dummy ExecutionNode. Memory is managed, do not delete.
   */
  auto generateNodeDummy() -> ExecutionNode*;

  auto manager() const -> AqlItemBlockManager&;

 private:
  static inline std::unique_ptr<mocks::MockAqlServer> _server;
  std::vector<std::unique_ptr<ExecutionNode>> _execNodes;

 protected:
  // available variables
  ResourceMonitor monitor{};
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};
  std::unique_ptr<arangodb::aql::Query> fakedQuery;
};

/**
 * @brief Shortcut handle for parameterized AqlExecutorTestCases with param
 *
 * @tparam T The Test Parameter used for gtest.
 * @tparam enableQueryTrace Enable Aql Profile Trace logging
 */
template <typename T, bool enableQueryTrace = false>
class AqlExecutorTestCaseWithParam : public AqlExecutorTestCase<enableQueryTrace>,
                                     public ::testing::WithParamInterface<T> {};

using ExecBlock = std::unique_ptr<ExecutionBlock>;

struct Pipeline {
  using PipelineStorage = std::deque<ExecBlock>;

  Pipeline() : _pipeline{} {};
  Pipeline(ExecBlock&& init) { _pipeline.emplace_back(std::move(init)); }
  Pipeline(std::deque<ExecBlock>&& init) : _pipeline(std::move(init)){};
  Pipeline(Pipeline& other) = delete;
  Pipeline(Pipeline&& other) : _pipeline(std::move(other._pipeline)){};

  Pipeline& operator=(Pipeline&& other) {
    _pipeline = std::move(other._pipeline);
    return *this;
  }

  ~Pipeline() {
    for (auto&& b : _pipeline) {
      b.release();
    }
  };

  bool empty() const { return _pipeline.empty(); }
  void reset() { _pipeline.clear(); }

  std::deque<ExecBlock> const& get() const { return _pipeline; };
  std::deque<ExecBlock>& get() { return _pipeline; };

 private:
  PipelineStorage _pipeline;
};

inline auto concatPipelines(Pipeline&& bottom, Pipeline&& top) -> Pipeline {
  if (!bottom.empty()) {
    bottom.get().back()->addDependency(top.get().begin()->get());
  }
  bottom.get().insert(std::end(bottom.get()), std::make_move_iterator(top.get().begin()),
                      std::make_move_iterator(top.get().end()));

  return std::move(bottom);
}

template <std::size_t inputColumns = 1, std::size_t outputColumns = 1>
struct ExecutorTestHelper {
  using SplitType = std::variant<std::vector<std::size_t>, std::size_t, std::monostate>;

  ExecutorTestHelper(ExecutorTestHelper const&) = delete;
  ExecutorTestHelper(ExecutorTestHelper&&) = delete;
  explicit ExecutorTestHelper(arangodb::aql::Query& query)
      : _expectedSkip{0},
        _expectedState{ExecutionState::HASMORE},
        _testStats{false},
        _unorderedOutput{false},
        _appendEmptyBlock{false},
        _unorderedSkippedRows{0},
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

  auto expectOutput(std::array<RegisterId, outputColumns> const& regs,
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
  }

  template <typename E>
  auto setExecBlock(typename E::Infos infos) -> ExecutorTestHelper& {
    // TODO: this unique_ptr goes out of scope and is free'd, but I think
    //       it is fine for NODE to be nullptr?
    auto& testeeNode = _execNodes.emplace_back(std::move(
        std::make_unique<SingletonNode>(_query.plan(), _execNodes.size())));
    _testee = std::make_unique<ExecutionBlockImpl<E>>(_query.engine(),
                                                      testeeNode.get(), std::move(infos));
    return *this;
  }

  template <typename E>
  auto createExecBlock(typename E::Infos infos) -> ExecBlock {
    // TODO: this unique_ptr goes out of scope and is free'd, but I think
    //       it is fine for NODE to be nullptr?
    auto& testeeNode = _execNodes.emplace_back(std::move(
        std::make_unique<SingletonNode>(_query.plan(), _execNodes.size())));
    return std::make_unique<ExecutionBlockImpl<E>>(_query.engine(), testeeNode.get(),
                                                   std::move(infos));
  }

  auto setPipeline(Pipeline&& pipeline) -> ExecutorTestHelper& {
    _pipeline = std::move(pipeline);
    return *this;
  }

  auto allowAnyOutputOrder(bool expected, size_t skippedRows = 0) -> ExecutorTestHelper& {
    _unorderedOutput = expected;
    _unorderedSkippedRows = skippedRows;
    return *this;
  }

  /**
   * @brief This appends an empty block after the input fully created.
   *        It simulates a situation where the Producer lies about the
   *        the last input with HASMORE, but it actually is not able
   *        to produce more.
   *
   * @param append If this should be enabled or not
   * @return ExecutorTestHelper& this for chaining
   */
  auto appendEmptyBlock(bool append) -> ExecutorTestHelper& {
    _appendEmptyBlock = append;
    return *this;
  }

  auto run() -> void {
    ResourceMonitor monitor;
    AqlItemBlockManager itemBlockManager(&monitor, SerializationFormat::SHADOWROWS);

    auto inputBlock = generateInputRanges(itemBlockManager);

      _testee->addDependency(inputBlock.get());

    AqlCallStack stack{_call};
    auto const [state, skipped, result] = _testee->execute(stack);
    EXPECT_EQ(skipped, _expectedSkip);

    EXPECT_EQ(state, _expectedState);
    if (result == nullptr) {
      // Empty output, possible if we skip all
      EXPECT_EQ(_output.size(), 0)
          << "Executor does not yield output, although it is expected";
    } else {
      SharedAqlItemBlockPtr expectedOutputBlock =
          buildBlock<outputColumns>(itemBlockManager, std::move(_output));
      std::vector<RegisterId> outRegVector(_outputRegisters.begin(),
                                           _outputRegisters.end());
      if (_unorderedOutput) {
        asserthelper::ValidateBlocksAreEqualUnordered(result, expectedOutputBlock,
                                                      _unorderedSkippedRows, outRegVector);
      } else {
        asserthelper::ValidateBlocksAreEqual(result, expectedOutputBlock, outRegVector);
      }
    }

    if (_testStats) {
      auto actualStats = _query.engine()->getStats();
      EXPECT_EQ(actualStats, _expectedStats);
    }
  };

  auto runPipeline() -> void {
    ResourceMonitor monitor;
    AqlItemBlockManager itemBlockManager(&monitor, SerializationFormat::SHADOWROWS);

    auto inputBlock = generateInputRanges(itemBlockManager);

    _pipeline.get().back()->addDependency(inputBlock.get());

    AqlCallStack stack{_call};
    auto const [state, skipped, result] = _pipeline.get().front()->execute(stack);
    EXPECT_EQ(skipped, _expectedSkip);

    EXPECT_EQ(state, _expectedState);
    if (result == nullptr) {
      // Empty output, possible if we skip all
      EXPECT_EQ(_output.size(), 0)
          << "Executor does not yield output, although it is expected";
    } else {
      SharedAqlItemBlockPtr expectedOutputBlock =
          buildBlock<outputColumns>(itemBlockManager, std::move(_output));
      std::vector<RegisterId> outRegVector(_outputRegisters.begin(),
                                           _outputRegisters.end());
      if (_unorderedOutput) {
        asserthelper::ValidateBlocksAreEqualUnordered(result, expectedOutputBlock,
                                                      _unorderedSkippedRows, outRegVector);
      } else {
        asserthelper::ValidateBlocksAreEqual(result, expectedOutputBlock, outRegVector);
      }
    }

    if (_testStats) {
      auto actualStats = _query.engine()->getStats();
      EXPECT_EQ(actualStats, _expectedStats);
    }
  };

 private:
  void testOutputBlock(SharedAqlItemBlockPtr const& outputBlock,
                       SharedAqlItemBlockPtr const& expectedOutputBlock) {
    velocypack::Options vpackOptions;

    if (expectedOutputBlock == nullptr) {
      EXPECT_EQ(outputBlock, nullptr);
    } else {
      EXPECT_NE(outputBlock, nullptr);
      EXPECT_NE(expectedOutputBlock, nullptr);
      EXPECT_EQ(outputBlock->size(), expectedOutputBlock->size());
      for (size_t i = 0; i < outputBlock->size(); i++) {
        for (size_t j = 0; j < outputColumns; j++) {
          AqlValue const& x = outputBlock->getValueReference(i, _outputRegisters[j]);
          AqlValue const& y = expectedOutputBlock->getValueReference(i, j);

          EXPECT_TRUE(AqlValue::Compare(&vpackOptions, x, y, true) == 0)
              << "Row " << i << " Column " << j << " (Reg "
              << _outputRegisters[j] << ") do not agree";
        }
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
    if (_appendEmptyBlock) {
      blockDeque.emplace_back(nullptr);
    }

    return std::make_unique<WaitingExecutionBlockMock>(
        _query.engine(), _dummyNode.get(), std::move(blockDeque),
        WaitingExecutionBlockMock::WaitingBehaviour::NEVER);
  }

  AqlCall _call;
  MatrixBuilder<inputColumns> _input;
  MatrixBuilder<outputColumns> _output;
  std::array<RegisterId, outputColumns> _outputRegisters;
  std::size_t _expectedSkip;
  ExecutionState _expectedState;
  ExecutionStats _expectedStats;
  bool _testStats;
  bool _unorderedOutput;
  bool _appendEmptyBlock;
  std::size_t _unorderedSkippedRows;

  SplitType _inputSplit = {std::monostate()};
  SplitType _outputSplit = {std::monostate()};

  arangodb::aql::Query& _query;
  std::unique_ptr<arangodb::aql::ExecutionNode> _dummyNode;
  ExecBlock _testee;
  Pipeline _pipeline;
  std::vector<std::unique_ptr<ExecutionNode>> _execNodes;
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

template <typename Executor>
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
