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
#include "Aql/Optimizer2/PlanNodes/IndexNode.h"
#include "Inspection/VPackInspection.h"
#include "InspectTestHelperMakros.h"
#include "velocypack/Collection.h"

#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::nodes;

class Optimizer2IndexNode : public testing::Test {
 protected:
  SharedSlice createMinimumBody() {
    return R"({
    "type" : "IndexNode",
    "dependencies" : [
      1
    ],
    "id" : 12,
    "estimatedCost" : 1,
    "estimatedNrItems" : 0,
    "needsGatherNodeSort" : false,
    "indexCoversProjections" : true,
    "useCache" : false,
    "count" : false,
    "producesResult" : true,
    "readOwnWrites" : false,
    "projections" : [
      "_key"
    ],
    "filterProjections" : [ ],
    "maxProjections" : 5,
    "limit" : 0,
    "lookahead" : 1,
    "database" : "_system",
    "collection" : "UnitTestsExplain",
    "satellite" : false,
    "numberOfShards" : 3,
    "isSatellite" : false,
    "isSatelliteOf" : null,
    "indexes" : [
      {
        "id" : "0",
        "type" : "primary",
        "name" : "primary",
        "fields" : [
          "_key"
        ],
        "selectivityEstimate" : 1,
        "unique" : true,
        "sparse" : false
      }
    ],
    "allCoveredByOneIndex" : false,
    "sorted" : true,
    "ascending" : true,
    "reverse" : false,
    "evalFCalls" : true,
    "waitForSync" : false
    })"_vpack;
  }

  template<typename T>
  VPackBuilder createMinimumBodyWithOneValue(std::string const& attributeName,
                                             T const& attributeValue) {
    SharedSlice IndexNodeBuffer = createMinimumBody();

    VPackBuilder b;
    {
      VPackObjectBuilder guard{&b};
      if constexpr (std::is_same_v<T, VPackSlice>) {
        b.add(attributeName, attributeValue);
      } else {
        b.add(attributeName, VPackValue(attributeValue));
      }
    }

    return arangodb::velocypack::Collection::merge(IndexNodeBuffer.slice(),
                                                   b.slice(), false);
  }

  // Tries to parse the given body and returns a ResulT of your Type under
  // test.
  StatusT<IndexNode> parse(SharedSlice body) {
    return deserializeWithStatus<IndexNode>(body);
  }
  // Tries to serialize the given object of your type and returns
  // a filled VPackBuilder
  StatusT<SharedSlice> serialize(IndexNode testee) {
    return serializeWithStatus<IndexNode>(testee);
  }
};

// Generic tests

GenerateIntegerAttributeTest(Optimizer2IndexNode, id);
GenerateDoubleAttributeTest(Optimizer2IndexNode, estimatedCost);
GenerateIntegerAttributeTest(Optimizer2IndexNode, estimatedNrItems);

// Default test

TEST_F(Optimizer2IndexNode, construction) {
  auto IndexNodeBuffer = R"({
    "type" : "IndexNode",
      "dependencies" : [
        1
      ],
      "id" : 12,
      "estimatedCost" : 1,
      "estimatedNrItems" : 0,
      "outVariable" : {
        "id" : 0,
        "name" : "u",
        "isFullDocumentFromCollection" : false,
        "isDataFromCollection" : false
      },
      "projections" : [
        "_key"
      ],
      "filterProjections" : [ ],
      "count" : false,
      "producesResult" : true,
      "readOwnWrites" : false,
      "useCache" : false,
      "maxProjections" : 5,
      "database" : "_system",
      "collection" : "UnitTestsExplain",
      "satellite" : false,
      "numberOfShards" : 3,
      "isSatellite" : false,
      "isSatelliteOf" : null,
      "needsGatherNodeSort" : false,
      "indexCoversProjections" : true,
      "indexes" : [
        {
          "id" : "0",
          "type" : "primary",
          "name" : "primary",
          "fields" : [
            "_key"
          ],
          "selectivityEstimate" : 1,
          "unique" : true,
          "sparse" : false
        }
      ],
      "condition" : {
      },
      "allCoveredByOneIndex" : false,
      "sorted" : true,
      "ascending" : true,
      "reverse" : false,
      "evalFCalls" : true,
      "waitForSync" : false,
      "limit" : 0,
      "lookahead" : 1
  })"_vpack;

  auto res = deserializeWithStatus<IndexNode>(IndexNodeBuffer);

  if (!res) {
    fmt::print("Something went wrong: {} {} ", res.error(), res.path());
    EXPECT_TRUE(res.ok());
  } else {
    auto indexNode = res.get();
    EXPECT_EQ(indexNode.type, "IndexNode");

    EXPECT_EQ(indexNode.id, 12u);
    EXPECT_EQ(indexNode.dependencies.size(), 1u);
    EXPECT_EQ(indexNode.dependencies.at(0), 1u);
    EXPECT_FALSE(indexNode.canThrow.has_value());
    EXPECT_EQ(indexNode.estimatedCost, 1u);
    EXPECT_EQ(indexNode.estimatedNrItems, 0u);
    // Specific
    EXPECT_FALSE(indexNode.needsGatherNodeSort);
    EXPECT_TRUE(indexNode.indexCoversProjections);
    EXPECT_EQ(indexNode.limit, 0u);
    EXPECT_EQ(indexNode.lookahead, 1u);
    // IndexOperatorOptions
    EXPECT_FALSE(indexNode.allCoveredByOneIndex);
    EXPECT_TRUE(indexNode.sorted);
    EXPECT_TRUE(indexNode.ascending);
    EXPECT_FALSE(indexNode.reverse);
    EXPECT_TRUE(indexNode.evalFCalls);
    EXPECT_FALSE(indexNode.waitForSync);
  }
}