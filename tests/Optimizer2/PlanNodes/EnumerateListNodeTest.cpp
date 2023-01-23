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
#include "Aql/Optimizer2/PlanNodes/EnumerateListNode.h"
#include "Inspection/VPackInspection.h"
#include "InspectTestHelperMakros.h"

#include "velocypack/Collection.h"

#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::nodes;

class Optimizer2EnumerateListNode : public testing::Test {
 protected:
  SharedSlice createMinimumBody() {
    return R"({
      "type": "EnumerateListNode",
      "dependencies": [
          2
      ],
      "id": 3,
      "estimatedCost": 6,
      "estimatedNrItems": 4,
      "inVariable": {
          "id": 2,
          "name": "1",
          "isFullDocumentFromCollection": false,
          "isDataFromCollection": false,
          "constantValue": [
              1,
              2,
              3,
              4
          ]
      },
      "outVariable": {
          "id": 0,
          "name": "u",
          "isFullDocumentFromCollection": false,
          "isDataFromCollection": false
      }
    })"_vpack;
  }

  template<typename T>
  VPackBuilder createMinimumBodyWithOneValue(std::string const& attributeName,
                                             T const& attributeValue) {
    SharedSlice EnumerateListNodeBuffer = createMinimumBody();

    VPackBuilder b;
    {
      VPackObjectBuilder guard{&b};
      if constexpr (std::is_same_v<T, VPackSlice>) {
        b.add(attributeName, attributeValue);
      } else {
        b.add(attributeName, VPackValue(attributeValue));
      }
    }

    return arangodb::velocypack::Collection::merge(
        EnumerateListNodeBuffer.slice(), b.slice(), false);
  }

  // Tries to parse the given body and returns a ResulT of your Type under
  // test.
  StatusT<EnumerateListNode> parse(SharedSlice body) {
    return deserializeWithStatus<EnumerateListNode>(body);
  }
  // Tries to serialize the given object of your type and returns
  // a filled VPackBuilder
  StatusT<SharedSlice> serialize(EnumerateListNode testee) {
    return serializeWithStatus<EnumerateListNode>(testee);
  }
};

// Generic tests

/*
 * Currently no generic tests, as we do not have specific attributes in this
 * node GenerateIntegerAttributeTest(Optimizer2EnumerateListNode, id);
 * GenerateIntegerAttributeTest(Optimizer2EnumerateListNode, estimatedCost);
 * GenerateIntegerAttributeTest(Optimizer2EnumerateListNode, estimatedNrItems);
 */

// Default test

TEST_F(Optimizer2EnumerateListNode, construction) {
  auto enumerateListNodeBuffer = createMinimumBody();

  auto res = deserializeWithStatus<EnumerateListNode>(enumerateListNodeBuffer);

  if (!res) {
    fmt::print("Something went wrong: {}", res.error());
    EXPECT_TRUE(res.ok());
  } else {
    auto enumerateListNode = res.get();
    EXPECT_EQ(enumerateListNode.type, "EnumerateListNode");
    // inVariable and outVariable will be tested in dedicated struct tests for
    // Variable types.
  }
}