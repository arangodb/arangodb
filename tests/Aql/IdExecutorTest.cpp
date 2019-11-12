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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "AqlItemBlockHelper.h"
#include "RowFetcherHelper.h"
#include "gtest/gtest.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ConstFetcher.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/IdExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/Stats.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::tests::aql {

class IdExecutorTest : public ::testing::Test {
 protected:
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  SharedAqlItemBlockPtr block;
  std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters;
  std::shared_ptr<std::unordered_set<RegisterId>> registersToKeep;  // this must be set correctly

  IdExecutorInfos infos;
  OutputAqlItemRow row;

  IdExecutorTest()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        block(new AqlItemBlock(itemBlockManager, 1000, 1)),
        outputRegisters(make_shared_unordered_set()),
        registersToKeep(make_shared_unordered_set({0})),
        infos(1 /*nrRegs*/, *registersToKeep /*toKeep*/, {} /*toClear*/),
        row(std::move(block), outputRegisters, registersToKeep, infos.registersToClear()) {}
};

TEST_F(IdExecutorTest, there_are_no_rows_upstream) {
  ConstFetcherHelper fetcher(itemBlockManager, nullptr);
  IdExecutor<ConstFetcher> testee(fetcher, infos);
  NoStats stats{};

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(row.produced());
}

TEST_F(IdExecutorTest, there_are_rows_in_the_upstream) {
  auto input = VPackParser::fromJson("[ [true], [false], [true] ]");
  ConstFetcherHelper fetcher(itemBlockManager, input->buffer());
  IdExecutor<ConstFetcher> testee(fetcher, infos);
  NoStats stats{};

  // This block consumes all rows at once.
  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::DONE);

  std::tie(state, stats) = testee.produceRows(row);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(row.produced());

  // verify result
  AqlValue value;
  auto block = row.stealBlock();
  for (std::size_t index = 0; index < 3; index++) {
    value = block->getValue(index, 0);
    ASSERT_TRUE(value.isBoolean());
    ASSERT_EQ(value.toBoolean(), input->slice().at(index).at(0).getBool());
  }
}

TEST_F(IdExecutorTest, test_produce_datarange) {
  // Remove me after merge
  auto input = VPackParser::fromJson("[ [true], [false], [true] ]");
  ConstFetcherHelper fetcher(itemBlockManager, input->buffer());
  // Remove me after merge
  IdExecutor<::arangodb::aql::BlockPassthrough::Enable, ConstFetcher> testee(fetcher, infos);

  SharedAqlItemBlockPtr inBlock =
      buildBlock<1>(itemBlockManager, {{R"(true)"}, {R"(false)"}, {R"(true)"}});

  AqlItemBlockInputRange inputRange{ExecutorState::DONE, inBlock, 0, inBlock->size()};

  EXPECT_EQ(row.numRowsWritten(), 0);
  // This block consumes all rows at once.
  auto const [state, stats, call] = testee.produceRows(1000, inputRange, row);
  EXPECT_EQ(state, ExecutorState::DONE);
  EXPECT_EQ(row.numRowsWritten(), 3);
  EXPECT_FALSE(inputRange.hasMore());

  // verify result
  AqlValue value;
  auto block = row.stealBlock();
  for (std::size_t index = 0; index < 3; index++) {
    value = block->getValue(index, 0);
    ASSERT_TRUE(value.isBoolean());
    ASSERT_EQ(value.toBoolean(), input->slice().at(index).at(0).getBool());
  }
}

TEST_F(IdExecutorTest, test_skip_datarange) {
  // Remove me after merge
  auto input = VPackParser::fromJson("[ [true], [false], [true] ]");
  ConstFetcherHelper fetcher(itemBlockManager, input->buffer());
  // Remove me after merge
  IdExecutor<::arangodb::aql::BlockPassthrough::Enable, ConstFetcher> testee(fetcher, infos);

  SharedAqlItemBlockPtr inBlock =
      buildBlock<1>(itemBlockManager, {{R"(true)"}, {R"(false)"}, {R"(true)"}});

  AqlItemBlockInputRange inputRange{ExecutorState::DONE, inBlock, 0, inBlock->size()};

  EXPECT_EQ(row.numRowsWritten(), 0);
  // This block consumes all rows at once.
  auto const [state, skipped, call] = testee.skipRowsRange(2, inputRange);
  EXPECT_EQ(state, ExecutorState::HASMORE);
  EXPECT_EQ(skipped, 2);
  EXPECT_EQ(row.numRowsWritten(), 0);
  EXPECT_TRUE(inputRange.hasMore());

  // We still have one value left inside the block: "false"
  {
    // pop false
    auto const [state, row] = inputRange.next();
    EXPECT_EQ(state, ExecutorState::DONE);
    EXPECT_TRUE(row.getValue(0).toBoolean());
  }
  EXPECT_FALSE(inputRange.hasMore());
}

}  // namespace arangodb::tests::aql
}  // namespace tests
}  // namespace arangodb
