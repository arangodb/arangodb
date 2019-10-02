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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "RowFetcherHelper.h"
#include "gtest/gtest.h"

#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Aql/SubqueryStartExecutor.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;

namespace {
ExecutorInfos MakeBaseInfos(RegisterId numRegs) {
  auto emptyRegisterList = std::make_shared<std::unordered_set<RegisterId>>(
      std::initializer_list<RegisterId>{});
  std::unordered_set<RegisterId> toKeep;
  for (RegisterId r = 0; r < numRegs; ++r) {
    toKeep.emplace(r);
  }
  return ExecutorInfos(emptyRegisterList, emptyRegisterList, numRegs, numRegs, {}, toKeep);
}

void TestShadowRow(SharedAqlItemBlockPtr const& block, size_t row, bool isRelevant) {
  EXPECT_TRUE(block->isShadowRow(row));
  // We do this additional if, in order to allow the outer test loop to continue
  // even if we do not have a shadow row.
  if (block->isShadowRow(row)) {
    ShadowAqlItemRow shadow{block, row};
    EXPECT_EQ(shadow.isRelevant(), isRelevant) << "Testing row " << row;
  }
}
}  // namespace

class SubqueryStartExecutorTest : public ::testing::Test {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};
};

TEST_F(SubqueryStartExecutorTest, check_properties) {
  EXPECT_TRUE(SubqueryStartExecutor::Properties::preservesOrder)
      << "The block has no effect on ordering of elements, it adds additional "
         "rows only.";
  EXPECT_EQ(SubqueryStartExecutor::Properties::allowsBlockPassthrough, ::arangodb::aql::BlockPassthrough::Disable)
      << "The block cannot be passThrough, as it increases the number of rows.";
  EXPECT_TRUE(SubqueryStartExecutor::Properties::inputSizeRestrictsOutputSize)
      << "The block is restricted by input, it will atMost produce 2 times the "
         "input. (Might be less if input contains shadowRows";
}

TEST_F(SubqueryStartExecutorTest, empty_input_does_not_add_shadow_rows) {
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 1)};
  VPackBuilder input;
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), false);
  auto infos = MakeBaseInfos(1);
  SubqueryStartExecutor testee(fetcher, infos);

  NoStats stats{};
  ExecutionState state{ExecutionState::HASMORE};
  OutputAqlItemRow output{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(output);
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_FALSE(output.produced());
  EXPECT_EQ(output.numRowsWritten(), 0);
}

TEST_F(SubqueryStartExecutorTest, adds_a_shadowrow_after_single_input) {
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 1)};
  auto input = VPackParser::fromJson(R"([
      ["a"]
  ])");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  auto infos = MakeBaseInfos(1);
  SubqueryStartExecutor testee(fetcher, infos);

  NoStats stats{};
  ExecutionState state{ExecutionState::HASMORE};
  OutputAqlItemRow output{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(output);
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_FALSE(output.produced());
  EXPECT_EQ(output.numRowsWritten(), 2);

  block = output.stealBlock();
  EXPECT_FALSE(block->isShadowRow(0));
  TestShadowRow(block, 1, true);
}

TEST_F(SubqueryStartExecutorTest, adds_a_shadowrow_after_every_input_line_in_single_pass) {
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 1)};
  auto input = VPackParser::fromJson(R"([
      ["a"],
      ["b"],
      ["c"]
  ])");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  auto infos = MakeBaseInfos(1);
  SubqueryStartExecutor testee(fetcher, infos);

  NoStats stats{};
  ExecutionState state{ExecutionState::HASMORE};
  OutputAqlItemRow output{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(output);
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_FALSE(output.produced());
  EXPECT_EQ(output.numRowsWritten(), 6);

  block = output.stealBlock();
  EXPECT_FALSE(block->isShadowRow(0));
  TestShadowRow(block, 1, true);
  EXPECT_FALSE(block->isShadowRow(2));
  TestShadowRow(block, 3, true);
  EXPECT_FALSE(block->isShadowRow(4));
  TestShadowRow(block, 5, true);
}

TEST_F(SubqueryStartExecutorTest, shadow_row_does_not_fit_in_current_block) {
  auto input = VPackParser::fromJson(R"([
      ["a"],
      ["b"],
      ["c"]
  ])");
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input->steal(), false);
  auto infos = MakeBaseInfos(1);
  SubqueryStartExecutor testee(fetcher, infos);

  NoStats stats{};
  ExecutionState state{ExecutionState::HASMORE};
  {
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 3, 1)};
    OutputAqlItemRow output{std::move(block), infos.getOutputRegisters(),
                            infos.registersToKeep(), infos.registersToClear()};
    std::tie(state, stats) = testee.produceRows(output);
    EXPECT_EQ(state, ExecutionState::HASMORE);
    EXPECT_FALSE(output.produced());
    EXPECT_EQ(output.numRowsWritten(), 3);

    block = output.stealBlock();
    EXPECT_FALSE(block->isShadowRow(0));
    TestShadowRow(block, 1, true);
    EXPECT_FALSE(block->isShadowRow(2));
  }
  {
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 3, 1)};
    OutputAqlItemRow output{std::move(block), infos.getOutputRegisters(),
                            infos.registersToKeep(), infos.registersToClear()};
    std::tie(state, stats) = testee.produceRows(output);
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_FALSE(output.produced());
    EXPECT_EQ(output.numRowsWritten(), 3);

    block = output.stealBlock();
    TestShadowRow(block, 0, true);
    EXPECT_FALSE(block->isShadowRow(1));
    TestShadowRow(block, 2, true);
  }
}

// TODO:
// This test can be enabled and should work as soon as the Fetcher skips non-relevant Subqueries
TEST_F(SubqueryStartExecutorTest, does_only_add_shadowrows_on_data_rows) {
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 1)};
  auto input = VPackParser::fromJson(R"([
      ["a"],
      ["b"],
      ["c"]
  ])");

  auto infos = MakeBaseInfos(1);
  {
    SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
        itemBlockManager, input->steal(), false);
    SubqueryStartExecutor testee(fetcher, infos);

    NoStats stats{};
    ExecutionState state{ExecutionState::HASMORE};
    OutputAqlItemRow output{std::move(block), infos.getOutputRegisters(),
                            infos.registersToKeep(), infos.registersToClear()};
    std::tie(state, stats) = testee.produceRows(output);
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_FALSE(output.produced());
    ASSERT_EQ(output.numRowsWritten(), 6);
    block = output.stealBlock();
    EXPECT_FALSE(block->isShadowRow(0));
    TestShadowRow(block, 1, true);
    EXPECT_FALSE(block->isShadowRow(2));
    TestShadowRow(block, 3, true);
    EXPECT_FALSE(block->isShadowRow(4));
    TestShadowRow(block, 5, true);
    // Taken from test above. We now have produced a block
    // having 3 data rows alternating with 3 shadow rows
  }
  {
    SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(itemBlockManager,
                                                                               6, false, block);
    SubqueryStartExecutor testee(fetcher, infos);
    block.reset(new AqlItemBlock(itemBlockManager, 1000, 1));

    NoStats stats{};
    ExecutionState state{ExecutionState::HASMORE};
    OutputAqlItemRow output{std::move(block), infos.getOutputRegisters(),
                            infos.registersToKeep(), infos.registersToClear()};
    std::tie(state, stats) = testee.produceRows(output);
    EXPECT_EQ(state, ExecutionState::DONE);
    EXPECT_FALSE(output.produced());
    ASSERT_EQ(output.numRowsWritten(), 9);
    block = output.stealBlock();
    EXPECT_FALSE(block->isShadowRow(0));
    TestShadowRow(block, 1, true);
    TestShadowRow(block, 2, false);
    EXPECT_FALSE(block->isShadowRow(3));
    TestShadowRow(block, 4, true);
    TestShadowRow(block, 5, false);
    EXPECT_FALSE(block->isShadowRow(6));
    TestShadowRow(block, 7, true);
    TestShadowRow(block, 8, false);
  }
}
