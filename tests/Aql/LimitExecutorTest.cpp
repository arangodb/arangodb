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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "AqlHelper.h"
#include "ExecutorTestHelper.h"
#include "RowFetcherHelper.h"
#include "VelocyPackHelper.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/LimitExecutor.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class LimitExecutorTest : public ::testing::Test {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  SharedAqlItemBlockPtr block;
  std::shared_ptr<const std::unordered_set<RegisterId>> outputRegisters;
  std::shared_ptr<const std::unordered_set<RegisterId>> registersToKeep;

  // Special parameters:
  // 4th offset
  // 5th limit
  // 6th fullCount
  // 7th queryDepth

  LimitExecutorTest()
      : itemBlockManager(&monitor),
        block(new AqlItemBlock(itemBlockManager, 1000, 1)),
        outputRegisters(std::make_shared<const std::unordered_set<RegisterId>>(
            std::initializer_list<RegisterId>{})),
        registersToKeep(std::make_shared<const std::unordered_set<RegisterId>>(
            std::initializer_list<RegisterId>{0})) {}
};

TEST_F(LimitExecutorTest, row_upstream_the_producer_doesnt_wait) {
  auto input = VPackParser::fromJson("[ [1] ]");
  LimitExecutorInfos infos(1, 1, {}, {0}, 0, 1, true);

  SingleRowFetcherHelper<true> fetcher(itemBlockManager, input->steal(), false);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};

  OutputAqlItemRow result{std::move(block), outputRegisters, registersToKeep,
                          infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(result.produced());
  ASSERT_EQ(1, stats.getFullCount());
}

TEST_F(LimitExecutorTest, row_upstream_the_producer_waits) {
  auto input = VPackParser::fromJson("[ [1] ]");
  LimitExecutorInfos infos(1, 1, {}, {0}, 0, 1, true);

  SingleRowFetcherHelper<true> fetcher(itemBlockManager, input->steal(), true);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};

  OutputAqlItemRow result{std::move(block), outputRegisters, registersToKeep,
                          infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_TRUE(!result.produced());
  ASSERT_EQ(0, stats.getFullCount());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(result.produced());
  ASSERT_EQ(1, stats.getFullCount());
}

TEST_F(LimitExecutorTest, rows_upstream_the_producer_doesnt_wait_limit_1_offset_0_fullcount_false) {
  auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
  LimitExecutorInfos infos(1, 1, {}, {0}, 0, 1, false);
  SingleRowFetcherHelper<true> fetcher(itemBlockManager, input->steal(), false);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};

  OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(!row.produced());
}

TEST_F(LimitExecutorTest, rows_upstream_the_producer_doesnt_wait_limit_1_offset_0_fullcount_true) {
  auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
  LimitExecutorInfos infos(1, 1, {}, {0}, 0, 1, true);
  SingleRowFetcherHelper<true> fetcher(itemBlockManager, input->steal(), false);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};

  OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(row.produced());
  ASSERT_EQ(4, stats.getFullCount());

  auto block = row.stealBlock();
  AqlValue value = block->getValue(0, 0);
  ASSERT_TRUE(value.isNumber());
  ASSERT_EQ(1, value.toInt64());
}

TEST_F(LimitExecutorTest, rows_upstream_the_producer_doesnt_wait_limit_1_offset_1_fullcount_true) {
  auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
  LimitExecutorInfos infos(1, 1, {}, {0}, 1, 1, true);
  SingleRowFetcherHelper<true> fetcher(itemBlockManager, input->steal(), false);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};

  OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(row.produced());
  ASSERT_EQ(4, stats.getFullCount());

  auto block = row.stealBlock();
  AqlValue value = block->getValue(0, 0);
  ASSERT_TRUE(value.isNumber());
  ASSERT_EQ(2, value.toInt64());
}

TEST_F(LimitExecutorTest, rows_upstream_the_producer_waits_limit_1_offset_0_fullcount_false) {
  auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
  LimitExecutorInfos infos(1, 1, {}, {0}, 0, 1, false);
  SingleRowFetcherHelper<true> fetcher(itemBlockManager, input->steal(), true);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};

  OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_TRUE(!row.produced());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(row.produced());

  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(!row.produced());

  auto block = row.stealBlock();
  AqlValue value = block->getValue(0, 0);
  ASSERT_TRUE(value.isNumber());
  ASSERT_EQ(1, value.toInt64());
}

TEST_F(LimitExecutorTest, rows_upstream_the_producer_waits_limit_1_offset_0_fullcount_true) {
  auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
  LimitExecutorInfos infos(1, 1, {}, {0}, 0, 1, true);
  SingleRowFetcherHelper<true> fetcher(itemBlockManager, input->steal(), true);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};
  size_t fullCount = 0;

  OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_TRUE(!row.produced());
  EXPECT_EQ(0, stats.getFullCount());
  fullCount += stats.getFullCount();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_TRUE(!row.produced());
  // This is actually implementation dependent. Important is only that the sum
  // of the stats equals 4, which is asserted at the end.
  EXPECT_EQ(0, stats.getFullCount());
  fullCount += stats.getFullCount();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_TRUE(!row.produced());
  // This is actually implementation dependent. Important is only that the sum
  // of the stats equals 4, which is asserted at the end.
  EXPECT_EQ(0, stats.getFullCount());
  fullCount += stats.getFullCount();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_TRUE(!row.produced());
  // This is actually implementation dependent. Important is only that the sum
  // of the stats equals 4, which is asserted at the end.
  EXPECT_EQ(0, stats.getFullCount());
  fullCount += stats.getFullCount();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(row.produced());
  // This is actually implementation dependent. Important is only that the sum
  // of the stats equals 4, which is asserted at the end.
  EXPECT_EQ(4, stats.getFullCount());
  fullCount += stats.getFullCount();

  ASSERT_EQ(4, fullCount);

  auto block = row.stealBlock();
  AqlValue value = block->getValue(0, 0);
  ASSERT_TRUE(value.isNumber());
  ASSERT_EQ(1, value.toInt64());
}

TEST_F(LimitExecutorTest, rows_upstream_the_producer_doesnt_wait_limit_6_offset_1_fullcount_false) {
  size_t constexpr offset = 1;
  size_t constexpr limit = 6;
  bool constexpr fullcount = false;
  bool constexpr waiting = false;
  auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
  LimitExecutorInfos infos(1, 1, {}, {0}, offset, limit, fullcount);
  SingleRowFetcherHelper<true> fetcher(itemBlockManager, input->steal(), waiting);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};

  OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(row.produced());

  auto block = row.stealBlock();
  EXPECT_EQ(3, block->size());
  AqlValue value = block->getValue(0, 0);
  ASSERT_TRUE(value.isNumber());
  EXPECT_EQ(2, value.toInt64());
  value = block->getValue(1, 0);
  ASSERT_TRUE(value.isNumber());
  EXPECT_EQ(3, value.toInt64());
  value = block->getValue(2, 0);
  ASSERT_TRUE(value.isNumber());
  EXPECT_EQ(4, value.toInt64());
}

TEST_F(LimitExecutorTest, rows_upstream_the_producer_doesnt_wait_limit_6_offset_1_fullcount_true) {
  size_t constexpr offset = 1;
  size_t constexpr limit = 6;
  bool constexpr fullcount = true;
  bool constexpr waiting = false;
  auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
  LimitExecutorInfos infos(1, 1, {}, {0}, offset, limit, fullcount);
  SingleRowFetcherHelper<true> fetcher(itemBlockManager, input->steal(), waiting);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};
  size_t fullCount = 0;

  OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(row.produced());
  row.advanceRow();
  EXPECT_EQ(2, stats.getFullCount());
  fullCount += stats.getFullCount();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(row.produced());
  row.advanceRow();
  EXPECT_EQ(1, stats.getFullCount());
  fullCount += stats.getFullCount();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(row.produced());
  EXPECT_EQ(1, stats.getFullCount());
  fullCount += stats.getFullCount();

  ASSERT_EQ(4, fullCount);

  auto block = row.stealBlock();
  EXPECT_EQ(3, block->size());
  AqlValue value = block->getValue(0, 0);
  ASSERT_TRUE(value.isNumber());
  EXPECT_EQ(2, value.toInt64());
  value = block->getValue(1, 0);
  ASSERT_TRUE(value.isNumber());
  EXPECT_EQ(3, value.toInt64());
  value = block->getValue(2, 0);
  ASSERT_TRUE(value.isNumber());
  EXPECT_EQ(4, value.toInt64());
}
TEST_F(LimitExecutorTest, rows_upstream_the_producer_waits_limit_6_offset_1_fullcount_false) {
  size_t constexpr offset = 1;
  size_t constexpr limit = 6;
  bool constexpr fullcount = false;
  bool constexpr waiting = true;
  auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
  LimitExecutorInfos infos(1, 1, {}, {0}, offset, limit, fullcount);
  SingleRowFetcherHelper<true> fetcher(itemBlockManager, input->steal(), waiting);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};

  OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_TRUE(!row.produced());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_TRUE(!row.produced());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_TRUE(!row.produced());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_TRUE(!row.produced());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(row.produced());

  auto block = row.stealBlock();
  EXPECT_EQ(3, block->size());
  AqlValue value = block->getValue(0, 0);
  ASSERT_TRUE(value.isNumber());
  EXPECT_EQ(2, value.toInt64());
  value = block->getValue(1, 0);
  ASSERT_TRUE(value.isNumber());
  EXPECT_EQ(3, value.toInt64());
  value = block->getValue(2, 0);
  ASSERT_TRUE(value.isNumber());
  EXPECT_EQ(4, value.toInt64());
}

TEST_F(LimitExecutorTest, rows_upstream_the_producer_waits_limit_6_offset_1_fullcount_true) {
  size_t constexpr offset = 1;
  size_t constexpr limit = 6;
  bool constexpr fullcount = true;
  bool constexpr waiting = true;
  auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
  LimitExecutorInfos infos(1, 1, {}, {0}, offset, limit, fullcount);
  SingleRowFetcherHelper<true> fetcher(itemBlockManager, input->steal(), waiting);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};
  size_t fullCount = 0;

  OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_TRUE(!row.produced());
  EXPECT_EQ(0, stats.getFullCount());
  fullCount += stats.getFullCount();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_TRUE(!row.produced());
  EXPECT_EQ(1, stats.getFullCount());
  fullCount += stats.getFullCount();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(row.produced());
  row.advanceRow();
  EXPECT_EQ(1, stats.getFullCount());
  fullCount += stats.getFullCount();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_TRUE(!row.produced());
  EXPECT_EQ(0, stats.getFullCount());
  fullCount += stats.getFullCount();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(row.produced());
  row.advanceRow();
  EXPECT_EQ(1, stats.getFullCount());
  fullCount += stats.getFullCount();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_TRUE(!row.produced());
  EXPECT_EQ(0, stats.getFullCount());
  fullCount += stats.getFullCount();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(row.produced());
  EXPECT_EQ(1, stats.getFullCount());
  fullCount += stats.getFullCount();

  ASSERT_EQ(4, fullCount);

  auto block = row.stealBlock();
  EXPECT_EQ(3, block->size());
  AqlValue value = block->getValue(0, 0);
  ASSERT_TRUE(value.isNumber());
  EXPECT_EQ(2, value.toInt64());
  value = block->getValue(1, 0);
  ASSERT_TRUE(value.isNumber());
  EXPECT_EQ(3, value.toInt64());
  value = block->getValue(2, 0);
  ASSERT_TRUE(value.isNumber());
  EXPECT_EQ(4, value.toInt64());
}


// Fields:
//  [0] bool waiting
//  [1] bool fullCount
using ExtendedLimitTestParameters = std::tuple<bool, bool>;

class ExtendedLimitExecutorTest
    : public ::testing::TestWithParam<ExtendedLimitTestParameters> {
 protected:
  // Params:
  bool waiting{};
  bool fullCount{};

  // Members:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  std::shared_ptr<const std::unordered_set<RegisterId>> outputRegisters;
  std::shared_ptr<const std::unordered_set<RegisterId>> registersToKeep;

  ExtendedLimitExecutorTest()
      : monitor(),
        itemBlockManager(&monitor),
        outputRegisters(std::make_shared<const std::unordered_set<RegisterId>>(
            std::initializer_list<RegisterId>{})),
        registersToKeep(std::make_shared<const std::unordered_set<RegisterId>>(
            std::initializer_list<RegisterId>{0})) {}

  virtual void SetUp() {
    ExtendedLimitTestParameters const& params = GetParam();
    std::tie(waiting, fullCount) = params;
  }
};


void removeWaiting(std::vector<ExecutorStepResult>& results) {
  std::vector<ExecutorStepResult> tmp;
  for (auto const result : results) {
    if (std::get<ExecutionState>(result) != ExecutionState::WAITING) {
      tmp.emplace_back(result);
    }
  }
  results.swap(tmp);
}

TEST_P(ExtendedLimitExecutorTest, rows_9_blocksize_3_offset_0_limit_10) {
  // Input spec:
  size_t constexpr blocksize = 3;
  size_t constexpr offset = 0;
  size_t constexpr limit = 10;
  SharedAqlItemBlockPtr const input =
      vPackBufferToAqlItemBlock(itemBlockManager, R"=( [ [0], [1], [2], [3], [4], [5], [6], [7], [8] ] )="_vpack);
  SingleRowFetcherHelper<true> fetcher(itemBlockManager, blocksize, waiting, input);
  LimitExecutorInfos infos(1, 1, {}, {0}, offset, limit, fullCount);

  // Output spec:
  SharedAqlItemBlockPtr const expectedOutput =
      vPackBufferToAqlItemBlock(itemBlockManager, R"=( [ [0], [1], [2], [3], [4], [5], [6], [7], [8] ] )="_vpack);
  // TODO: As produceRow never returns WAITING, because that's "eaten" by fetchBlockForPassthrough,
  //  this cannot be checked. Should it be?
  std::vector<ExecutorStepResult> expectedStates{
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::WAITING, 0},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::HASMORE, 3},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::WAITING, 0},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::HASMORE, 3},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::WAITING, 0},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::DONE, 3},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::DONE, 1},
  };
  if (!waiting) {
    removeWaiting(expectedStates);
  }
  ExecutionStats expectedStats{};
  if (fullCount) {
    expectedStats.fullCount = 9;
  } else {
    expectedStats.fullCount = 0;
  }

  // Run:
  LimitExecutor testee(fetcher, infos);
  // Allocate at least one output row more than expected!
  SharedAqlItemBlockPtr block = itemBlockManager.requestBlock(expectedOutput->size() + 1, 1);
  OutputAqlItemRow outputRow{block, outputRegisters, registersToKeep, infos.registersToClear()};

  auto result = runExecutor(itemBlockManager, testee, outputRow);
  auto& actualOutput = std::get<SharedAqlItemBlockPtr>(result);
  auto& actualStats = std::get<ExecutionStats>(result);
  auto& actualStates = std::get<std::vector<ExecutorStepResult>>(result);

  EXPECT_EQ(expectedStats, actualStats);
  EXPECT_EQ(expectedStates, actualStates);
  ASSERT_FALSE(actualOutput == nullptr);
  EXPECT_EQ(*expectedOutput, *actualOutput);
}

INSTANTIATE_TEST_CASE_P(LimitExecutorVariations, ExtendedLimitExecutorTest,
                        testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
