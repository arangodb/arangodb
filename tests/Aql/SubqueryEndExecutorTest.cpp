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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "AqlItemBlockHelper.h"
#include "RowFetcherHelper.h"
#include "gtest/gtest.h"

#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Aql/SubqueryEndExecutor.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;

namespace {
SubqueryEndExecutorInfos MakeBaseInfos() {

  // one input register (RegisterId 0) and one output register (RegisterId 1)
  // size_t regs = 2;

  auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
    std::initializer_list<RegisterId>{0});

  auto outputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
    std::initializer_list<RegisterId>({0}));

  // std::unordered_set<RegisterId> toKeep;
  // for (RegisterId r = 0; r < regs; ++r) {
  //   toKeep.emplace(r);
  // }

  return SubqueryEndExecutorInfos(inputRegisters, outputRegisters, 1,
                                  1, {}, {}, nullptr, RegisterId{0});
}

}  // namespace

class SubqueryEndExecutorTest : public ::testing::Test {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

  void InsertNewShadowRowAfter(std::set<size_t> ns, SharedAqlItemBlockPtr const& inputBlock,
                               SharedAqlItemBlockPtr& outputBlock) {

    RegisterId numRegisters = inputBlock->getNrRegs();

    outputBlock.reset(new AqlItemBlock(itemBlockManager, inputBlock->size() + ns.size(), numRegisters));

    // We do not add or remove anything, just move
    auto outputRegisters = std::make_shared<const std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});
    auto registersToKeep = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});
    for (RegisterId r = 0; r < numRegisters; ++r) {
      registersToKeep->emplace(r);
    }
    auto registersToClear = std::make_shared<const std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});
    OutputAqlItemRow testee(std::move(outputBlock), outputRegisters,
                            registersToKeep, registersToClear);

    for (size_t rowIdx = 0; rowIdx < inputBlock->size(); ++rowIdx) {
      ASSERT_FALSE(testee.isFull());
      // simply copy over every row, and insert a shadowRow after it
      InputAqlItemRow source{inputBlock, rowIdx};
      testee.copyRow(source);
      ASSERT_TRUE(testee.produced());
      ASSERT_FALSE(testee.isFull());
      testee.advanceRow();
      if (ns.find(rowIdx) != ns.end()) {
        testee.createShadowRow(source);
        ASSERT_TRUE(testee.produced());
        testee.advanceRow();
      }
    }
    outputBlock = testee.stealBlock();
    ASSERT_EQ(outputBlock->size(), inputBlock->size() + 1);
  }
  void ReplaceByShadowRowAt(std::set<size_t> ns, SharedAqlItemBlockPtr const& inputBlock,
                               SharedAqlItemBlockPtr& outputBlock) {

    RegisterId numRegisters = inputBlock->getNrRegs();

    outputBlock.reset(new AqlItemBlock(itemBlockManager, inputBlock->size(), numRegisters));

    // We do not add or remove anything, just move
    auto outputRegisters = std::make_shared<const std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});
    auto registersToKeep = std::make_shared<std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});
    for (RegisterId r = 0; r < numRegisters; ++r) {
      registersToKeep->emplace(r);
    }
    auto registersToClear = std::make_shared<const std::unordered_set<RegisterId>>(
        std::initializer_list<RegisterId>{});
    OutputAqlItemRow testee(std::move(outputBlock), outputRegisters,
                            registersToKeep, registersToClear);

    for (size_t rowIdx = 0; rowIdx < inputBlock->size(); ++rowIdx) {
      ASSERT_FALSE(testee.isFull());
      // simply copy over every row, and insert a shadowRow after it
      InputAqlItemRow source{inputBlock, rowIdx};
      if (ns.find(rowIdx) != ns.end()) {
        testee.createShadowRow(source);
        ASSERT_TRUE(testee.produced());
        testee.advanceRow();
      } else {
        testee.copyRow(source);
        ASSERT_TRUE(testee.produced());
        ASSERT_FALSE(testee.isFull());
        testee.advanceRow();
      }
    }
    outputBlock = testee.stealBlock();
    ASSERT_EQ(outputBlock->size(), inputBlock->size());
  }
};

TEST_F(SubqueryEndExecutorTest, check_properties) {
  EXPECT_TRUE(SubqueryEndExecutor::Properties::preservesOrder)
      << "The block has no effect on ordering of elements, it adds additional "
         "rows only.";
  EXPECT_FALSE(SubqueryEndExecutor::Properties::allowsBlockPassthrough)
      << "The block cannot be passThrough, as it increases the number of rows.";
  EXPECT_TRUE(SubqueryEndExecutor::Properties::inputSizeRestrictsOutputSize)
      << "The block produces one output row per input row plus potentially a "
         "shadow rows which is bounded by the structure of the query";
};


TEST_F(SubqueryEndExecutorTest, empty_input_expects_shadow_rows) {
  SharedAqlItemBlockPtr outputBlock;
  SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, {{1}});

  ReplaceByShadowRowAt({0}, inputBlock, outputBlock);
  inputBlock.swap(outputBlock);

  SingleRowFetcherHelper<false> fetcher(itemBlockManager, inputBlock->size(), false, inputBlock);

  auto infos = MakeBaseInfos();
  SubqueryEndExecutor testee(fetcher, infos);

  NoStats stats{};
  ExecutionState state{ExecutionState::HASMORE};
  outputBlock.reset(new AqlItemBlock(itemBlockManager, inputBlock->size(), 1));
  OutputAqlItemRow output{std::move(outputBlock), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(output);
  EXPECT_TRUE(state == ExecutionState::DONE);
  EXPECT_FALSE(output.produced());
  EXPECT_EQ(output.numRowsWritten(), 0);
}

TEST_F(SubqueryEndExecutorTest, shadowrow_after_input) {
  SharedAqlItemBlockPtr outputBlock;

  SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, {{42},{34}});

  InsertNewShadowRowAfter({1}, inputBlock, outputBlock);
  inputBlock.swap(outputBlock);

  SingleRowFetcherHelper<false> fetcher(itemBlockManager, inputBlock->size(), false, inputBlock);

  auto infos = MakeBaseInfos();

  SubqueryEndExecutor testee(fetcher, infos);

  NoStats stats{};
  ExecutionState state{ExecutionState::HASMORE};

  outputBlock.reset(new AqlItemBlock(itemBlockManager, inputBlock->size(), 1));
  OutputAqlItemRow output{std::move(outputBlock), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(output);
  EXPECT_TRUE(state == ExecutionState::DONE);
  EXPECT_TRUE(output.produced());
  EXPECT_EQ(output.numRowsWritten(), 1);
}

TEST_F(SubqueryEndExecutorTest, two_shadowrows_after_input) {
  SharedAqlItemBlockPtr outputBlock;

  SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, {{42},{34}});

  InsertNewShadowRowAfter({1,2}, inputBlock, outputBlock);
  inputBlock.swap(outputBlock);

  SingleRowFetcherHelper<false> fetcher(itemBlockManager, inputBlock->size(), false, inputBlock);

  auto infos = MakeBaseInfos();

  SubqueryEndExecutor testee(fetcher, infos);

  NoStats stats{};
  ExecutionState state{ExecutionState::HASMORE};

  outputBlock.reset(new AqlItemBlock(itemBlockManager, inputBlock->size() + 1, 1));
  OutputAqlItemRow output{std::move(outputBlock), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(output);
  EXPECT_TRUE(state == ExecutionState::DONE);
  EXPECT_TRUE(output.produced());
  EXPECT_EQ(output.numRowsWritten(), 1);
}

