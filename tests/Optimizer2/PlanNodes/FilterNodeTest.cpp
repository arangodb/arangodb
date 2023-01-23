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
#include "Aql/Optimizer2/PlanNodes/FilterNode.h"

#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::nodes;

TEST(Optimizer2FilterNode, construction) {
  auto FilterNodeBuffer = R"({
    "type": "FilterNode",
    "dependencies": [4],
    "id": 5,
    "estimatedCost": 18,
    "estimatedNrItems": 5,
    "inVariable": {
      "id": 3,
      "name": "2",
      "isFullDocumentFromCollection": false,
      "isDataFromCollection": false
    }
  })"_vpack;

  auto res = deserializeWithErrorT<FilterNode>(FilterNodeBuffer);

  if (!res) {
    fmt::print("Something went wrong: {} {}", res.error().error(),
               res.error().path());
    EXPECT_TRUE(res.ok());
  } else {
    auto filterNode = res.get();
    EXPECT_EQ(filterNode.type, "FilterNode");
    EXPECT_EQ(filterNode.id, 5u);
    EXPECT_EQ(filterNode.dependencies.size(), 1u);
    EXPECT_EQ(filterNode.dependencies.at(0), 4u);
    EXPECT_FALSE(filterNode.canThrow.has_value());
    EXPECT_EQ(filterNode.estimatedCost, 18u);
    EXPECT_EQ(filterNode.estimatedNrItems, 5u);
    // inVariable
    EXPECT_EQ(filterNode.inVariable.id, 3u);
    EXPECT_EQ(filterNode.inVariable.name, "2");
    EXPECT_FALSE(filterNode.inVariable.isFullDocumentFromCollection);
    EXPECT_FALSE(filterNode.inVariable.isDataFromCollection);
  }
}