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


#include "catch.hpp"
#include "AqlItemBlockHelper.h"

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

static void AssertEntry(InputAqlItemRow& in, RegisterId reg, VPackSlice value) {
  AqlValue v = in.getValue(reg);
  INFO("Expecting: " << value.toJson() << " got: " << v.slice().toJson());
  REQUIRE(basics::VelocyPackHelper::compare(value, v.slice(), true) == 0);
}

static void AssertResultMatrix(AqlItemBlock* in, VPackSlice result,
                               std::unordered_set<RegisterId> const& regsToKeep,
                               bool assertNotInline = false) {
  INFO("Expecting: " << result.toJson() << " Got: " << *in);
  REQUIRE(result.isArray());
  REQUIRE(in->size() == result.length());
  for (size_t i = 0; i < in->size(); ++i) {
    InputAqlItemRow validator{in, i, 0};
    VPackSlice row = result.at(i);
    REQUIRE(row.isArray());
    REQUIRE(in->getNrRegs() == row.length());
    for (RegisterId j = 0; j < in->getNrRegs(); ++j) {
      AqlValue v = validator.getValue(j);
      if (regsToKeep.find(j) == regsToKeep.end()) {
        // If this should not be kept it has to be set to NONE!
        REQUIRE(v.slice().isNone());
      } else {
        REQUIRE(basics::VelocyPackHelper::compare(row.at(j), v.slice(), true) == 0);
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

  WHEN("only copying from source to target") {
    auto outputData = std::make_unique<AqlItemBlock>(&monitor, 3, 3);
    ExecutorInfos executorInfos{{}, {}, 3, 3, {}};
    std::unordered_set<RegisterId> const& regsToKeep =
        executorInfos.registersToKeep();

    OutputAqlItemRow testee(std::move(outputData), executorInfos);

    THEN("the output rows need to be valid even if the source rows are gone") {
      {
        // Make sure this data is cleared before the assertions
        auto inputData = buildBlock<3>(&monitor, {
          {{ {1}, {2}, {3} }},
          {{ {4}, {5}, {6} }},
          {{ {"\"a\""}, {"\"b\""}, {"\"c\""} }}
        });

        InputAqlItemRow source{inputData.get(), 0, 0};

        testee.copyRow(source);
        REQUIRE(testee.produced());

        source = {inputData.get(), 1, 0};
        testee.advanceRow();
        testee.copyRow(source);
        REQUIRE(testee.produced());

        source = {inputData.get(), 2, 0};
        testee.advanceRow();
        testee.copyRow(source);
        REQUIRE(testee.produced());
      }
      auto expected = VPackParser::fromJson("[[1,2,3],[4,5,6],[\"a\",\"b\",\"c\"]]");
      outputData = testee.stealBlock();
      AssertResultMatrix(outputData.get(), expected->slice(), regsToKeep);
    }

    THEN("the data should stay valid for large input values") {
      {
        // Make sure this data is cleared before the assertions
        // Every of these entries has a size > 16 uint_8 
        auto inputData = buildBlock<3>(&monitor, {
          {{ {"\"aaaaaaaaaaaaaaaaaaaa\""}, {"\"bbbbbbbbbbbbbbbbbbbb\""}, {"\"cccccccccccccccccccc\""} }},
          {{ {"\"dddddddddddddddddddd\""}, {"\"eeeeeeeeeeeeeeeeeeee\""}, {"\"ffffffffffffffffffff\""} }},
          {{ {"\"gggggggggggggggggggg\""}, {"\"hhhhhhhhhhhhhhhhhhhh\""}, {"\"iiiiiiiiiiiiiiiiiiii\""} }}
        });

        InputAqlItemRow source{inputData.get(), 0, 0};

        testee.copyRow(source);
        REQUIRE(testee.produced());

        source = {inputData.get(), 1, 0};
        testee.advanceRow();
        testee.copyRow(source);
        REQUIRE(testee.produced());

        source = {inputData.get(), 2, 0};
        testee.advanceRow();
        testee.copyRow(source);
        REQUIRE(testee.produced());
      }
      auto expected = VPackParser::fromJson("["
        "[\"aaaaaaaaaaaaaaaaaaaa\", \"bbbbbbbbbbbbbbbbbbbb\", \"cccccccccccccccccccc\"],"
        "[\"dddddddddddddddddddd\", \"eeeeeeeeeeeeeeeeeeee\", \"ffffffffffffffffffff\"],"
        "[\"gggggggggggggggggggg\", \"hhhhhhhhhhhhhhhhhhhh\", \"iiiiiiiiiiiiiiiiiiii\"]"
      "]");
      outputData = testee.stealBlock();
      AssertResultMatrix(outputData.get(), expected->slice(), regsToKeep, true);
    }

  }

  WHEN("only copying from source to target but multiplying rows") {
    auto outputData = std::make_unique<AqlItemBlock>(&monitor, 9, 3);
    ExecutorInfos executorInfos{{}, {}, 3, 3, {}};
    std::unordered_set<RegisterId> const& regsToKeep =
      executorInfos.registersToKeep();

    OutputAqlItemRow testee(std::move(outputData), executorInfos);

    THEN("the output rows need to be valid even if the source rows are gone") {
      {
        // Make sure this data is cleared before the assertions
        auto inputData = buildBlock<3>(&monitor, {
          {{ {1}, {2}, {3} }},
          {{ {4}, {5}, {6} }},
          {{ {"\"a\""}, {"\"b\""}, {"\"c\""} }}
        });


        for (size_t i = 0; i < 3; ++i) {
          // Iterate over source rows
          InputAqlItemRow source{inputData.get(), i, 0};
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
      auto expected = VPackParser::fromJson("["
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
      outputData = testee.stealBlock();
      AssertResultMatrix(outputData.get(), expected->slice(), regsToKeep);
    }
  }

  WHEN("dropping a register from source while writing to target") {
    auto outputData = std::make_unique<AqlItemBlock>(&monitor, 3, 3);
    ExecutorInfos executorInfos{{}, {}, 3, 3, {1}};
    std::unordered_set<RegisterId> const& regsToKeep =
      executorInfos.registersToKeep();

    OutputAqlItemRow testee(std::move(outputData), executorInfos);

    THEN("the output rows need to be valid even if the source rows are gone") {
      {
        // Make sure this data is cleared before the assertions
        auto inputData = buildBlock<3>(&monitor, {
          {{ {1}, {2}, {3} }},
          {{ {4}, {5}, {6} }},
          {{ {"\"a\""}, {"\"b\""}, {"\"c\""} }}
        });


        for (size_t i = 0; i < 3; ++i) {
          // Iterate over source rows
          InputAqlItemRow source{inputData.get(), i, 0};
          testee.copyRow(source);
          REQUIRE(testee.produced());
          if (i < 2) {
            // Not at the last one, we are at the end
            testee.advanceRow();
          }
        }
      }
      auto expected = VPackParser::fromJson("["
        "[1,2,3],"
        "[4,5,6],"
        "[\"a\",\"b\",\"c\"]"
      "]");
      outputData = testee.stealBlock();
      AssertResultMatrix(outputData.get(), expected->slice(), regsToKeep);
    }
  }

  WHEN("writing rows to target") {
    auto outputData = std::make_unique<AqlItemBlock>(&monitor, 3, 5);
    std::unique_ptr<ExecutorInfos> executorInfos{nullptr};

    THEN("should keep all registers and add new values") {
      executorInfos = std::make_unique<ExecutorInfos>(
          std::unordered_set<RegisterId>{},
          std::unordered_set<RegisterId>{3, 4}, 3, 5,
          std::unordered_set<RegisterId>{});
    }
    THEN("should be able to drop registers and write new values") {
      executorInfos = std::make_unique<ExecutorInfos>(
          std::unordered_set<RegisterId>{},
          std::unordered_set<RegisterId>{3, 4}, 3, 5,
          std::unordered_set<RegisterId>{1, 2});
    }
    std::unordered_set<RegisterId> regsToKeep =
        executorInfos->registersToKeep();

    OutputAqlItemRow testee(std::move(outputData), *executorInfos);
    {
      // Make sure this data is cleared before the assertions
      auto inputData = buildBlock<3>(&monitor, {
        {{ {1}, {2}, {3} }},
        {{ {4}, {5}, {6} }},
        {{ {"\"a\""}, {"\"b\""}, {"\"c\""} }}
      });


      for (size_t i = 0; i < 3; ++i) {
        // Iterate over source rows
        InputAqlItemRow source{inputData.get(), i, 0};
        for(size_t j = 3; j < 5; ++j) {
          AqlValue v{ AqlValueHintInt{(int64_t)(j + 5)} };
          testee.setValue(j, source, v);
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
    auto expected = VPackParser::fromJson("["
      "[1,2,3,8,9],"
      "[4,5,6,8,9],"
      "[\"a\",\"b\",\"c\",8,9]"
    "]");
    // add these two here as they are needed for output validation but not for copy in ItemRows
    regsToKeep.emplace(3);
    regsToKeep.emplace(4);
    outputData = testee.stealBlock();
    AssertResultMatrix(outputData.get(), expected->slice(), regsToKeep);
  }

}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
