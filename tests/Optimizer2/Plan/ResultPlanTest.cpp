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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/Optimizer2/Inspection/StatusT.h"

#include "VelocypackUtils/VelocyPackStringLiteral.h"
#include "Aql/Optimizer2/Plan/ResultPlan.h"
#include "Inspection/VPackInspection.h"
#include "InspectTestHelperMakros.h"
#include "velocypack/Collection.h"

#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::plan;

class Optimizer2ResultPlan : public testing::Test {
 protected:
  SharedSlice createMinimumBody() {
    return R"({
      "result": [
         1
       ],
       "hasMore": false,
       "cached": false,
       "extra": {
         "warnings": [],
         "stats": {
           "writesExecuted": 0,
           "writesIgnored": 0,
           "scannedFull": 0,
           "scannedIndex": 0,
           "cursorsCreated": 0,
           "cursorsRearmed": 0,
           "cacheHits": 0,
           "cacheMisses": 0,
           "filtered": 0,
           "httpRequests": 0,
           "executionTime": 4.57,
           "peakMemoryUsage": 0,
           "intermediateCommits": 0
         }
       },
       "error": false,
       "code": 201      
    })"_vpack;
  }

  template<typename T>
  VPackBuilder createMinimumBodyWithOneValue(std::string const& attributeName,
                                             T const& attributeValue) {
    SharedSlice VariableBuffer = createMinimumBody();

    VPackBuilder b;
    {
      VPackObjectBuilder guard{&b};
      if constexpr (std::is_same_v<T, VPackSlice>) {
        b.add(attributeName, attributeValue);
      } else {
        b.add(attributeName, VPackValue(attributeValue));
      }
    }

    return arangodb::velocypack::Collection::merge(VariableBuffer.slice(),
                                                   b.slice(), false);
  }

  // Tries to parse the given body and returns a ResulT of your Type under
  // test.
  StatusT<ResultPlan> parse(SharedSlice body) {
    return deserializeWithStatus<ResultPlan>(body);
  }
  // Tries to serialize the given object of your type and returns
  // a filled VPackBuilder
  StatusT<SharedSlice> serialize(ResultPlan testee) {
    return serializeWithStatus<ResultPlan>(testee);
  }
};

// Generic tests
// No generic test macros right now

// Default test

TEST_F(Optimizer2ResultPlan, construction) {
  auto VariableBuffer = createMinimumBody();

  auto res = deserializeWithStatus<ResultPlan>(VariableBuffer);

  if (!res) {
    fmt::print("Something went wrong: {} {} ", res.error(), res.path());
    EXPECT_TRUE(res.ok());
  } else {
    auto result = res.get();

    // top level
    EXPECT_TRUE(result.result.slice().isArray());
    EXPECT_EQ(result.result.slice().at(0).getInt(), 1u);
    EXPECT_FALSE(result.error);
    EXPECT_FALSE(result.hasMore);
    EXPECT_FALSE(result.cached);
    EXPECT_EQ(result.code, 201u);

    // stats
    EXPECT_EQ(result.extra.warnings.size(), 0u);
    EXPECT_EQ(result.extra.stats.writesExecuted, 0u);
    EXPECT_EQ(result.extra.stats.writesIgnored, 0u);
    EXPECT_EQ(result.extra.stats.scannedFull, 0u);
    EXPECT_EQ(result.extra.stats.scannedIndex, 0u);
    EXPECT_EQ(result.extra.stats.cursorsCreated, 0u);
    EXPECT_EQ(result.extra.stats.cursorsRearmed, 0u);
    EXPECT_EQ(result.extra.stats.cacheHits, 0u);
    EXPECT_EQ(result.extra.stats.cacheMisses, 0u);
    EXPECT_EQ(result.extra.stats.filtered, 0u);
    EXPECT_EQ(result.extra.stats.httpRequests, 0u);
    EXPECT_EQ(result.extra.stats.executionTime, 4.57);
    EXPECT_EQ(result.extra.stats.peakMemoryUsage, 0u);
    EXPECT_EQ(result.extra.stats.intermediateCommits, 0u);
  }
}
