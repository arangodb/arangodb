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

#include "VelocypackUtils/VelocyPackStringLiteral.h"

#include "Aql/Optimizer2/Types/Types.h"
#include "Aql/Optimizer2/Inspection/StatusT.h"
#include "Aql/Optimizer2/PlanNodes/EnumerateCollectionNode.h"

#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::nodes;

TEST(Optimizer2EnumerateCollectionNode, construction) {
  auto enumerateCollectionNodeBuffer = R"({
    "type": "EnumerateCollectionNode",
    "dependencies": [1],
    "id": 2,
    "estimatedCost": 2,
    "estimatedNrItems": 0,
    "random": false,
    "indexHint": {
      "forced": false,
      "lookahead": 1,
      "type": "none"
    },
    "outVariable": {
      "id": 0,
      "name": "x",
      "isFullDocumentFromCollection": true,
      "isDataFromCollection": true
    },
    "projections": [],
    "filterProjections": [],
    "count": false,
    "producesResult": true,
    "readOwnWrites": false,
    "useCache": true,
    "maxProjections": 5,
    "database": "_system",
    "collection": "_graphs",
    "satellite": false,
    "isSatellite": false,
    "isSatelliteOf": null
  })"_vpack;

  auto res = deserializeWithErrorT<EnumerateCollectionNode>(
      enumerateCollectionNodeBuffer);

  if (!res) {
    fmt::print("Something went wrong: {} {}", res.error().error(),
               res.error().path());
    EXPECT_TRUE(res.ok());
  } else {
    auto enumerateCollectionNode = res.get();
    EXPECT_EQ(enumerateCollectionNode.type, "EnumerateCollectionNode");
    EXPECT_EQ(enumerateCollectionNode.dependencies.at(0), 1u);
    EXPECT_EQ(enumerateCollectionNode.id, 2u);

    // FILTER a == true
    // FILTER b == true
    // => 1x FILTER (a == true) && (b == true)

    // indexHint
    EXPECT_FALSE(enumerateCollectionNode.indexHint.forced);
    EXPECT_EQ(enumerateCollectionNode.indexHint.lookahead, 1ul);
    EXPECT_EQ(enumerateCollectionNode.indexHint.type, "none");
    // optional
    EXPECT_FALSE(enumerateCollectionNode.canThrow.has_value());

    // outVariable
    EXPECT_EQ(enumerateCollectionNode.outVariable.value().id, 0u);
    EXPECT_EQ(enumerateCollectionNode.outVariable.value().name, "x");
    EXPECT_TRUE(enumerateCollectionNode.outVariable.value()
                    .isFullDocumentFromCollection);
    EXPECT_TRUE(
        enumerateCollectionNode.outVariable.value().isDataFromCollection);
    EXPECT_FALSE(
        enumerateCollectionNode.outVariable.value().constantValue.has_value());

    EXPECT_EQ(enumerateCollectionNode.estimatedCost, 2u);
    EXPECT_EQ(enumerateCollectionNode.estimatedNrItems, 0u);

    // EnumerateCollectionNode additional specifics
    /*EXPECT_TRUE(std::holds_alternative<
                arangodb::aql::optimizer2::ProjectionType::ProjectionArray>(
        enumerateCollectionNode.projections.projection));
    EXPECT_EQ(
        std::get<arangodb::aql::optimizer2::ProjectionType::ProjectionArray>(
            enumerateCollectionNode.projections.projection)
            .size(),
        0ul);
    EXPECT_EQ(
        std::get<arangodb::aql::optimizer2::ProjectionType::ProjectionArray>(
            enumerateCollectionNode.filterProjections.projection)
            .size(),
        0ul);*/
    EXPECT_FALSE(enumerateCollectionNode.count);
    EXPECT_TRUE(enumerateCollectionNode.producesResult);
    EXPECT_FALSE(enumerateCollectionNode.readOwnWrites);
    EXPECT_TRUE(enumerateCollectionNode.useCache);
    EXPECT_EQ(enumerateCollectionNode.maxProjections, 5u);
    EXPECT_EQ(enumerateCollectionNode.database, "_system");
    EXPECT_EQ(enumerateCollectionNode.collection, "_graphs");
    EXPECT_FALSE(enumerateCollectionNode.satellite.value());
    EXPECT_TRUE(enumerateCollectionNode.isSatellite.has_value());
    EXPECT_FALSE(enumerateCollectionNode.isSatellite.value());
    EXPECT_FALSE(enumerateCollectionNode.isSatelliteOf.has_value());
  }
}