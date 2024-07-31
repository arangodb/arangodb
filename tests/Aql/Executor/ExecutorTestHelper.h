////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "gtest/gtest.h"

#include "Aql/AqlItemBlockHelper.h"
#include "Aql/ExecutionBlockPipeline.h"
#include "Aql/ExecutionNode/MockTypedNode.h"
#include "Aql/WaitingExecutionBlockMock.h"
#include "Mocks/Servers.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlCallStack.h"
#include "Aql/BlockCollector.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/SingletonNode.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutionStats.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Aql/SharedQueryState.h"
#include "Containers/Enumerate.h"

#include <numeric>
#include <tuple>

namespace arangodb::tests::aql {
/**
 * @brief Static helper class just offers helper methods
 * Do never instantiate
 *
 */
class asserthelper {
 private:
  asserthelper() {}

 public:
  static auto AqlValuesAreIdentical(AqlValue const& lhs, AqlValue const& rhs)
      -> bool;

  static auto RowsAreIdentical(SharedAqlItemBlockPtr actual, size_t actualRow,
                               SharedAqlItemBlockPtr expected,
                               size_t expectedRow,
                               std::optional<std::vector<RegisterId>> const&
                                   onlyCompareRegisters = std::nullopt) -> bool;

  static auto ValidateAqlValuesAreEqual(SharedAqlItemBlockPtr actual,
                                        size_t actualRow,
                                        RegisterId actualRegister,
                                        SharedAqlItemBlockPtr expected,
                                        size_t expectedRow,
                                        RegisterId expectedRegister) -> void;

  static auto ValidateBlocksAreEqual(
      SharedAqlItemBlockPtr actual, SharedAqlItemBlockPtr expected,
      std::optional<std::vector<RegisterId>> const& onlyCompareRegisters =
          std::nullopt) -> void;

  static auto ValidateBlocksAreEqualUnordered(
      SharedAqlItemBlockPtr actual, SharedAqlItemBlockPtr expected,
      std::size_t numRowsNotContained = 0,
      std::optional<std::vector<RegisterId>> const& onlyCompareRegisters =
          std::nullopt) -> void;

  static auto ValidateBlocksAreEqualUnordered(
      SharedAqlItemBlockPtr actual, SharedAqlItemBlockPtr expected,
      std::unordered_set<size_t>& matchedRows,
      std::size_t numRowsNotContained = 0,
      std::optional<std::vector<RegisterId>> const& onlyCompareRegisters =
          std::nullopt) -> void;
};

using SplitType =
    std::variant<std::vector<std::size_t>, std::size_t, std::monostate>;

auto inline to_string(SplitType const& splitType) -> std::string {
  using namespace std::string_literals;
  return std::visit(overload{
                        [](std::vector<std::size_t> list) {
                          return fmt::format("list{{{}}}",
                                             fmt::join(list, ","));
                        },
                        [](std::size_t interval) {
                          return fmt::format("interval{{{}}}", interval);
                        },
                        [](std::monostate) { return "none"s; },
                    },
                    splitType);
}

template<std::size_t inputColumns = 1, std::size_t outputColumns = 1>
struct ExecutorTestHelper {
  ExecutorTestHelper(ExecutorTestHelper const&) = delete;
  ExecutorTestHelper(ExecutorTestHelper&&) = delete;
  explicit ExecutorTestHelper(Query& query,
                              AqlItemBlockManager& itemBlockManager)
      : _expectedSkip{},
        _expectedState{ExecutionState::HASMORE},
        _testStats{false},
        _unorderedOutput{false},
        _appendEmptyBlock{false},
        _unorderedSkippedRows{0},
        _query(query),
        _itemBlockManager(itemBlockManager),
        _dummyNode{std::make_unique<SingletonNode>(
            const_cast<ExecutionPlan*>(
                _query.rootEngine()->root()->getPlanNode()->plan()),
            ExecutionNodeId{42})} {}

  auto setCallStack(AqlCallStack stack) -> ExecutorTestHelper& {
    _callStack = stack;
    return *this;
  }

  auto setCall(AqlCall c) -> ExecutorTestHelper& {
    _callStack = AqlCallStack{AqlCallList{c}};
    return *this;
  }

  auto setInputValue(MatrixBuilder<inputColumns> in,
                     std::vector<std::pair<size_t, uint64_t>> shadowRows = {})
      -> ExecutorTestHelper& {
    _input = std::move(in);
    _inputShadowRows = std::move(shadowRows);
    return *this;
  }

  template<typename... Ts>
  auto setInputValueList(Ts&&... ts) -> ExecutorTestHelper& {
    _input = MatrixBuilder<inputColumns>{{ts}...};
    _inputShadowRows = {};
    return *this;
  }

  auto setInputFromRowNum(size_t rows) -> ExecutorTestHelper& {
    static_assert(inputColumns == 1);
    _input.clear();
    for (auto i = size_t{0}; i < rows; ++i) {
      _input.emplace_back(RowBuilder<1>{static_cast<int>(i)});
    }
    _inputShadowRows = {};
    return *this;
  }

  auto setInputSplit(std::vector<std::size_t> list) -> ExecutorTestHelper& {
    _inputSplit = std::move(list);
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

  template<typename T>
  auto setOutputSplit(T&& list) -> ExecutorTestHelper& {
    ASSERT_FALSE(true);
    _outputSplit = std::forward<T>(list);
    return *this;
  }

  auto setTesteeNodeType(ExecutionNode::NodeType nodeType)
      -> ExecutorTestHelper& {
    _testeeNodeType = nodeType;
    return *this;
  }

  auto setInputSubqueryDepth(std::size_t depth) -> ExecutorTestHelper& {
    _inputSubqueryDepth = depth;
    return *this;
  }

  auto setWaitingBehaviour(
      WaitingExecutionBlockMock::WaitingBehaviour waitingBehaviour)
      -> ExecutorTestHelper& {
    _waitingBehaviour = waitingBehaviour;
    return *this;
  }

  auto setWakeupCallback(
      WaitingExecutionBlockMock::WakeupCallback wakeupCallback)
      -> ExecutorTestHelper& {
    _wakeupCallback = std::move(wakeupCallback);
    return *this;
  }

  auto setExecuteCallback(
      WaitingExecutionBlockMock::ExecuteCallback executeCallback)
      -> ExecutorTestHelper& {
    _executeCallback = std::move(executeCallback);
    return *this;
  }

  auto expectOutput(
      std::array<RegisterId, outputColumns> const& regs,
      MatrixBuilder<outputColumns> const& out,
      std::vector<std::pair<size_t, uint64_t>> const& shadowRows = {})
      -> ExecutorTestHelper& {
    _outputRegisters = regs;
    _output = out;
    _outputShadowRows = shadowRows;
    return *this;
  }

  template<typename... Ts>
  auto expectOutputValueList(Ts&&... ts) -> ExecutorTestHelper& {
    static_assert(outputColumns == 1);
    _outputRegisters[0] = 1;
    _output = MatrixBuilder<outputColumns>{{ts}...};
    return *this;
  }

  /**
   * @brief
   *
   * @tparam Ts numeric type, can actually only be size_t
   * @param skipOnLevel List of skip counters returned per level. subquery skips
   * first, the last entry is the skip on the executor
   * @return ExecutorTestHelper& chaining!
   */
  template<typename T, typename... Ts>
  auto expectSkipped(T skipFirst, Ts const... skipOnHigherLevel)
      -> ExecutorTestHelper& {
    _expectedSkip = SkipResult{};
    // This is obvious, proof: Homework.
    (_expectedSkip.didSkip(static_cast<size_t>(skipFirst)), ...,
     (_expectedSkip.incrementSubquery(),
      _expectedSkip.didSkip(static_cast<size_t>(skipOnHigherLevel))));

    // NOTE: the above will increment didSkip by the first entry.
    // For all following entries it will first increment the subquery depth
    // and then add the didSkip on them.
    return *this;
  }

  auto expectSkipped(SkipResult expectedSkip) -> ExecutorTestHelper& {
    _expectedSkip = std::move(expectedSkip);
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

  auto allowAnyOutputOrder(bool expected, size_t skippedRows = 0)
      -> ExecutorTestHelper& {
    _unorderedOutput = expected;
    _unorderedSkippedRows = skippedRows;
    return *this;
  }

  /**
   * @brief Add a dependency, i.e. add an ExecutionBlock to the *end* of the
   * execution pipeline
   *
   * @tparam E The executor template parameter
   * @param executorInfos to build the executor
   * @param nodeType The type of executor node, only used for debug printing,
   * defaults to SINGLETON
   * @return ExecutorTestHelper&
   */
  template<typename E>
  auto addDependency(
      RegisterInfos registerInfos, typename E::Infos executorInfos,
      ExecutionNode::NodeType nodeType = ExecutionNode::SINGLETON)
      -> ExecutorTestHelper& {
    _pipeline.addDependency(createExecBlock<E>(
        std::move(registerInfos), std::move(executorInfos), nodeType));
    return *this;
  }

  /**
   * @brief Add a consumer, i.e. add an ExecutionBlock to the *beginning* of the
   * execution pipeline
   *
   * @tparam E The executor template parameter
   * @param executorInfos to build the executor
   * @param nodeType The type of executor node, only used for debug printing,
   * defaults to SINGLETON
   * @return ExecutorTestHelper&
   */
  template<typename E>
  auto addConsumer(RegisterInfos registerInfos, typename E::Infos executorInfos,
                   ExecutionNode::NodeType nodeType = ExecutionNode::SINGLETON)
      -> ExecutorTestHelper& {
    _pipeline.addConsumer(createExecBlock<E>(
        std::move(registerInfos), std::move(executorInfos), nodeType));
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

  auto prepareInput() -> ExecutorTestHelper& {
    auto inputBlock = generateInputRanges(_itemBlockManager);

    TRI_ASSERT(!_pipeline.empty());

    _pipeline.addDependency(std::move(inputBlock));

    return *this;
  }

  auto executeOnlyOnce() -> ExecutorTestHelper& {
    auto const [state, skipped, result] =
        _pipeline.get().front()->execute(_callStack);
    _finalState = state;
    _skippedTotal.merge(skipped, false);
    if (result != nullptr) {
      _allResults.add(result);
    }

    return *this;
  }

  auto executeOnce() -> ExecutorTestHelper& {
    auto const [state, skipped, result] =
        _pipeline.get().front()->execute(_callStack);
    _finalState = state;
    auto& call = _callStack.modifyTopCall();
    _skippedTotal.merge(skipped, false);
    call.didSkip(skipped.getSkipCount());
    if (result != nullptr) {
      call.didProduce(result->numRows());
      _allResults.add(result);
    }
    call.resetSkipCount();

    return *this;
  }

  auto checkExpectations() -> ExecutorTestHelper& {
    EXPECT_EQ(_skippedTotal, _expectedSkip);
    EXPECT_EQ(_finalState, _expectedState);
    SharedAqlItemBlockPtr result = _allResults.steal();
    if (result == nullptr) {
      // Empty output, possible if we skip all
      EXPECT_EQ(_output.size(), 0)
          << "Executor does not yield output, although it is expected";
    } else {
      SharedAqlItemBlockPtr expectedOutputBlock = buildBlock<outputColumns>(
          _itemBlockManager, std::move(_output), _outputShadowRows);
      std::vector<RegisterId> outRegVector(_outputRegisters.begin(),
                                           _outputRegisters.end());
      if (_unorderedOutput) {
        asserthelper::ValidateBlocksAreEqualUnordered(
            result, expectedOutputBlock, _unorderedSkippedRows, outRegVector);
      } else {
        asserthelper::ValidateBlocksAreEqual(result, expectedOutputBlock,
                                             outRegVector);
      }
    }

    if (_testStats) {
      ExecutionStats actualStats;
      _query.rootEngine()->collectExecutionStats(actualStats);
      // simon: engine does not collect most block stats
      for (auto const& block : _pipeline.get()) {
        block->collectExecStats(actualStats);
      }
      EXPECT_EQ(actualStats, _expectedStats);
    }

    return *this;
  }

  auto run(bool const loop = false) -> void {
    prepareInput();

    if (!loop) {
      executeOnlyOnce();
    } else {
      do {
        executeOnce();
      } while (
          _finalState != ExecutionState::DONE &&
          (!_callStack.peek().hasSoftLimit() ||
           (_callStack.peek().getLimit() + _callStack.peek().getOffset()) > 0));
    }
    checkExpectations();
  };

  auto query() -> Query& { return _query; }
  auto query() const -> Query const& { return _query; }
  auto engine() const -> ExecutionEngine const* { return query().rootEngine(); }
  auto sharedState() const -> std::shared_ptr<SharedQueryState> const& {
    return engine()->sharedState();
  }
  template<typename F>
  auto setWakeupHandler(F&& func) requires std::is_invocable_r_v<bool, F> {
    return sharedState()->setWakeupHandler(std::forward<F>(func));
  }

  auto pipeline() -> Pipeline& { return _pipeline; }

 private:
  /**
   * @brief create an ExecutionBlock without tying it into the pipeline.
   *
   * @tparam E The executor template parameter
   * @param executorInfos to build the executor
   * @param nodeType The type of executor node, only used for debug printing,
   * defaults to SINGLETON
   * @return ExecBlock
   *
   * Now private to prevent us from leaking memory
   */
  template<typename E>
  auto createExecBlock(
      RegisterInfos registerInfos, typename E::Infos executorInfos,
      ExecutionNode::NodeType nodeType = ExecutionNode::SINGLETON)
      -> ExecBlock {
    auto& testeeNode = _execNodes.emplace_back(std::make_unique<MockTypedNode>(
        _query.plan(), ExecutionNodeId{_execNodes.size()}, nodeType));
    return std::make_unique<ExecutionBlockImpl<E>>(
        _query.rootEngine(), testeeNode.get(), std::move(registerInfos),
        std::move(executorInfos));
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

    auto sit = _inputShadowRows.begin();
    auto const send = _inputShadowRows.end();

    auto baseRowIndex = std::size_t{0};

    auto const buildAndEnqueueBlock =
        [&itemBlockManager, &blockDeque, &sit, &send, &baseRowIndex](
            MatrixBuilder<inputColumns> matrix, std::size_t currentRowIndex) {
          SharedAqlItemBlockPtr inputBlock =
              buildBlock<inputColumns>(itemBlockManager, std::move(matrix));
          // inputBlock contains the _input slice [baseRowIndex,
          // currentRowIndex] (inclusive). Set shadow rows where necessary:
          TRI_ASSERT(sit == send or baseRowIndex <= sit->first);
          for (; sit != send && sit->first <= currentRowIndex; ++sit) {
            auto const [sidx, sdepth] = *sit;
            inputBlock->makeShadowRow(sidx - baseRowIndex, sdepth);
          }
          blockDeque.emplace_back(std::move(inputBlock));
          matrix.clear();
          // set base index for the next block (if any)
          baseRowIndex = currentRowIndex + 1;
        };

    for (auto const& [currentRowIndex, value] : enumerate(_input)) {
      matrix.push_back(value);

      TRI_ASSERT(!_inputSplit.valueless_by_exception());

      bool openNewBlock = std::visit(
          overload{[&](VectorSizeT& list) {
                     if (*iter != *end && matrix.size() == **iter) {
                       iter->operator++();
                       return true;
                     }

                     return false;
                   },
                   [&](std::size_t size) { return matrix.size() == size; },
                   [](std::monostate) { return false; }},
          _inputSplit);
      if (openNewBlock) {
        buildAndEnqueueBlock(std::move(matrix), currentRowIndex);
      }
    }

    if (!matrix.empty()) {
      buildAndEnqueueBlock(std::move(matrix), _input.size());
    }
    if (_appendEmptyBlock) {
      blockDeque.emplace_back(nullptr);
    }

    return std::make_unique<WaitingExecutionBlockMock>(
        _query.rootEngine(), _dummyNode.get(), std::move(blockDeque),
        _waitingBehaviour, _inputSubqueryDepth, _wakeupCallback,
        _executeCallback);
  }

  // Default initialize with a fetchAll call.
  AqlCallStack _callStack{AqlCallList{AqlCall{}}};
  MatrixBuilder<inputColumns> _input;
  MatrixBuilder<outputColumns> _output;
  std::vector<std::pair<size_t, uint64_t>> _inputShadowRows{};
  std::vector<std::pair<size_t, uint64_t>> _outputShadowRows{};
  std::array<RegisterId, outputColumns> _outputRegisters;
  SkipResult _expectedSkip;
  ExecutionState _expectedState;
  ExecutionStats _expectedStats;
  bool _testStats;
  ExecutionNode::NodeType _testeeNodeType{ExecutionNode::MAX_NODE_TYPE_VALUE};
  WaitingExecutionBlockMock::WaitingBehaviour _waitingBehaviour =
      WaitingExecutionBlockMock::NEVER;
  WaitingExecutionBlockMock::WakeupCallback _wakeupCallback{};
  WaitingExecutionBlockMock::ExecuteCallback _executeCallback{};
  bool _unorderedOutput;
  bool _appendEmptyBlock;
  std::size_t _unorderedSkippedRows;
  std::size_t _inputSubqueryDepth = 0;

  SplitType _inputSplit = {std::monostate()};
  SplitType _outputSplit = {std::monostate()};

  arangodb::aql::Query& _query;
  arangodb::aql::AqlItemBlockManager& _itemBlockManager;
  std::unique_ptr<arangodb::aql::ExecutionNode> _dummyNode;
  Pipeline _pipeline;
  std::vector<std::unique_ptr<MockTypedNode>> _execNodes;

  // results
  ExecutionState _finalState = ExecutionState::HASMORE;
  SkipResult _skippedTotal{};
  BlockCollector _allResults{&_itemBlockManager};
};

}  // namespace arangodb::tests::aql
