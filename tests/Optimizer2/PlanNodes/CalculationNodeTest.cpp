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
#include "Aql/Optimizer2/PlanNodes/CalculationNode.h"

#include <Inspection/VPackWithErrorT.h>
#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::nodes;

TEST(Optimizer2CalculationNode, construction) {
  auto CalculationNodeBuffer = R"({
    "type": "CalculationNode",
    "dependencies": [
      1
    ],
    "id": 2,
    "estimatedCost": 2,
    "estimatedNrItems": 1,
    "expression": {
      "type": "value",
      "typeID": 40,
      "value": 1,
      "vType": "int",
      "vTypeID": 2
    },
    "outVariable": {
      "id": 0,
      "name": "x",
      "isFullDocumentFromCollection": false,
      "isDataFromCollection": false,
      "constantValue": 1
    },
    "canThrow": false,
    "expressionType": "json"
  })"_vpack;

  auto res = deserializeWithErrorT<CalculationNode>(CalculationNodeBuffer);

  if (!res) {
    fmt::print("Something went wrong: {} {}", res.error().error(),
               res.error().path());
    EXPECT_TRUE(res.ok());
  } else {
    auto calculationNode = res.get();
    EXPECT_EQ(calculationNode.type, "CalculationNode");
    EXPECT_EQ(calculationNode.id, 2u);
    EXPECT_EQ(calculationNode.dependencies.size(), 1u);
    EXPECT_EQ(calculationNode.dependencies.at(0), 1u);
    EXPECT_TRUE(calculationNode.canThrow.has_value());
    EXPECT_EQ(calculationNode.estimatedCost, 2u);
    EXPECT_EQ(calculationNode.estimatedNrItems, 1u);
    // expression
    EXPECT_EQ(calculationNode.expression.type, "value");
    EXPECT_EQ(calculationNode.expression.typeID, 40u);
    EXPECT_EQ(calculationNode.expression.value.value().slice().getInt(), 1u);
    EXPECT_EQ(calculationNode.expression.vType, "int");
    EXPECT_EQ(calculationNode.expression.vTypeID, 2u);
    // outVariable
    EXPECT_EQ(calculationNode.outVariable.id, 0u);
    EXPECT_EQ(calculationNode.outVariable.name, "x");
    EXPECT_FALSE(calculationNode.outVariable.isFullDocumentFromCollection);
    EXPECT_FALSE(calculationNode.outVariable.isDataFromCollection);
    EXPECT_TRUE(calculationNode.outVariable.constantValue.has_value());
    EXPECT_EQ(calculationNode.outVariable.constantValue->slice().getInt(), 1);
  }
}