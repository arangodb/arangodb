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
#include "AqlItemBlockHelper.h"
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
#include <numeric>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::aql {
void PrintTo(LimitStats const &stats, std::ostream *os) {
  *os << "LimitStats{" << stats.getFullCount() << "}";
}
}

namespace arangodb::tests::aql {

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
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        block(new AqlItemBlock(itemBlockManager, 1000, 1)),
        outputRegisters(std::make_shared<const std::unordered_set<RegisterId>>(
            std::initializer_list<RegisterId>{})),
        registersToKeep(std::make_shared<const std::unordered_set<RegisterId>>(
            std::initializer_list<RegisterId>{0})) {}
};

TEST_F(LimitExecutorTest, row_upstream_the_producer_doesnt_wait) {
  auto input = VPackParser::fromJson("[ [1] ]");
  LimitExecutorInfos infos(1, 1, {}, {0}, 0, 1, true);

  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, input->steal(), false);
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

  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, input->steal(), true);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};

  OutputAqlItemRow result{std::move(block), outputRegisters, registersToKeep,
                          infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_FALSE(result.produced());
  ASSERT_EQ(0, stats.getFullCount());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(result.produced());
  ASSERT_EQ(1, stats.getFullCount());
}

TEST_F(LimitExecutorTest, rows_upstream_the_producer_doesnt_wait_limit_1_offset_0_fullcount_false) {
  auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
  LimitExecutorInfos infos(1, 1, {}, {0}, 0, 1, false);
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, input->steal(), false);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};

  OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_FALSE(row.produced());
}

TEST_F(LimitExecutorTest, rows_upstream_the_producer_doesnt_wait_limit_1_offset_0_fullcount_true) {
  auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
  LimitExecutorInfos infos(1, 1, {}, {0}, 0, 1, true);
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, input->steal(), false);
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
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, input->steal(), false);
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
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, input->steal(), true);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};

  OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_FALSE(row.produced());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(row.produced());

  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_FALSE(row.produced());

  auto block = row.stealBlock();
  AqlValue value = block->getValue(0, 0);
  ASSERT_TRUE(value.isNumber());
  ASSERT_EQ(1, value.toInt64());
}

TEST_F(LimitExecutorTest, rows_upstream_the_producer_waits_limit_1_offset_0_fullcount_true) {
  auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
  LimitExecutorInfos infos(1, 1, {}, {0}, 0, 1, true);
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, input->steal(), true);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};
  size_t fullCount = 0;

  OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_FALSE(row.produced());
  EXPECT_EQ(0, stats.getFullCount());
  fullCount += stats.getFullCount();

  // In the following, the EXPECTs against stats.getFullCount() after each
  // produceRows() call are not strictly required, but implementation dependent.
  // The implementation of LimitExecutor would be allowed to return rows it has
  // already seen at these points.
  // It is sufficient that the sum of the stats equals 4, which is asserted at
  // the end. So the intermediate EXPECTs are against the actual implementation
  // and thus just there in order to find the location of an error faster.

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_FALSE(row.produced());
  EXPECT_EQ(0, stats.getFullCount());  // not strictly required, see comment above
  fullCount += stats.getFullCount();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_FALSE(row.produced());
  EXPECT_EQ(0, stats.getFullCount());  // not strictly required, see comment above
  fullCount += stats.getFullCount();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_FALSE(row.produced());
  EXPECT_EQ(0, stats.getFullCount());  // not strictly required, see comment above
  fullCount += stats.getFullCount();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::DONE, state);
  ASSERT_TRUE(row.produced());
  EXPECT_EQ(4, stats.getFullCount());  // not strictly required, see comment above
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
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, input->steal(), waiting);
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
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, input->steal(), waiting);
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
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, input->steal(), waiting);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};

  OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_FALSE(row.produced());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_FALSE(row.produced());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_FALSE(row.produced());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::HASMORE, state);
  ASSERT_TRUE(row.produced());
  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_FALSE(row.produced());

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
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, input->steal(), waiting);
  LimitExecutor testee(fetcher, infos);
  LimitStats stats{};
  size_t fullCount = 0;

  OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_FALSE(row.produced());
  EXPECT_EQ(0, stats.getFullCount());
  fullCount += stats.getFullCount();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(ExecutionState::WAITING, state);
  ASSERT_FALSE(row.produced());
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
  ASSERT_FALSE(row.produced());
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
  ASSERT_FALSE(row.produced());
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

class LimitExecutorTestBase {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  std::shared_ptr<const std::unordered_set<RegisterId>> outputRegisters;
  std::shared_ptr<const std::unordered_set<RegisterId>> registersToKeep;

  LimitExecutorTestBase()
      : monitor(),
        itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        outputRegisters(std::make_shared<const std::unordered_set<RegisterId>>(
            std::initializer_list<RegisterId>{})),
        registersToKeep(std::make_shared<const std::unordered_set<RegisterId>>(
            std::initializer_list<RegisterId>{0})) {}
};

// skip and fullCount cannot go together: Only the last limit block may get
// fullCount, so there is no block after that could skip.
// For these cases, use this class.
class LimitExecutorWaitingTest : public LimitExecutorTestBase,
                                 public ::testing::TestWithParam<bool> {
 protected:
  bool waiting{};

  LimitExecutorWaitingTest() : LimitExecutorTestBase() {}

  virtual void SetUp() { waiting = GetParam(); }
};

// Fields:
//  [0] bool waiting
//  [1] bool fullCount
using ExtendedLimitTestParameters = std::tuple<bool, bool>;

class LimitExecutorWaitingFullCountTest
    : public LimitExecutorTestBase,
      public ::testing::TestWithParam<ExtendedLimitTestParameters> {
 protected:
  bool waiting{};
  bool fullCount{};

  LimitExecutorWaitingFullCountTest() : LimitExecutorTestBase() {}

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

TEST_P(LimitExecutorWaitingFullCountTest, rows_9_blocksize_3_limit_10) {
  // Input spec:
  size_t constexpr blocksize = 3;
  size_t constexpr offset = 0;
  size_t constexpr limit = 10;
  SharedAqlItemBlockPtr const input =
      buildBlock<1>(itemBlockManager, {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}});
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, blocksize, waiting, input);
  LimitExecutorInfos infos(1, 1, {}, {0}, offset, limit, fullCount);

  // Output spec:
  SharedAqlItemBlockPtr const expectedOutput =
      buildBlock<1>(itemBlockManager, {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}});
  size_t const expectedOutputSize =
      expectedOutput == nullptr ? 0 : expectedOutput->size();
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
  SharedAqlItemBlockPtr block = itemBlockManager.requestBlock(expectedOutputSize + 1, 1);
  OutputAqlItemRow outputRow{block, outputRegisters, registersToKeep,
                             infos.registersToClear()};

  auto result =
      runExecutor(itemBlockManager, testee, outputRow, 0, expectedOutputSize, false);
  auto& actualOutput = std::get<SharedAqlItemBlockPtr>(result);
  auto& actualStats = std::get<ExecutionStats>(result);
  auto& actualStates = std::get<std::vector<ExecutorStepResult>>(result);

  EXPECT_EQ(expectedStats, actualStats);
  EXPECT_EQ(expectedStates, actualStates);
  if (expectedOutput == nullptr) {
    ASSERT_EQ(actualOutput, nullptr);
  } else {
    ASSERT_FALSE(actualOutput == nullptr);
    EXPECT_EQ(*expectedOutput, *actualOutput);
  }
}

TEST_P(LimitExecutorWaitingFullCountTest, rows_9_blocksize_3_limit_4) {
  // Input spec:
  size_t constexpr blocksize = 3;
  size_t constexpr offset = 0;
  size_t constexpr limit = 4;
  SharedAqlItemBlockPtr const input =
      buildBlock<1>(itemBlockManager, {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}});
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, blocksize, waiting, input);
  LimitExecutorInfos infos(1, 1, {}, {0}, offset, limit, fullCount);

  // Output spec:
  SharedAqlItemBlockPtr const expectedOutput =
      buildBlock<1>(itemBlockManager, {{0}, {1}, {2}, {3}});
  size_t const expectedOutputSize =
      expectedOutput == nullptr ? 0 : expectedOutput->size();
  std::vector<ExecutorStepResult> expectedStates{
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::WAITING, 0},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::HASMORE, 3},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::WAITING, 0},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::HASMORE, 3},
  };
  if (fullCount) {
    expectedStates.emplace_back(ExecutorCall::PRODUCE_ROWS, ExecutionState::WAITING, 0);
  }
  expectedStates.emplace_back(ExecutorCall::PRODUCE_ROWS, ExecutionState::DONE, 1);
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
  SharedAqlItemBlockPtr block = itemBlockManager.requestBlock(expectedOutputSize + 1, 1);
  OutputAqlItemRow outputRow{block, outputRegisters, registersToKeep,
                             infos.registersToClear()};

  auto result =
      runExecutor(itemBlockManager, testee, outputRow, 0, expectedOutputSize, false);
  auto& actualOutput = std::get<SharedAqlItemBlockPtr>(result);
  auto& actualStats = std::get<ExecutionStats>(result);
  auto& actualStates = std::get<std::vector<ExecutorStepResult>>(result);

  EXPECT_EQ(expectedStats, actualStats);
  EXPECT_EQ(expectedStates, actualStates);
  if (expectedOutput == nullptr) {
    ASSERT_EQ(actualOutput, nullptr);
  } else {
    ASSERT_FALSE(actualOutput == nullptr);
    EXPECT_EQ(*expectedOutput, *actualOutput);
  }
}

TEST_P(LimitExecutorWaitingFullCountTest, rows_9_blocksize_3_limit_0) {
  // Input spec:
  size_t constexpr blocksize = 3;
  size_t constexpr offset = 0;
  size_t constexpr limit = 0;
  SharedAqlItemBlockPtr const input =
      buildBlock<1>(itemBlockManager, {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}});
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, blocksize, waiting, input);
  LimitExecutorInfos infos(1, 1, {}, {0}, offset, limit, fullCount);

  // Output spec:
  SharedAqlItemBlockPtr const expectedOutput = buildBlock<1>(itemBlockManager, {});
  size_t const expectedOutputSize =
      expectedOutput == nullptr ? 0 : expectedOutput->size();
  std::vector<ExecutorStepResult> expectedStates{};
  if (fullCount) {
    expectedStates.emplace_back(ExecutorCall::FETCH_FOR_PASSTHROUGH,
                                ExecutionState::WAITING, 0);
    expectedStates.emplace_back(ExecutorCall::FETCH_FOR_PASSTHROUGH,
                                ExecutionState::WAITING, 0);
    expectedStates.emplace_back(ExecutorCall::FETCH_FOR_PASSTHROUGH,
                                ExecutionState::WAITING, 0);
  }
  expectedStates.emplace_back(ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::DONE, 0);
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
  SharedAqlItemBlockPtr block = itemBlockManager.requestBlock(expectedOutputSize + 1, 1);
  OutputAqlItemRow outputRow{block, outputRegisters, registersToKeep,
                             infos.registersToClear()};

  auto result =
      runExecutor(itemBlockManager, testee, outputRow, 0, expectedOutputSize, false);
  auto& actualOutput = std::get<SharedAqlItemBlockPtr>(result);
  auto& actualStats = std::get<ExecutionStats>(result);
  auto& actualStates = std::get<std::vector<ExecutorStepResult>>(result);

  EXPECT_EQ(expectedStats, actualStats);
  EXPECT_EQ(expectedStates, actualStates);
  if (expectedOutput == nullptr) {
    ASSERT_EQ(actualOutput, nullptr);
  } else {
    ASSERT_FALSE(actualOutput == nullptr);
    EXPECT_EQ(*expectedOutput, *actualOutput);
  }
}

TEST_P(LimitExecutorWaitingFullCountTest, rows_9_blocksize_3_offset_4_limit_4) {
  // Input spec:
  size_t constexpr blocksize = 3;
  size_t constexpr offset = 4;
  size_t constexpr limit = 4;
  SharedAqlItemBlockPtr const input =
      buildBlock<1>(itemBlockManager, {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}});
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, blocksize, waiting, input);
  LimitExecutorInfos infos(1, 1, {}, {0}, offset, limit, fullCount);

  // Output spec:
  SharedAqlItemBlockPtr const expectedOutput =
      buildBlock<1>(itemBlockManager, {{4}, {5}, {6}, {7}});
  size_t const expectedOutputSize =
      expectedOutput == nullptr ? 0 : expectedOutput->size();
  std::vector<ExecutorStepResult> expectedStates{
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::WAITING, 0},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::WAITING, 0},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::HASMORE, 2},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::WAITING, 0},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::HASMORE, 3},
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
  SharedAqlItemBlockPtr block = itemBlockManager.requestBlock(expectedOutputSize + 1, 1);
  OutputAqlItemRow outputRow{block, outputRegisters, registersToKeep,
                             infos.registersToClear()};

  auto result =
      runExecutor(itemBlockManager, testee, outputRow, 0, expectedOutputSize, false);
  auto& actualOutput = std::get<SharedAqlItemBlockPtr>(result);
  auto& actualStats = std::get<ExecutionStats>(result);
  auto& actualStates = std::get<std::vector<ExecutorStepResult>>(result);

  EXPECT_EQ(expectedStats, actualStats);
  EXPECT_EQ(expectedStates, actualStates);
  if (expectedOutput == nullptr) {
    ASSERT_EQ(actualOutput, nullptr);
  } else {
    ASSERT_FALSE(actualOutput == nullptr);
    EXPECT_EQ(*expectedOutput, *actualOutput);
  }
}

TEST_P(LimitExecutorWaitingFullCountTest, rows_9_blocksize_3_offset_10_limit_1) {
  // Input spec:
  size_t constexpr blocksize = 3;
  size_t constexpr offset = 10;
  size_t constexpr limit = 1;
  SharedAqlItemBlockPtr const input =
      buildBlock<1>(itemBlockManager, {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}});
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, blocksize, waiting, input);
  LimitExecutorInfos infos(1, 1, {}, {0}, offset, limit, fullCount);

  // Output spec:
  SharedAqlItemBlockPtr const expectedOutput = buildBlock<1>(itemBlockManager, {});
  size_t const expectedOutputSize =
      expectedOutput == nullptr ? 0 : expectedOutput->size();
  std::vector<ExecutorStepResult> expectedStates{
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::WAITING, 0},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::WAITING, 0},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::WAITING, 0},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::DONE, 0},
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
  SharedAqlItemBlockPtr block = itemBlockManager.requestBlock(expectedOutputSize + 1, 1);
  OutputAqlItemRow outputRow{block, outputRegisters, registersToKeep,
                             infos.registersToClear()};

  auto result =
      runExecutor(itemBlockManager, testee, outputRow, 0, expectedOutputSize, false);
  auto& actualOutput = std::get<SharedAqlItemBlockPtr>(result);
  auto& actualStats = std::get<ExecutionStats>(result);
  auto& actualStates = std::get<std::vector<ExecutorStepResult>>(result);

  EXPECT_EQ(expectedStats, actualStats);
  EXPECT_EQ(expectedStates, actualStates);
  if (expectedOutput == nullptr) {
    ASSERT_EQ(actualOutput, nullptr);
  } else {
    ASSERT_FALSE(actualOutput == nullptr);
    EXPECT_EQ(*expectedOutput, *actualOutput);
  }
}

INSTANTIATE_TEST_CASE_P(LimitExecutorVariations, LimitExecutorWaitingFullCountTest,
                        testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(LimitExecutorWaitingTest, rows_9_blocksize_3_skip_4_offset_1_limit_7) {
  // Input spec:
  size_t constexpr blocksize = 3;
  size_t constexpr offset = 1;
  size_t constexpr limit = 7;
  size_t constexpr skip = 4;
  size_t constexpr readRows = 2;
  bool constexpr skipAfter = true;
  SharedAqlItemBlockPtr const input =
      buildBlock<1>(itemBlockManager, {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}});
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, blocksize, waiting, input);
  LimitExecutorInfos infos(1, 1, {}, {0}, offset, limit, false);

  // Output spec:
  SharedAqlItemBlockPtr const expectedOutput =
      buildBlock<1>(itemBlockManager, {{5}, {6}});
  size_t const expectedOutputSize =
      expectedOutput == nullptr ? 0 : expectedOutput->size();
  std::vector<ExecutorStepResult> expectedStates{
      {ExecutorCall::SKIP_ROWS, ExecutionState::WAITING, 0},
      {ExecutorCall::SKIP_ROWS, ExecutionState::WAITING, 0},
      {ExecutorCall::SKIP_ROWS, ExecutionState::HASMORE, 4},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::HASMORE, 1},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::WAITING, 0},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::HASMORE, 3},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::SKIP_ROWS, ExecutionState::DONE, 1},
  };
  if (!waiting) {
    removeWaiting(expectedStates);
  }
  ExecutionStats expectedStats{};
  expectedStats.fullCount = 0;

  // Run:
  LimitExecutor testee(fetcher, infos);
  // Allocate at least one output row more than expected!
  SharedAqlItemBlockPtr block = itemBlockManager.requestBlock(expectedOutputSize + 1, 1);
  OutputAqlItemRow outputRow{block, outputRegisters, registersToKeep,
                             infos.registersToClear()};

  auto result = runExecutor(itemBlockManager, testee, outputRow, skip, readRows, skipAfter);
  auto& actualOutput = std::get<SharedAqlItemBlockPtr>(result);
  auto& actualStats = std::get<ExecutionStats>(result);
  auto& actualStates = std::get<std::vector<ExecutorStepResult>>(result);

  EXPECT_EQ(expectedStats, actualStats);
  EXPECT_EQ(expectedStates, actualStates);
  if (expectedOutput == nullptr) {
    ASSERT_EQ(actualOutput, nullptr);
  } else {
    ASSERT_FALSE(actualOutput == nullptr);
    EXPECT_EQ(*expectedOutput, *actualOutput);
  }
}

TEST_P(LimitExecutorWaitingTest, rows_9_blocksize_3_skip_4_offset_1_limit_3) {
  // Input spec:
  size_t constexpr blocksize = 3;
  size_t constexpr offset = 1;
  size_t constexpr limit = 3;
  size_t constexpr skip = 4;
  size_t constexpr readRows = 1;
  bool constexpr skipAfter = true;
  SharedAqlItemBlockPtr const input =
      buildBlock<1>(itemBlockManager, {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}});
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, blocksize, waiting, input);
  LimitExecutorInfos infos(1, 1, {}, {0}, offset, limit, false);

  // Output spec:
  SharedAqlItemBlockPtr const expectedOutput = buildBlock<1>(itemBlockManager, {});
  size_t const expectedOutputSize =
      expectedOutput == nullptr ? 0 : expectedOutput->size();
  std::vector<ExecutorStepResult> expectedStates{
      {ExecutorCall::SKIP_ROWS, ExecutionState::WAITING, 0},
      {ExecutorCall::SKIP_ROWS, ExecutionState::WAITING, 0},
      {ExecutorCall::SKIP_ROWS, ExecutionState::DONE, 3},
  };
  if (!waiting) {
    removeWaiting(expectedStates);
  }
  ExecutionStats expectedStats{};
  expectedStats.fullCount = 0;

  // Run:
  LimitExecutor testee(fetcher, infos);
  // Allocate at least one output row more than expected!
  SharedAqlItemBlockPtr block = itemBlockManager.requestBlock(expectedOutputSize + 1, 1);
  OutputAqlItemRow outputRow{block, outputRegisters, registersToKeep,
                             infos.registersToClear()};

  auto result = runExecutor(itemBlockManager, testee, outputRow, skip, readRows, skipAfter);
  auto& actualOutput = std::get<SharedAqlItemBlockPtr>(result);
  auto& actualStats = std::get<ExecutionStats>(result);
  auto& actualStates = std::get<std::vector<ExecutorStepResult>>(result);

  EXPECT_EQ(expectedStats, actualStats);
  EXPECT_EQ(expectedStates, actualStates);
  if (expectedOutput == nullptr) {
    ASSERT_EQ(actualOutput, nullptr);
  } else {
    ASSERT_FALSE(actualOutput == nullptr);
    EXPECT_EQ(*expectedOutput, *actualOutput);
  }
}

TEST_P(LimitExecutorWaitingTest, rows_9_blocksize_3_skip_2_read_1_offset_2_limit_4) {
  // Input spec:
  size_t constexpr blocksize = 3;
  size_t constexpr offset = 2;
  size_t constexpr limit = 4;
  size_t constexpr skip = 2;
  size_t constexpr readRows = 1;
  bool constexpr skipAfter = true;
  SharedAqlItemBlockPtr const input =
      buildBlock<1>(itemBlockManager, {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}});
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, blocksize, waiting, input);
  LimitExecutorInfos infos(1, 1, {}, {0}, offset, limit, false);

  // Output spec:
  SharedAqlItemBlockPtr const expectedOutput = buildBlock<1>(itemBlockManager, {{4}});
  size_t const expectedOutputSize =
      expectedOutput == nullptr ? 0 : expectedOutput->size();
  std::vector<ExecutorStepResult> expectedStates{
      {ExecutorCall::SKIP_ROWS, ExecutionState::WAITING, 0},
      {ExecutorCall::SKIP_ROWS, ExecutionState::WAITING, 0},
      {ExecutorCall::SKIP_ROWS, ExecutionState::HASMORE, 2},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::HASMORE, 2},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::SKIP_ROWS, ExecutionState::DONE, 1},
  };
  if (!waiting) {
    removeWaiting(expectedStates);
  }
  ExecutionStats expectedStats{};
  expectedStats.fullCount = 0;

  // Run:
  LimitExecutor testee(fetcher, infos);
  // Allocate at least one output row more than expected!
  SharedAqlItemBlockPtr block = itemBlockManager.requestBlock(expectedOutputSize + 1, 1);
  OutputAqlItemRow outputRow{block, outputRegisters, registersToKeep,
                             infos.registersToClear()};

  auto result = runExecutor(itemBlockManager, testee, outputRow, skip, readRows, skipAfter);
  auto& actualOutput = std::get<SharedAqlItemBlockPtr>(result);
  auto& actualStats = std::get<ExecutionStats>(result);
  auto& actualStates = std::get<std::vector<ExecutorStepResult>>(result);

  EXPECT_EQ(expectedStats, actualStats);
  EXPECT_EQ(expectedStates, actualStates);
  if (expectedOutput == nullptr) {
    ASSERT_EQ(actualOutput, nullptr);
  } else {
    ASSERT_FALSE(actualOutput == nullptr);
    EXPECT_EQ(*expectedOutput, *actualOutput);
  }
}

TEST_P(LimitExecutorWaitingTest, rows_9_blocksize_3_skip_10_limit_12) {
  // Input spec:
  size_t constexpr blocksize = 3;
  size_t constexpr offset = 0;
  size_t constexpr limit = 12;
  size_t constexpr skip = 10;
  size_t constexpr readRows = 1;
  bool constexpr skipAfter = true;
  SharedAqlItemBlockPtr const input =
      buildBlock<1>(itemBlockManager, {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}});
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, blocksize, waiting, input);
  LimitExecutorInfos infos(1, 1, {}, {0}, offset, limit, false);

  // Output spec:
  SharedAqlItemBlockPtr const expectedOutput = buildBlock<1>(itemBlockManager, {});
  size_t const expectedOutputSize =
      expectedOutput == nullptr ? 0 : expectedOutput->size();
  std::vector<ExecutorStepResult> expectedStates{
      {ExecutorCall::SKIP_ROWS, ExecutionState::WAITING, 0},
      {ExecutorCall::SKIP_ROWS, ExecutionState::WAITING, 0},
      {ExecutorCall::SKIP_ROWS, ExecutionState::WAITING, 0},
      {ExecutorCall::SKIP_ROWS, ExecutionState::DONE, 9},
  };
  if (!waiting) {
    removeWaiting(expectedStates);
  }
  ExecutionStats expectedStats{};
  expectedStats.fullCount = 0;

  // Run:
  LimitExecutor testee(fetcher, infos);
  // Allocate at least one output row more than expected!
  SharedAqlItemBlockPtr block = itemBlockManager.requestBlock(expectedOutputSize + 1, 1);
  OutputAqlItemRow outputRow{block, outputRegisters, registersToKeep,
                             infos.registersToClear()};

  auto result = runExecutor(itemBlockManager, testee, outputRow, skip, readRows, skipAfter);
  auto& actualOutput = std::get<SharedAqlItemBlockPtr>(result);
  auto& actualStats = std::get<ExecutionStats>(result);
  auto& actualStates = std::get<std::vector<ExecutorStepResult>>(result);

  EXPECT_EQ(expectedStats, actualStats);
  EXPECT_EQ(expectedStates, actualStates);
  if (expectedOutput == nullptr) {
    ASSERT_EQ(actualOutput, nullptr);
  } else {
    ASSERT_FALSE(actualOutput == nullptr);
    EXPECT_EQ(*expectedOutput, *actualOutput);
  }
}

TEST_P(LimitExecutorWaitingTest, rows_9_blocksize_3_skip_1_read_1_limit_12) {
  // Input spec:
  size_t constexpr blocksize = 3;
  size_t constexpr offset = 0;
  size_t constexpr limit = 12;
  size_t constexpr skip = 1;
  size_t constexpr readRows = 1;
  bool constexpr skipAfter = true;
  SharedAqlItemBlockPtr const input =
      buildBlock<1>(itemBlockManager, {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}});
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> fetcher(itemBlockManager, blocksize, waiting, input);
  LimitExecutorInfos infos(1, 1, {}, {0}, offset, limit, false);

  // Output spec:
  SharedAqlItemBlockPtr const expectedOutput = buildBlock<1>(itemBlockManager, {{1}});
  size_t const expectedOutputSize =
      expectedOutput == nullptr ? 0 : expectedOutput->size();
  std::vector<ExecutorStepResult> expectedStates{
      {ExecutorCall::SKIP_ROWS, ExecutionState::WAITING, 0},
      {ExecutorCall::SKIP_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::FETCH_FOR_PASSTHROUGH, ExecutionState::HASMORE, 2},
      {ExecutorCall::PRODUCE_ROWS, ExecutionState::HASMORE, 1},
      {ExecutorCall::SKIP_ROWS, ExecutionState::WAITING, 0},
      {ExecutorCall::SKIP_ROWS, ExecutionState::WAITING, 0},
      {ExecutorCall::SKIP_ROWS, ExecutionState::DONE, 7},
  };
  if (!waiting) {
    removeWaiting(expectedStates);
  }
  ExecutionStats expectedStats{};
  expectedStats.fullCount = 0;

  // Run:
  LimitExecutor testee(fetcher, infos);
  // Allocate at least one output row more than expected!
  SharedAqlItemBlockPtr block = itemBlockManager.requestBlock(expectedOutputSize + 1, 1);
  OutputAqlItemRow outputRow{block, outputRegisters, registersToKeep,
                             infos.registersToClear()};

  auto result = runExecutor(itemBlockManager, testee, outputRow, skip, readRows, skipAfter);
  auto& actualOutput = std::get<SharedAqlItemBlockPtr>(result);
  auto& actualStats = std::get<ExecutionStats>(result);
  auto& actualStates = std::get<std::vector<ExecutorStepResult>>(result);

  EXPECT_EQ(expectedStats, actualStats);
  EXPECT_EQ(expectedStates, actualStates);
  if (expectedOutput == nullptr) {
    ASSERT_EQ(actualOutput, nullptr);
  } else {
    ASSERT_FALSE(actualOutput == nullptr);
    EXPECT_EQ(*expectedOutput, *actualOutput);
  }
}

INSTANTIATE_TEST_CASE_P(LimitExecutorVariations, LimitExecutorWaitingTest, testing::Bool());

/*
 * How a test case for LimitExecutor is described:
 *
 * Obviously, we need the LimitExecutor parameters
 *  1) offset,
 *  2) limit, and
 *  3) fullCount.
 * We also need an input, specified as a
 *  4) vector of input lengths,
 * which maps to a vector of input blocks, each with the specified number of
 * rows.
 * Finally, we need a call in form of an
 *  5) AqlCall
 * which breaks down to:
 *     - offset
 *     - limit,
 *     - hard/soft ~, and
 *     - fullCount.
 * Plus something like
 *  6) doneResultIsEmpty
 * to cover both the case where the last upstream non-empty result returns with
 * HASMORE, or immediately with DONE.
 */

// New test class for the new "execute" API.
// TODO Can be renamed when the old tests are removed.
class LimitExecutorExecuteApiTest
    : public LimitExecutorTestBase,
      public ::testing::TestWithParam<std::tuple<size_t, size_t, bool, std::vector<size_t>, AqlCall, bool>> {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  SharedAqlItemBlockPtr block;
  std::shared_ptr<const std::unordered_set<RegisterId>> outputRegisters;
  std::shared_ptr<const std::unordered_set<RegisterId>> registersToKeep;

  // Should never be called, and can be removed as soon as the LimitExecutor's
  // Fetcher argument&member are removed.
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable> dummyFetcher;

  LimitExecutorExecuteApiTest()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        block(new AqlItemBlock(itemBlockManager, 1000, 1)),
        outputRegisters(std::make_shared<const std::unordered_set<RegisterId>>(
            std::initializer_list<RegisterId>{})),
        registersToKeep(std::make_shared<const std::unordered_set<RegisterId>>(
            std::initializer_list<RegisterId>{0})),
        dummyFetcher(itemBlockManager, 1, false, nullptr) {}


  auto buildBlockRange(size_t const begin, size_t const end) -> SharedAqlItemBlockPtr {
    auto builder = MatrixBuilder<1>{};
    builder.reserve(end - begin);
    for (size_t i = begin; i < end; ++i) {
      builder.emplace_back(RowBuilder<1>{i});
    }
    return buildBlock<1>(itemBlockManager, std::move(builder));
  }
};

auto const testingOffsets = ::testing::Values(0, 1, 2, 3, 10, 100'000'000);
auto const testingLimits = ::testing::Values(0, 1, 2, 3, 10, 100'000'000);
auto const testingFullCount = ::testing::Bool();
using InputLengths = std::vector<size_t>;
auto const testingInputLengths = ::testing::Values(
    // 0
    InputLengths{},
    // 1
    InputLengths{1},
    // 2
    InputLengths{2},
    InputLengths{1, 1},
    // 3
    InputLengths{3},
    InputLengths{1, 2},
    InputLengths{2, 1},
    InputLengths{1, 1, 1},
    // 4
    InputLengths{4},
    InputLengths{3, 1},
    InputLengths{2, 2},
    // 9
    InputLengths{9},
    // 10
    InputLengths{10},
    InputLengths{9, 1},
    // 11
    InputLengths{11},
    InputLengths{10, 1},
    InputLengths{9, 2},
    InputLengths{9, 1, 1},
    // 19
    InputLengths{19},
    // 20
    InputLengths{20},
    InputLengths{1, 19},
    InputLengths{19, 1},
    InputLengths{10, 10},
    // 21
    InputLengths{21},
    InputLengths{20, 1},
    InputLengths{19, 2},
    InputLengths{19, 1, 1},
    InputLengths{10, 10, 1},
    InputLengths{1, 9, 9, 1, 1}
);
auto const testingAqlCalls = ::testing::Values(AqlCall{});
auto const testingDoneResultIsEmpty = ::testing::Bool();

auto const limitTestCases =
    ::testing::Combine(testingOffsets, testingLimits, testingFullCount,
                       testingInputLengths, testingAqlCalls, testingDoneResultIsEmpty);

TEST_P(LimitExecutorExecuteApiTest, testSuite) {
  // Input.
  auto const& [offset, limit, fullCount, inputLengths, clientCall, doneResultIsEmpty] =
      GetParam();

  auto const numInputRows =
      std::accumulate(inputLengths.begin(), inputLengths.end(), size_t{0});
  {  // Validation of the test case:
    TRI_ASSERT(std::all_of(inputLengths.begin(), inputLengths.end(),
                           [](auto l) { return l > 0; }));
  }

  auto const nonNegativeSubtraction = [](auto minuend, auto subtrahend) {
    // same as std::max(0, minuend - subtrahend), but safe from underflows
    return minuend - std::min(minuend, subtrahend);
  };

  // Expected output, though the expectedPassedBlocks are also the input.
  // Note that structured bindings are *not* captured by lambdas, at least in C++17.
  // So we must explicity capture them.
  auto const [expectedSkipped, expectedPassedBlocks, expectedStats] =
      std::invoke([&, offset = offset, limit = limit, fullCount = fullCount,
                   &inputLengths = inputLengths, clientCall = clientCall]() {
        std::vector<SharedAqlItemBlockPtr> blocks;
        auto const effectiveOffset = clientCall.getOffset() + offset;
    // The combined limit of a call and a LimitExecutor:
    auto const effectiveLimit =
        std::min(clientCall.getLimit(),
                 nonNegativeSubtraction(limit, clientCall.getOffset()));
    auto i = size_t{0};
    for (auto const length : inputLengths) {
      // In each iteration, we calculate a range (begin, end) ~= (i, i+length),
      // but potentially restricted by both offset and limit.
      auto const localLimit = nonNegativeSubtraction(effectiveLimit, i);
      auto const localOffset = nonNegativeSubtraction(effectiveOffset, i);
      auto const limitedLength = std::min(length, localLimit);
      auto const skip = std::min(limitedLength, localOffset);
      auto const begin = i + skip;
      auto const end = i + limitedLength;
      // Both during the offset, and after the limit, begin equals end.
      if (begin < end) {
        blocks.emplace_back(buildBlockRange(begin, end));
      }
      i += length;
    }
    auto const skipped = std::min(numInputRows, effectiveOffset);
    auto stats = LimitStats{};
    if (fullCount) {
      stats.incrFullCountBy(numInputRows);
    }
    return std::make_tuple(skipped, blocks, stats);
  });
  {
    auto const numReturnedRows =
        std::accumulate(expectedPassedBlocks.begin(), expectedPassedBlocks.end(),
                        size_t{0}, [](auto const& accum, auto const& it) {
                          return accum + it->size();
                        });
    TRI_ASSERT(numReturnedRows <= limit);
    TRI_ASSERT(numReturnedRows <= clientCall.getLimit());
  }

  auto infos = LimitExecutorInfos{1, 1, {}, {0}, offset, limit, fullCount};
  auto testee = LimitExecutor{dummyFetcher, infos};


  auto inputRange = AqlItemBlockInputRange{ExecutorState::HASMORE};
  auto output =
      std::make_unique<OutputAqlItemRow>(nullptr, outputRegisters, registersToKeep,
                                         infos.registersToClear(), AqlCall{clientCall},
                                         OutputAqlItemRow::CopyRowBehavior::DoNotCopyInputRows);

  auto executorState = ExecutorState::HASMORE;
  auto skippedUpstream = size_t{0};
  auto nextInputBlockIt = expectedPassedBlocks.begin();

  auto accumulatedStats = LimitStats{};
  auto skipped = size_t{0};
  auto outputBlocks = std::vector<SharedAqlItemBlockPtr>{};

  // auto simulateUpstreamCall =
  //     [&](AqlCall call) -> std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr> {
  //
  // };
  //
  // auto callExecutor = [&]() -> std::tuple<ExecutorState, LimitStats, size_t, AqlCall> {
  //
  // };

  // TODO If the clientCall contained a softLimit, call again afterwards with a
  //  new AqlCall{}.
  while (executorState != ExecutorState::DONE) {
    auto upstreamCall = AqlCall{};
    auto stats = LimitStats{};
    if (output->getClientCall().skipNow()) {
      TRI_ASSERT(!inputRange.hasDataRow());
      auto const originalCall = output->getClientCall();
      auto const originalRange = inputRange;
      auto call = output->stealClientCall();
      auto skippedLocal = size_t{};
      std::tie(executorState, stats, skippedLocal, upstreamCall) = testee.skipRowsRange(inputRange,  call);
      accumulatedStats += stats;
      skipped += skippedLocal;
      EXPECT_EQ(inputRange.getRowIndex(), originalRange.getRowIndex());
      if (originalCall.getOffset() > 0) {
        // TODO are these correct?
        EXPECT_EQ(originalCall.getOffset() + offset, upstreamCall.getOffset());
        EXPECT_GE(originalCall.getOffset(), call.getOffset());
        EXPECT_EQ(originalCall.getOffset() - call.getOffset(), skippedLocal);
        EXPECT_GE(originalRange.skippedInFlight(), inputRange.skippedInFlight());
        EXPECT_EQ(inputRange.skippedInFlight() - originalRange.skippedInFlight(), skippedLocal);
      }
      output->setCall(std::move(call));
    } else {
      std::tie(executorState, stats, upstreamCall) = testee.produceRows(inputRange, *output);
      accumulatedStats += stats;
      if (output->numRowsWritten() > 0) {
        EXPECT_TRUE(output->isFull());
      }
      // TODO add assertions
    }
    EXPECT_LE(upstreamCall.getOffset() + skippedUpstream, expectedSkipped);
    skippedUpstream += upstreamCall.getOffset();

    {
      auto const wantsMore = upstreamCall.getLimit() > 0;
      auto const hasMore = nextInputBlockIt != expectedPassedBlocks.end();
      auto const fastForward = upstreamCall.hasHardLimit() && upstreamCall.getLimit() == 0;
      if (wantsMore && hasMore) {
        EXPECT_FALSE(inputRange.hasDataRow());
        auto const nextBlock = *nextInputBlockIt;
        ++nextInputBlockIt;
        inputRange = AqlItemBlockInputRange{ExecutorState::HASMORE, upstreamCall.getOffset(), nextBlock, 0};
        auto call = output->stealClientCall();
        if (output->isInitialized()) {
          outputBlocks.emplace_back(output->stealBlock());
        }
        output = std::make_unique<OutputAqlItemRow>(
            nextBlock, outputRegisters, registersToKeep, infos.registersToClear(),
            std::move(call), OutputAqlItemRow::CopyRowBehavior::DoNotCopyInputRows);
      } else if (!hasMore || fastForward) {
        // TODO Add rows skipped due to fullCount
        inputRange = AqlItemBlockInputRange{ExecutorState::DONE, upstreamCall.getOffset()};
      } else {
        TRI_ASSERT(hasMore && !fastForward && !wantsMore);
        TRI_ASSERT(upstreamCall.getLimit() == 0 && !upstreamCall.hasHardLimit());
        // We got a soft limit of 0.
        inputRange = AqlItemBlockInputRange{ExecutorState::HASMORE, upstreamCall.getOffset()};
        // I don't think this should happen:
        ASSERT_TRUE(false);
      }
    }
  }
  if (output->isInitialized()) {
    outputBlocks.emplace_back(output->stealBlock());
  }
  EXPECT_EQ(expectedSkipped, skipped);
  EXPECT_EQ(expectedStats, accumulatedStats);
  EXPECT_EQ(expectedPassedBlocks, outputBlocks);
}

auto printTestCase = [](testing::TestParamInfo<std::tuple<size_t, size_t, bool, std::vector<size_t>, AqlCall, bool>> const& paramInfo) -> std::string {
  auto const& [offset, limit, fullCount, inputLengths, clientCall, doneResultIsEmpty] = paramInfo.param;

  std::stringstream out;

  out << "offset" << offset;
  out << "limit" << limit;
  out << "fullCount" << (fullCount ? "True" : "False");
  out << "inputLengths";
  for (auto const &it : inputLengths) {
    out << it << "_";
  }
  out << "clientCall";
  {
    if (clientCall.getOffset() > 0) {
      out << "_offset" << clientCall;
    }
    if (clientCall.hasHardLimit() || clientCall.hasSoftLimit()) {
      auto const clientLimit = std::get<std::size_t>(std::min(clientCall.softLimit, clientCall.hardLimit));
      out << "_limit" << clientLimit;
    }
    if (clientCall.needsFullCount()) {
      out << "_fullCount";
    }
  }
  out << "doneResultIsEmpty" << (doneResultIsEmpty ? "True" : "False");

  return out.str();
};

INSTANTIATE_TEST_CASE_P(LimitExecutorExecuteApiVariations, LimitExecutorExecuteApiTest,
    limitTestCases, printTestCase);

}  // namespace arangodb::tests::aql
