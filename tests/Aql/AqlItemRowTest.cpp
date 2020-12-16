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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "AqlItemBlockHelper.h"
#include "gtest/gtest.h"

#include "Aql/AqlItemBlockManager.h"
#include "Aql/AqlItemRowPrinter.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/ShadowAqlItemRow.h"

#include "Basics/VelocyPackHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class AqlItemRowsTest : public ::testing::Test {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{monitor, SerializationFormat::SHADOWROWS};
  velocypack::Options const* const options{&velocypack::Options::Defaults};

  void AssertResultMatrix(AqlItemBlock* in, VPackSlice result,
                          RegIdFlatSet const& regsToKeep, bool assertNotInline = false) {
    ASSERT_TRUE(result.isArray());
    ASSERT_EQ(in->size(), result.length());
    for (size_t rowIdx = 0; rowIdx < in->size(); ++rowIdx) {
      VPackSlice row = result.at(rowIdx);
      ASSERT_TRUE(row.isArray());
      ASSERT_EQ(in->getNrRegs(), row.length());
      for (RegisterId regId = 0; regId < in->getNrRegs(); ++regId) {
        AqlValue v = in->getValueReference(rowIdx, regId);
        if (regsToKeep.find(regId) == regsToKeep.end()) {
          // If this should not be kept it has to be set to NONE!
          ASSERT_TRUE(v.slice().isNone());
        } else {
          ASSERT_TRUE(basics::VelocyPackHelper::equal(row.at(regId), v.slice(), true));
          // Work around test as we are unable to check the type via API.
          if (assertNotInline) {
            // If this object is not inlined it requires some memory
            ASSERT_NE(v.memoryUsage(), 0);
          } else {
            // If it is inlined it does not require memory.
            ASSERT_EQ(v.memoryUsage(), 0);
          }
        }
      }
    }
  }
};

TEST_F(AqlItemRowsTest, only_copying_from_source_to_target_narrow) {
  SharedAqlItemBlockPtr outputBlock{new AqlItemBlock(itemBlockManager, 3, 3)};
  RegisterInfos executorInfos{{}, {}, 3, 3, {}, {RegIdSet{0, 1, 2}}};
  auto outputRegisters = executorInfos.getOutputRegisters();
  auto registersToKeep = executorInfos.registersToKeep();

  OutputAqlItemRow testee(std::move(outputBlock), outputRegisters,
                          registersToKeep, executorInfos.registersToClear());

  {
    // Make sure this data is cleared before the assertions
    auto inputBlock =
        buildBlock<3>(itemBlockManager, {{{{1}, {2}, {3}}},
                                         {{{4}, {5}, {6}}},
                                         {{{"\"a\""}, {"\"b\""}, {"\"c\""}}}});

    InputAqlItemRow source{inputBlock, 0};

    testee.copyRow(source);
    ASSERT_TRUE(testee.produced());

    source = {inputBlock, 1};
    testee.advanceRow();
    testee.copyRow(source);
    ASSERT_TRUE(testee.produced());

    source = {inputBlock, 2};
    testee.advanceRow();
    testee.copyRow(source);
    ASSERT_TRUE(testee.produced());
  }
  auto expected =
      VPackParser::fromJson("[[1,2,3],[4,5,6],[\"a\",\"b\",\"c\"]]");
  outputBlock = testee.stealBlock();
  AssertResultMatrix(outputBlock.get(), expected->slice(), registersToKeep.back());
}

TEST_F(AqlItemRowsTest, only_copying_from_source_to_target_wide) {
  SharedAqlItemBlockPtr outputBlock{new AqlItemBlock(itemBlockManager, 3, 3)};
  RegisterInfos executorInfos{{}, {}, 3, 3, {}, {RegIdSet{0, 1, 2}}};
  auto outputRegisters = executorInfos.getOutputRegisters();
  auto registersToKeep = executorInfos.registersToKeep();

  OutputAqlItemRow testee(std::move(outputBlock), outputRegisters,
                          registersToKeep, executorInfos.registersToClear());

  {
    // Make sure this data is cleared before the assertions
    // Every of these entries has a size > 16 uint_8
    auto inputBlock = buildBlock<3>(
        itemBlockManager,
        {{{{"\"aaaaaaaaaaaaaaaaaaaa\""}, {"\"bbbbbbbbbbbbbbbbbbbb\""}, {"\"cccccccccccccccccccc\""}}},
         {{{"\"dddddddddddddddddddd\""}, {"\"eeeeeeeeeeeeeeeeeeee\""}, {"\"ffffffffffffffffffff\""}}},
         {{{"\"gggggggggggggggggggg\""}, {"\"hhhhhhhhhhhhhhhhhhhh\""}, {"\"iiiiiiiiiiiiiiiiiiii\""}}}});

    InputAqlItemRow source{inputBlock, 0};

    testee.copyRow(source);
    ASSERT_TRUE(testee.produced());

    source = {inputBlock, 1};
    testee.advanceRow();
    testee.copyRow(source);
    ASSERT_TRUE(testee.produced());

    source = {inputBlock, 2};
    testee.advanceRow();
    testee.copyRow(source);
    ASSERT_TRUE(testee.produced());
  }
  auto expected = VPackParser::fromJson(
      "["
      "[\"aaaaaaaaaaaaaaaaaaaa\", \"bbbbbbbbbbbbbbbbbbbb\", "
      "\"cccccccccccccccccccc\"],"
      "[\"dddddddddddddddddddd\", \"eeeeeeeeeeeeeeeeeeee\", "
      "\"ffffffffffffffffffff\"],"
      "[\"gggggggggggggggggggg\", \"hhhhhhhhhhhhhhhhhhhh\", "
      "\"iiiiiiiiiiiiiiiiiiii\"]"
      "]");
  outputBlock = testee.stealBlock();
  AssertResultMatrix(outputBlock.get(), expected->slice(), registersToKeep.back(), true);
}

TEST_F(AqlItemRowsTest, only_copying_from_source_to_target_but_multiplying_rows) {
  SharedAqlItemBlockPtr outputBlock{new AqlItemBlock(itemBlockManager, 9, 3)};
  RegisterInfos executorInfos{{}, {}, 3, 3, {}, {RegIdSet{0, 1, 2}}};
  auto outputRegisters = executorInfos.getOutputRegisters();
  auto registersToKeep = executorInfos.registersToKeep();

  OutputAqlItemRow testee(std::move(outputBlock), outputRegisters,
                          registersToKeep, executorInfos.registersToClear());

  {
    // Make sure this data is cleared before the assertions
    auto inputBlock =
        buildBlock<3>(itemBlockManager, {{{{1}, {2}, {3}}},
                                         {{{4}, {5}, {6}}},
                                         {{{"\"a\""}, {"\"b\""}, {"\"c\""}}}});

    for (size_t i = 0; i < 3; ++i) {
      // Iterate over source rows
      InputAqlItemRow source{inputBlock, i};
      for (size_t j = 0; j < 3; ++j) {
        testee.copyRow(source);
        ASSERT_TRUE(testee.produced());
        if (i < 2 || j < 2) {
          // Not at the last one, we are at the end
          testee.advanceRow();
        }
      }
    }
  }
  auto expected = VPackParser::fromJson(
      "["
      "[1,2,3],"
      "[1,2,3],"
      "[1,2,3],"
      "[4,5,6],"
      "[4,5,6],"
      "[4,5,6],"
      "[\"a\",\"b\",\"c\"],"
      "[\"a\",\"b\",\"c\"],"
      "[\"a\",\"b\",\"c\"]"
      "]");
  outputBlock = testee.stealBlock();
  AssertResultMatrix(outputBlock.get(), expected->slice(), registersToKeep.back());
}

TEST_F(AqlItemRowsTest, dropping_a_register_from_source_while_writing_to_target) {
  SharedAqlItemBlockPtr outputBlock{new AqlItemBlock(itemBlockManager, 3, 3)};
  RegisterInfos executorInfos{{}, {}, 3, 3, RegIdSet{1}, {RegIdSet{0, 2}}};
  auto outputRegisters = executorInfos.getOutputRegisters();
  auto registersToKeep = executorInfos.registersToKeep();

  OutputAqlItemRow testee(std::move(outputBlock), outputRegisters,
                          registersToKeep, executorInfos.registersToClear());

  {
    // Make sure this data is cleared before the assertions
    auto inputBlock =
        buildBlock<3>(itemBlockManager, {{{{1}, {2}, {3}}},
                                         {{{4}, {5}, {6}}},
                                         {{{"\"a\""}, {"\"b\""}, {"\"c\""}}}});

    for (size_t i = 0; i < 3; ++i) {
      // Iterate over source rows
      InputAqlItemRow source{inputBlock, i};
      testee.copyRow(source);
      ASSERT_TRUE(testee.produced());
      if (i < 2) {
        // Not at the last one, we are at the end
        testee.advanceRow();
      }
    }
  }
  auto expected = VPackParser::fromJson(
      "["
      "[1,2,3],"
      "[4,5,6],"
      "[\"a\",\"b\",\"c\"]"
      "]");
  outputBlock = testee.stealBlock();
  AssertResultMatrix(outputBlock.get(), expected->slice(), registersToKeep.back());
}

TEST_F(AqlItemRowsTest, writing_rows_to_target) {
  RegisterId nrInputRegisters = 0;
  RegisterId nrOutputRegisters = 0;

  auto outputRegisters = RegIdSet{3, 4};
  auto registersToClear = RegIdFlatSet{1, 2};
  auto registersToKeep = RegIdFlatSetStack{RegIdFlatSet{0}};
  nrInputRegisters = 3;
  nrOutputRegisters = 5;

  SharedAqlItemBlockPtr outputBlock{new AqlItemBlock(itemBlockManager, 3, 5)};
  RegisterInfos executorInfos{{},
                              outputRegisters,
                              nrInputRegisters,
                              nrOutputRegisters,
                              registersToClear,
                              registersToKeep};

  OutputAqlItemRow testee(std::move(outputBlock), outputRegisters,
                          registersToKeep, executorInfos.registersToClear());

  auto& regsToKeep = registersToKeep.back();
  {
    // Make sure this data is cleared before the assertions
    auto inputBlock =
        buildBlock<3>(itemBlockManager, {{{{1}, {2}, {3}}},
                                         {{{4}, {5}, {6}}},
                                         {{{"\"a\""}, {"\"b\""}, {"\"c\""}}}});

    for (size_t i = 0; i < 3; ++i) {
      // Iterate over source rows
      InputAqlItemRow source{inputBlock, i};
      for (RegisterId j = 3; j < 5; ++j) {
        AqlValue v{AqlValueHintInt{(int64_t)(j + 5)}};
        testee.cloneValueInto(j, source, v);
        if (j == 3) {
          // We are not allowed to declare an incomplete row as produced
          ASSERT_FALSE(testee.produced());
        }
      }
      ASSERT_TRUE(testee.produced());
      if (i < 2) {
        // Not at the last one, we are at the end
        testee.advanceRow();
      }
    }
  }
  auto expected = VPackParser::fromJson(
      "["
      "[1,2,3,8,9],"
      "[4,5,6,8,9],"
      "[\"a\",\"b\",\"c\",8,9]"
      "]");
  // add these two here as they are needed for output validation but not for copy in ItemRows
  regsToKeep.emplace(3);
  regsToKeep.emplace(4);
  outputBlock = testee.stealBlock();
  AssertResultMatrix(outputBlock.get(), expected->slice(), regsToKeep);
}

static_assert(GTEST_HAS_TYPED_TEST, "We need typed tests for the following:");

template <class RowType>
class AqlItemRowsCommonEqTest : public ::testing::Test {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{monitor, SerializationFormat::SHADOWROWS};
  velocypack::Options const* const options{&velocypack::Options::Defaults};
};

using RowTypes = ::testing::Types<InputAqlItemRow, ShadowAqlItemRow>;

TYPED_TEST_CASE(AqlItemRowsCommonEqTest, RowTypes);

template <class T> T createInvalidRow();
template <> InputAqlItemRow createInvalidRow<InputAqlItemRow>() { return InputAqlItemRow{CreateInvalidInputRowHint{}}; }
template <> ShadowAqlItemRow createInvalidRow<ShadowAqlItemRow>() { return ShadowAqlItemRow{CreateInvalidShadowRowHint{}}; }

TYPED_TEST(AqlItemRowsCommonEqTest, row_eq_operators) {
  using RowType = TypeParam;
  // We use the same value (and shadow row depth) for all rows, so we surely
  // test identicality.
  SharedAqlItemBlockPtr block =
      buildBlock<1>(this->itemBlockManager, {{{0}}, {{0}}});
  SharedAqlItemBlockPtr otherBlock =
      buildBlock<1>(this->itemBlockManager, {{{0}}});
  if (std::is_same<RowType, ShadowAqlItemRow>::value) {
    block->makeShadowRow(0, 0);
    block->makeShadowRow(1, 0);
    otherBlock->makeShadowRow(0, 0);
  }

  RowType const invalidRow = createInvalidRow<RowType>();
  RowType const otherInvalidRow = createInvalidRow<RowType>();

  // same rows must be equal
  EXPECT_TRUE((RowType{block, 0}.isSameBlockAndIndex(RowType{block, 0})));
  EXPECT_TRUE((RowType{block, 1}.isSameBlockAndIndex(RowType{block, 1})));

  // different rows in the same block must be non-equal
  EXPECT_FALSE((RowType{block, 0}.isSameBlockAndIndex(RowType{block, 1})));
  EXPECT_FALSE((RowType{block, 1}.isSameBlockAndIndex(RowType{block, 0})));

  // rows in different blocks must be non-equal
  EXPECT_FALSE((RowType{block, 0}.isSameBlockAndIndex(RowType{otherBlock, 0})));
  EXPECT_FALSE((RowType{block, 1}.isSameBlockAndIndex(RowType{otherBlock, 0})));
  EXPECT_FALSE((RowType{otherBlock, 0}.isSameBlockAndIndex(RowType{block, 0})));
  EXPECT_FALSE((RowType{otherBlock, 0}.isSameBlockAndIndex(RowType{block, 1})));

  // comparisons with an invalid row must be false
  EXPECT_FALSE((RowType{block, 0}.isSameBlockAndIndex(invalidRow)));
  EXPECT_FALSE((invalidRow.isSameBlockAndIndex(RowType{block, 0})));

  // two invalid rows must be equal
  EXPECT_TRUE((invalidRow.isSameBlockAndIndex(otherInvalidRow)));
}

TYPED_TEST(AqlItemRowsCommonEqTest, row_equivalence) {
  using RowType = TypeParam;
  auto const options = this->options;
  SharedAqlItemBlockPtr block =
      buildBlock<1>(this->itemBlockManager, {{{0}}, {{1}}});
  SharedAqlItemBlockPtr otherBlock =
      buildBlock<1>(this->itemBlockManager, {{{1}}});
  if (std::is_same<RowType, ShadowAqlItemRow>::value) {
    block->makeShadowRow(0, 0);
    block->makeShadowRow(1, 0);
    otherBlock->makeShadowRow(0, 0);
  }

  RowType const invalidRow = createInvalidRow<RowType>();
  RowType const otherInvalidRow = createInvalidRow<RowType>();

  // same rows must be considered equivalent
  EXPECT_TRUE((RowType{block, 0}.equates(RowType{block, 0}, options)));
  EXPECT_TRUE((RowType{block, 1}.equates(RowType{block, 1}, options)));

  // different rows must be non-equivalent
  EXPECT_FALSE((RowType{block, 0}.equates(RowType{block, 1}, options)));
  EXPECT_FALSE((RowType{block, 1}.equates(RowType{block, 0}, options)));

  // different row in different block must be non-equivalent, even with the same index
  EXPECT_FALSE((RowType{block, 0}.equates(RowType{otherBlock, 0}, options)));
  EXPECT_FALSE((RowType{otherBlock, 0}.equates(RowType{block, 0}, options)));

  // an equivalent row in a different block must be considered equivalent, even with a different index
  EXPECT_TRUE((RowType{block, 1}.equates(RowType{otherBlock, 0}, options)));
  EXPECT_TRUE((RowType{otherBlock, 0}.equates(RowType{block, 1}, options)));

  // comparisons with an invalid row must be false
  EXPECT_FALSE((RowType{block, 0}.equates(invalidRow, options)));
  EXPECT_FALSE((invalidRow.equates(RowType{block, 0}, options)));

  // two invalid rows must be equal
  EXPECT_TRUE((invalidRow.equates(otherInvalidRow, options)));
}

class AqlShadowRowsEqTest : public ::testing::Test {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{monitor, SerializationFormat::SHADOWROWS};
  velocypack::Options const* const options{&velocypack::Options::Defaults};
};

TEST_F(AqlShadowRowsEqTest, shadow_row_depth_equivalence) {
  // In this test, we check for (non-)equivalence of shadow row depth.
  // This is essentially the same test as (AqlItemRowsCommonEqTest, row_equivalence),
  // but instead of the values differing, the shadow row depth does.
  SharedAqlItemBlockPtr block =
      buildBlock<1>(this->itemBlockManager, {{{0}}, {{0}}});
  SharedAqlItemBlockPtr otherBlock =
      buildBlock<1>(this->itemBlockManager, {{{0}}});
  block->makeShadowRow(0, 0);
  block->makeShadowRow(1, 1);
  otherBlock->makeShadowRow(0, 1);

  // same rows must be considered equivalent
  EXPECT_TRUE((ShadowAqlItemRow{block, 0}.equates(ShadowAqlItemRow{block, 0}, options)));
  EXPECT_TRUE((ShadowAqlItemRow{block, 1}.equates(ShadowAqlItemRow{block, 1}, options)));

  // different rows must be non-equivalent
  EXPECT_FALSE((ShadowAqlItemRow{block, 0}.equates(ShadowAqlItemRow{block, 1}, options)));
  EXPECT_FALSE((ShadowAqlItemRow{block, 1}.equates(ShadowAqlItemRow{block, 0}, options)));

  // different row in different block must be non-equivalent, even with the same index
  EXPECT_FALSE((ShadowAqlItemRow{block, 0}.equates(ShadowAqlItemRow{otherBlock, 0}, options)));
  EXPECT_FALSE((ShadowAqlItemRow{otherBlock, 0}.equates(ShadowAqlItemRow{block, 0}, options)));

  // an equivalent row in a different block must be considered equivalent, even with a different index
  EXPECT_TRUE((ShadowAqlItemRow{block, 1}.equates(ShadowAqlItemRow{otherBlock, 0}, options)));
  EXPECT_TRUE((ShadowAqlItemRow{otherBlock, 0}.equates(ShadowAqlItemRow{block, 1}, options)));
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
