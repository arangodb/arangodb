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

#include "Logger/LogMacros.h"

#include "Basics/VelocyPackHelper.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;
using namespace arangodb::basics;

namespace {
SubqueryEndExecutorInfos MakeBaseInfos() {
  // one input register (RegisterId 0) and one output register (RegisterId 1)
  // size_t regs = 2;

  auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
      std::initializer_list<RegisterId>{0});

  auto outputRegisters = std::make_shared<std::unordered_set<RegisterId>>(
      std::initializer_list<RegisterId>({0}));

  return SubqueryEndExecutorInfos(inputRegisters, outputRegisters, 1, 1, {}, {},
                                  nullptr, RegisterId{0});
}

}  // namespace

class SubqueryEndExecutorTest : public ::testing::Test {
public:
  SubqueryEndExecutorTest() : infos(MakeBaseInfos()) {};
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};
  SubqueryEndExecutorInfos infos;


  void ExpectedValues(OutputAqlItemRow& itemRow,
                      const std::vector<std::string> expectedStrings,
                      std::unordered_map<size_t, uint64_t> const& shadowRows) const {
    auto block = itemRow.stealBlock();

    ASSERT_EQ(expectedStrings.size(), block->size());

    for (size_t rowIdx = 0; rowIdx < block->size(); rowIdx++) {
      if (block->isShadowRow(rowIdx)) {
        ShadowAqlItemRow shadow{block, rowIdx};

        auto depth = shadowRows.find(rowIdx);
        if (depth != shadowRows.end()) {
          EXPECT_EQ(depth->second, shadow.getDepth());
        } else {
          FAIL() << "did not expect row " << rowIdx << " to be a shadow row";
        }
      } else {
        EXPECT_EQ(shadowRows.find(rowIdx), shadowRows.end())
          << "expected row " << rowIdx << " to be a shadow row";

        auto const expected = VPackParser::fromJson(expectedStrings.at(rowIdx))->slice();
        InputAqlItemRow input{block, rowIdx};
        auto value = input.getValue(0).slice();
        EXPECT_TRUE(VelocyPackHelper::equal(value, expected, false))
            << input.getValue(0).slice().toJson() << " != " << expected.toJson();
      }
    }
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
  SharedAqlItemBlockPtr inputBlock = buildBlock<1>(itemBlockManager, {{1}}, {{0, 0}});

  SingleRowFetcherHelper<false> fetcher(itemBlockManager, inputBlock->size(), false, inputBlock);
  SubqueryEndExecutor testee(fetcher, infos);

  NoStats stats{};
  ExecutionState state{ExecutionState::HASMORE};

  outputBlock.reset(new AqlItemBlock(itemBlockManager, inputBlock->size(), 1));
  OutputAqlItemRow output{std::move(outputBlock), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(output);
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(output.numRowsWritten(), 1);

  ExpectedValues(output, {"[]"}, {});
}

TEST_F(SubqueryEndExecutorTest, single_input_expects_shadow_rows) {
  SharedAqlItemBlockPtr outputBlock;
  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{1}, {1}}, {{1, 0}});

  SingleRowFetcherHelper<false> fetcher(itemBlockManager, inputBlock->size(), false, inputBlock);

  SubqueryEndExecutor testee(fetcher, infos);

  NoStats stats{};
  ExecutionState state{ExecutionState::HASMORE};
  outputBlock.reset(new AqlItemBlock(itemBlockManager, inputBlock->size(), 1));
  OutputAqlItemRow output{std::move(outputBlock), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(output);
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(output.numRowsWritten(), 1);

  ExpectedValues(output, {"[1]"}, {});
}

TEST_F(SubqueryEndExecutorTest, two_inputs_one_shadowrow) {
  SharedAqlItemBlockPtr outputBlock;
  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{42}, {34}, {1}}, {{2, 0}});

  SingleRowFetcherHelper<false> fetcher(itemBlockManager, inputBlock->size(), false, inputBlock);

  SubqueryEndExecutor testee(fetcher, infos);

  NoStats stats{};
  ExecutionState state{ExecutionState::HASMORE};

  outputBlock.reset(new AqlItemBlock(itemBlockManager, inputBlock->size(), 1));
  OutputAqlItemRow output{std::move(outputBlock), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(output);
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(output.numRowsWritten(), 1);

  ExpectedValues(output, {"[42,34]"}, {});
}

TEST_F(SubqueryEndExecutorTest, two_inputs_two_shadowrows) {
  SharedAqlItemBlockPtr outputBlock;

  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{42}, {1}, {34}, {1}}, {{1, 0}, {3, 0}});

  SingleRowFetcherHelper<false> fetcher(itemBlockManager, inputBlock->size(), false, inputBlock);

  SubqueryEndExecutor testee(fetcher, infos);

  NoStats stats{};
  ExecutionState state{ExecutionState::HASMORE};

  outputBlock.reset(new AqlItemBlock(itemBlockManager, inputBlock->size(), 1));
  OutputAqlItemRow output{std::move(outputBlock), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(output);
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(output.numRowsWritten(), 2);
  ExpectedValues(output, {"[42]", "[34]"}, {});
}

TEST_F(SubqueryEndExecutorTest, two_input_one_shadowrow_two_irrelevant) {
  SharedAqlItemBlockPtr outputBlock;
  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{42}, {42}, {42}, {42}, {42}},
                    {{2, 0}, {3, 1}, {4, 2}});

  SingleRowFetcherHelper<false> fetcher(itemBlockManager, inputBlock->size(), false, inputBlock);

  SubqueryEndExecutor testee(fetcher, infos);

  NoStats stats{};
  ExecutionState state{ExecutionState::HASMORE};

  outputBlock.reset(new AqlItemBlock(itemBlockManager, inputBlock->size(), 1));
  OutputAqlItemRow output{std::move(outputBlock), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};

  std::tie(state, stats) = testee.produceRows(output);
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(output.numRowsWritten(), 3);
  ExpectedValues(output, {"[42, 42]", "", ""}, {{1,0}, {2,1}});
}

TEST_F(SubqueryEndExecutorTest, consume_output_of_subquery_end_executor) {
  NoStats stats{};
  ExecutionState state{ExecutionState::HASMORE};

  SharedAqlItemBlockPtr outputBlock;
  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{42}, {42}, {42}, {42}, {42}},
                    {{2, 0}, {3, 1}, {4, 2}});

  SingleRowFetcherHelper<false> fetcher(itemBlockManager, inputBlock->size(), false, inputBlock);

  SubqueryEndExecutor testee(fetcher, infos);

  outputBlock.reset(new AqlItemBlock(itemBlockManager, inputBlock->size(), 1));
  OutputAqlItemRow output{std::move(outputBlock), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee.produceRows(output);
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(output.numRowsWritten(), 3);

  //  ExpectedValues(output, { "[42, 42]", "", "" });

  outputBlock = output.stealBlock();
  inputBlock.swap(outputBlock);
  SingleRowFetcherHelper<false> fetcher2(itemBlockManager, inputBlock->size(),
                                         false, inputBlock);
  SubqueryEndExecutor testee2(fetcher2, infos);
  outputBlock.reset(new AqlItemBlock(itemBlockManager, inputBlock->size(), 1));
  OutputAqlItemRow output2{std::move(outputBlock), infos.getOutputRegisters(),
                           infos.registersToKeep(), infos.registersToClear()};
  std::tie(state, stats) = testee2.produceRows(output2);
  EXPECT_EQ(state, ExecutionState::DONE);
  EXPECT_EQ(output2.numRowsWritten(), 2);

  ExpectedValues(output2, {"[ [42, 42] ]", ""}, {{1,0}});
}

// TODO: This is a "death test" with malformed shadow row layout (an irrelevant shadow row before any other row)
// See https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#death-tests-and-threads

using SubqueryEndExecutorTest_DeathTest = SubqueryEndExecutorTest;

TEST_F(SubqueryEndExecutorTest_DeathTest, no_shadow_row) {
  SharedAqlItemBlockPtr outputBlock;
  SharedAqlItemBlockPtr inputBlock =
    buildBlock<1>(itemBlockManager, {{1}});

  SingleRowFetcherHelper<false> fetcher(itemBlockManager, inputBlock->size(), false, inputBlock);

  SubqueryEndExecutor testee(fetcher, infos);

  NoStats stats{};
  ExecutionState state{ExecutionState::HASMORE};

  outputBlock.reset(new AqlItemBlock(itemBlockManager, inputBlock->size(), 1));
  OutputAqlItemRow output{std::move(outputBlock), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  EXPECT_DEATH(std::tie(state, stats) = testee.produceRows(output), ".*");
}

TEST_F(SubqueryEndExecutorTest_DeathTest, misplaced_irrelevant_shadowrow) {
  SharedAqlItemBlockPtr outputBlock;
  SharedAqlItemBlockPtr inputBlock =
      buildBlock<1>(itemBlockManager, {{42}, {42}, {42}}, {{1, 1}, {2, 1}});

  SingleRowFetcherHelper<false> fetcher(itemBlockManager, inputBlock->size(), false, inputBlock);

  SubqueryEndExecutor testee(fetcher, infos);

  NoStats stats{};
  ExecutionState state{ExecutionState::HASMORE};

  outputBlock.reset(new AqlItemBlock(itemBlockManager, inputBlock->size(), 1));
  OutputAqlItemRow output{std::move(outputBlock), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear()};
  EXPECT_DEATH(std::tie(state, stats) = testee.produceRows(output), ".*");
}
