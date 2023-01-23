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
#include "Aql/Optimizer2/PlanNodes/ReturnNode.h"

#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::nodes;

TEST(Optimizer2ReturnNode, construction) {
  auto returnNodeBuffer = R"({
    "type": "ReturnNode",
    "id": 3,
    "count": true,
    "dependencies": [1],
    "inVariable": {
      "id": 1,
      "name": "0",
      "isFullDocumentFromCollection": false,
      "isDataFromCollection": false,
      "constantValue": 1
    },
    "estimatedCost": 3,
    "estimatedNrItems": 1
  })"_vpack;

  auto res = deserializeWithErrorT<ReturnNode>(returnNodeBuffer);

  if (!res) {
    fmt::print("Something went wrong: {} {}", res.error().error(),
               res.error().path());
    EXPECT_TRUE(res.ok());
  } else {
    auto returnNode = res.get();
    EXPECT_EQ(returnNode.type, "ReturnNode");
    EXPECT_EQ(returnNode.id, 3u);
    EXPECT_TRUE(returnNode.count);
    EXPECT_EQ(returnNode.dependencies.at(0), 1u);
    EXPECT_FALSE(returnNode.canThrow.has_value());

    EXPECT_EQ(returnNode.inVariable.id, 1u);
    EXPECT_EQ(returnNode.inVariable.name, "0");
    EXPECT_FALSE(returnNode.inVariable.isFullDocumentFromCollection);
    EXPECT_FALSE(returnNode.inVariable.isDataFromCollection);
    EXPECT_TRUE(returnNode.inVariable.constantValue.has_value());
    EXPECT_EQ(returnNode.inVariable.constantValue->slice().getInt(), 1);

    EXPECT_EQ(returnNode.estimatedCost, 3u);
    EXPECT_EQ(returnNode.estimatedNrItems, 1u);
  }
}