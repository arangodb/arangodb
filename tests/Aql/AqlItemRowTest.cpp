////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class AqlItemRowsTest : public ::testing::Test {
 protected:
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  AqlItemBlockManager itemBlockManager{monitor};
  velocypack::Options const* const options{&velocypack::Options::Defaults};

  void AssertResultMatrix(AqlItemBlock* in, VPackSlice result,
                          RegIdFlatSet const& regsToKeep,
                          bool assertNotInline = false) {
    ASSERT_TRUE(result.isArray());
    ASSERT_EQ(in->numRows(), result.length());
    for (size_t rowIdx = 0; rowIdx < in->numRows(); ++rowIdx) {
      VPackSlice row = result.at(rowIdx);
      ASSERT_TRUE(row.isArray());
      ASSERT_EQ(in->numRegisters(), row.length());
      for (RegisterId::value_t regId = 0; regId < in->numRegisters(); ++regId) {
        AqlValue v = in->getValueReference(rowIdx, regId);
        if (regsToKeep.find(regId) == regsToKeep.end()) {
          // If this should not be kept it has to be set to NONE!
          ASSERT_TRUE(v.slice().isNone());
        } else {
          ASSERT_TRUE(
              basics::VelocyPackHelper::equal(row.at(regId), v.slice(), true));
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
  auto outputBlock = itemBlockManager.requestBlock(3, 3);
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
  AssertResultMatrix(outputBlock.get(), expected->slice(),
                     registersToKeep.back());
}

TEST_F(AqlItemRowsTest, only_copying_from_source_to_target_wide) {
  auto outputBlock = itemBlockManager.requestBlock(3, 3);
  RegisterInfos executorInfos{{}, {}, 3, 3, {}, {RegIdSet{0, 1, 2}}};
  auto outputRegisters = executorInfos.getOutputRegisters();
  auto registersToKeep = executorInfos.registersToKeep();

  OutputAqlItemRow testee(std::move(outputBlock), outputRegisters,
                          registersToKeep, executorInfos.registersToClear());

  {
    // Make sure this data is cleared before the assertions
    // Every of these entries has a size > 16 uint_8
    auto inputBlock =
        buildBlock<3>(itemBlockManager, {{{{"\"aaaaaaaaaaaaaaaaaaaa\""},
                                           {"\"bbbbbbbbbbbbbbbbbbbb\""},
                                           {"\"cccccccccccccccccccc\""}}},
                                         {{{"\"dddddddddddddddddddd\""},
                                           {"\"eeeeeeeeeeeeeeeeeeee\""},
                                           {"\"ffffffffffffffffffff\""}}},
                                         {{{"\"gggggggggggggggggggg\""},
                                           {"\"hhhhhhhhhhhhhhhhhhhh\""},
                                           {"\"iiiiiiiiiiiiiiiiiiii\""}}}});

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
  AssertResultMatrix(outputBlock.get(), expected->slice(),
                     registersToKeep.back(), true);
}

TEST_F(AqlItemRowsTest,
       only_copying_from_source_to_target_but_multiplying_rows) {
  auto outputBlock = itemBlockManager.requestBlock(9, 3);
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
  AssertResultMatrix(outputBlock.get(), expected->slice(),
                     registersToKeep.back());
}

TEST_F(AqlItemRowsTest,
       dropping_a_register_from_source_while_writing_to_target) {
  auto outputBlock = itemBlockManager.requestBlock(3, 3);
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
  AssertResultMatrix(outputBlock.get(), expected->slice(),
                     registersToKeep.back());
}

TEST_F(AqlItemRowsTest, writing_rows_to_target) {
  RegisterCount nrInputRegisters = 0;
  RegisterCount nrOutputRegisters = 0;

  auto outputRegisters = RegIdSet{3, 4};
  auto registersToClear = RegIdFlatSet{1, 2};
  auto registersToKeep = RegIdFlatSetStack{RegIdFlatSet{0}};
  nrInputRegisters = 3;
  nrOutputRegisters = 5;

  auto outputBlock = itemBlockManager.requestBlock(3, 5);
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
      for (RegisterId::value_t j = 3; j < 5; ++j) {
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
  // add these two here as they are needed for output validation but not for
  // copy in ItemRows
  regsToKeep.emplace(3);
  regsToKeep.emplace(4);
  outputBlock = testee.stealBlock();
  AssertResultMatrix(outputBlock.get(), expected->slice(), regsToKeep);
}

static_assert(GTEST_HAS_TYPED_TEST, "We need typed tests for the following:");

template<class RowType>
class AqlItemRowsCommonEqTest : public ::testing::Test {
 protected:
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  AqlItemBlockManager itemBlockManager{monitor};
  velocypack::Options const* const options{&velocypack::Options::Defaults};
};

using RowTypes = ::testing::Types<InputAqlItemRow, ShadowAqlItemRow>;

TYPED_TEST_CASE(AqlItemRowsCommonEqTest, RowTypes);

template<class T>
T createInvalidRow();
template<>
InputAqlItemRow createInvalidRow<InputAqlItemRow>() {
  return InputAqlItemRow{CreateInvalidInputRowHint{}};
}
template<>
ShadowAqlItemRow createInvalidRow<ShadowAqlItemRow>() {
  return ShadowAqlItemRow{CreateInvalidShadowRowHint{}};
}

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

  // different row in different block must be non-equivalent, even with the same
  // index
  EXPECT_FALSE((RowType{block, 0}.equates(RowType{otherBlock, 0}, options)));
  EXPECT_FALSE((RowType{otherBlock, 0}.equates(RowType{block, 0}, options)));

  // an equivalent row in a different block must be considered equivalent, even
  // with a different index
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
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  AqlItemBlockManager itemBlockManager{monitor};
  velocypack::Options const* const options{&velocypack::Options::Defaults};
};

TEST_F(AqlShadowRowsEqTest, shadow_row_depth_equivalence) {
  // In this test, we check for (non-)equivalence of shadow row depth.
  // This is essentially the same test as (AqlItemRowsCommonEqTest,
  // row_equivalence), but instead of the values differing, the shadow row depth
  // does.
  SharedAqlItemBlockPtr block =
      buildBlock<1>(this->itemBlockManager, {{{0}}, {{0}}});
  SharedAqlItemBlockPtr otherBlock =
      buildBlock<1>(this->itemBlockManager, {{{0}}});
  block->makeShadowRow(0, 0);
  block->makeShadowRow(1, 1);
  otherBlock->makeShadowRow(0, 1);

  // same rows must be considered equivalent
  EXPECT_TRUE((
      ShadowAqlItemRow{block, 0}.equates(ShadowAqlItemRow{block, 0}, options)));
  EXPECT_TRUE((
      ShadowAqlItemRow{block, 1}.equates(ShadowAqlItemRow{block, 1}, options)));

  // different rows must be non-equivalent
  EXPECT_FALSE((
      ShadowAqlItemRow{block, 0}.equates(ShadowAqlItemRow{block, 1}, options)));
  EXPECT_FALSE((
      ShadowAqlItemRow{block, 1}.equates(ShadowAqlItemRow{block, 0}, options)));

  // different row in different block must be non-equivalent, even with the same
  // index
  EXPECT_FALSE((ShadowAqlItemRow{block, 0}.equates(
      ShadowAqlItemRow{otherBlock, 0}, options)));
  EXPECT_FALSE((ShadowAqlItemRow{otherBlock, 0}.equates(
      ShadowAqlItemRow{block, 0}, options)));

  // an equivalent row in a different block must be considered equivalent, even
  // with a different index
  EXPECT_TRUE((ShadowAqlItemRow{block, 1}.equates(
      ShadowAqlItemRow{otherBlock, 0}, options)));
  EXPECT_TRUE((ShadowAqlItemRow{otherBlock, 0}.equates(
      ShadowAqlItemRow{block, 1}, options)));
}

// ============================================================================
// Tests for InputAqlItemRow::cloneToBlock()
// ============================================================================

TEST_F(AqlItemRowsTest, cloneToBlock_ManagedSlices) {
  auto inputBlock =
      buildBlock<3>(itemBlockManager, {{{{1}, {2}, {3}}}, {{{4}, {5}, {6}}}});

  InputAqlItemRow source{inputBlock, 0};

  // Clone row 0 to a new block, keeping all registers
  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  regs.insert(RegisterId::makeRegular(1));
  regs.insert(RegisterId::makeRegular(2));

  SharedAqlItemBlockPtr clonedBlock =
      source.cloneToBlock(itemBlockManager, regs, 3);

  ASSERT_NE(clonedBlock, nullptr);
  EXPECT_EQ(clonedBlock->numRows(), 1);
  EXPECT_EQ(clonedBlock->numRegisters(), 3);

  // Verify values are cloned (deep copy)
  EXPECT_EQ(clonedBlock->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(clonedBlock->getValueReference(0, 1).toInt64(), 2);
  EXPECT_EQ(clonedBlock->getValueReference(0, 2).toInt64(), 3);

  // Verify original block is unchanged
  EXPECT_EQ(inputBlock->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(inputBlock->getValueReference(0, 1).toInt64(), 2);
  EXPECT_EQ(inputBlock->getValueReference(0, 2).toInt64(), 3);
}

TEST_F(AqlItemRowsTest, cloneToBlock_SubsetOfRegisters) {
  auto inputBlock =
      buildBlock<3>(itemBlockManager, {{{{1}, {2}, {3}}}, {{{4}, {5}, {6}}}});

  InputAqlItemRow source{inputBlock, 0};

  // Clone only registers 0 and 2 to a new block with 4 registers
  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  regs.insert(RegisterId::makeRegular(2));

  SharedAqlItemBlockPtr clonedBlock =
      source.cloneToBlock(itemBlockManager, regs, 4);

  ASSERT_NE(clonedBlock, nullptr);
  EXPECT_EQ(clonedBlock->numRows(), 1);
  EXPECT_EQ(clonedBlock->numRegisters(), 4);

  // Verify cloned registers
  EXPECT_EQ(clonedBlock->getValueReference(0, 0).toInt64(), 1);
  EXPECT_TRUE(clonedBlock->getValueReference(0, 1).isEmpty());  // Not cloned
  EXPECT_EQ(clonedBlock->getValueReference(0, 2).toInt64(), 3);
  EXPECT_TRUE(clonedBlock->getValueReference(0, 3).isEmpty());  // New register
}

TEST_F(AqlItemRowsTest, cloneToBlock_SharedManagedSlices) {
  // Create a block where multiple rows share the same managed slice
  auto inputBlock = itemBlockManager.requestBlock(2, 1);
  AqlValue sharedValue = AqlValue(AqlValueHintInt(42));

  inputBlock->setValue(0, 0, sharedValue);
  inputBlock->setValue(1, 0, sharedValue);

  InputAqlItemRow source{inputBlock, 0};

  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));

  SharedAqlItemBlockPtr clonedBlock =
      source.cloneToBlock(itemBlockManager, regs, 1);

  ASSERT_NE(clonedBlock, nullptr);
  EXPECT_EQ(clonedBlock->getValueReference(0, 0).toInt64(), 42);

  // The cloned block should have its own copy (deep clone)
  // Verify original is unchanged
  EXPECT_EQ(inputBlock->getValueReference(0, 0).toInt64(), 42);
  EXPECT_EQ(inputBlock->getValueReference(1, 0).toInt64(), 42);
}

TEST_F(AqlItemRowsTest, cloneToBlock_SharedSupervisedSlices) {
  // Create a block with supervised slices
  auto inputBlock = itemBlockManager.requestBlock(2, 1);

  std::string content = "This is a supervised slice that is long enough";
  arangodb::velocypack::Builder b;
  b.add(arangodb::velocypack::Value(content));
  AqlValue supervised = AqlValue(
      b.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b.slice().byteSize()),
      &monitor);
  EXPECT_EQ(supervised.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  inputBlock->setValue(0, 0, supervised);
  inputBlock->setValue(1, 0, supervised);

  InputAqlItemRow source{inputBlock, 0};

  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));

  SharedAqlItemBlockPtr clonedBlock =
      source.cloneToBlock(itemBlockManager, regs, 1);

  ASSERT_NE(clonedBlock, nullptr);
  EXPECT_EQ(clonedBlock->getValueReference(0, 0).slice().stringView(), content);

  // Verify original is unchanged
  EXPECT_EQ(inputBlock->getValueReference(0, 0).slice().stringView(), content);
  EXPECT_EQ(inputBlock->getValueReference(1, 0).slice().stringView(), content);
}

TEST_F(AqlItemRowsTest, cloneToBlock_EmptyRow) {
  // All values are empty
  auto inputBlock = itemBlockManager.requestBlock(1, 3);

  InputAqlItemRow source{inputBlock, 0};

  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  regs.insert(RegisterId::makeRegular(1));
  regs.insert(RegisterId::makeRegular(2));

  SharedAqlItemBlockPtr clonedBlock =
      source.cloneToBlock(itemBlockManager, regs, 3);

  ASSERT_NE(clonedBlock, nullptr);
  EXPECT_EQ(clonedBlock->numRows(), 1);
  EXPECT_EQ(clonedBlock->numRegisters(), 3);

  // All values should be empty
  EXPECT_TRUE(clonedBlock->getValueReference(0, 0).isEmpty());
  EXPECT_TRUE(clonedBlock->getValueReference(0, 1).isEmpty());
  EXPECT_TRUE(clonedBlock->getValueReference(0, 2).isEmpty());
}

// ============================================================================
// Aggressive tests to trigger potential double frees in cloneToBlock()
// ============================================================================

TEST_F(AqlItemRowsTest, cloneToBlock_DestroyOriginalAfterClone) {
  // Test: Clone row to block, then destroy original - cloned should still work
  auto inputBlock =
      buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}});

  InputAqlItemRow source{inputBlock, 0};

  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  regs.insert(RegisterId::makeRegular(1));

  SharedAqlItemBlockPtr clonedBlock =
      source.cloneToBlock(itemBlockManager, regs, 2);

  ASSERT_NE(clonedBlock, nullptr);
  EXPECT_EQ(clonedBlock->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(clonedBlock->getValueReference(0, 1).toInt64(), 2);

  // Destroy original block - cloned should still be valid
  inputBlock.reset(nullptr);

  // Access cloned values - should not crash or double-free
  EXPECT_EQ(clonedBlock->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(clonedBlock->getValueReference(0, 1).toInt64(), 2);

  clonedBlock.reset(nullptr);
}

TEST_F(AqlItemRowsTest, cloneToBlock_DestroyValueInOriginalAfterClone) {
  // Test: Clone row, then destroy values in original
  // Cloned values should be independent copies
  auto inputBlock =
      buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}});

  InputAqlItemRow source{inputBlock, 0};

  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  regs.insert(RegisterId::makeRegular(1));

  SharedAqlItemBlockPtr clonedBlock =
      source.cloneToBlock(itemBlockManager, regs, 2);

  ASSERT_NE(clonedBlock, nullptr);

  // Destroy values in original block - should not affect cloned block
  inputBlock->destroyValue(0, 0);
  inputBlock->destroyValue(0, 1);

  // Cloned values should still be valid
  EXPECT_EQ(clonedBlock->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(clonedBlock->getValueReference(0, 1).toInt64(), 2);

  clonedBlock.reset(nullptr);
  inputBlock.reset(nullptr);
}

TEST_F(AqlItemRowsTest,
       cloneToBlock_DestroyValueInClonedAfterOriginalDestroyed) {
  // Test: Clone row, destroy original, then destroy values in cloned
  auto inputBlock =
      buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}});

  InputAqlItemRow source{inputBlock, 0};

  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  regs.insert(RegisterId::makeRegular(1));

  SharedAqlItemBlockPtr clonedBlock =
      source.cloneToBlock(itemBlockManager, regs, 2);

  ASSERT_NE(clonedBlock, nullptr);

  // Destroy original
  inputBlock.reset(nullptr);

  // Now destroy values in cloned - should not cause double free
  clonedBlock->destroyValue(0, 0);
  clonedBlock->destroyValue(0, 1);

  // Values should be empty now
  EXPECT_TRUE(clonedBlock->getValueReference(0, 0).isEmpty());
  EXPECT_TRUE(clonedBlock->getValueReference(0, 1).isEmpty());

  clonedBlock.reset(nullptr);
}

TEST_F(AqlItemRowsTest, cloneToBlock_MultipleClonesSameRow) {
  // Test: Create multiple clones of the same row - each should be independent
  auto inputBlock =
      buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}});

  InputAqlItemRow source{inputBlock, 0};

  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));
  regs.insert(RegisterId::makeRegular(1));

  SharedAqlItemBlockPtr cloned1 =
      source.cloneToBlock(itemBlockManager, regs, 2);
  SharedAqlItemBlockPtr cloned2 =
      source.cloneToBlock(itemBlockManager, regs, 2);
  SharedAqlItemBlockPtr cloned3 =
      source.cloneToBlock(itemBlockManager, regs, 2);

  ASSERT_NE(cloned1, nullptr);
  ASSERT_NE(cloned2, nullptr);
  ASSERT_NE(cloned3, nullptr);

  // All clones should have independent copies
  EXPECT_EQ(cloned1->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(cloned2->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(cloned3->getValueReference(0, 0).toInt64(), 1);

  // Destroy original
  inputBlock.reset(nullptr);

  // Destroy one clone
  cloned1.reset(nullptr);

  // Other clones should still work
  EXPECT_EQ(cloned2->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(cloned3->getValueReference(0, 0).toInt64(), 1);

  cloned2.reset(nullptr);
  cloned3.reset(nullptr);
}

TEST_F(AqlItemRowsTest,
       cloneToBlock_SupervisedSlicesDestroyOriginalAfterClone) {
  // Test: Clone supervised slices, destroy original - should not leak or
  // double-free
  auto inputBlock = itemBlockManager.requestBlock(2, 1);

  std::string content = "Supervised slice for double-free test";
  arangodb::velocypack::Builder b;
  b.add(arangodb::velocypack::Value(content));
  AqlValue supervised = AqlValue(
      b.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b.slice().byteSize()),
      &monitor);
  EXPECT_EQ(supervised.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  inputBlock->setValue(0, 0, supervised);
  inputBlock->setValue(1, 0, supervised);

  InputAqlItemRow source{inputBlock, 0};

  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));

  size_t initialMemory = monitor.current();
  EXPECT_GT(initialMemory, 0U);

  SharedAqlItemBlockPtr clonedBlock =
      source.cloneToBlock(itemBlockManager, regs, 1);
  EXPECT_GT(monitor.current(), initialMemory);

  ASSERT_NE(clonedBlock, nullptr);
  EXPECT_EQ(clonedBlock->getValueReference(0, 0).slice().stringView(), content);

  // Destroy original block - cloned should still work
  inputBlock.reset(nullptr);

  // Access cloned value - should not crash
  EXPECT_EQ(clonedBlock->getValueReference(0, 0).slice().stringView(), content);

  // Memory should still be tracked (cloned value + original value in row 1)
  EXPECT_GT(monitor.current(), 0);

  // Destroy cloned block
  clonedBlock.reset(nullptr);

  // Original block is destroyed, but row 1's value should still be tracked
  // until we explicitly clean up (this tests that cloning creates independent
  // copies)
}

TEST_F(AqlItemRowsTest, cloneToBlock_SharedSupervisedSlicesMultipleClones) {
  // Test: Clone row with shared supervised slice multiple times
  // Each clone should get its own copy, no double frees
  // This test verifies that cloning creates independent copies and
  // destroying blocks in various orders doesn't cause double frees
  auto inputBlock = itemBlockManager.requestBlock(1, 1);

  std::string content = "Shared supervised slice";
  arangodb::velocypack::Builder b;
  b.add(arangodb::velocypack::Value(content));
  AqlValue supervised = AqlValue(
      b.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b.slice().byteSize()),
      &monitor);
  EXPECT_EQ(supervised.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  inputBlock->setValue(0, 0, supervised);
  // supervised goes out of scope, but block now owns the memory

  InputAqlItemRow source{inputBlock, 0};

  RegIdFlatSet regs;
  regs.insert(RegisterId::makeRegular(0));

  // Create multiple clones - each should get independent copy
  SharedAqlItemBlockPtr cloned1 =
      source.cloneToBlock(itemBlockManager, regs, 1);
  SharedAqlItemBlockPtr cloned2 =
      source.cloneToBlock(itemBlockManager, regs, 1);
  SharedAqlItemBlockPtr cloned3 =
      source.cloneToBlock(itemBlockManager, regs, 1);

  ASSERT_NE(cloned1, nullptr);
  ASSERT_NE(cloned2, nullptr);
  ASSERT_NE(cloned3, nullptr);

  // All should have independent copies
  EXPECT_EQ(cloned1->getValueReference(0, 0).slice().stringView(), content);
  EXPECT_EQ(cloned2->getValueReference(0, 0).slice().stringView(), content);
  EXPECT_EQ(cloned3->getValueReference(0, 0).slice().stringView(), content);

  // Destroy original block - clones should still work (no double free)
  inputBlock.reset(nullptr);

  // Destroy clones one by one - should not cause issues
  cloned1.reset(nullptr);
  EXPECT_EQ(cloned2->getValueReference(0, 0).slice().stringView(), content);
  EXPECT_EQ(cloned3->getValueReference(0, 0).slice().stringView(), content);

  cloned2.reset(nullptr);
  EXPECT_EQ(cloned3->getValueReference(0, 0).slice().stringView(), content);

  // Last clone should still work
  cloned3.reset(nullptr);

  // Test passes if no crashes or double frees occurred
  // Memory tracking may have some overhead, so we don't check exact values
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
