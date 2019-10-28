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

#include "AqlItemBlockHelper.h"
#include "RowFetcherHelper.h"
#include "gtest/gtest.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/CountCollectExecutor.h"
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

class CountCollectExecutorTest : public ::testing::Test {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  RegisterId nrOutputReg = 2;
  SharedAqlItemBlockPtr block;
  std::shared_ptr<const std::unordered_set<RegisterId>> outputRegisters;

  CountCollectExecutorTest()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        nrOutputReg(2),
        block(new AqlItemBlock(itemBlockManager, 1000, nrOutputReg)),
        outputRegisters(std::make_shared<const std::unordered_set<RegisterId>>(
            std::initializer_list<RegisterId>{1})) {}
};

TEST_F(CountCollectExecutorTest, there_are_no_rows_upstream_the_producer_doesnt_wait) {
  CountCollectExecutorInfos infos(1 /* outputRegId */, 1 /* nrIn */, nrOutputReg, {}, {});
  VPackBuilder input;
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), false);
  CountCollectExecutor testee(fetcher, infos);
  NoStats stats{};

  OutputAqlItemRow result{std::move(block), outputRegisters,
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(result.produced());

  auto block = result.stealBlock();
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_EQ(x.toInt64(), 0);

  ASSERT_EQ(0, fetcher.totalSkipped());
}

TEST_F(CountCollectExecutorTest, there_are_now_rows_upstream_the_producer_waits) {
  CountCollectExecutorInfos infos(1 /* outputRegId */, 1 /* nrIn */, nrOutputReg, {}, {});
  VPackBuilder input;
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), true);
  CountCollectExecutor testee(fetcher, infos);
  NoStats stats{};

  OutputAqlItemRow result{std::move(block), outputRegisters,
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(result.produced());

  auto block = result.stealBlock();
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_EQ(x.toInt64(), 0);

  ASSERT_EQ(0, fetcher.totalSkipped());
}

TEST_F(CountCollectExecutorTest, there_are_rows_in_the_upstream_the_producer_doesnt_wait) {
  CountCollectExecutorInfos infos(1 /* outputRegId */, 1 /* nrIn */, nrOutputReg, {}, {});
  auto input = VPackParser::fromJson("[ [1], [2], [3] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  CountCollectExecutor testee(fetcher, infos);
  NoStats stats{};

  OutputAqlItemRow result{std::move(block), outputRegisters,
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(result.produced());

  auto block = result.stealBlock();
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_EQ(x.toInt64(), 3);

  ASSERT_EQ(3, fetcher.totalSkipped());
}

TEST_F(CountCollectExecutorTest, there_are_rows_in_the_upstream_the_producer_waits) {
  CountCollectExecutorInfos infos(1 /* outputRegId */, 1 /* nrIn */, nrOutputReg, {}, {});
  auto input = VPackParser::fromJson("[ [1], [2], [3] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), true);
  CountCollectExecutor testee(fetcher, infos);
  NoStats stats{};
  OutputAqlItemRow result{std::move(block), outputRegisters,
                          infos.registersToKeep(), infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(result.produced());

  auto block = result.stealBlock();
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_EQ(x.toInt64(), 3);

  ASSERT_EQ(3, fetcher.totalSkipped());
}

TEST_F(CountCollectExecutorTest, test_produce_datarange) {
  CountCollectExecutorInfos infos(1 /* outputRegId */, 1 /* nrIn */, nrOutputReg, {}, {});
  auto fakeUnusedBlock = VPackParser::fromJson("[  ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, fakeUnusedBlock->steal(), false);
  CountCollectExecutor testee(fetcher, infos);

  SharedAqlItemBlockPtr inBlock = buildBlock<1>(itemBlockManager, {{}});
  AqlItemBlockInputRange input{ExecutorState::DONE, inBlock, 0, inBlock->size()};

  OutputAqlItemRow output(std::move(block), outputRegisters,
                          infos.registersToKeep(), infos.registersToClear());
  EXPECT_EQ(output.numRowsWritten(), 0);
  auto const [state, stats, call] = testee.produceRows(1000, input, output);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_TRUE(output.produced());

  auto block = output.stealBlock();
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_EQ(x.toInt64(), 0);

  ASSERT_EQ(0, fetcher.totalSkipped());
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
