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

// Helpers
#include "Aql/Optimizer2/Inspection/StatusT.h"
#include "VelocypackUtils/VelocyPackStringLiteral.h"
#include "InspectTestHelperMakros.h"
#include "velocypack/Collection.h"
#include "Inspection/VPackInspection.h"
// Node Class
#include "Aql/Optimizer2/PlanNodes/CollectNode.h"

// Libraries
#include <Inspection/VPackWithErrorT.h>
#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::nodes;

class Optimizer2CollectNode : public testing::Test {
 protected:
  SharedSlice createMinimumBody() {
    return R"({
      "type": "CollectNode",
        "dependencies": [
          6
        ],
        "id": 7,
        "estimatedCost": 9603,
        "estimatedNrItems": 1600,
        "groups": [
          {
            "outVariable": {
              "id": 1,
              "name": "group",
              "isFullDocumentFromCollection": false,
              "isDataFromCollection": false
            },
            "inVariable": {
              "id": 6,
              "name": "5",
              "isFullDocumentFromCollection": false,
              "isDataFromCollection": false
            }
          }
        ],
        "aggregates": [
          {
            "outVariable": {
              "id": 2,
              "name": "length",
              "isFullDocumentFromCollection": false,
              "isDataFromCollection": false
            },
            "type": "LENGTH"
          },
          {
            "outVariable": {
              "id": 3,
              "name": "min",
              "isFullDocumentFromCollection": false,
              "isDataFromCollection": false
            },
            "inVariable": {
              "id": 10,
              "name": "9",
              "isFullDocumentFromCollection": false,
              "isDataFromCollection": false
            },
            "type": "MIN"
          }
        ],
        "isDistinctCommand": false,
        "specialized": true,
        "collectOptions": {
          "method": "hash"
        }
    })"_vpack;
  }

  template<typename T>
  VPackBuilder createMinimumBodyWithOneValue(std::string const& attributeName,
                                             T const& attributeValue) {
    SharedSlice buffer = createMinimumBody();

    VPackBuilder b;
    {
      VPackObjectBuilder guard{&b};
      if constexpr (std::is_same_v<T, VPackSlice>) {
        b.add(attributeName, attributeValue);
      } else {
        b.add(attributeName, VPackValue(attributeValue));
      }
    }

    return arangodb::velocypack::Collection::merge(buffer.slice(), b.slice(),
                                                   false);
  }

  // Tries to parse the given body and returns a ResulT of your Type under
  // test.
  StatusT<CollectNode> parse(SharedSlice body) {
    return deserializeWithStatus<CollectNode>(body);
  }
  // Tries to serialize the given object of your type and returns
  // a filled VPackBuilder
  StatusT<SharedSlice> serialize(CollectNode testee) {
    return serializeWithStatus<CollectNode>(testee);
  }
};

// Generic tests

GenerateBoolAttributeTest(Optimizer2CollectNode, isDistinctCommand);
GenerateBoolAttributeTest(Optimizer2CollectNode, specialized);

TEST_F(Optimizer2CollectNode, construction) {
  auto CollectNodeBuffer = createMinimumBody();
  auto res = deserializeWithErrorT<CollectNode>(CollectNodeBuffer);

  if (!res) {
    fmt::print("Something went wrong: {} {}", res.error().error(),
               res.error().path());
    EXPECT_TRUE(res.ok());
  } else {
    auto collectNode = res.get();
    EXPECT_EQ(collectNode.type, "CollectNode");

    // Groups
    EXPECT_EQ(collectNode.groups.size(), 1ul);
    EXPECT_EQ(collectNode.groups.at(0).outVariable.id, 1ul);
    EXPECT_EQ(collectNode.groups.at(0).outVariable.name, "group");
    EXPECT_FALSE(
        collectNode.groups.at(0).outVariable.isFullDocumentFromCollection);
    EXPECT_FALSE(collectNode.groups.at(0).outVariable.isDataFromCollection);
    EXPECT_EQ(collectNode.groups.at(0).inVariable.id, 6ul);
    EXPECT_EQ(collectNode.groups.at(0).inVariable.name, "5");
    EXPECT_FALSE(
        collectNode.groups.at(0).inVariable.isFullDocumentFromCollection);
    EXPECT_FALSE(collectNode.groups.at(0).inVariable.isDataFromCollection);

    // Aggregates
    EXPECT_EQ(collectNode.aggregates.size(), 2ul);
    EXPECT_EQ(collectNode.aggregates.at(0).outVariable.id, 2ul);
    EXPECT_EQ(collectNode.aggregates.at(0).outVariable.name, "length");
    EXPECT_FALSE(
        collectNode.aggregates.at(0).outVariable.isFullDocumentFromCollection);
    EXPECT_FALSE(collectNode.aggregates.at(0).outVariable.isDataFromCollection);
    EXPECT_EQ(collectNode.aggregates.at(0).type, "LENGTH");

    EXPECT_TRUE(collectNode.aggregates.at(1).inVariable.has_value());
    EXPECT_EQ(collectNode.aggregates.at(1).inVariable.value().id, 10ul);
    EXPECT_EQ(collectNode.aggregates.at(1).inVariable.value().name, "9");
    EXPECT_FALSE(collectNode.aggregates.at(1)
                     .inVariable.value()
                     .isFullDocumentFromCollection);
    EXPECT_FALSE(
        collectNode.aggregates.at(1).inVariable.value().isDataFromCollection);
    EXPECT_EQ(collectNode.aggregates.at(1).outVariable.id, 3ul);
    EXPECT_EQ(collectNode.aggregates.at(1).outVariable.name, "min");
    EXPECT_FALSE(
        collectNode.aggregates.at(1).outVariable.isFullDocumentFromCollection);
    EXPECT_FALSE(collectNode.aggregates.at(1).outVariable.isDataFromCollection);
    EXPECT_EQ(collectNode.aggregates.at(1).type, "MIN");

    // Expression
    EXPECT_FALSE(collectNode.expression.has_value());
    // outVariable
    EXPECT_FALSE(collectNode.outVariable.has_value());
    // keepVariables
    EXPECT_FALSE(collectNode.keepVariables.has_value());

    EXPECT_EQ(collectNode.collectOptions.method,
              arangodb::aql::optimizer2::nodes::CollectNodeStructs::
                  CollectMethod::HASH);
    EXPECT_FALSE(collectNode.isDistinctCommand);
    EXPECT_TRUE(collectNode.specialized);
  }
}