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

#include "Aql/InputAqlItemRow.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/VelocyPackHelper.h"

#include <boost/container/vector.hpp>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

namespace arangodb {
namespace tests {
namespace aql {

class AqlItemBlockTest : public ::testing::Test {
 protected:
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  AqlItemBlockManager itemBlockManager{monitor};
  velocypack::Options const* const options{&velocypack::Options::Defaults};
  std::shared_ptr<VPackBuilder> _dummyData{VPackParser::fromJson(R"(
          [
              "a",
              "b",
              "c",
              "d",
              {
                  "a": "b",
                  "this": "is too large to be inlined"
              },
              {
                  "c": "d",
                  "this": "is too large to be inlined"
              }
          ]
      )")};

  VPackSlice dummyData(size_t index) {
    TRI_ASSERT(index < _dummyData->slice().length());
    return _dummyData->slice().at(index);
  }

  void compareWithDummy(SharedAqlItemBlockPtr const& testee, size_t row,
                        RegisterId column, size_t dummyIndex) {
    EXPECT_EQ(VelocyPackHelper::compare(
                  testee->getValueReference(row, column).slice(),
                  dummyData(dummyIndex), false),
              0)
        << testee->getValueReference(row, column).slice().toJson() << " vs "
        << dummyData(dummyIndex).toJson();
  }

  void assertShadowRowIndexes(SharedAqlItemBlockPtr const& testee,
                              std::vector<size_t> indexes) {
    if (indexes.empty()) {
      EXPECT_FALSE(testee->hasShadowRows());
      EXPECT_EQ(testee->numShadowRows(), 0);
    } else {
      EXPECT_TRUE(testee->hasShadowRows());
      EXPECT_GT(testee->numShadowRows(), 0);
    }

    auto [sb, se] = testee->getShadowRowIndexesFrom(0);
    EXPECT_EQ(std::distance(sb, se), indexes.size());
    for (auto const& it : indexes) {
      EXPECT_NE(std::find(sb, se, it), se);
    }
    size_t old = 0;
    // Set is ordered increasingly
    auto it = sb;
    while (it != se) {
      ASSERT_LE(old, *it);
      old = *it;
      ++it;
    }
  }
};

TEST_F(AqlItemBlockTest, test_get_block_id) {
  EXPECT_EQ(itemBlockManager.getBucketId(0), 0);
  EXPECT_EQ(itemBlockManager.getBucketId(1), 0);
  EXPECT_EQ(itemBlockManager.getBucketId(2), 1);
  EXPECT_EQ(itemBlockManager.getBucketId(3), 1);
  EXPECT_EQ(itemBlockManager.getBucketId(4), 2);
  EXPECT_EQ(itemBlockManager.getBucketId(5), 2);
  EXPECT_EQ(itemBlockManager.getBucketId(6), 2);
  EXPECT_EQ(itemBlockManager.getBucketId(7), 2);
  EXPECT_EQ(itemBlockManager.getBucketId(8), 3);
  EXPECT_EQ(itemBlockManager.getBucketId(9), 3);
  EXPECT_EQ(itemBlockManager.getBucketId(10), 3);
  EXPECT_EQ(itemBlockManager.getBucketId(15), 3);
  EXPECT_EQ(itemBlockManager.getBucketId(16), 4);
  EXPECT_EQ(itemBlockManager.getBucketId(31), 4);
  EXPECT_EQ(itemBlockManager.getBucketId(32), 5);
  EXPECT_EQ(itemBlockManager.getBucketId(63), 5);
  EXPECT_EQ(itemBlockManager.getBucketId(64), 6);
  EXPECT_EQ(itemBlockManager.getBucketId(100), 6);
  EXPECT_EQ(itemBlockManager.getBucketId(127), 6);
  EXPECT_EQ(itemBlockManager.getBucketId(128), 7);
  EXPECT_EQ(itemBlockManager.getBucketId(255), 7);
  EXPECT_EQ(itemBlockManager.getBucketId(256), 8);
  EXPECT_EQ(itemBlockManager.getBucketId(511), 8);
  EXPECT_EQ(itemBlockManager.getBucketId(512), 9);
  EXPECT_EQ(itemBlockManager.getBucketId(1000), 9);
  EXPECT_EQ(itemBlockManager.getBucketId(1023), 9);
  EXPECT_EQ(itemBlockManager.getBucketId(1024), 10);
  EXPECT_EQ(itemBlockManager.getBucketId(2048), 11);
  EXPECT_EQ(itemBlockManager.getBucketId(4095), 11);
  EXPECT_EQ(itemBlockManager.getBucketId(4096), 11);
  EXPECT_EQ(itemBlockManager.getBucketId(4097), 11);
  EXPECT_EQ(itemBlockManager.getBucketId(5000), 11);
  EXPECT_EQ(itemBlockManager.getBucketId(8192), 11);
  EXPECT_EQ(itemBlockManager.getBucketId(10000), 11);
  EXPECT_EQ(itemBlockManager.getBucketId(100000), 11);
  EXPECT_EQ(itemBlockManager.getBucketId(1000000), 11);
  EXPECT_EQ(itemBlockManager.getBucketId(10000000), 11);
}

TEST_F(AqlItemBlockTest, test_read_values_reference) {
  auto block = buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}});
  EXPECT_EQ(block->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(block->getValueReference(0, 1).toInt64(), 2);
  EXPECT_EQ(block->getValueReference(1, 0).toInt64(), 3);
  EXPECT_EQ(block->getValueReference(1, 1).toInt64(), 4);
}

TEST_F(AqlItemBlockTest, test_read_values_copy) {
  auto block = buildBlock<2>(itemBlockManager, {{{{5}, {6}}}, {{{7}, {8}}}});
  EXPECT_EQ(block->getValue(0, 0).toInt64(), 5);
  EXPECT_EQ(block->getValue(0, 1).toInt64(), 6);
  EXPECT_EQ(block->getValue(1, 0).toInt64(), 7);
  EXPECT_EQ(block->getValue(1, 1).toInt64(), 8);
}

TEST_F(AqlItemBlockTest, test_write_values) {
  auto block = itemBlockManager.requestBlock(2, 2);
  AqlValue a(AqlValueHintInt(1));
  AqlValue b(AqlValueHintInt(2));
  AqlValue c(AqlValueHintInt(3));
  AqlValue d(AqlValueHintInt(4));

  block->setValue(0, 0, a);
  block->setValue(0, 1, b);
  block->setValue(1, 0, c);
  block->setValue(1, 1, d);

  EXPECT_EQ(block->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(block->getValueReference(0, 1).toInt64(), 2);
  EXPECT_EQ(block->getValueReference(1, 0).toInt64(), 3);
  EXPECT_EQ(block->getValueReference(1, 1).toInt64(), 4);
}

TEST_F(AqlItemBlockTest, test_emplace_values) {
  auto block = itemBlockManager.requestBlock(2, 2);
  block->emplaceValue(0, 0, AqlValueHintInt(1));
  block->emplaceValue(0, 1, AqlValueHintInt(2));
  block->emplaceValue(1, 0, AqlValueHintInt(3));
  block->emplaceValue(1, 1, AqlValueHintInt(4));

  EXPECT_EQ(block->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(block->getValueReference(0, 1).toInt64(), 2);
  EXPECT_EQ(block->getValueReference(1, 0).toInt64(), 3);
  EXPECT_EQ(block->getValueReference(1, 1).toInt64(), 4);
}

TEST_F(AqlItemBlockTest, test_block_contains_shadow_rows) {
  auto block = buildBlock<1>(itemBlockManager, {{{5}}, {{6}}, {{7}}, {{8}}});

  // No shadow Rows included
  assertShadowRowIndexes(block, {});

  // add a shadow row
  block->makeShadowRow(1, 0);
  assertShadowRowIndexes(block, {1});

  // add another shadow row
  block->makeShadowRow(2, 0);
  assertShadowRowIndexes(block, {1, 2});
}

TEST_F(AqlItemBlockTest, test_serialization_deserialization_1) {
  auto block = itemBlockManager.requestBlock(2, 2);

  block->emplaceValue(0, 0, dummyData(0));
  block->emplaceValue(0, 1, dummyData(1));
  block->emplaceValue(1, 0, dummyData(2));
  block->emplaceValue(1, 1, dummyData(4));
  VPackBuilder result;
  result.openObject();
  block->toVelocyPack(nullptr, result);
  ASSERT_TRUE(result.isOpenObject());
  result.close();

  SharedAqlItemBlockPtr testee =
      itemBlockManager.requestAndInitBlock(result.slice());

  // Check exposed attributes
  EXPECT_EQ(testee->numRows(), block->numRows());
  EXPECT_EQ(testee->numRegisters(), block->numRegisters());
  EXPECT_EQ(testee->numEntries(), block->numEntries());
  EXPECT_EQ(testee->capacity(), block->capacity());
  // check data
  compareWithDummy(testee, 0, 0, 0);
  compareWithDummy(testee, 0, 1, 1);
  compareWithDummy(testee, 1, 0, 2);
  compareWithDummy(testee, 1, 1, 4);
  assertShadowRowIndexes(testee, {});
}

TEST_F(AqlItemBlockTest, test_serialization_deserialization_2) {
  // Only write a single value in a single line with 3 registers.
  // Use first, second and third position independently.
  // All other positions remain empty
  for (RegisterId::value_t dataPosition = 0; dataPosition < 3; ++dataPosition) {
    auto block = itemBlockManager.requestBlock(1, 3);

    block->emplaceValue(0, dataPosition, dummyData(4));
    VPackBuilder result;
    result.openObject();
    block->toVelocyPack(nullptr, result);
    ASSERT_TRUE(result.isOpenObject());
    result.close();

    SharedAqlItemBlockPtr testee =
        itemBlockManager.requestAndInitBlock(result.slice());

    // Check exposed attributes
    EXPECT_EQ(testee->numRows(), block->numRows());
    EXPECT_EQ(testee->numRegisters(), block->numRegisters());
    EXPECT_EQ(testee->numEntries(), block->numEntries());
    EXPECT_EQ(testee->capacity(), block->capacity());
    // check data
    for (RegisterId::value_t i = 0; i < 3; ++i) {
      if (i == dataPosition) {
        compareWithDummy(testee, 0, i, 4);
      } else {
        EXPECT_TRUE(testee->getValueReference(0, i).isEmpty());
      }
    }
    assertShadowRowIndexes(testee, {});
  }
}

TEST_F(AqlItemBlockTest, test_serialization_deserialization_3) {
  // Only write a single value twice in a single line with 3 registers.
  // Use first, second and third position as empty independently.
  // All other positions use the value
  for (RegisterId::value_t dataPosition = 0; dataPosition < 3; ++dataPosition) {
    auto block = itemBlockManager.requestBlock(1, 3);
    for (RegisterId::value_t i = 0; i < 3; ++i) {
      if (i != dataPosition) {
        block->emplaceValue(0, i, dummyData(4));
      }
    }
    VPackBuilder result;
    result.openObject();
    block->toVelocyPack(nullptr, result);
    ASSERT_TRUE(result.isOpenObject());
    result.close();

    SharedAqlItemBlockPtr testee =
        itemBlockManager.requestAndInitBlock(result.slice());

    // Check exposed attributes
    EXPECT_EQ(testee->numRows(), block->numRows());
    EXPECT_EQ(testee->numRegisters(), block->numRegisters());
    EXPECT_EQ(testee->numEntries(), block->numEntries());
    EXPECT_EQ(testee->capacity(), block->capacity());
    // check data
    for (RegisterId::value_t i = 0; i < 3; ++i) {
      if (i != dataPosition) {
        compareWithDummy(testee, 0, i, 4);
      } else {
        EXPECT_TRUE(testee->getValueReference(0, i).isEmpty());
      }
    }
    assertShadowRowIndexes(testee, {});
  }
}

TEST_F(AqlItemBlockTest, test_serialization_deserialization_shadowrows) {
  auto block = itemBlockManager.requestBlock(4, 2);

  block->emplaceValue(0, 0, dummyData(0));
  block->emplaceValue(0, 1, dummyData(1));

  block->emplaceValue(1, 0, dummyData(0));
  block->emplaceValue(1, 1, dummyData(1));
  block->makeShadowRow(1, 0);

  block->emplaceValue(2, 0, dummyData(2));
  block->emplaceValue(2, 1, dummyData(4));

  block->emplaceValue(3, 0, dummyData(2));
  block->emplaceValue(3, 1, dummyData(4));
  block->makeShadowRow(3, 0);

  assertShadowRowIndexes(block, {1, 3});

  VPackBuilder result;
  result.openObject();
  block->toVelocyPack(nullptr, result);
  ASSERT_TRUE(result.isOpenObject());
  result.close();

  SharedAqlItemBlockPtr testee =
      itemBlockManager.requestAndInitBlock(result.slice());

  // Check exposed attributes
  EXPECT_EQ(testee->numRows(), block->numRows());
  EXPECT_EQ(testee->numRegisters(), block->numRegisters());
  EXPECT_EQ(testee->numEntries(), block->numEntries());
  EXPECT_EQ(testee->capacity(), block->capacity());
  // check data
  EXPECT_FALSE(testee->isShadowRow(0));
  compareWithDummy(testee, 0, 0, 0);
  compareWithDummy(testee, 0, 1, 1);

  EXPECT_TRUE(testee->isShadowRow(1));
  compareWithDummy(testee, 1, 0, 0);
  compareWithDummy(testee, 1, 1, 1);

  EXPECT_FALSE(testee->isShadowRow(2));
  compareWithDummy(testee, 2, 0, 2);
  compareWithDummy(testee, 2, 1, 4);

  EXPECT_TRUE(testee->isShadowRow(3));
  compareWithDummy(testee, 3, 0, 2);
  compareWithDummy(testee, 3, 1, 4);

  assertShadowRowIndexes(testee, {1, 3});
}

TEST_F(AqlItemBlockTest, test_serialization_deserialization_slices) {
  auto block = itemBlockManager.requestBlock(2, 2);
  block->emplaceValue(0, 0, dummyData(4));
  block->emplaceValue(0, 1, dummyData(5));
  block->emplaceValue(1, 0, dummyData(0));
  block->emplaceValue(1, 1, dummyData(1));
  {
    auto slice = block->slice(0, 1);
    VPackBuilder result;
    result.openObject();
    slice->toVelocyPack(nullptr, result);
    ASSERT_TRUE(result.isOpenObject());
    result.close();

    SharedAqlItemBlockPtr testee =
        itemBlockManager.requestAndInitBlock(result.slice());

    // Check exposed attributes
    EXPECT_EQ(testee->numRows(), 1);
    EXPECT_EQ(testee->numRegisters(), block->numRegisters());
    // check data
    compareWithDummy(testee, 0, 0, 4);
    compareWithDummy(testee, 0, 1, 5);
  }

  {
    auto slice = block->slice(1, 2);
    VPackBuilder result;
    result.openObject();
    slice->toVelocyPack(nullptr, result);
    ASSERT_TRUE(result.isOpenObject());
    result.close();

    SharedAqlItemBlockPtr testee =
        itemBlockManager.requestAndInitBlock(result.slice());

    // Check exposed attributes
    EXPECT_EQ(testee->numRows(), 1);
    EXPECT_EQ(testee->numRegisters(), block->numRegisters());
    // check data
    compareWithDummy(testee, 0, 0, 0);
    compareWithDummy(testee, 0, 1, 1);

    assertShadowRowIndexes(testee, {});
  }
}

TEST_F(AqlItemBlockTest, test_serialization_deserialization_with_ranges) {
  auto block = itemBlockManager.requestBlock(3, 2);
  block->emplaceValue(0, 0, dummyData(4));
  block->emplaceValue(0, 1, dummyData(5));
  block->emplaceValue(1, 0, dummyData(0));
  block->emplaceValue(1, 1, dummyData(1));
  block->emplaceValue(2, 0, dummyData(2));
  block->emplaceValue(2, 1, dummyData(3));
  {
    // test range 0->1
    VPackBuilder result;
    result.openObject();
    block->toVelocyPack(0, 1, nullptr, result);
    ASSERT_TRUE(result.isOpenObject());
    result.close();

    SharedAqlItemBlockPtr testee =
        itemBlockManager.requestAndInitBlock(result.slice());

    // Check exposed attributes
    EXPECT_EQ(testee->numRows(), 1);
    EXPECT_EQ(testee->numRegisters(), block->numRegisters());
    // check data
    compareWithDummy(testee, 0, 0, 4);
    compareWithDummy(testee, 0, 1, 5);

    assertShadowRowIndexes(testee, {});
  }

  {
    // Test range 1->2
    VPackBuilder result;
    result.openObject();
    block->toVelocyPack(1, 2, nullptr, result);
    ASSERT_TRUE(result.isOpenObject());
    result.close();

    SharedAqlItemBlockPtr testee =
        itemBlockManager.requestAndInitBlock(result.slice());

    // Check exposed attributes
    EXPECT_EQ(testee->numRows(), 1);
    EXPECT_EQ(testee->numRegisters(), block->numRegisters());
    // check data
    compareWithDummy(testee, 0, 0, 0);
    compareWithDummy(testee, 0, 1, 1);

    assertShadowRowIndexes(testee, {});
  }

  {
    // Test range 0->2
    VPackBuilder result;
    result.openObject();
    block->toVelocyPack(0, 2, nullptr, result);
    ASSERT_TRUE(result.isOpenObject());
    result.close();

    SharedAqlItemBlockPtr testee =
        itemBlockManager.requestAndInitBlock(result.slice());

    // Check exposed attributes
    EXPECT_EQ(testee->numRows(), 2);
    EXPECT_EQ(testee->numRegisters(), block->numRegisters());
    // check data
    compareWithDummy(testee, 0, 0, 4);
    compareWithDummy(testee, 0, 1, 5);
    compareWithDummy(testee, 1, 0, 0);
    compareWithDummy(testee, 1, 1, 1);

    assertShadowRowIndexes(testee, {});
  }
}

TEST_F(AqlItemBlockTest, test_serialization_deserialization_input_row) {
  auto block = itemBlockManager.requestBlock(2, 2);
  block->emplaceValue(0, 0, dummyData(4));
  block->emplaceValue(0, 1, dummyData(5));
  block->emplaceValue(1, 0, dummyData(0));
  block->emplaceValue(1, 1, dummyData(1));
  {
    InputAqlItemRow input{block, 0};
    VPackBuilder result;
    result.openObject();
    input.toVelocyPack(nullptr, result);
    ASSERT_TRUE(result.isOpenObject());
    result.close();

    SharedAqlItemBlockPtr testee =
        itemBlockManager.requestAndInitBlock(result.slice());

    // Check exposed attributes
    EXPECT_EQ(testee->numRows(), 1);
    EXPECT_EQ(testee->numRegisters(), block->numRegisters());
    // check data
    compareWithDummy(testee, 0, 0, 4);
    compareWithDummy(testee, 0, 1, 5);
  }

  {
    InputAqlItemRow input{block, 1};
    VPackBuilder result;
    result.openObject();
    input.toVelocyPack(nullptr, result);
    ASSERT_TRUE(result.isOpenObject());
    result.close();

    SharedAqlItemBlockPtr testee =
        itemBlockManager.requestAndInitBlock(result.slice());

    // Check exposed attributes
    EXPECT_EQ(testee->numRows(), 1);
    EXPECT_EQ(testee->numRegisters(), block->numRegisters());
    // check data
    compareWithDummy(testee, 0, 0, 0);
    compareWithDummy(testee, 0, 1, 1);
    assertShadowRowIndexes(testee, {});
  }
}

// ============================================================================
// Tests for AqlItemBlock::cloneDataAndMoveShadow()
// ============================================================================

TEST_F(AqlItemBlockTest, cloneDataAndMoveShadow_NoShadowRows) {
  auto block = buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}});

  // Clone data and move shadow (but there are no shadow rows)
  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->numRows(), block->numRows());
  EXPECT_EQ(cloned->numRegisters(), block->numRegisters());
  EXPECT_FALSE(cloned->hasShadowRows());

  // Verify data is cloned (deep copy)
  EXPECT_EQ(cloned->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(cloned->getValueReference(0, 1).toInt64(), 2);
  EXPECT_EQ(cloned->getValueReference(1, 0).toInt64(), 3);
  EXPECT_EQ(cloned->getValueReference(1, 1).toInt64(), 4);

  // Verify original is unchanged
  EXPECT_EQ(block->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(block->getValueReference(0, 1).toInt64(), 2);
  EXPECT_EQ(block->getValueReference(1, 0).toInt64(), 3);
  EXPECT_EQ(block->getValueReference(1, 1).toInt64(), 4);
}

TEST_F(AqlItemBlockTest, cloneDataAndMoveShadow_WithShadowRows) {
  auto block = buildBlock<2>(itemBlockManager,
                             {{{{1}, {2}}}, {{{3}, {4}}}, {{{5}, {6}}}},
                             /* shadow row */ {{/* row */ 1, /* depth */ 0}});

  // Row 1 is a shadow row
  ASSERT_TRUE(block->isShadowRow(1));
  ASSERT_FALSE(block->isShadowRow(0));
  ASSERT_FALSE(block->isShadowRow(2));

  // Clone data and move shadow rows
  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->numRows(), block->numRows());
  EXPECT_EQ(cloned->numRegisters(), block->numRegisters());
  EXPECT_TRUE(cloned->hasShadowRows());
  EXPECT_TRUE(cloned->isShadowRow(1));
  EXPECT_EQ(cloned->getShadowRowDepth(1), 0);

  // Verify data rows are cloned (deep copy)
  EXPECT_EQ(cloned->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(cloned->getValueReference(0, 1).toInt64(), 2);
  EXPECT_EQ(cloned->getValueReference(2, 0).toInt64(), 5);
  EXPECT_EQ(cloned->getValueReference(2, 1).toInt64(), 6);

  // Shadow row data should be moved (stolen from original)
  EXPECT_EQ(cloned->getValueReference(1, 0).toInt64(), 3);
  EXPECT_EQ(cloned->getValueReference(1, 1).toInt64(), 4);

  // Original shadow row should be empty (values were stolen)
  EXPECT_TRUE(block->getValueReference(1, 0).isEmpty());
  EXPECT_TRUE(block->getValueReference(1, 1).isEmpty());
}

TEST_F(AqlItemBlockTest, cloneDataAndMoveShadow_SharedManagedSlices) {
  auto block = itemBlockManager.requestBlock(3, 1);

  // Create a managed slice and use it in multiple rows
  AqlValue shared = AqlValue(AqlValueHintInt(100));
  block->setValue(0, 0, shared);
  block->setValue(1, 0, shared);
  block->setValue(2, 0, shared);

  // Make row 1 a shadow row
  block->makeShadowRow(1, 0);

  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->numRows(), 3);
  EXPECT_EQ(cloned->numRegisters(), 1);
  EXPECT_TRUE(cloned->isShadowRow(1));

  // Data rows (0, 2) should be cloned
  EXPECT_EQ(cloned->getValueReference(0, 0).toInt64(), 100);
  EXPECT_EQ(cloned->getValueReference(2, 0).toInt64(), 100);

  // Shadow row (1) should be moved
  EXPECT_EQ(cloned->getValueReference(1, 0).toInt64(), 100);
  EXPECT_TRUE(
      block->getValueReference(1, 0).isEmpty());  // Stolen from original
}

TEST_F(AqlItemBlockTest, cloneDataAndMoveShadow_SharedSupervisedSlices) {
  auto block = itemBlockManager.requestBlock(3, 1);

  // Create a supervised slice and use it in multiple rows
  std::string content = "Supervised slice content";
  arangodb::velocypack::Builder b;
  b.add(arangodb::velocypack::Value(content));
  AqlValue supervised = AqlValue(
      b.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b.slice().byteSize()),
      &monitor);
  EXPECT_EQ(supervised.type(), AqlValue::VPACK_SUPERVISED_SLICE);

  block->setValue(0, 0, supervised);
  block->setValue(1, 0, supervised);
  block->setValue(2, 0, supervised);
  auto initialMemory = monitor.current();

  // Make row 1 a shadow row
  block->makeShadowRow(1, 0);

  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();
  // This assertion is tricky
  // The first row's SupervisedSlide was clone (new allocation)
  // The second row's SupervisedSlice was moved (no new allocation)
  // The third row's SupervisedSlice was shared because it was cached by the
  // first row (no new allocation)
  EXPECT_EQ(monitor.current(), initialMemory * 2);

  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->numRows(), 3);
  EXPECT_EQ(cloned->numRegisters(), 1);
  EXPECT_TRUE(cloned->isShadowRow(1));

  // Data rows (0, 2) should be cloned
  EXPECT_EQ(cloned->getValueReference(0, 0).slice().stringView(), content);
  EXPECT_EQ(cloned->getValueReference(2, 0).slice().stringView(), content);

  // Shadow row (1) should be moved
  EXPECT_EQ(cloned->getValueReference(1, 0).slice().stringView(), content);
  EXPECT_TRUE(
      block->getValueReference(1, 0).isEmpty());  // Stolen from original

  // Clean up
  cloned.reset(nullptr);
  block.reset(nullptr);

  // All memory should be released
  EXPECT_EQ(monitor.current(), 0U);
}

TEST_F(AqlItemBlockTest, cloneDataAndMoveShadow_SupervisedSlices) {
  auto block = itemBlockManager.requestBlock(3, 1);

  size_t memSize = 0;
  std::string content = "Supervised slice content";
  for (size_t i = 0; i < 3; i++) {
    velocypack::Builder b;
    b.add(velocypack::Value(content));
    AqlValue supervised = AqlValue(b.slice(), &monitor);
    block->setValue(i, 0, supervised);
    memSize = supervised.memoryUsage();
  }
  auto initialMemory = monitor.current();

  // Make row 1 a shadow row
  block->makeShadowRow(1, 0);

  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();
  // The original block has independent three SupervisedSlices
  // The cloned block should also have three independent SupervisedSlices
  // but, the original second one was stolen by the cloned block
  // so we subtract one memSize
  EXPECT_EQ(monitor.current(), initialMemory * 2 - memSize);

  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->numRows(), 3);
  EXPECT_EQ(cloned->numRegisters(), 1);
  EXPECT_TRUE(cloned->isShadowRow(1));

  // Data rows (0, 2) should be cloned
  EXPECT_EQ(cloned->getValueReference(0, 0).slice().stringView(), content);
  EXPECT_EQ(cloned->getValueReference(2, 0).slice().stringView(), content);

  // Shadow row (1) should be moved
  EXPECT_EQ(cloned->getValueReference(1, 0).slice().stringView(), content);
  EXPECT_TRUE(
      block->getValueReference(1, 0).isEmpty());  // Stolen from original

  // Clean up
  cloned.reset(nullptr);
  block.reset(nullptr);

  // All memory should be released
  EXPECT_EQ(monitor.current(), 0U);
}

TEST_F(AqlItemBlockTest, cloneDataAndMoveShadow_MultipleShadowRows) {
  auto block =
      buildBlock<1>(itemBlockManager, {{{{1}}, {{2}}, {{3}}, {{4}}, {{5}}}},
                    {{1, 0}, {3, 1}});

  // Rows 1 and 3 are shadow rows
  ASSERT_TRUE(block->isShadowRow(1));
  ASSERT_TRUE(block->isShadowRow(3));
  ASSERT_EQ(block->getShadowRowDepth(1), 0);
  ASSERT_EQ(block->getShadowRowDepth(3), 1);

  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->numRows(), 5);
  EXPECT_TRUE(cloned->hasShadowRows());
  EXPECT_TRUE(cloned->isShadowRow(1));
  EXPECT_TRUE(cloned->isShadowRow(3));
  EXPECT_EQ(cloned->getShadowRowDepth(1), 0);
  EXPECT_EQ(cloned->getShadowRowDepth(3), 1);

  // Data rows should be cloned
  EXPECT_EQ(cloned->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(cloned->getValueReference(2, 0).toInt64(), 3);
  EXPECT_EQ(cloned->getValueReference(4, 0).toInt64(), 5);

  // Shadow rows should be moved (stolen)
  EXPECT_EQ(cloned->getValueReference(1, 0).toInt64(), 2);
  EXPECT_EQ(cloned->getValueReference(3, 0).toInt64(), 4);

  // Original shadow rows should be empty
  EXPECT_TRUE(block->getValueReference(1, 0).isEmpty());
  EXPECT_TRUE(block->getValueReference(3, 0).isEmpty());
}

TEST_F(AqlItemBlockTest, cloneDataAndMoveShadow_MixedManagedAndSupervised) {
  auto block = itemBlockManager.requestBlock(3, 2);

  // Row 0: managed slices
  AqlValue managed1 = AqlValue(AqlValueHintInt(10));
  AqlValue managed2 = AqlValue(AqlValueHintInt(20));
  block->setValue(0, 0, managed1);
  block->setValue(0, 1, managed2);

  // Row 1: supervised slices (will be shadow row)
  std::string content1 = "Supervised content 1";
  std::string content2 = "Supervised content 2";
  arangodb::velocypack::Builder b1, b2;
  b1.add(arangodb::velocypack::Value(content1));
  b2.add(arangodb::velocypack::Value(content2));
  AqlValue supervised1 = AqlValue(
      b1.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b1.slice().byteSize()),
      &monitor);
  EXPECT_EQ(supervised1.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  AqlValue supervised2 = AqlValue(
      b2.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b2.slice().byteSize()),
      &monitor);
  EXPECT_EQ(supervised2.type(), AqlValue::VPACK_SUPERVISED_SLICE);
  block->setValue(1, 0, supervised1);
  block->setValue(1, 1, supervised2);
  block->makeShadowRow(1, 0);

  // Row 2: mixed
  block->setValue(2, 0, managed1);     // Reuse managed
  block->setValue(2, 1, supervised1);  // Reuse supervised

  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->numRows(), 3);
  EXPECT_EQ(cloned->numRegisters(), 2);
  EXPECT_TRUE(cloned->isShadowRow(1));

  // Row 0 (data): cloned
  EXPECT_EQ(cloned->getValueReference(0, 0).toInt64(), 10);
  EXPECT_EQ(cloned->getValueReference(0, 1).toInt64(), 20);

  // Row 1 (shadow): moved
  EXPECT_EQ(cloned->getValueReference(1, 0).slice().stringView(), content1);
  EXPECT_EQ(cloned->getValueReference(1, 1).slice().stringView(), content2);
  EXPECT_TRUE(block->getValueReference(1, 0).isEmpty());  // Stolen
  EXPECT_TRUE(block->getValueReference(1, 1).isEmpty());  // Stolen

  // Row 2 (data): cloned
  EXPECT_EQ(cloned->getValueReference(2, 0).toInt64(), 10);
  EXPECT_EQ(cloned->getValueReference(2, 1).slice().stringView(), content1);

  // Clean up
  cloned.reset(nullptr);
  block.reset(nullptr);
  EXPECT_EQ(monitor.current(), 0U);
}

// ============================================================================
// Aggressive tests to trigger potential double frees in
// cloneDataAndMoveShadow()
// ============================================================================

TEST_F(AqlItemBlockTest, cloneDataAndMoveShadow_DestroyOriginalAfterClone) {
  // Test: Clone block, then destroy original - cloned should still work
  auto block = buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}});

  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(cloned->getValueReference(0, 1).toInt64(), 2);

  // Destroy original block - cloned should still be valid
  block.reset(nullptr);

  // Access cloned values - should not crash or double-free
  EXPECT_EQ(cloned->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(cloned->getValueReference(0, 1).toInt64(), 2);
  EXPECT_EQ(cloned->getValueReference(1, 0).toInt64(), 3);
  EXPECT_EQ(cloned->getValueReference(1, 1).toInt64(), 4);

  cloned.reset(nullptr);
}

TEST_F(AqlItemBlockTest,
       cloneDataAndMoveShadow_DestroyValueInOriginalAfterClone) {
  // Test: Clone block, then try to destroy values in original
  // This should be safe because cloned values are independent copies
  auto block = buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}});

  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  ASSERT_NE(cloned, nullptr);

  // Destroy values in original block - should not affect cloned block
  block->destroyValue(0, 0);
  block->destroyValue(0, 1);
  block->destroyValue(1, 0);
  block->destroyValue(1, 1);

  // Cloned values should still be valid
  EXPECT_EQ(cloned->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(cloned->getValueReference(0, 1).toInt64(), 2);
  EXPECT_EQ(cloned->getValueReference(1, 0).toInt64(), 3);
  EXPECT_EQ(cloned->getValueReference(1, 1).toInt64(), 4);

  cloned.reset(nullptr);
  block.reset(nullptr);
}

TEST_F(AqlItemBlockTest,
       cloneDataAndMoveShadow_DestroyValueInClonedAfterOriginalDestroyed) {
  // Test: Clone block, destroy original, then destroy values in cloned
  // This tests that cloned block manages its own memory correctly
  auto block = buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}});

  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  ASSERT_NE(cloned, nullptr);

  // Destroy original
  block.reset(nullptr);

  // Now destroy values in cloned - should not cause double free
  cloned->destroyValue(0, 0);
  cloned->destroyValue(0, 1);
  cloned->destroyValue(1, 0);
  cloned->destroyValue(1, 1);

  // Values should be empty now
  EXPECT_TRUE(cloned->getValueReference(0, 0).isEmpty());
  EXPECT_TRUE(cloned->getValueReference(0, 1).isEmpty());
  EXPECT_TRUE(cloned->getValueReference(1, 0).isEmpty());
  EXPECT_TRUE(cloned->getValueReference(1, 1).isEmpty());

  cloned.reset(nullptr);
}

TEST_F(AqlItemBlockTest,
       cloneDataAndMoveShadow_ShadowRowDestroyOriginalAfterMove) {
  // Test: Move shadow row, then destroy original - should not double-free
  auto block =
      buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}}, {{1, 0}});

  ASSERT_TRUE(block->isShadowRow(1));

  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->getValueReference(1, 0).toInt64(), 3);
  EXPECT_EQ(cloned->getValueReference(1, 1).toInt64(), 4);

  // Original shadow row should be empty (stolen)
  EXPECT_TRUE(block->getValueReference(1, 0).isEmpty());
  EXPECT_TRUE(block->getValueReference(1, 1).isEmpty());

  // Destroy original block - cloned shadow row should still work
  block.reset(nullptr);

  // Access cloned shadow row - should not crash
  EXPECT_EQ(cloned->getValueReference(1, 0).toInt64(), 3);
  EXPECT_EQ(cloned->getValueReference(1, 1).toInt64(), 4);
  EXPECT_TRUE(cloned->isShadowRow(1));

  cloned.reset(nullptr);
}

TEST_F(AqlItemBlockTest, cloneDataAndMoveShadow_MultipleClonesSameBlock) {
  // Test: Create multiple clones of the same block - each should be independent
  auto block = buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}});

  SharedAqlItemBlockPtr cloned1 = block->cloneDataAndMoveShadow();
  SharedAqlItemBlockPtr cloned2 = block->cloneDataAndMoveShadow();
  SharedAqlItemBlockPtr cloned3 = block->cloneDataAndMoveShadow();

  ASSERT_NE(cloned1, nullptr);
  ASSERT_NE(cloned2, nullptr);
  ASSERT_NE(cloned3, nullptr);

  // All clones should have independent copies
  EXPECT_EQ(cloned1->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(cloned2->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(cloned3->getValueReference(0, 0).toInt64(), 1);

  // Destroy original
  block.reset(nullptr);

  // Destroy one clone
  cloned1.reset(nullptr);

  // Other clones should still work
  EXPECT_EQ(cloned2->getValueReference(0, 0).toInt64(), 1);
  EXPECT_EQ(cloned3->getValueReference(0, 0).toInt64(), 1);

  cloned2.reset(nullptr);
  cloned3.reset(nullptr);
}

TEST_F(AqlItemBlockTest,
       cloneDataAndMoveShadow_SupervisedSlicesDestroyOriginalAfterClone) {
  // Test: Clone supervised slices, destroy original - should not leak or
  // double-free
  auto block = itemBlockManager.requestBlock(2, 1);

  std::string content = "Supervised slice for double-free test";
  arangodb::velocypack::Builder b;
  b.add(arangodb::velocypack::Value(content));
  AqlValue supervised = AqlValue(
      b.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b.slice().byteSize()),
      &monitor);

  block->setValue(0, 0, supervised);
  block->setValue(1, 0, supervised);

  size_t initialMemory = monitor.current();
  EXPECT_GT(initialMemory, 0U);

  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->getValueReference(0, 0).slice().stringView(), content);
  EXPECT_EQ(cloned->getValueReference(1, 0).slice().stringView(), content);

  // Destroy original block - cloned should still work
  block.reset(nullptr);

  // Access cloned values - should not crash
  EXPECT_EQ(cloned->getValueReference(0, 0).slice().stringView(), content);
  EXPECT_EQ(cloned->getValueReference(1, 0).slice().stringView(), content);

  // Memory should still be tracked
  EXPECT_GT(monitor.current(), 0U);

  // Destroy cloned block - all memory should be released
  cloned.reset(nullptr);
  EXPECT_EQ(monitor.current(), 0U);
}

TEST_F(AqlItemBlockTest,
       cloneDataAndMoveShadow_ShadowRowSupervisedSlicesDestroyAfterMove) {
  // Test: Move shadow row with supervised slices, destroy original - should not
  // double-free
  auto block = itemBlockManager.requestBlock(3, 1);

  // Row 0: data row with supervised slice
  std::string content1 = "Data row supervised slice";
  arangodb::velocypack::Builder b1;
  b1.add(arangodb::velocypack::Value(content1));
  AqlValue supervised1 = AqlValue(
      b1.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b1.slice().byteSize()),
      &monitor);
  block->setValue(0, 0, supervised1);

  // Row 1: shadow row with supervised slice (will be moved)
  std::string content2 = "Shadow row supervised slice";
  arangodb::velocypack::Builder b2;
  b2.add(arangodb::velocypack::Value(content2));
  AqlValue supervised2 = AqlValue(
      b2.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b2.slice().byteSize()),
      &monitor);
  block->setValue(1, 0, supervised2);
  block->makeShadowRow(1, 0);

  // Row 2: data row with supervised slice
  block->setValue(2, 0, supervised1);  // Reuse supervised1

  size_t initialMemory = monitor.current();

  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  ASSERT_NE(cloned, nullptr);
  EXPECT_TRUE(cloned->isShadowRow(1));

  // Shadow row should be moved (stolen from original)
  EXPECT_EQ(cloned->getValueReference(1, 0).slice().stringView(), content2);
  EXPECT_TRUE(block->getValueReference(1, 0).isEmpty());

  // Destroy original block - cloned should still work
  block.reset(nullptr);

  // Access all cloned values - should not crash
  EXPECT_EQ(cloned->getValueReference(0, 0).slice().stringView(), content1);
  EXPECT_EQ(cloned->getValueReference(1, 0).slice().stringView(), content2);
  EXPECT_EQ(cloned->getValueReference(2, 0).slice().stringView(), content1);

  // Memory should still be tracked
  EXPECT_GT(monitor.current(), initialMemory);

  // Destroy cloned block - all memory should be released
  cloned.reset(nullptr);
  EXPECT_EQ(monitor.current(), 0U);
}

// ============================================================================
// Aggressive tests to trigger use-after-free in serialization (toVelocyPack)
// Based on real bug: heap-use-after-free when serializing after
// cloneDataAndMoveShadow
// ============================================================================

TEST_F(AqlItemBlockTest, toVelocyPack_AfterCloneDataAndMoveShadow) {
  // Test: Serialize block after cloneDataAndMoveShadow - shadow rows were
  // stolen This should not cause use-after-free when accessing stolen values
  auto block =
      buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}}, {{1, 0}});

  ASSERT_TRUE(block->isShadowRow(1));

  // Clone and move shadow row (steals values from original)
  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  ASSERT_NE(cloned, nullptr);
  EXPECT_TRUE(block->getValueReference(1, 0).isEmpty());  // Stolen

  // Try to serialize original block - should handle empty/stolen values
  // gracefully This could trigger use-after-free if toVelocyPack accesses freed
  // memory
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  // Verify serialization succeeded
  EXPECT_TRUE(result.slice().hasKey("nrItems"));
  EXPECT_TRUE(result.slice().hasKey("raw"));

  cloned.reset(nullptr);
  block.reset(nullptr);
}

TEST_F(AqlItemBlockTest, toVelocyPack_AfterStealAndEraseValue) {
  // Test: Serialize block after stealing values - should not access freed
  // memory
  auto block = buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}});

  // Steal a value
  AqlValue stolen = block->stealAndEraseValue(0, 0);
  EXPECT_TRUE(block->getValueReference(0, 0).isEmpty());

  // Try to serialize - should handle empty value gracefully
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  EXPECT_TRUE(result.slice().hasKey("nrItems"));

  // Destroy stolen value
  stolen.destroy();
  block.reset(nullptr);
}

TEST_F(AqlItemBlockTest,
       toVelocyPack_SupervisedSlicesAfterCloneDataAndMoveShadow) {
  // Test: Serialize supervised slices after cloneDataAndMoveShadow
  // This is the scenario from the error - supervised slices that were moved
  auto block = itemBlockManager.requestBlock(3, 1);

  std::string content1 = "Supervised slice 1";
  std::string content2 = "Supervised slice 2";
  std::string content3 = "Supervised slice 3";

  arangodb::velocypack::Builder b1, b2, b3;
  b1.add(arangodb::velocypack::Value(content1));
  b2.add(arangodb::velocypack::Value(content2));
  b3.add(arangodb::velocypack::Value(content3));

  AqlValue supervised1 = AqlValue(
      b1.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b1.slice().byteSize()),
      &monitor);
  AqlValue supervised2 = AqlValue(
      b2.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b2.slice().byteSize()),
      &monitor);
  AqlValue supervised3 = AqlValue(
      b3.slice(),
      static_cast<arangodb::velocypack::ValueLength>(b3.slice().byteSize()),
      &monitor);

  block->setValue(0, 0, supervised1);
  block->setValue(1, 0, supervised2);
  block->setValue(2, 0, supervised3);
  block->makeShadowRow(1, 0);  // Row 1 is shadow row

  // Clone and move shadow row
  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  ASSERT_NE(cloned, nullptr);
  EXPECT_TRUE(block->getValueReference(1, 0).isEmpty());  // Stolen

  // CRITICAL: Try to serialize original block after shadow row was stolen
  // This is where the use-after-free occurred in the real bug
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  // Should not crash or access freed memory
  EXPECT_TRUE(result.slice().hasKey("nrItems"));
  EXPECT_TRUE(result.slice().hasKey("raw"));

  cloned.reset(nullptr);
  block.reset(nullptr);
  EXPECT_EQ(monitor.current(), 0U);
}

TEST_F(AqlItemBlockTest, rowToSimpleVPack_AfterStealAndEraseValue) {
  // Test: Serialize a row after stealing its values
  auto block = buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}});

  // Steal value from row 0
  AqlValue stolen = block->stealAndEraseValue(0, 0);
  EXPECT_TRUE(block->getValueReference(0, 0).isEmpty());

  // Try to serialize row 0 - should handle empty value
  velocypack::Builder result;
  block->rowToSimpleVPack(0, options, result);

  // Should not crash
  EXPECT_TRUE(result.slice().isArray());

  stolen.destroy();
  block.reset(nullptr);
}

TEST_F(AqlItemBlockTest, toVelocyPack_AfterDestroyValue) {
  // Test: Serialize after destroying some values
  auto block = buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}});

  // Destroy a value
  block->destroyValue(0, 0);
  EXPECT_TRUE(block->getValueReference(0, 0).isEmpty());

  // Try to serialize
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  EXPECT_TRUE(result.slice().hasKey("nrItems"));

  block.reset(nullptr);
}

TEST_F(AqlItemBlockTest, toVelocyPack_MultipleSerializationsAfterClone) {
  // Test: Serialize multiple times after cloning - values should remain valid
  auto block = buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}});

  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  // Serialize original multiple times
  for (int i = 0; i < 3; i++) {
    velocypack::Builder result;
    result.openObject();
    block->toVelocyPack(0, block->numRows(), options, result);
    result.close();
    EXPECT_TRUE(result.slice().hasKey("nrItems"));
  }

  // Serialize cloned multiple times
  for (int i = 0; i < 3; i++) {
    velocypack::Builder result;
    result.openObject();
    cloned->toVelocyPack(0, cloned->numRows(), options, result);
    result.close();
    EXPECT_TRUE(result.slice().hasKey("nrItems"));
  }

  cloned.reset(nullptr);
  block.reset(nullptr);
}

TEST_F(AqlItemBlockTest,
       toVelocyPack_AfterCloneDataAndMoveShadow_AllShadowRows) {
  // Test: All rows are shadow rows, all get stolen, then serialize
  auto block = buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}},
                             {{0, 0}, {1, 1}});

  ASSERT_TRUE(block->isShadowRow(0));
  ASSERT_TRUE(block->isShadowRow(1));

  // Clone and move all shadow rows
  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  ASSERT_NE(cloned, nullptr);
  EXPECT_TRUE(block->getValueReference(0, 0).isEmpty());
  EXPECT_TRUE(block->getValueReference(0, 1).isEmpty());
  EXPECT_TRUE(block->getValueReference(1, 0).isEmpty());
  EXPECT_TRUE(block->getValueReference(1, 1).isEmpty());

  // Try to serialize original - all values are stolen/empty
  velocypack::Builder result;
  result.openObject();
  block->toVelocyPack(0, block->numRows(), options, result);
  result.close();

  EXPECT_TRUE(result.slice().hasKey("nrItems"));

  cloned.reset(nullptr);
  block.reset(nullptr);
}

TEST_F(AqlItemBlockTest, toSimpleVPack_AfterCloneDataAndMoveShadow) {
  // Test: Use toSimpleVPack after cloneDataAndMoveShadow
  auto block =
      buildBlock<2>(itemBlockManager, {{{{1}, {2}}}, {{{3}, {4}}}}, {{1, 0}});

  SharedAqlItemBlockPtr cloned = block->cloneDataAndMoveShadow();

  ASSERT_NE(cloned, nullptr);
  EXPECT_TRUE(block->getValueReference(1, 0).isEmpty());
  EXPECT_TRUE(block->getValueReference(1, 1).isEmpty());

  // Try toSimpleVPack on original block
  velocypack::Builder result;
  block->toSimpleVPack(options, result);

  EXPECT_TRUE(result.slice().hasKey("nrItems"));
  EXPECT_TRUE(result.slice().hasKey("matrix"));

  cloned.reset(nullptr);
  block.reset(nullptr);
}

TEST_F(AqlItemBlockTest, toVelocyPack_ConcurrentAccessAfterSteal) {
  // Test: Access block in multiple ways after stealing values
  auto block = buildBlock<3>(itemBlockManager, {{{{1}}, {{2}}, {{3}}}});

  // Steal middle value
  AqlValue stolen = block->stealAndEraseValue(1, 0);

  // Access in various ways that might trigger use-after-free
  EXPECT_TRUE(block->getValueReference(1, 0).isEmpty());

  // Serialize
  velocypack::Builder result1;
  result1.openObject();
  block->toVelocyPack(0, block->numRows(), options, result1);
  result1.close();

  // Serialize specific row
  velocypack::Builder result2;
  block->rowToSimpleVPack(1, options, result2);

  // Get value (should be empty)
  AqlValue val = block->getValue(1, 0);
  EXPECT_TRUE(val.isEmpty());

  stolen.destroy();
  block.reset(nullptr);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
