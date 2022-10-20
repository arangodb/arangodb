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

#include "Basics/VelocyPackStringLiteral.h"
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
      "type": "IndexNode",
      "dependencies": [],
      "id": 0,
      "estimatedCost": 0,
      "estimatedNrItems": 0
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
GenerateIntegerAttributeTest(Optimizer2IndexNode, estimatedCost);
GenerateIntegerAttributeTest(Optimizer2IndexNode, estimatedNrItems);

// Default test

TEST_F(Optimizer2IndexNode, construction) {
  auto IndexNodeBuffer = R"({
    "type": "IndexNode",
    "dependencies": [4],
    "id": 5,
    "estimatedCost": 18,
    "estimatedNrItems": 5
  })"_vpack;

  auto res = deserializeWithStatus<IndexNode>(IndexNodeBuffer);

  if (!res) {
    fmt::print("Something went wrong: {}", res.error());
  } else {
    auto indexNode = res.get();
    EXPECT_EQ(indexNode.type, "IndexNode");

    EXPECT_EQ(indexNode.id, 5u);
    EXPECT_EQ(indexNode.dependencies.size(), 1u);
    EXPECT_EQ(indexNode.dependencies.at(0), 4u);
    EXPECT_FALSE(indexNode.canThrow.has_value());
    EXPECT_EQ(indexNode.estimatedCost, 18u);
    EXPECT_EQ(indexNode.estimatedNrItems, 5u);
  }
}