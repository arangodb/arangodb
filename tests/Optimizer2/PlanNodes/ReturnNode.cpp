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

#include "Basics/VelocyPackStringLiteral.h"

#include "Inspection/StatusT.h"
#include "PlanNodes/ReturnNode.h"

#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::nodes;

TEST(Optimizer2Blub, peter_peter) {
  auto poly = R"({
    "type": "Polygon",
    "coordinates": [[10,10],[20,20],[20,10],[10,20],[10,10]]
  })"_vpack;

  auto res = deserializeWithStatus<Return>(poly);

  if (!res) {
    fmt::print("Something went wrong: {}", res.error());
  }

  EXPECT_TRUE(false) << "Expected true to be piotr";
}
