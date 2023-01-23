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
#include "Aql/Optimizer2/PlanNodes/SortNode.h"

// Libraries
#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::nodes;

class Optimizer2SortNode : public testing::Test {
 protected:
  SharedSlice createMinimumBody() {
    return R"({
                "type": "SortNode",
                "dependencies": [
                    5
                ],
                "id": 6,
                "estimatedCost": 2,
                "estimatedNrItems": 0,
                "elements": [
                    {
                        "inVariable": {
                            "id": 4,
                            "name": "3",
                            "isFullDocumentFromCollection": false,
                            "isDataFromCollection": false
                        },
                        "ascending": true
                    }
                ],
                "stable": false,
                "limit": 0,
                "strategy": "standard"
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
  StatusT<SortNode> parse(SharedSlice body) {
    return deserializeWithStatus<SortNode>(body);
  }
  // Tries to serialize the given object of your type and returns
  // a filled VPackBuilder
  StatusT<SharedSlice> serialize(SortNode testee) {
    return serializeWithStatus<SortNode>(testee);
  }
};

// Generic tests

GenerateBoolAttributeTest(Optimizer2SortNode, stable);
GenerateIntegerAttributeTest(Optimizer2SortNode, limit);
GenerateStringAttributeTest(Optimizer2SortNode, strategy);

TEST_F(Optimizer2SortNode, construction) {
  auto SortNodeBuffer = createMinimumBody();
  auto res = deserializeWithErrorT<SortNode>(SortNodeBuffer);

  if (!res) {
    fmt::print("Something went wrong: {} {}", res.error().error(),
               res.error().path());
    EXPECT_TRUE(res.ok());
  } else {
    auto sortNode = res.get();
    EXPECT_EQ(sortNode.type, "SortNode");
    EXPECT_FALSE(sortNode.stable);
    EXPECT_EQ(sortNode.limit, 0ul);
    EXPECT_EQ(sortNode.strategy, "standard");
    EXPECT_EQ(sortNode.elements.size(), 1ul);
    EXPECT_TRUE(sortNode.elements.at(0).ascending);
    EXPECT_EQ(sortNode.elements.at(0).inVariable.id, 4ul);
    EXPECT_EQ(sortNode.elements.at(0).inVariable.name, "3");
    EXPECT_FALSE(sortNode.elements.at(0).path.has_value());
    EXPECT_FALSE(
        sortNode.elements.at(0).inVariable.isFullDocumentFromCollection);
    EXPECT_FALSE(sortNode.elements.at(0).inVariable.isDataFromCollection);
  }
}