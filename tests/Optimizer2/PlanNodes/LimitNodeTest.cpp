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
#include "Aql/Optimizer2/PlanNodes/LimitNode.h"

#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::nodes;
/* TODO TODO TODO
TEST(Optimizer2LimitNode, construction) {
  auto LimitNodeBuffer = R"({
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

  auto res = deserializeWithStatus<LimitNode>(LimitNodeBuffer);

  if (!res) {
    fmt::print("Something went wrong: {}", res.error());
  } else {
    auto limitNode = res.get();
    EXPECT_EQ(LimitNode.type, "LimitNode");
    // TODO
  }
}*/