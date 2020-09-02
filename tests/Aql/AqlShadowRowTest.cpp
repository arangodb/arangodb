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

#include "Aql/AqlItemBlockManager.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ShadowAqlItemRow.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <boost/container/flat_set.hpp>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

namespace arangodb {
namespace tests {
namespace aql {

class AqlShadowItemRowTest : public ::testing::Test {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor, SerializationFormat::SHADOWROWS};

  void AssertResultRow(InputAqlItemRow const& input, VPackSlice result,
                       std::unordered_set<RegisterId> const& regsToIgnore = {}) {
    ASSERT_TRUE(result.isArray());
    ASSERT_TRUE(input.isInitialized());
    ASSERT_EQ(input.getNumRegisters(), static_cast<size_t>(result.length()));
    for (RegisterId i = 0; i < input.getNumRegisters(); ++i) {
      if (regsToIgnore.find(i) == regsToIgnore.end()) {
        auto val = input.getValue(i);
        ASSERT_TRUE(VelocyPackHelper::equal(val.slice(), result.at(i), false))
            << "Comparing failed on entry " << i << " reason: " << val.slice().toJson()
            << " is not equal to: " << result.at(i).toJson();
      }
    }
  }

  void InsertNewShadowRowAfterEachDataRow(size_t targetNumberOfRows,
                                          SharedAqlItemBlockPtr const& inputBlock,
                                          SharedAqlItemBlockPtr& outputBlock) {
    auto numRegisters = inputBlock->numRegisters();
    outputBlock = itemBlockManager.requestBlock(targetNumberOfRows, numRegisters);
    // We do not add or remove anything, just move
    auto outputRegisters = RegIdSet{};
    size_t maxShadowRowDepth = 0;
    for (size_t rowIdx = 0; rowIdx < inputBlock->numRows(); ++rowIdx) {
      if (inputBlock->isShadowRow(rowIdx)) {
        maxShadowRowDepth =
            std::max(maxShadowRowDepth, inputBlock->getShadowRowDepth(rowIdx) + 1);
      }
    }

    auto protoRegSet = RegIdFlatSet{};
    for (RegisterId r = 0; r < numRegisters; ++r) {
      protoRegSet.emplace(r);
    }

    RegIdFlatSetStack registersToKeep;
    std::generate_n(std::back_inserter(registersToKeep), maxShadowRowDepth + 2, [&]{ return protoRegSet; });

    auto registersToClear = RegIdFlatSet{};
    OutputAqlItemRow testee(std::move(outputBlock), outputRegisters,
                            registersToKeep, registersToClear);

    for (size_t rowIdx = 0; rowIdx < inputBlock->numRows(); ++rowIdx) {
      ASSERT_FALSE(testee.isFull());
      if (!inputBlock->isShadowRow(rowIdx)) {
        // simply copy over every row, and insert a shadowRow after it
        InputAqlItemRow source{inputBlock, rowIdx};
        testee.copyRow(source);
        ASSERT_TRUE(testee.produced());
        ASSERT_FALSE(testee.isFull());
        testee.advanceRow();
        testee.createShadowRow(source);
        ASSERT_TRUE(testee.produced());
        testee.advanceRow();
      } else {
        // increase depth of shadowRow
        ShadowAqlItemRow source{inputBlock, rowIdx};
        testee.increaseShadowRowDepth(source);
        ASSERT_TRUE(testee.produced());
        testee.advanceRow();
      }
    }
    ASSERT_TRUE(testee.isFull());
    ASSERT_EQ(testee.numRowsWritten(), targetNumberOfRows);
    outputBlock = testee.stealBlock();
    ASSERT_EQ(outputBlock->numRows(), targetNumberOfRows);
  }

  void ConsumeRelevantShadowRows(size_t targetNumberOfRows,
                                 SharedAqlItemBlockPtr const& inputBlock,
                                 SharedAqlItemBlockPtr& outputBlock) {
    auto numRegisters = inputBlock->numRegisters();
    outputBlock.reset(new AqlItemBlock(itemBlockManager, targetNumberOfRows,
                                       numRegisters + 1));
    // We do not add or remove anything, just move
    auto outputRegisters = RegIdSet{static_cast<RegisterId>(numRegisters)};
    size_t maxShadowRowDepth = 0;
    for (size_t rowIdx = 0; rowIdx < inputBlock->numRows(); ++rowIdx) {
      if (inputBlock->isShadowRow(rowIdx)) {
        maxShadowRowDepth =
            std::max(maxShadowRowDepth, inputBlock->getShadowRowDepth(rowIdx) + 1);
      }
    }

    auto protoRegSet = RegIdFlatSet{};
    for (RegisterId r = 0; r < numRegisters; ++r) {
      protoRegSet.emplace(r);
    }

    RegIdFlatSetStack registersToKeep;
    std::generate_n(std::back_inserter(registersToKeep), maxShadowRowDepth + 2, [&]{ return protoRegSet; });

    auto registersToClear = RegIdFlatSet{};
    OutputAqlItemRow testee(std::move(outputBlock), outputRegisters,
                            registersToKeep, registersToClear);

    AqlValue shadowRowData{VPackSlice::emptyArraySlice()};

    // Let this go out of scope before assertions, to make sure no references are bound here.
    for (size_t rowIdx = 0; rowIdx < inputBlock->numRows(); ++rowIdx) {
      ASSERT_FALSE(testee.isFull());

      // Transform relevant ShadowRows to new DataRows
      // Copy over irrelevant shadowRows
      if (inputBlock->isShadowRow(rowIdx)) {
        ShadowAqlItemRow source{inputBlock, rowIdx};
        if (source.isRelevant()) {
          {
            bool mustDestroy = true;
            AqlValue clonedValue = shadowRowData.clone();
            AqlValueGuard guard{clonedValue, mustDestroy};
            testee.consumeShadowRow(static_cast<RegisterId>(numRegisters), source, guard);
          }
          ASSERT_TRUE(testee.produced());
          testee.advanceRow();
        } else {
          // decrease depth of shadowRow
          ShadowAqlItemRow source{inputBlock, rowIdx};
          testee.decreaseShadowRowDepth(source);
          ASSERT_TRUE(testee.produced());
          testee.advanceRow();
        }
      }
    }
    ASSERT_TRUE(testee.isFull());
    ASSERT_EQ(testee.numRowsWritten(), targetNumberOfRows);
    outputBlock = testee.stealBlock();
    ASSERT_EQ(outputBlock->numRows(), targetNumberOfRows);
  }
};

TEST_F(AqlShadowItemRowTest, inject_new_shadow_rows) {
  auto inputBlock =
      buildBlock<3>(itemBlockManager, {{{{1}, {2}, {3}}},
                                       {{{4}, {5}, {6}}},
                                       {{{"\"a\""}, {"\"b\""}, {"\"c\""}}}});
  SharedAqlItemBlockPtr outputBlock;
  InsertNewShadowRowAfterEachDataRow(6, inputBlock, outputBlock);
  auto expected =
      VPackParser::fromJson("[[1,2,3],[4,5,6],[\"a\",\"b\",\"c\"]]");
  for (size_t rowIdx = 0; rowIdx < outputBlock->numRows(); ++rowIdx) {
    if (rowIdx % 2 == 0) {
      // Data Row Case
      ASSERT_FALSE(outputBlock->isShadowRow(rowIdx));
      InputAqlItemRow testResult{outputBlock, rowIdx};
      AssertResultRow(testResult, expected->slice().at(rowIdx / 2));
    } else {
      // ShadowRowCase
      ASSERT_TRUE(outputBlock->isShadowRow(rowIdx));
      ShadowAqlItemRow testResult{outputBlock, rowIdx};
      ASSERT_TRUE(testResult.isRelevant());
    }
  }
}

TEST_F(AqlShadowItemRowTest, consume_shadow_rows) {
  auto inputBlock =
      buildBlock<3>(itemBlockManager, {{{{1}, {2}, {3}}},
                                       {{{4}, {5}, {6}}},
                                       {{{"\"a\""}, {"\"b\""}, {"\"c\""}}}});
  SharedAqlItemBlockPtr outputBlock;
  InsertNewShadowRowAfterEachDataRow(6, inputBlock, outputBlock);
  // We validated that outputBlock is correct in first test.

  // Now consume the ShadowRows again
  // In this test we simply dump datarows
  // and create new datarows out of shadowRows, writing a new value to them
  inputBlock.swap(outputBlock);
  ConsumeRelevantShadowRows(3, inputBlock, outputBlock);

  auto expected =
      VPackParser::fromJson("[[1,2,3,[]],[4,5,6,[]],[\"a\",\"b\",\"c\",[]]]");
  for (size_t rowIdx = 0; rowIdx < outputBlock->numRows(); ++rowIdx) {
    ASSERT_FALSE(outputBlock->isShadowRow(rowIdx));
    InputAqlItemRow testResult{outputBlock, rowIdx};
    AssertResultRow(testResult, expected->slice().at(rowIdx));
  }
}

TEST_F(AqlShadowItemRowTest, multi_level_shadow_rows) {
  auto inputBlock =
      buildBlock<3>(itemBlockManager, {{{{1}, {2}, {3}}},
                                       {{{4}, {5}, {6}}},
                                       {{{"\"a\""}, {"\"b\""}, {"\"c\""}}}});
  SharedAqlItemBlockPtr outputBlock;
  InsertNewShadowRowAfterEachDataRow(6, inputBlock, outputBlock);
  // We validated that outputBlock is correct in first test.

  // Now insert an additional level
  inputBlock.swap(outputBlock);
  InsertNewShadowRowAfterEachDataRow(9, inputBlock, outputBlock);
  {
    auto expected =
        VPackParser::fromJson("[[1,2,3],[4,5,6],[\"a\",\"b\",\"c\"]]");
    for (size_t rowIdx = 0; rowIdx < outputBlock->numRows(); ++rowIdx) {
      switch (rowIdx % 3) {
        case 0:
          // First is always datarow
          {
            ASSERT_FALSE(outputBlock->isShadowRow(rowIdx));
            InputAqlItemRow testResult{outputBlock, rowIdx};
            AssertResultRow(testResult, expected->slice().at(rowIdx / 3));
            break;
          }
        case 1:
          // Second is top-level Subquery
          {
            ASSERT_TRUE(outputBlock->isShadowRow(rowIdx));
            ShadowAqlItemRow testResult{outputBlock, rowIdx};
            ASSERT_TRUE(testResult.isRelevant());
            break;
          }
        case 2:
          // Third is subquery one level lower
          {
            ASSERT_TRUE(outputBlock->isShadowRow(rowIdx));
            ShadowAqlItemRow testResult{outputBlock, rowIdx};
            ASSERT_FALSE(testResult.isRelevant());
            break;
          }
      }
    }
  }

  // Now consume the inner level of ShadowRows again
  // In this test we simply dump datarows
  // and create new datarows out of shadowRows, writing a new value to them
  inputBlock.swap(outputBlock);
  ConsumeRelevantShadowRows(6, inputBlock, outputBlock);

  {
    auto expected = VPackParser::fromJson(
        "[[1,2,3,[]],[4,5,6,[]],[\"a\",\"b\",\"c\", []]]");
    for (size_t rowIdx = 0; rowIdx < outputBlock->numRows(); ++rowIdx) {
      switch (rowIdx % 2) {
        case 0:
          // First is always datarow
          {
            ASSERT_FALSE(outputBlock->isShadowRow(rowIdx));
            InputAqlItemRow testResult{outputBlock, rowIdx};
            AssertResultRow(testResult, expected->slice().at(rowIdx / 2));
            break;
          }
        case 1:
          // Second is top-level Subquery
          {
            ASSERT_TRUE(outputBlock->isShadowRow(rowIdx));
            ShadowAqlItemRow testResult{outputBlock, rowIdx};
            ASSERT_TRUE(testResult.isRelevant());
            break;
          }
      }
    }
  }
  /*
  // Now consume the outer level of ShadowRows again
  // In this test we simply dump datarows
  // and create new datarows out of shadowRows, writing a new value to them
  inputBlock.swap(outputBlock);
  ConsumeRelevantShadowRows(3, inputBlock, outputBlock);

  {
    auto expected = VPackParser::fromJson(
        "[[1,2,3,[]],[4,5,6,[]],[\"a\",\"b\",\"c\", []]]");
    for (size_t rowIdx = 0; rowIdx < outputBlock->numRows(); ++rowIdx) {
        ASSERT_FALSE(outputBlock->isShadowRow(rowIdx));
        InputAqlItemRow testResult{outputBlock, rowIdx};
        AssertResultRow(testResult, expected->slice().at(rowIdx));
      }
    }
  }
   */
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
