////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include "AqlExecutorTestCase.h"

#include "Aql/EmptyExecutorInfos.h"
#include "Aql/IdExecutor.h"
#include "Aql/ParallelUnsortedGatherExecutor.h"
#include "Aql/SortRegister.h"
#include "Aql/SortingGatherExecutor.h"
#include "Aql/UnsortedGatherExecutor.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::tests::aql {

enum class ExecutorType { UNSORTED, SORTING_HEAP, SORTING_MINELEMENT };

std::ostream& operator<<(std::ostream& out, ExecutorType type) {
  switch (type) {
    case ExecutorType::UNSORTED:
      return out << "UNSORTED";
    case ExecutorType::SORTING_HEAP:
      return out << "SORTING_HEAP";
    case ExecutorType::SORTING_MINELEMENT:
      return out << "SORTING_MINELEMENT";
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
}

auto dependencyNumber = testing::Values(1, 2, 3);
auto parallelism = testing::Values(GatherNode::Parallelism::Serial,
                                   GatherNode::Parallelism::Parallel);

std::ostream& operator<<(std::ostream& out, GatherNode::Parallelism type) {
  switch (type) {
    case GatherNode::Parallelism::Serial:
      return out << "Serial";
    case GatherNode::Parallelism::Parallel:
      return out << "Parallel";
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
}

auto combinations =
    ::testing::Combine(::testing::Values(ExecutorType::UNSORTED, ExecutorType::SORTING_HEAP,
                                         ExecutorType::SORTING_MINELEMENT),
                       dependencyNumber, parallelism);

using CommonParameter = std::tuple<ExecutorType, size_t, GatherNode::Parallelism>;

class DataBuilder {
 public:
  DataBuilder(AqlItemBlockManager& manager, bool storeShadowValue)
      : _manager(manager), _storeShadowValue(storeShadowValue) {
    _data.push_back({});
  }

  auto stealDeque() -> std::deque<SharedAqlItemBlockPtr> {
    if (_numRows > 0) {
      _workingCopy->shrink(_numRows);
      _queue.push_back(std::move(_workingCopy));
    }
    return std::move(_queue);
  }

  auto addValue(int64_t val) -> void {
    ensureBlock();
    _workingCopy->setValue(_numRows, 0, AqlValue{AqlValueHintInt{val}});
    auto [it, inserted] = _data.back().emplace(val);
    ASSERT_TRUE(inserted) << "Tried to insert same value twice";
    _numRows++;
  }

  auto addShadowRow(int64_t val, uint64_t depth) -> void {
    ensureBlock();
    if (_storeShadowValue) {
      _workingCopy->emplaceValue(_numRows, 0, AqlValue{AqlValueHintInt{val}});
    }
    _workingCopy->setShadowRowDepth(_numRows, AqlValue{AqlValueHintUInt{depth}});
    _subqueryData.emplace_back(std::make_pair(val, depth));
    if (depth == 0) {
      _data.push_back({});
    }
    _numRows++;
  }

  auto getData() const -> std::vector<std::unordered_set<int64_t>> const& {
    return _data;
  }

  auto getSubqueryData() const -> std::vector<std::pair<int64_t, uint64_t>> const& {
    return _subqueryData;
  }

 private:
  auto ensureBlock() -> void {
    if (_numRows == 1000) {
      _queue.emplace_back(std::move(_workingCopy));
      _numRows = 0;
    }
    if (_workingCopy == nullptr) {
      _workingCopy = _manager.requestBlock(ExecutionBlock::DefaultBatchSize, 1);
    }
  }

  SharedAqlItemBlockPtr _workingCopy{nullptr};
  size_t _numRows{0};
  std::vector<std::unordered_set<int64_t>> _data{};
  std::vector<std::pair<int64_t, uint64_t>> _subqueryData{};
  std::deque<SharedAqlItemBlockPtr> _queue{};
  AqlItemBlockManager& _manager;
  bool _storeShadowValue{false};
};

class ResultMaps {
 public:
  ResultMaps() {}

  auto mergeBuilder(DataBuilder const& builder) {
    auto const& subqueries = builder.getSubqueryData();
    if (_subqueryData.empty()) {
      _subqueryData = subqueries;
    } else {
      // All subqueries data has to be identical
      ASSERT_EQ(_subqueryData.size(), subqueries.size()) << "Test setup broken";
      for (size_t i = 0; i < _subqueryData.size(); ++i) {
        auto const& [val, sub] = subqueries[i];
        auto const& [expVal, expSub] = _subqueryData[i];
        ASSERT_EQ(val, expVal) << "Test Setup broken";
        ASSERT_EQ(sub, expSub) << "Test Setup broken";
      }
    }

    // Copy here in order to use merge below
    // For some reason merge is not implemeneted on const sets
    // but moves data from one set into the other
    auto data = builder.getData();
    if (!_subqueryData.empty()) {
      // We have a subquery, so the data container holds 1 element to much.
      ASSERT_TRUE(data.back().empty())
          << "Beyond the last shadowRow there is no data";
      data.pop_back();
    }
    if (_data.empty()) {
      _data = data;
      _dataProduced.resize(_data.size(), false);
    } else {
      // Data needs to be integrated.
      // We do not care from which branch the data is provided
      // number of subqueries needs to be identical though
      ASSERT_EQ(_data.size(), data.size()) << "Test setup broken";

      for (size_t i = 0; i < data.size(); ++i) {
        size_t expectedSize = _data[i].size() + data[i].size();
        _data[i].merge(data[i]);
        // We cannot have any value in both sets.
        ASSERT_EQ(_data[i].size(), expectedSize)
            << "Test setup broken, a value has been used in multiple "
               "branches";
      }
    }
  }

  auto testValueAllowed(int64_t val) -> void {
    ASSERT_LT(_dataReadIndex, _data.size());
    ASSERT_LT(_dataReadIndex, _dataProduced.size());
    auto& allowed = _data[_dataReadIndex];
    EXPECT_EQ(1, allowed.erase(val));
    _dataProduced[_dataReadIndex] = true;
  }

  auto testSubqueryValue(int64_t val, uint64_t depth) -> void {
    ASSERT_LT(_subqueryReadIndex, _subqueryData.size());
    auto const& [expVal, expDepth] = _subqueryData[_subqueryReadIndex];
    _subqueryReadIndex++;
    if (depth == 0) {
      // We consumed the ShadowRow for the data, let us check for the next set of data rows.
      _dataReadIndex++;
    }
    EXPECT_EQ(val, expVal);
    EXPECT_EQ(depth, expDepth);
  }

  auto testValuesSkippedInRun(size_t count, size_t index) -> void {
    ASSERT_LT(index, _data.size());
    EXPECT_EQ(_data[index].size(), count);
  }

  auto testSkippedInEachRun(size_t count) -> void {
    for (size_t i = 0; i < _data.size(); ++i) {
      testValuesSkippedInRun(count, i);
    }
  }

  auto testAllValuesProduced() -> void { testSkippedInEachRun(0); }

  auto testAllValuesProducedOfRun(size_t index) -> void {
    testValuesSkippedInRun(0, index);
  }

  auto testAllValuesSkippedInRun(size_t index) -> void {
    ASSERT_LT(index, _dataProduced.size());
    EXPECT_FALSE(_dataProduced[index]);
  }

  auto testAllValuesSkipped() -> void {
    for (size_t i = 0; i < _data.size(); ++i) {
      testAllValuesSkippedInRun(i);
    }
  }

 private:
  std::vector<std::unordered_set<int64_t>> _data{};
  std::vector<std::pair<int64_t, uint64_t>> _subqueryData{};
  std::vector<bool> _dataProduced{};
  size_t _dataReadIndex{0};
  size_t _subqueryReadIndex{0};
};

class CommonGatherExecutorTest
    : public AqlExecutorTestCaseWithParam<CommonParameter, false> {
 protected:
  CommonGatherExecutorTest() {}

  auto getExecutor(std::deque<size_t> subqueryRuns, size_t dataSize = 10)
      -> std::pair<std::unique_ptr<ExecutionBlock>, ResultMaps> {
    auto exec = buildExecutor();
    TRI_ASSERT(exec != nullptr);
    auto res = generateData(*exec, subqueryRuns, dataSize);
    return {std::move(exec), res};
  }

  auto assertResultValid(SharedAqlItemBlockPtr block, ResultMaps& result) -> void {
    if (block != nullptr) {
      for (size_t row = 0; row < block->size(); ++row) {
        if (block->isShadowRow(row)) {
          ShadowAqlItemRow in{block, row};
          auto val = in.getValue(0);
          ASSERT_TRUE(val.isNumber());
          int64_t v = val.toInt64();
          auto depthVal = in.getDepth();
          result.testSubqueryValue(v, depthVal);
        } else {
          InputAqlItemRow in{block, row};
          auto val = in.getValue(0);
          ASSERT_TRUE(val.isNumber());
          int64_t v = val.toInt64();
          result.testValueAllowed(v);
        }
      }
    }
  }

  auto toCallList(AqlCall const& call) -> AqlCallList {
    return AqlCallList{call};
  }

  auto fetchAllCall() -> AqlCallList { return toCallList(AqlCall{}); }

  auto skipThenFetchCall(size_t offset) -> AqlCallList {
    return toCallList(AqlCall{offset});
  }

 private:
  auto getType() -> ExecutorType {
    auto const& [type, clients, parallelism] = GetParam();
    return type;
  }

  auto getClients() -> size_t {
    auto const& [type, clients, parallelism] = GetParam();
    return clients;
  }

  auto parallelism() -> GatherNode::Parallelism {
    auto const& [type, clients, parallelism] = GetParam();
    return parallelism;
  }

  /**
   * @brief Generate the data values.
   *        Every entry in the vector is a seperate subquery run
   *        in the set every possible value is stored exactly once.
   *
   * @param block The block where we inject dependencies
   * @param subqueryRuns Number of shadowRows on every level (0 => mainquery)
   *                     For every outer run we will have all of the inner runs
   *                     e.g. {2, 5} will have 2 main query runs, each with 5 subquery runs.
   *                     Empty vector means no shadowRows
   * @return std::pair<std::vector<std::unordered_map<size_t>>, std::vector<size_t>>
   */
  auto generateData(ExecutionBlock& block, std::deque<size_t> subqueryRuns,
                    size_t dataSize) -> ResultMaps {
    // ResultMaps
    ResultMaps res;
    int64_t dataVal = 0;

    for (size_t i = 0; i < getClients(); ++i) {
      // The builder and Subquery value needs to be
      // per client, all of them get the same Subqueries.
      // Only the first block will write the value into the shadowRow though.
      // In AQL the shadowRow has been moved so only one branch is allowed to contain the data.
      DataBuilder builder{manager(), i == 0};
      uint64_t subVal = 0;
      fillBlockQueue(builder, dataVal, subVal, subqueryRuns, dataSize);

      res.mergeBuilder(builder);

      auto producer = std::make_unique<WaitingExecutionBlockMock>(
          fakedQuery->rootEngine(), generateNodeDummy(), builder.stealDeque(),
          WaitingExecutionBlockMock::WaitingBehaviour::NEVER, subqueryRuns.size());
      block.addDependency(producer.get());
      _blockLake.emplace_back(std::move(producer));
    }

    return res;
  }

  auto fillBlockQueue(DataBuilder& builder, int64_t& dataVal, uint64_t& subVal,
                      std::deque<size_t> subqueryRuns, size_t dataSize) -> void {
    if (subqueryRuns.empty()) {
      for (size_t i = 0; i < dataSize; ++i) {
        builder.addValue(dataVal++);
      }
    } else {
      size_t runs = subqueryRuns.front();
      subqueryRuns.pop_front();
      for (size_t i = 0; i < runs; ++i) {
        // Fill in data from inner subqueries
        fillBlockQueue(builder, dataVal, subVal, subqueryRuns, dataSize);
        // Fill in ShadowRow
        builder.addShadowRow(subVal++, subqueryRuns.size());
      }
    }
  }

  auto buildRegisterInfos() -> RegisterInfos {
    return RegisterInfos(RegIdSet{}, {}, 1, 1, {}, {RegIdSet{0}});
  }

  auto buildExecutor() -> std::unique_ptr<ExecutionBlock> {
    auto regInfos = buildRegisterInfos();
    switch (getType()) {
      case ExecutorType::UNSORTED:
        return unsortedExecutor(std::move(regInfos));
      case ExecutorType::SORTING_HEAP:
        return sortedExecutor(std::move(regInfos), GatherNode::SortMode::Heap);
      case ExecutorType::SORTING_MINELEMENT:
        return sortedExecutor(std::move(regInfos), GatherNode::SortMode::MinElement);
      default:
        THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }
  }

  auto unsortedExecutor(RegisterInfos&& regInfos) -> std::unique_ptr<ExecutionBlock> {
    if (parallelism() == GatherNode::Parallelism::Parallel) {
      return std::make_unique<ExecutionBlockImpl<ParallelUnsortedGatherExecutor>>(
          fakedQuery->rootEngine(), generateNodeDummy(ExecutionNode::GATHER),
          std::move(regInfos), EmptyExecutorInfos());
    }
    IdExecutorInfos execInfos{false};
    return std::make_unique<ExecutionBlockImpl<UnsortedGatherExecutor>>(
        fakedQuery->rootEngine(), generateNodeDummy(ExecutionNode::GATHER),
        std::move(regInfos), std::move(execInfos));
  }

  auto sortedExecutor(RegisterInfos&& regInfos, GatherNode::SortMode sortMode)
      -> std::unique_ptr<ExecutionBlock> {
    std::vector<SortRegister> sortRegister{SortRegister{0, SortElement{nullptr, true}}};
    auto executorInfos =
        SortingGatherExecutorInfos(std::move(sortRegister), *fakedQuery.get(),
                                   sortMode, 0, parallelism());
    return std::make_unique<ExecutionBlockImpl<SortingGatherExecutor>>(
        fakedQuery->rootEngine(), generateNodeDummy(ExecutionNode::GATHER),
        std::move(regInfos), std::move(executorInfos));
  }

  // Memory Management for ExecutionBlocks
  std::vector<std::unique_ptr<ExecutionBlock>> _blockLake;
};  // namespace arangodb::tests::aql

INSTANTIATE_TEST_CASE_P(CommonGatherTests, CommonGatherExecutorTest, combinations);

TEST_P(CommonGatherExecutorTest, get_all) {
  auto [exec, result] = getExecutor({});

  // Default Stack, fetch all unlimited
  AqlCallStack stack{fetchAllCall()};
  ExecutionState state = ExecutionState::HASMORE;
  SkipResult skipped{};
  SharedAqlItemBlockPtr block{nullptr};
  while (state != ExecutionState::DONE) {
    // In this test we do not care for waiting.
    std::tie(state, skipped, block) = exec->execute(stack);

    ASSERT_TRUE(skipped.nothingSkipped());
    assertResultValid(block, result);
  }
  result.testAllValuesProduced();
}

TEST_P(CommonGatherExecutorTest, get_all_sub_1) {
  auto [exec, result] = getExecutor({4});

  // Default Stack, fetch all unlimited
  AqlCallStack stack{fetchAllCall()};
  stack.pushCall(fetchAllCall());
  ExecutionState state = ExecutionState::HASMORE;
  SkipResult skipped{};
  SharedAqlItemBlockPtr block{nullptr};
  while (state != ExecutionState::DONE) {
    // In this test we do not care for waiting.
    std::tie(state, skipped, block) = exec->execute(stack);

    ASSERT_TRUE(skipped.nothingSkipped());
    assertResultValid(block, result);
  }
  result.testAllValuesProduced();
}

TEST_P(CommonGatherExecutorTest, get_all_sub_2) {
  auto [exec, result] = getExecutor({3, 5});

  // Default Stack, fetch all unlimited
  AqlCallStack stack{fetchAllCall()};
  stack.pushCall(fetchAllCall());
  stack.pushCall(fetchAllCall());
  ExecutionState state = ExecutionState::HASMORE;
  SkipResult skipped{};
  SharedAqlItemBlockPtr block{nullptr};
  while (state != ExecutionState::DONE) {
    // In this test we do not care for waiting.
    std::tie(state, skipped, block) = exec->execute(stack);

    ASSERT_TRUE(skipped.nothingSkipped());
    assertResultValid(block, result);
  }
  result.testAllValuesProduced();
}

TEST_P(CommonGatherExecutorTest, skip_data) {
  auto [exec, result] = getExecutor({});

  // Default Stack, fetch all unlimited
  AqlCallStack stack{skipThenFetchCall(5)};
  ExecutionState state = ExecutionState::HASMORE;
  SkipResult skipped{};
  SharedAqlItemBlockPtr block{nullptr};
  while (state != ExecutionState::DONE) {
    // In this test we do not care for waiting.
    std::tie(state, skipped, block) = exec->execute(stack);

    ASSERT_FALSE(skipped.nothingSkipped());
    EXPECT_EQ(skipped.getSkipOnSubqueryLevel(0), 5);
    assertResultValid(block, result);
  }
  result.testSkippedInEachRun(5);
}

TEST_P(CommonGatherExecutorTest, skip_data_sub_1) {
  auto [exec, result] = getExecutor({4});

  // Default Stack, fetch all unlimited
  AqlCallStack stack{fetchAllCall()};
  stack.pushCall(skipThenFetchCall(5));
  ExecutionState state = ExecutionState::HASMORE;
  SkipResult skipped{};
  SharedAqlItemBlockPtr block{nullptr};
  while (state != ExecutionState::DONE) {
    // In this test we do not care for waiting.
    std::tie(state, skipped, block) = exec->execute(stack);

    EXPECT_EQ(skipped.getSkipOnSubqueryLevel(0), 5);
    EXPECT_EQ(skipped.getSkipOnSubqueryLevel(1), 0);
    assertResultValid(block, result);
  }
  result.testSkippedInEachRun(5);
}

TEST_P(CommonGatherExecutorTest, skip_data_sub_2) {
  auto [exec, result] = getExecutor({3, 5});

  // Default Stack, fetch all unlimited
  AqlCallStack stack{fetchAllCall()};
  stack.pushCall(fetchAllCall());
  stack.pushCall(skipThenFetchCall(5));
  ExecutionState state = ExecutionState::HASMORE;
  SkipResult skipped{};
  SharedAqlItemBlockPtr block{nullptr};
  while (state != ExecutionState::DONE) {
    // In this test we do not care for waiting.
    std::tie(state, skipped, block) = exec->execute(stack);
    assertResultValid(block, result);
    EXPECT_EQ(skipped.getSkipCount(), 5);
    EXPECT_EQ(skipped.getSkipOnSubqueryLevel(0), 5);
    EXPECT_EQ(skipped.getSkipOnSubqueryLevel(1), 0);
    EXPECT_EQ(skipped.getSkipOnSubqueryLevel(2), 0);
  }
  result.testSkippedInEachRun(5);
}

TEST_P(CommonGatherExecutorTest, skip_main_query_sub_1) {
  auto [exec, result] = getExecutor({3});

  // Default Stack, fetch all unlimited
  AqlCallStack stack{skipThenFetchCall(1)};
  stack.pushCall(fetchAllCall());
  ExecutionState state = ExecutionState::HASMORE;
  SkipResult skipped{};
  SharedAqlItemBlockPtr block{nullptr};
  while (state != ExecutionState::DONE) {
    // In this test we do not care for waiting.
    std::tie(state, skipped, block) = exec->execute(stack);
    assertResultValid(block, result);
    // TODO Apply skipped on Stack
    EXPECT_EQ(skipped.getSkipCount(), 0);
    EXPECT_EQ(skipped.getSkipOnSubqueryLevel(0), 0);
    EXPECT_EQ(skipped.getSkipOnSubqueryLevel(1), 1);
  }
  result.testAllValuesSkippedInRun(0);
  result.testValuesSkippedInRun(0, 1);
  result.testValuesSkippedInRun(0, 2);
}

}  // namespace arangodb::tests::aql