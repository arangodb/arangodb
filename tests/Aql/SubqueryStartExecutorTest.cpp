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

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockHelper.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Aql/SubqueryStartExecutor.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "Logger/LogMacros.h"
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
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher{
      itemBlockManager, VPackParser::fromJson("[]")->steal(), false};
  AqlCall call{};
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
  auto infos = MakeBaseInfos(1);

  SubqueryStartExecutor testee(fetcher, infos);

  NoStats stats{};
  auto state = ExecutorState::HASMORE;

  auto input = AqlItemBlockInputRange{ExecutorState::DONE};

  SharedAqlItemBlockPtr block = itemBlockManager.requestBlock(1000, 1);
  OutputAqlItemRow output{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};

  std::tie(state, stats, call) = testee.produceRows(input, output);

  EXPECT_EQ(state, ExecutorState::DONE);
  EXPECT_FALSE(output.produced());
  EXPECT_EQ(output.numRowsWritten(), 0);
}

TEST_F(SubqueryStartExecutorTest, adds_a_shadowrow_after_single_input) {
  SharedAqlItemBlockPtr block = itemBlockManager.requestBlock(1000, 1);

  auto infos = MakeBaseInfos(1);
  SubqueryStartExecutor testee(fetcher, infos);

  NoStats stats{};
  auto state = ExecutorState::HASMORE;

  auto inputBlock = buildBlock<1>(itemBlockManager, MatrixBuilder<1>{{{{R"("a")"}}}});
  auto input =
      AqlItemBlockInputRange{ExecutorState::DONE, inputBlock, 0, inputBlock->size()};

  OutputAqlItemRow output{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};

  std::tie(state, stats, call) = testee.produceRows(input, output);

  EXPECT_EQ(state, ExecutorState::DONE);
  EXPECT_FALSE(output.produced());
  EXPECT_EQ(output.numRowsWritten(), 2);

  block = output.stealBlock();
  EXPECT_FALSE(block->isShadowRow(0));
  TestShadowRow(block, 1, true);
}

TEST_F(SubqueryStartExecutorTest, adds_a_shadowrow_after_every_input_line_in_single_pass) {
  auto infos = MakeBaseInfos(1);
  SubqueryStartExecutor testee(fetcher, infos);

  NoStats stats{};
  auto state = ExecutorState::HASMORE;

  auto inputBlock =
      buildBlock<1>(itemBlockManager,
                    MatrixBuilder<1>{{{{{R"("a")"}}}, {{{R"("b")"}}}, {{R"("c")"}}}});
  auto input =
      AqlItemBlockInputRange{ExecutorState::DONE, inputBlock, 0, inputBlock->size()};

  SharedAqlItemBlockPtr block = itemBlockManager.requestBlock(1000, 1);
  OutputAqlItemRow output{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};

  std::tie(state, stats, call) = testee.produceRows(input, output);

  EXPECT_EQ(state, ExecutorState::DONE);
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
  auto infos = MakeBaseInfos(1);
  auto inputBlock =
      buildBlock<1>(itemBlockManager,
                    MatrixBuilder<1>{{{{{R"("a")"}}}, {{{R"("b")"}}}, {{R"("c")"}}}});
  auto input =
      AqlItemBlockInputRange{ExecutorState::DONE, inputBlock, 0, inputBlock->size()};

  SharedAqlItemBlockPtr block = itemBlockManager.requestBlock(3, 1);
  OutputAqlItemRow output{std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};

  SubqueryStartExecutor testee(fetcher, infos);

  NoStats stats{};
  auto state = ExecutorState::HASMORE;
  {
    std::tie(state, stats, call) = testee.produceRows(input, output);

    EXPECT_EQ(state, ExecutorState::HASMORE);
    EXPECT_FALSE(output.produced());
    EXPECT_EQ(output.numRowsWritten(), 3);

    block = output.stealBlock();
    EXPECT_FALSE(block->isShadowRow(0));
    TestShadowRow(block, 1, true);
    EXPECT_FALSE(block->isShadowRow(2));
  }
  {
    SharedAqlItemBlockPtr block = itemBlockManager.requestBlock(3, 1);
    OutputAqlItemRow output{std::move(block), infos.getOutputRegisters(),
                            infos.registersToKeep(), infos.registersToClear()};

    std::tie(state, stats, call) = testee.produceRows(input, output);

    EXPECT_EQ(state, ExecutorState::DONE);
    EXPECT_FALSE(output.produced());
    EXPECT_EQ(output.numRowsWritten(), 3);

    block = output.stealBlock();
    TestShadowRow(block, 0, true);
    EXPECT_FALSE(block->isShadowRow(1));
    TestShadowRow(block, 2, true);
  }
}
