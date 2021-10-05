////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "Aql/InputAqlItemRow.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

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
  AqlItemBlockManager itemBlockManager{monitor, SerializationFormat::SHADOWROWS};
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
    EXPECT_EQ(VelocyPackHelper::compare(testee->getValueReference(row, column).slice(),
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
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 2, 2)};
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
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 2, 2)};
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
  auto block = buildBlock<1>(itemBlockManager, {
                                                   {{5}},
                                                   {{6}},
                                                   {{7}},
                                                   {{8}}
                                               });

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
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 2, 2)};

  block->emplaceValue(0, 0, dummyData(0));
  block->emplaceValue(0, 1, dummyData(1));
  block->emplaceValue(1, 0, dummyData(2));
  block->emplaceValue(1, 1, dummyData(4));
  VPackBuilder result;
  result.openObject();
  block->toVelocyPack(nullptr, result);
  ASSERT_TRUE(result.isOpenObject());
  result.close();

  SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

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
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1, 3)};

    block->emplaceValue(0, dataPosition, dummyData(4));
    VPackBuilder result;
    result.openObject();
    block->toVelocyPack(nullptr, result);
    ASSERT_TRUE(result.isOpenObject());
    result.close();

    SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

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
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1, 3)};
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

    SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

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
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 4, 2)};

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

  SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

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
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 2, 2)};
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

    SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

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

    SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

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
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 3, 2)};
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

    SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

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

    SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

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

    SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

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
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 2, 2)};
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

    SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

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

    SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

    // Check exposed attributes
    EXPECT_EQ(testee->numRows(), 1);
    EXPECT_EQ(testee->numRegisters(), block->numRegisters());
    // check data
    compareWithDummy(testee, 0, 0, 0);
    compareWithDummy(testee, 0, 1, 1);
    assertShadowRowIndexes(testee, {});
  }
}

class AqlItemBlockClassicTest : public ::testing::Test {
 protected:
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  AqlItemBlockManager itemBlockManager{monitor, SerializationFormat::CLASSIC};
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
    EXPECT_EQ(VelocyPackHelper::compare(testee->getValueReference(row, column).slice(),
                                        dummyData(dummyIndex), false),
              0)
        << testee->getValueReference(row, column).slice().toJson() << " vs "
        << dummyData(dummyIndex).toJson();
  }
};

TEST_F(AqlItemBlockClassicTest, test_serialization_deserialization_1) {
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 2, 2)};

  block->emplaceValue(0, 0, dummyData(0));
  block->emplaceValue(0, 1, dummyData(1));
  block->emplaceValue(1, 0, dummyData(2));
  block->emplaceValue(1, 1, dummyData(4));
  VPackBuilder result;
  result.openObject();
  block->toVelocyPack(nullptr, result);
  ASSERT_TRUE(result.isOpenObject());
  result.close();

  SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

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
}

TEST_F(AqlItemBlockClassicTest, test_serialization_deserialization_2) {
  // Only write a single value in a single line with 3 registers.
  // Use first, second and third position independently.
  // All other positions remain empty
  for (RegisterId::value_t dataPosition = 0; dataPosition < 3; ++dataPosition) {
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1, 3)};

    block->emplaceValue(0, dataPosition, dummyData(4));
    VPackBuilder result;
    result.openObject();
    block->toVelocyPack(nullptr, result);
    ASSERT_TRUE(result.isOpenObject());
    result.close();

    SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

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
  }
}

TEST_F(AqlItemBlockClassicTest, test_serialization_deserialization_3) {
  // Only write a single value twice in a single line with 3 registers.
  // Use first, second and third position as empty independently.
  // All other positions use the value
  for (RegisterId::value_t dataPosition = 0; dataPosition < 3; ++dataPosition) {
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1, 3)};
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

    SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

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
  }
}

TEST_F(AqlItemBlockClassicTest, test_serialization_deserialization_shadowrows) {
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 4, 2)};

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

  VPackBuilder result;
  result.openObject();
  block->toVelocyPack(nullptr, result);
  ASSERT_TRUE(result.isOpenObject());
  result.close();

  // The shadow row information will be lost!
  SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

  // Check exposed attributes
  EXPECT_EQ(testee->numRows(), block->numRows());
  EXPECT_EQ(testee->numRegisters(), block->numRegisters());
  EXPECT_EQ(testee->numEntries(), block->numEntries());
  EXPECT_EQ(testee->capacity(), block->capacity());
  // check data
  EXPECT_FALSE(testee->isShadowRow(0));
  compareWithDummy(testee, 0, 0, 0);
  compareWithDummy(testee, 0, 1, 1);

  EXPECT_FALSE(testee->isShadowRow(1));
  compareWithDummy(testee, 1, 0, 0);
  compareWithDummy(testee, 1, 1, 1);

  EXPECT_FALSE(testee->isShadowRow(2));
  compareWithDummy(testee, 2, 0, 2);
  compareWithDummy(testee, 2, 1, 4);

  EXPECT_FALSE(testee->isShadowRow(3));
  compareWithDummy(testee, 3, 0, 2);
  compareWithDummy(testee, 3, 1, 4);
}

TEST_F(AqlItemBlockClassicTest, test_serialization_deserialization_slices) {
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 2, 2)};
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

    SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

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

    SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

    // Check exposed attributes
    EXPECT_EQ(testee->numRows(), 1);
    EXPECT_EQ(testee->numRegisters(), block->numRegisters());
    // check data
    compareWithDummy(testee, 0, 0, 0);
    compareWithDummy(testee, 0, 1, 1);
  }
}

TEST_F(AqlItemBlockClassicTest, test_serialization_deserialization_input_row) {
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 2, 2)};
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

    SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

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

    SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

    // Check exposed attributes
    EXPECT_EQ(testee->numRows(), 1);
    EXPECT_EQ(testee->numRegisters(), block->numRegisters());
    // check data
    compareWithDummy(testee, 0, 0, 0);
    compareWithDummy(testee, 0, 1, 1);
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
