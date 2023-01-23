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
#include "Aql/Optimizer2/PlanNodes/TraversalNode.h"

#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::nodes;

TEST(Optimizer2traversalNode, construction) {
  auto TraversalNodeBuffer = R"({
    "type": "TraversalNode",
    "dependencies": [1],
    "id": 2,
    "estimatedCost": 6,
    "estimatedNrItems": 1,
    "database": "_system",
    "graph": "knows_graph",
    "isLocalGraphNode": false,
    "isUsedAsSatellite": false,
    "graphDefinition": {
      "vertexCollectionNames": ["persons"],
      "edgeCollectionNames": ["knows"]
    },
    "defaultDirection": 2,
    "directions": [2],
    "edgeCollections": [
      "knows"
    ],
    "vertexCollections": [
      "persons"
    ],
    "collectionToShard": {},
    "vertexOutVariable": {
      "id": 0,
      "name": "v",
      "isFullDocumentFromCollection": true,
      "isDataFromCollection": true
    },
    "edgeOutVariable": {
      "id": 1,
      "name": "e",
      "isFullDocumentFromCollection": true,
      "isDataFromCollection": true
    },
    "isSmart": false,
    "isDisjoint": false,
    "forceOneShardAttributeValue": false,
    "tmpObjVariable": {
      "id": 4,
      "name": "3",
      "isFullDocumentFromCollection": false,
      "isDataFromCollection": false
    },
    "tmpObjVarNode": {
      "type": "reference",
      "typeID": 45,
      "name": "3",
      "id": 4
    },
    "tmpIdNode": {
      "type": "value",
      "typeID": 40,
      "value": "",
      "vType": "string",
      "vTypeID": 4
    },
    "options": {
      "parallelism": 1,
      "refactor": true,
      "produceVertices": true,
      "maxProjections": 5,
      "minDepth": 1,
      "maxDepth": 3,
      "neighbors": false,
      "uniqueVertices": "none",
      "uniqueEdges": "path",
      "order": "dfs",
      "weightAttribute": "",
      "defaultWeight": 1,
      "producePathsVertices": true,
      "producePathsEdges": true,
      "producePathsWeights": false,
      "type": "traversal"
    },
    "indexes": {
      "base": [
        {
          "id": "1",
          "type": "edge",
          "name": "edge",
          "fields": [
              "_from"
          ],
          "selectivityEstimate": 0.6,
          "unique": false,
          "sparse": false
        }
      ],
      "levels": {}
    },
    "vertexId": "persons/bob",
    "pathOutVariable": {
      "id": 2,
      "name": "p",
      "isFullDocumentFromCollection": false,
      "isDataFromCollection": false
    },
    "fromCondition": {
      "type": "compare ==",
      "typeID": 25,
      "excludesNull": false,
      "subNodes": [{
        "type": "attribute access",
        "typeID": 35,
        "name": "_from",
        "subNodes": [{
          "type": "reference",
          "typeID": 45,
          "name": "3",
          "id": 4
        }]
      },
      {
        "type": "value",
        "typeID": 40,
        "value": "",
        "vType": "string",
        "vTypeID": 4
      }]
    },
    "toCondition": {
      "type": "compare ==",
      "typeID": 25,
      "excludesNull": false,
      "subNodes": [{
        "type": "attribute access",
        "typeID": 35,
        "name": "_to",
        "subNodes": [{
          "type": "reference",
          "typeID": 45,
          "name": "3",
          "id": 4
        }]
      },
      {
        "type": "value",
        "typeID": 40,
        "value": "",
        "vType": "string",
        "vTypeID": 4
      }]
    }
  })"_vpack;

  auto res = deserializeWithStatus<TraversalNode>(TraversalNodeBuffer);

  if (!res) {
    fmt::print("Something went wrong: {} {} ", res.error(), res.path());
    EXPECT_TRUE(res.ok());
  } else {
    auto traversalNode = res.get();
    EXPECT_EQ(traversalNode.type, "TraversalNode");
    EXPECT_EQ(traversalNode.id, 2u);
    EXPECT_EQ(traversalNode.dependencies.size(), 1u);
    EXPECT_EQ(traversalNode.dependencies.at(0), 1u);
    EXPECT_FALSE(traversalNode.canThrow.has_value());
    EXPECT_EQ(traversalNode.estimatedCost, 6u);
    EXPECT_EQ(traversalNode.estimatedNrItems, 1u);
    // TODO
    // WIP: Not tested all variables here
    // HINT: We'll use mchackis MAKROS later here to imrove on this.
  }
}