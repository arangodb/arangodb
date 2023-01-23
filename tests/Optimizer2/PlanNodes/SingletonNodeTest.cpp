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
#include "Aql/Optimizer2/PlanNodes/SingletonNode.h"

#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::nodes;

TEST(Optimizer2SingletonNode, construction) {
  auto SingletonNodeBuffer = R"({
    "type": "SingletonNode",
    "dependencies": [],
    "id": 1,
    "estimatedCost": 1,
    "estimatedNrItems": 1
  })"_vpack;

  auto res = deserializeWithErrorT<SingletonNode>(SingletonNodeBuffer);

  if (!res) {
    fmt::print("Something went wrong: {} {}", res.error().error(),
               res.error().path());
    EXPECT_TRUE(res.ok());
  } else {
    auto SingletonNode = res.get();
    EXPECT_EQ(SingletonNode.type, "SingletonNode");
    EXPECT_EQ(SingletonNode.id, 1u);
    EXPECT_EQ(SingletonNode.dependencies.size(), 0u);
    EXPECT_FALSE(SingletonNode.canThrow.has_value());
    EXPECT_EQ(SingletonNode.estimatedCost, 1u);
    EXPECT_EQ(SingletonNode.estimatedNrItems, 1u);
  }
}