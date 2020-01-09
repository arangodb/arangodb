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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "AqlItemBlockHelper.h"
#include "RowFetcherHelper.h"
#include "gtest/gtest.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/FilterExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/Stats.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class FilterExecutorTest : public ::testing::Test {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  SharedAqlItemBlockPtr block;
  std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters;
  std::shared_ptr<std::unordered_set<RegisterId>>& registersToKeep;
  FilterExecutorInfos infos;

  FilterExecutorTest()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        block(new AqlItemBlock(itemBlockManager, 1000, 1)),
        outputRegisters(make_shared_unordered_set()),
        registersToKeep(outputRegisters),
        infos(0, 1, 1, {}, {}) {}
};

TEST_F(FilterExecutorTest, there_are_no_rows_upstream_the_producer_does_not_wait) {
  VPackBuilder input;
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), false);
  FilterExecutor testee(fetcher, infos);
  FilterStats stats{};

  OutputAqlItemRow result(std::move(block), outputRegisters, registersToKeep,
                          infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());
}

TEST_F(FilterExecutorTest, there_are_no_rows_upstream_the_producer_waits) {
  VPackBuilder input;
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), true);
  FilterExecutor testee(fetcher, infos);
  FilterStats stats{};

  OutputAqlItemRow result(std::move(block), outputRegisters, registersToKeep,
                          infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());
  ASSERT_EQ(stats.getFiltered(), 0);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());
  ASSERT_EQ(stats.getFiltered(), 0);
}

TEST_F(FilterExecutorTest, there_are_rows_in_the_upstream_the_producer_does_not_wait) {
  auto input = VPackParser::fromJson(
      "[ [true], [false], [true], [false], [false], [true] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  FilterExecutor testee(fetcher, infos);
  FilterStats stats{};

  OutputAqlItemRow row(std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_EQ(stats.getFiltered(), 0);
  ASSERT_TRUE(row.produced());

  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_EQ(stats.getFiltered(), 1);
  ASSERT_TRUE(row.produced());

  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_EQ(stats.getFiltered(), 2);
  ASSERT_TRUE(row.produced());

  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_EQ(stats.getFiltered(), 0);
  ASSERT_FALSE(row.produced());
}

TEST_F(FilterExecutorTest, there_are_rows_in_the_upstream_the_producer_waits) {
  auto input = VPackParser::fromJson(
      "[ [true], [false], [true], [false], [false], [true] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), true);
  FilterExecutor testee(fetcher, infos);
  FilterStats stats{};

  OutputAqlItemRow row(std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear());

  /*
  1  produce => WAIT                 RES1
  2  produce => HASMORE, Row 1     RES1
  3  => WAIT                         RES2
  4  => WAIT                         RES2
  5   => HASMORE, Row 3            RES2
  6   => WAIT,                       RES3
  7   => WAIT,                       RES3
  8   => WAIT,                       RES3
  9   => DONE, Row 6               RES3
  */

  // 1
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(row.produced());
  ASSERT_EQ(stats.getFiltered(), 0);

  // 2
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();
  ASSERT_EQ(stats.getFiltered(), 0);

  // 3
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(row.produced());
  ASSERT_EQ(stats.getFiltered(), 0);

  // 4
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(row.produced());
  // We have one filter here
  ASSERT_EQ(stats.getFiltered(), 1);

  // 5
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();
  ASSERT_EQ(stats.getFiltered(), 0);

  // 6
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(row.produced());
  ASSERT_EQ(stats.getFiltered(), 0);

  // 7
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(row.produced());
  ASSERT_EQ(stats.getFiltered(), 1);

  // 7
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(row.produced());
  ASSERT_EQ(stats.getFiltered(), 1);

  // 8
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(row.produced());
  row.advanceRow();
  ASSERT_EQ(stats.getFiltered(), 0);
}

TEST_F(FilterExecutorTest,
       there_are_rows_in_the_upstream_and_the_last_one_has_to_be_filtered_the_producer_does_not_wait) {
  auto input = VPackParser::fromJson(
      "[ [true], [false], [true], [false], [false], [true], [false] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  FilterExecutor testee(fetcher, infos);
  FilterStats stats{};

  OutputAqlItemRow row(std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_EQ(stats.getFiltered(), 0);
  ASSERT_TRUE(row.produced());

  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_EQ(stats.getFiltered(), 1);
  ASSERT_TRUE(row.produced());

  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_EQ(stats.getFiltered(), 2);
  ASSERT_TRUE(row.produced());

  row.advanceRow();

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_EQ(stats.getFiltered(), 1);
  ASSERT_FALSE(row.produced());

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_EQ(stats.getFiltered(), 0);
  ASSERT_FALSE(row.produced());
}

TEST_F(FilterExecutorTest,
       there_are_rows_in_the_upstream_and_the_last_one_has_to_be_filtered_the_producer_waits) {
  auto input = VPackParser::fromJson(
      "[ [true], [false], [true], [false], [false], [true], [false] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), true);
  FilterExecutor testee(fetcher, infos);
  FilterStats stats{};

  OutputAqlItemRow result(std::move(block), outputRegisters, registersToKeep,
                          infos.registersToClear());

  /*
  produce => WAIT                  RES1
  produce => HASMORE, Row 1        RES1
  => WAIT                          RES2
  => WAIT                          RES2
   => HASMORE, Row 3               RES2
   => WAIT,                        RES3
   => WAIT,                        RES3
   => WAIT,                        RES3
   => HASMORE, Row 6               RES3
   => WAITING,                     RES3
   => DONE, no output!             RES3
    */

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());
  ASSERT_EQ(stats.getFiltered(), 0);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  ASSERT_EQ(stats.getFiltered(), 0);

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());
  ASSERT_EQ(stats.getFiltered(), 0);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());
  ASSERT_EQ(stats.getFiltered(), 1);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  ASSERT_EQ(stats.getFiltered(), 0);

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());
  ASSERT_EQ(stats.getFiltered(), 0);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());
  ASSERT_EQ(stats.getFiltered(), 1);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());
  ASSERT_EQ(stats.getFiltered(), 1);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  ASSERT_EQ(stats.getFiltered(), 0);

  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());
  ASSERT_EQ(stats.getFiltered(), 0);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());
  ASSERT_EQ(stats.getFiltered(), 1);
}

TEST_F(FilterExecutorTest, test_produce_datarange) {
  // This fetcher will not be called!
  // After Execute is done this fetcher shall be removed, the Executor does not need it anymore!
  auto fakeUnusedBlock = VPackParser::fromJson("[  ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, fakeUnusedBlock->steal(), false);

  // This is the relevant part of the test
  FilterExecutor testee(fetcher, infos);
  SharedAqlItemBlockPtr inBlock =
      buildBlock<1>(itemBlockManager,
                    {{R"(true)"}, {R"(false)"}, {R"(true)"}, {R"(false)"}, {R"(true)"}});

  AqlItemBlockInputRange input{ExecutorState::DONE, inBlock, 0, inBlock->size()};

  OutputAqlItemRow output(std::move(block), outputRegisters, registersToKeep,
                          infos.registersToClear());
  EXPECT_EQ(output.numRowsWritten(), 0);
  auto const [state, stats, call] = testee.produceRows(input, output);
  EXPECT_EQ(state, ExecutorState::DONE);
  EXPECT_EQ(stats.getFiltered(), 2);
  EXPECT_EQ(output.numRowsWritten(), 3);
  EXPECT_FALSE(input.hasDataRow());
}

TEST_F(FilterExecutorTest, test_produce_datarange_need_more) {
  // This fetcher will not be called!
  // After Execute is done this fetcher shall be removed, the Executor does not need it anymore!
  auto fakeUnusedBlock = VPackParser::fromJson("[  ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, fakeUnusedBlock->steal(), false);

  // This is the relevant part of the test
  FilterExecutor testee(fetcher, infos);
  SharedAqlItemBlockPtr inBlock =
      buildBlock<1>(itemBlockManager,
                    {{R"(true)"}, {R"(false)"}, {R"(true)"}, {R"(false)"}, {R"(true)"}});
  size_t hardLimit = 1000;
  AqlItemBlockInputRange input{ExecutorState::HASMORE, inBlock, 0, inBlock->size()};
  AqlCall limitedCall{};
  limitedCall.hardLimit = hardLimit;
  OutputAqlItemRow output(std::move(block), outputRegisters, registersToKeep,
                          infos.registersToClear(), std::move(limitedCall));
  EXPECT_EQ(output.numRowsWritten(), 0);
  auto const [state, stats, call] = testee.produceRows(input, output);
  EXPECT_EQ(state, ExecutorState::HASMORE);
  EXPECT_EQ(stats.getFiltered(), 2);
  EXPECT_EQ(output.numRowsWritten(), 3);
  EXPECT_FALSE(input.hasDataRow());
  // Test the Call we send to upstream
  EXPECT_EQ(call.offset, 0);
  EXPECT_FALSE(call.hasHardLimit());
  // We have a given softLimit, so we do not do overfetching
  EXPECT_EQ(call.getLimit(), hardLimit - 3);
  EXPECT_FALSE(call.fullCount);
}

TEST_F(FilterExecutorTest, test_skip_datarange_need_more) {
  // This fetcher will not be called!
  // After Execute is done this fetcher shall be removed, the Executor does not need it anymore!
  auto fakeUnusedBlock = VPackParser::fromJson("[  ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, fakeUnusedBlock->steal(), false);

  // This is the relevant part of the test
  FilterExecutor testee(fetcher, infos);
  SharedAqlItemBlockPtr inBlock =
      buildBlock<1>(itemBlockManager,
                    {{R"(true)"}, {R"(false)"}, {R"(true)"}, {R"(false)"}, {R"(true)"}});

  AqlItemBlockInputRange input{ExecutorState::HASMORE, inBlock, 0, inBlock->size()};
  AqlCall clientCall;
  clientCall.offset = 1000;

  auto const [state, skipped, call] = testee.skipRowsRange(input, clientCall);
  EXPECT_EQ(state, ExecutorState::HASMORE);
  EXPECT_EQ(skipped, 3);
  EXPECT_EQ(clientCall.getOffset(), 1000 - 3);
  EXPECT_FALSE(input.hasDataRow());

  // Test the Call we send to upstream
  EXPECT_EQ(call.offset, 0);
  EXPECT_FALSE(call.hasHardLimit());
  // Avoid overfetching. I do not have a strong requirement on this
  // test, however this is what we do right now.
  EXPECT_EQ(call.getLimit(), 997);
  EXPECT_FALSE(call.fullCount);
}

TEST_F(FilterExecutorTest, test_produce_datarange_has_more) {
  // This fetcher will not be called!
  // After Execute is done this fetcher shall be removed, the Executor does not need it anymore!
  auto fakeUnusedBlock = VPackParser::fromJson("[  ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, fakeUnusedBlock->steal(), false);

  // This is the relevant part of the test
  FilterExecutor testee(fetcher, infos);
  SharedAqlItemBlockPtr inBlock =
      buildBlock<1>(itemBlockManager,
                    {{R"(true)"}, {R"(false)"}, {R"(true)"}, {R"(false)"}, {R"(true)"}});

  AqlItemBlockInputRange input{ExecutorState::DONE, inBlock, 0, inBlock->size()};
  block.reset(new AqlItemBlock(itemBlockManager, 2, 1));
  OutputAqlItemRow output(std::move(block), outputRegisters, registersToKeep,
                          infos.registersToClear());

  auto const [state, stats, call] = testee.produceRows(input, output);
  EXPECT_EQ(state, ExecutorState::HASMORE);
  EXPECT_EQ(stats.getFiltered(), 1);
  EXPECT_EQ(output.numRowsWritten(), 2);
  EXPECT_TRUE(input.hasDataRow());
  // We still have two values in block: false and true
  {
    // pop false
    auto const [state, row] = input.nextDataRow();
    EXPECT_EQ(state, ExecutorState::HASMORE);
    EXPECT_FALSE(row.getValue(0).toBoolean());
  }
  {
    // pop true
    auto const [state, row] = input.nextDataRow();
    EXPECT_EQ(state, ExecutorState::DONE);
    EXPECT_TRUE(row.getValue(0).toBoolean());
  }
  EXPECT_FALSE(input.hasDataRow());
}

TEST_F(FilterExecutorTest, test_skip_datarange_has_more) {
  // This fetcher will not be called!
  // After Execute is done this fetcher shall be removed, the Executor does not need it anymore!
  auto fakeUnusedBlock = VPackParser::fromJson("[  ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, fakeUnusedBlock->steal(), false);

  // This is the relevant part of the test
  FilterExecutor testee(fetcher, infos);
  SharedAqlItemBlockPtr inBlock =
      buildBlock<1>(itemBlockManager,
                    {{R"(true)"}, {R"(false)"}, {R"(true)"}, {R"(false)"}, {R"(true)"}});

  AqlItemBlockInputRange input{ExecutorState::DONE, inBlock, 0, inBlock->size()};
  AqlCall clientCall;
  clientCall.offset = 2;
  auto const [state, skipped, call] = testee.skipRowsRange(input, clientCall);
  EXPECT_EQ(state, ExecutorState::HASMORE);
  EXPECT_EQ(skipped, 2);
  EXPECT_EQ(clientCall.getOffset(), 0);
  EXPECT_TRUE(input.hasDataRow());
  // We still have two values in block: false and true
  {
    // pop false
    auto const [state, row] = input.nextDataRow();
    EXPECT_EQ(state, ExecutorState::HASMORE);
    EXPECT_FALSE(row.getValue(0).toBoolean());
  }
  {
    // pop true
    auto const [state, row] = input.nextDataRow();
    EXPECT_EQ(state, ExecutorState::DONE);
    EXPECT_TRUE(row.getValue(0).toBoolean());
  }
  EXPECT_FALSE(input.hasDataRow());
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
