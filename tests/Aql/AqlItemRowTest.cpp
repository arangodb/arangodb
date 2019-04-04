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
#include "catch.hpp"

#include "Aql/AqlItemBlockManager.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"

#include "Basics/VelocyPackHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

static void AssertResultMatrix(AqlItemBlock* in, VPackSlice result,
                               std::unordered_set<RegisterId> const& regsToKeep,
                               bool assertNotInline = false) {
  INFO("Expecting: " << result.toJson() << " Got: " << *in);
  REQUIRE(result.isArray());
  REQUIRE(in->size() == result.length());
  for (size_t rowIdx = 0; rowIdx < in->size(); ++rowIdx) {
    VPackSlice row = result.at(rowIdx);
    REQUIRE(row.isArray());
    REQUIRE(in->getNrRegs() == row.length());
    for (RegisterId regId = 0; regId < in->getNrRegs(); ++regId) {
      AqlValue v = in->getValueReference(rowIdx, regId);
      if (regsToKeep.find(regId) == regsToKeep.end()) {
        // If this should not be kept it has to be set to NONE!
        REQUIRE(v.slice().isNone());
      } else {
        REQUIRE(basics::VelocyPackHelper::compare(row.at(regId), v.slice(), true) == 0);
        // Work around test as we are unable to check the type via API.
        if (assertNotInline) {
          // If this object is not inlined it requires some memory
          REQUIRE(v.memoryUsage() != 0);
        } else {
          // If it is inlined it does not require memory.
          REQUIRE(v.memoryUsage() == 0);
        }
      }
    }
  }
}

SCENARIO("AqlItemRows", "[AQL][EXECUTOR][ITEMROW]") {
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};

  WHEN("only copying from source to target") {
    SharedAqlItemBlockPtr outputBlock{new AqlItemBlock(itemBlockManager, 3, 3)};
    ExecutorInfos executorInfos{{}, {}, 3, 3, {}, {0, 1, 2}};
    auto outputRegisters = executorInfos.getOutputRegisters();
    auto registersToKeep = executorInfos.registersToKeep();

    OutputAqlItemRow testee(std::move(outputBlock), outputRegisters,
                            registersToKeep, executorInfos.registersToClear());

    THEN("the output rows need to be valid even if the source rows are gone") {
      {
        // Make sure this data is cleared before the assertions
        auto inputBlock =
            buildBlock<3>(itemBlockManager, {{{{1}, {2}, {3}}},
                                     {{{4}, {5}, {6}}},
                                     {{{"\"a\""}, {"\"b\""}, {"\"c\""}}}});

        InputAqlItemRow source{inputBlock, 0};

        testee.copyRow(source);
        REQUIRE(testee.produced());

        source = {inputBlock, 1};
        testee.advanceRow();
        testee.copyRow(source);
        REQUIRE(testee.produced());

        source = {inputBlock, 2};
        testee.advanceRow();
        testee.copyRow(source);
        REQUIRE(testee.produced());
      }
      auto expected =
          VPackParser::fromJson("[[1,2,3],[4,5,6],[\"a\",\"b\",\"c\"]]");
      outputBlock = testee.stealBlock();
      AssertResultMatrix(outputBlock.get(), expected->slice(), *registersToKeep);
    }

    THEN("the data should stay valid for large input values") {
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
        REQUIRE(testee.produced());

        source = {inputBlock, 1};
        testee.advanceRow();
        testee.copyRow(source);
        REQUIRE(testee.produced());

        source = {inputBlock, 2};
        testee.advanceRow();
        testee.copyRow(source);
        REQUIRE(testee.produced());
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
      AssertResultMatrix(outputBlock.get(), expected->slice(), *registersToKeep, true);
    }
  }

  WHEN("only copying from source to target but multiplying rows") {
    SharedAqlItemBlockPtr outputBlock{new AqlItemBlock(itemBlockManager, 9, 3)};
    ExecutorInfos executorInfos{{}, {}, 3, 3, {}, {0, 1, 2}};
    auto outputRegisters = executorInfos.getOutputRegisters();
    auto registersToKeep = executorInfos.registersToKeep();

    OutputAqlItemRow testee(std::move(outputBlock), outputRegisters,
                            registersToKeep, executorInfos.registersToClear());

    THEN("the output rows need to be valid even if the source rows are gone") {
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
            REQUIRE(testee.produced());
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
      AssertResultMatrix(outputBlock.get(), expected->slice(), *registersToKeep);
    }
  }

  WHEN("dropping a register from source while writing to target") {
    SharedAqlItemBlockPtr outputBlock{new AqlItemBlock(itemBlockManager, 3, 3)};
    ExecutorInfos executorInfos{{}, {}, 3, 3, {1}, {0, 2}};
    auto outputRegisters = executorInfos.getOutputRegisters();
    auto registersToKeep = executorInfos.registersToKeep();

    OutputAqlItemRow testee(std::move(outputBlock), outputRegisters,
                            registersToKeep, executorInfos.registersToClear());

    THEN("the output rows need to be valid even if the source rows are gone") {
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
          REQUIRE(testee.produced());
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
      AssertResultMatrix(outputBlock.get(), expected->slice(), *registersToKeep);
    }
  }

  WHEN("writing rows to target") {
    auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>();
    auto outputRegisters = std::make_shared<std::unordered_set<RegisterId>>();
    std::shared_ptr<std::unordered_set<RegisterId>> registersToClear = make_shared_unordered_set();
    std::shared_ptr<std::unordered_set<RegisterId>> registersToKeep = make_shared_unordered_set();
    RegisterId nrInputRegisters = 0;
    RegisterId nrOutputRegisters = 0;

    THEN("should keep all registers and add new values") {
      *inputRegisters = {};
      *outputRegisters = {3, 4};
      *registersToClear = {};
      *registersToKeep = {0, 1, 2};
      nrInputRegisters = 3;
      nrOutputRegisters = 5;
    }
    THEN("should be able to drop registers and write new values") {
      *inputRegisters = {};
      *outputRegisters = {3, 4};
      *registersToClear = {1, 2};
      *registersToKeep = {0};
      nrInputRegisters = 3;
      nrOutputRegisters = 5;
    }
    SharedAqlItemBlockPtr outputBlock{new AqlItemBlock(itemBlockManager, 3, 5)};
    ExecutorInfos executorInfos{inputRegisters, outputRegisters, nrInputRegisters,
                                nrOutputRegisters, *registersToClear, *registersToKeep};
    std::unordered_set<RegisterId> &regsToKeep = *registersToKeep;

    OutputAqlItemRow testee(std::move(outputBlock), outputRegisters,
                            registersToKeep, executorInfos.registersToClear());
    {
      // Make sure this data is cleared before the assertions
      auto inputBlock = buildBlock<3>(itemBlockManager, {{{{1}, {2}, {3}}},
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
            REQUIRE(!testee.produced());
          }
        }
        REQUIRE(testee.produced());
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
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
