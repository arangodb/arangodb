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
#include "Aql/Optimizer2/PlanNodes/LimitNode.h"

// Libraries
#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::nodes;

class Optimizer2LimitNode : public testing::Test {
 protected:
  SharedSlice createMinimumBody() {
    return R"({
      "type": "LimitNode",
      "dependencies": [
        4
      ],
      "id": 5,
      "estimatedCost": 30022,
      "estimatedNrItems": 20,
      "offset": 0,
      "limit": 20,
      "fullCount": false
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
  StatusT<LimitNode> parse(SharedSlice body) {
    return deserializeWithStatus<LimitNode>(body);
  }
  // Tries to serialize the given object of your type and returns
  // a filled VPackBuilder
  StatusT<SharedSlice> serialize(LimitNode testee) {
    return serializeWithStatus<LimitNode>(testee);
  }
};

// Generic tests

GenerateIntegerAttributeTest(Optimizer2LimitNode, offset);
GenerateIntegerAttributeTest(Optimizer2LimitNode, limit);
GenerateBoolAttributeTest(Optimizer2LimitNode, fullCount);

TEST_F(Optimizer2LimitNode, construction) {
  auto LimitNodeBuffer = createMinimumBody();
  auto res = deserializeWithErrorT<LimitNode>(LimitNodeBuffer);

  if (!res) {
    fmt::print("Something went wrong: {} {}", res.error().error(),
               res.error().path());
    EXPECT_TRUE(res.ok());
  } else {
    auto limitNode = res.get();
    EXPECT_EQ(limitNode.type, "LimitNode");
    EXPECT_EQ(limitNode.offset, 0ul);
    EXPECT_EQ(limitNode.limit, 20ul);
    EXPECT_FALSE(limitNode.fullCount);
  }
}