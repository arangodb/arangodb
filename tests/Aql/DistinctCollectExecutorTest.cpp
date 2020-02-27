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

#include "gtest/gtest.h"

#include "RowFetcherHelper.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/Collection.h"
#include "Aql/DistinctCollectExecutor.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "ExecutorTestHelper.h"
#include "Mocks/Servers.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <functional>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

using DistinctCollectTestHelper = ExecutorTestHelper<1, 1>;
using DistinctCollectSplitType = DistinctCollectTestHelper::SplitType;

class DistinctCollectExecutorTest
    : public AqlExecutorTestCaseWithParam<std::tuple<DistinctCollectSplitType>> {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  arangodb::transaction::Methods* trx;

  std::unordered_set<RegisterId> const regToClear;
  std::unordered_set<RegisterId> const regToKeep;

  std::unordered_set<RegisterId> readableInputRegisters = {0};
  std::unordered_set<RegisterId> writeableOutputRegisters = {1};

  SharedAqlItemBlockPtr block;
  VPackBuilder input;
  NoStats stats;

  DistinctCollectExecutorInfos infos;

  DistinctCollectExecutorTest()
      : trx(fakedQuery->trx()),
        infos(1 /*nrIn*/, 2 /*nrOut*/, regToClear, regToKeep,
              std::move(readableInputRegisters), std::move(writeableOutputRegisters),
              std::make_pair<RegisterId, RegisterId>(1, 0), trx) {}
};

TEST_P(DistinctCollectExecutorTest, split_1) {
  auto [split] = GetParam();

  ExecutorTestHelper(*fakedQuery)
      .setExecBlock<DistinctCollectExecutor>(std::move(infos))
      .setInputValueList(1, 1, 1, 2, 3, 4, 4, 5)
      .setInputSplitType(split)
      .setCall(AqlCall{2, AqlCall::Infinity{}, 2, true})
      .expectOutputValueList(3, 4)
      .expectSkipped(3)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_P(DistinctCollectExecutorTest, split_2) {
  auto [split] = GetParam();

  ExecutorTestHelper(*fakedQuery)
      .setExecBlock<DistinctCollectExecutor>(std::move(infos))
      .setInputValueList(1, 1, 1, 2, 3, 4, 4, 5)
      .setInputSplitType(split)
      .setCall(AqlCall{0, AqlCall::Infinity{}, 2, true})
      .expectOutputValueList(1, 2)
      .expectSkipped(3)
      .expectedState(ExecutionState::DONE)
      .run();
}

#if 0

TEST_F(DistinctCollectExecutorTest, if_no_rows_in_upstream_the_producer_doesnt_wait) {
  DistinctCollectExecutorInfos infos(2 /*nrIn*/, 2 /*nrOut*/, regToClear,
                                     regToKeep, std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::make_pair<RegisterId, RegisterId>(1, 2), trx);
  block.reset(new AqlItemBlock(itemBlockManager, 1000, 2));

  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(itemBlockManager, input.steal(), false);
  DistinctCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());
}

TEST_F(DistinctCollectExecutorTest, if_no_rows_in_upstream_the_producer_waits) {
  DistinctCollectExecutorInfos infos(2 /*nrIn*/, 2 /*nrOut*/, regToClear,
                                     regToKeep, std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::make_pair<RegisterId, RegisterId>(1, 2), trx);
  block.reset(new AqlItemBlock(itemBlockManager, 1000, 2));

  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(itemBlockManager, input.steal(), true);
  DistinctCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());
}

TEST_F(DistinctCollectExecutorTest,
       there_are_rows_in_the_upstream_no_distinct_values_the_producer_doesnt_wait) {
  readableInputRegisters.insert(0);
  writeableOutputRegisters.insert(1);
  RegisterId nrOutputRegister = 2;
  DistinctCollectExecutorInfos infos(1 /*nrInputReg*/,
                                     nrOutputRegister /*nrOutputReg*/, regToClear,
                                     regToKeep, std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::make_pair<RegisterId, RegisterId>(1, 0), trx);
  block.reset(new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister));

  auto input = VPackParser::fromJson("[ [1], [2] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(itemBlockManager, input->steal(), false);
  DistinctCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());

  auto block = result.stealBlock();
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_EQ(x.toInt64(), 1);

  AqlValue z = block->getValue(1, 1);
  ASSERT_TRUE(z.isNumber());
  ASSERT_EQ(z.toInt64(), 2);
}

TEST_F(DistinctCollectExecutorTest,
       there_are_rows_in_the_upstream_no_distinct_values_the_producer_waits) {
  readableInputRegisters.insert(0);
  writeableOutputRegisters.insert(1);
  RegisterId nrOutputRegister = 2;
  DistinctCollectExecutorInfos infos(1 /*nrInputReg*/,
                                     nrOutputRegister /*nrOutputReg*/, regToClear,
                                     regToKeep, std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::make_pair<RegisterId, RegisterId>(1, 0), trx);
  block.reset(new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister));

  auto input = VPackParser::fromJson("[ [1], [2] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(itemBlockManager, input->steal(), true);
  DistinctCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());

  auto block = result.stealBlock();
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_EQ(x.toInt64(), 1);

  AqlValue z = block->getValue(1, 1);
  ASSERT_TRUE(z.isNumber());
  ASSERT_EQ(z.toInt64(), 2);
}

TEST_F(DistinctCollectExecutorTest,
       there_are_rows_in_the_upstream_with_distinct_values_the_producer_doesnt_wait) {
  readableInputRegisters.insert(0);
  writeableOutputRegisters.insert(1);
  RegisterId nrOutputRegister = 2;
  DistinctCollectExecutorInfos infos(1 /*nrInputReg*/,
                                     nrOutputRegister /*nrOutputReg*/, regToClear,
                                     regToKeep, std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::make_pair<RegisterId, RegisterId>(1, 0), trx);
  block.reset(new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister));

  auto input = VPackParser::fromJson("[ [1], [2], [3], [1], [2] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(itemBlockManager, input->steal(), false);
  DistinctCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());

  auto block = result.stealBlock();
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_EQ(x.toInt64(), 1);

  AqlValue z = block->getValue(1, 1);
  ASSERT_TRUE(z.isNumber());
  ASSERT_EQ(z.toInt64(), 2);

  AqlValue y = block->getValue(2, 1);
  ASSERT_TRUE(y.isNumber());
  ASSERT_EQ(y.toInt64(), 3);
}

TEST_F(DistinctCollectExecutorTest,
       there_are_rows_in_the_upstream_with_distinct_values_the_producer_waits) {
  readableInputRegisters.insert(0);
  writeableOutputRegisters.insert(1);
  RegisterId nrOutputRegister = 2;
  DistinctCollectExecutorInfos infos(1 /*nrInputReg*/,
                                     nrOutputRegister /*nrOutputReg*/, regToClear,
                                     regToKeep, std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::make_pair<RegisterId, RegisterId>(1, 0), trx);
  block.reset(new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister));

  auto input = VPackParser::fromJson("[ [1], [2], [3], [1], [2] ]");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(itemBlockManager, input->steal(), true);
  DistinctCollectExecutor testee(fetcher, infos);

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_TRUE(result.produced());
  result.advanceRow();

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::HASMORE);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());

  auto block = result.stealBlock();
  AqlValue x = block->getValue(0, 1);
  ASSERT_TRUE(x.isNumber());
  ASSERT_EQ(x.toInt64(), 1);

  AqlValue z = block->getValue(1, 1);
  ASSERT_TRUE(z.isNumber());
  ASSERT_EQ(z.toInt64(), 2);

  AqlValue y = block->getValue(2, 1);
  ASSERT_TRUE(y.isNumber());
  ASSERT_EQ(y.toInt64(), 3);
}
#endif

template <size_t... vs>
const DistinctCollectSplitType splitIntoBlocks =
    DistinctCollectSplitType{std::vector<std::size_t>{vs...}};
template <size_t step>
const DistinctCollectSplitType splitStep = DistinctCollectSplitType{step};

INSTANTIATE_TEST_CASE_P(DistinctCollectExecutor, DistinctCollectExecutorTest,
                        ::testing::Values(splitIntoBlocks<2, 3>,
                                          splitIntoBlocks<3, 4>, splitStep<2>, splitStep<1>));
}  // namespace aql
}  // namespace tests
}  // namespace arangodb
