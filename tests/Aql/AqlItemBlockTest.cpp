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
////////////////////////////////////////////////////////////////////////////////

#include "AqlItemBlockHelper.h"
#include "gtest/gtest.h"

#include "Aql/InputAqlItemRow.h"
#include "Aql/RegIdFlatSet.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/VelocyPackHelper.h"

#include <boost/container/flat_set.hpp>
#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

#include <cmath>
#include <memory>
#include <string>

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

TEST_F(AqlItemBlockTest, test_serialization_dedups_equal_content) {
  std::string const content = "shared content, too long to be inlined";
  std::string const other = "other content, too long to be inlined";
  AqlValue a{content};
  AqlValue b{content};
  ASSERT_NE(a.data(), b.data());

  auto block = itemBlockManager.requestBlock(3, 2);
  block->setValue(0, 0, a);
  block->setValue(0, 1, b);
  block->setValue(1, 0, AqlValue{other});
  block->setValue(1, 1, a);
  block->setValue(2, 0, b);

  VPackBuilder result;
  result.openObject();
  block->toVelocyPack(nullptr, result);
  result.close();

  // two leading nulls, then one entry per distinct value
  EXPECT_EQ(result.slice().get("raw").length(), 2 + 2);

  SharedAqlItemBlockPtr testee =
      itemBlockManager.requestAndInitBlock(result.slice());
  auto stringAt = [&](size_t row, RegisterId::value_t col) {
    return testee->getValueReference(row, col).slice().stringView();
  };
  EXPECT_EQ(stringAt(0, 0), content);
  EXPECT_EQ(stringAt(0, 1), content);
  EXPECT_EQ(stringAt(1, 0), other);
  EXPECT_EQ(stringAt(1, 1), content);
  EXPECT_EQ(stringAt(2, 0), content);
}

TEST_F(AqlItemBlockTest, test_serialization_keeps_distinct_encodings) {
  auto makeDouble = [](double d) {
    VPackBuilder b;
    b.add(VPackValue(d));
    return AqlValue{b.slice()};
  };
  auto block = itemBlockManager.requestBlock(6, 1);
  block->setValue(0, 0, AqlValue{AqlValueHintInt{42}});
  block->setValue(1, 0, makeDouble(42.0));
  block->setValue(2, 0, makeDouble(0.0));
  block->setValue(3, 0, makeDouble(-0.0));
  block->setValue(4, 0, AqlValue{VPackParser::fromJson("[]")->slice()});
  block->setValue(5, 0, AqlValue{VPackParser::fromJson("[null]")->slice()});

  VPackBuilder result;
  result.openObject();
  block->toVelocyPack(nullptr, result);
  result.close();

  EXPECT_EQ(result.slice().get("raw").length(), 2 + 6);

  SharedAqlItemBlockPtr testee =
      itemBlockManager.requestAndInitBlock(result.slice());
  EXPECT_TRUE(testee->getValueReference(0, 0).slice().isInteger());
  EXPECT_TRUE(testee->getValueReference(1, 0).slice().isDouble());
  EXPECT_FALSE(
      std::signbit(testee->getValueReference(2, 0).slice().getDouble()));
  EXPECT_TRUE(
      std::signbit(testee->getValueReference(3, 0).slice().getDouble()));
  EXPECT_EQ(testee->getValueReference(4, 0).length(), 0);
  EXPECT_EQ(testee->getValueReference(5, 0).length(), 1);
}

TEST_F(AqlItemBlockTest, test_clone_to_block_dedups_equal_content) {
  auto const expected = VPackParser::fromJson(
      R"({"thisField": "is too large to be inlined, deliberately so"})");
  auto bytes = expected->slice();
  DocumentData doc =
      std::make_unique<std::string>(bytes.startAs<char>(), bytes.byteSize());

  auto block = itemBlockManager.requestBlock(1, 4);
  block->setValue(0, 0, AqlValue{doc});
  block->setValue(0, 1, AqlValue{bytes});
  block->setValue(0, 2, AqlValue{bytes});
  block->setValue(0, 3, AqlValue{std::string(200, 'x')});
  ASSERT_EQ(block->getValueReference(0, 0).type(),
            AqlValue::VPACK_MANAGED_STRING);
  ASSERT_NE(block->getValueReference(0, 1).data(),
            block->getValueReference(0, 2).data());

  RegIdFlatSet const registers{RegisterId{0}, RegisterId{1}, RegisterId{2},
                               RegisterId{3}};
  SharedAqlItemBlockPtr copy =
      InputAqlItemRow{block, 0}.cloneToBlock(itemBlockManager, registers, 4);
  AqlValue const& first = copy->getValueReference(0, 0);
  EXPECT_NE(first.data(), block->getValueReference(0, 1).data());
  block.reset(nullptr);

  EXPECT_EQ(first.type(), AqlValue::VPACK_MANAGED_SLICE);
  EXPECT_TRUE(first.slice().binaryEquals(expected->slice()));
  for (RegisterId::value_t col : {1, 2}) {
    EXPECT_EQ(copy->getValueReference(0, col).data(), first.data());
  }
  EXPECT_NE(copy->getValueReference(0, 3).data(), first.data());
}

// only the first clone gets the shadow row's data, as Gather expects
TEST_F(AqlItemBlockTest,
       test_scatter_clone_shadow_row_only_one_client_gets_data) {
  auto const json =
      R"({"thisField": "is too large to be inlined, deliberately so"})";
  auto const expected = Parser::fromJson(json);

  auto block = itemBlockManager.requestBlock(2, 1);
  block->setValue(
      0, 0, AqlValue{AqlValueHintSliceCopy{Parser::fromJson(json)->slice()}});
  block->setValue(
      1, 0, AqlValue{AqlValueHintSliceCopy{Parser::fromJson(json)->slice()}});
  block->makeShadowRow(1, 0);

  VPackBuilder result;
  result.openObject();
  block->toVelocyPack(nullptr, result);
  result.close();

  SharedAqlItemBlockPtr testee =
      itemBlockManager.requestAndInitBlock(result.slice());

  SharedAqlItemBlockPtr client1 = testee->cloneDataAndMoveShadow();
  SharedAqlItemBlockPtr client2 = testee->cloneDataAndMoveShadow();

  ASSERT_FALSE(client1->getValueReference(1, 0).isEmpty());
  EXPECT_EQ(VelocyPackHelper::compare(client1->getValueReference(1, 0).slice(),
                                      expected->slice(), false),
            0);

  EXPECT_TRUE(client2->getValueReference(1, 0).isEmpty());
}

TEST_F(AqlItemBlockTest, test_scatter_clone_normal_rows_sharing_value) {
  auto const json =
      R"({"thisField": "is too large to be inlined, deliberately so"})";
  auto const expected = Parser::fromJson(json);

  auto block = itemBlockManager.requestBlock(2, 1);
  block->setValue(
      0, 0, AqlValue{AqlValueHintSliceCopy{Parser::fromJson(json)->slice()}});
  block->setValue(
      1, 0, AqlValue{AqlValueHintSliceCopy{Parser::fromJson(json)->slice()}});

  VPackBuilder result;
  result.openObject();
  block->toVelocyPack(nullptr, result);
  ASSERT_TRUE(result.isOpenObject());
  result.close();

  // the round trip dedups both rows into one pointer
  SharedAqlItemBlockPtr testee =
      itemBlockManager.requestAndInitBlock(result.slice());
  ASSERT_FALSE(testee->isShadowRow(1));
  ASSERT_EQ(testee->getValueReference(0, 0).data(),
            testee->getValueReference(1, 0).data());

  SharedAqlItemBlockPtr client1 = testee->cloneDataAndMoveShadow();
  SharedAqlItemBlockPtr client2 = testee->cloneDataAndMoveShadow();

  for (auto const* client : {&client1, &client2}) {
    for (size_t row : {size_t{0}, size_t{1}}) {
      ASSERT_FALSE((*client)->getValueReference(row, 0).isEmpty());
      EXPECT_EQ(VelocyPackHelper::compare(
                    (*client)->getValueReference(row, 0).slice(),
                    expected->slice(), false),
                0);
    }
  }
}

TEST_F(AqlItemBlockTest,
       test_scatter_clone_shadow_row_sharing_value_keeps_source_valid) {
  auto const json =
      R"({"thisField": "is too large to be inlined, deliberately so"})";
  auto const expected = Parser::fromJson(json);

  auto block = itemBlockManager.requestBlock(2, 1);
  block->setValue(
      0, 0, AqlValue{AqlValueHintSliceCopy{Parser::fromJson(json)->slice()}});
  block->setValue(
      1, 0, AqlValue{AqlValueHintSliceCopy{Parser::fromJson(json)->slice()}});
  block->makeShadowRow(1, 0);

  VPackBuilder result;
  result.openObject();
  block->toVelocyPack(nullptr, result);
  result.close();

  // the round trip dedups row 0 and the shadow row into one pointer
  SharedAqlItemBlockPtr testee =
      itemBlockManager.requestAndInitBlock(result.slice());

  SharedAqlItemBlockPtr client1 = testee->cloneDataAndMoveShadow();
  client1.reset(nullptr);

  EXPECT_EQ(VelocyPackHelper::compare(testee->getValueReference(0, 0).slice(),
                                      expected->slice(), false),
            0);
}

TEST_F(AqlItemBlockTest,
       test_scatter_clone_shadow_row_with_value_in_two_cells) {
  auto const expected = Parser::fromJson(
      R"({"thisField": "is too large to be inlined, deliberately so"})");

  auto block = itemBlockManager.requestBlock(2, 2);
  AqlValue shared{AqlValueHintSliceCopy{expected->slice()}};
  block->setValue(1, 0, shared);
  block->setValue(1, 1, shared);
  block->makeShadowRow(1, 0);

  SharedAqlItemBlockPtr copy = block->cloneDataAndMoveShadow();
  block.reset(nullptr);

  for (RegisterId::value_t col : {0, 1}) {
    EXPECT_TRUE(copy->getValueReference(1, col).slice().binaryEquals(
        expected->slice()));
  }
  copy->destroyValue(1, 0);
  EXPECT_TRUE(
      copy->getValueReference(1, 1).slice().binaryEquals(expected->slice()));
}

TEST_F(AqlItemBlockTest, test_steal_value_shared_pointer_keeps_refcount) {
  auto const json =
      R"({"thisField": "is too large to be inlined, deliberately so"})";
  auto const expected = Parser::fromJson(json);

  auto block = itemBlockManager.requestBlock(2, 1);
  AqlValue shared{AqlValueHintSliceCopy{expected->slice()}};
  block->setValue(0, 0, shared);
  block->setValue(1, 0, shared);

  InputAqlItemRow row{block, 0};
  AqlValue stolen = row.stealValue(RegisterId{0});
  AqlValueGuard guard{stolen, true};
  EXPECT_NE(stolen.data(), shared.data());

  block->destroyValue(0, 0);

  EXPECT_EQ(VelocyPackHelper::compare(block->getValueReference(1, 0).slice(),
                                      expected->slice(), false),
            0);
}

TEST_F(AqlItemBlockTest, test_clone_shared_managed_string_to_managed_slice) {
  auto const expected = Parser::fromJson(
      R"({"thisField": "is too large to be inlined, deliberately so"})");
  auto bytes = expected->slice();

  for (bool viaScatter : {false, true}) {
    auto block = itemBlockManager.requestBlock(1, 2);
    DocumentData doc =
        std::make_unique<std::string>(bytes.startAs<char>(), bytes.byteSize());
    AqlValue shared{doc};
    ASSERT_EQ(shared.type(), AqlValue::VPACK_MANAGED_STRING);
    block->setValue(0, 0, shared);
    block->setValue(0, 1, shared);

    SharedAqlItemBlockPtr copy =
        viaScatter ? block->cloneDataAndMoveShadow() : block->slice(0, 1);
    block.reset(nullptr);

    for (RegisterId::value_t col : {0, 1}) {
      EXPECT_EQ(copy->getValueReference(0, col).type(),
                AqlValue::VPACK_MANAGED_SLICE);
      EXPECT_TRUE(copy->getValueReference(0, col).slice().binaryEquals(
          expected->slice()));
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
