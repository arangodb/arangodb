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

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class AqlItemBlockTest : public ::testing::Test {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};
};

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

TEST_F(AqlItemBlockTest, test_serialization_deserialization) {
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 2, 2)};

  block->emplaceValue(0, 0, AqlValueHintInt(5));
  block->emplaceValue(0, 1, AqlValueHintInt(6));
  block->emplaceValue(1, 0, AqlValueHintInt(7));
  block->emplaceValue(1, 1, AqlValueHintInt(8));
  VPackBuilder result;
  result.openObject();
  block->toVelocyPack(nullptr, result);
  ASSERT_TRUE(result.isOpenObject());
  result.close();

  SharedAqlItemBlockPtr testee = itemBlockManager.requestAndInitBlock(result.slice());

  // Check exposed attributes
  EXPECT_EQ(testee->size(), block->size());
  EXPECT_EQ(testee->getNrRegs(), block->getNrRegs());
  EXPECT_EQ(testee->numEntries(), block->numEntries());
  EXPECT_EQ(testee->capacity(), block->capacity());
  // check data
  EXPECT_EQ(testee->getValueReference(0, 0).toInt64(), 5);
  EXPECT_EQ(testee->getValueReference(0, 1).toInt64(), 6);
  EXPECT_EQ(testee->getValueReference(1, 0).toInt64(), 7);
  EXPECT_EQ(testee->getValueReference(1, 1).toInt64(), 8);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb