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
#include "Aql/Optimizer2/PlanNodes/InsertNode.h"

// Libraries
#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::nodes;

class Optimizer2InsertNode : public testing::Test {
 protected:
  SharedSlice createMinimumBody() {
    return R"({
      "type": "InsertNode",
      "dependencies": [
        2
      ],
      "id": 3,
      "estimatedCost": 2,
      "estimatedNrItems": 0,
      "database": "_system",
      "collection": "UnitTestsExplain",
      "satellite": false,
      "isSatellite": false,
      "isSatelliteOf": null,
      "indexes": [
        {
          "id": "0",
          "type": "primary",
          "name": "primary",
          "fields": [
            "_key"
          ],
          "unique": true,
          "sparse": false
        }
      ],
      "countStats": true,
      "producesResults": false,
      "modificationFlags": {
        "waitForSync": false,
        "skipDocumentValidation": false,
        "keepNull": true,
        "mergeObjects": true,
        "ignoreRevs": true,
        "isRestore": false,
        "ignoreErrors": false,
        "ignoreDocumentNotFound": false,
        "readCompleteInput": false,
        "consultAqlWriteFilter": false,
        "exclusive": false
      },
      "inVariable": {
        "id": 0,
        "name": "u",
        "isFullDocumentFromCollection": true,
        "isDataFromCollection": true
      }
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
  StatusT<InsertNode> parse(SharedSlice body) {
    return deserializeWithStatus<InsertNode>(body);
  }
  // Tries to serialize the given object of your type and returns
  // a filled VPackBuilder
  StatusT<SharedSlice> serialize(InsertNode testee) {
    return serializeWithStatus<InsertNode>(testee);
  }
};

TEST_F(Optimizer2InsertNode, construction) {
  auto InsertNodeBuffer = createMinimumBody();
  auto res = deserializeWithStatus<InsertNode>(InsertNodeBuffer);

  if (!res) {
    fmt::print("Something went wrong: {} {} ", res.error(), res.path());
    EXPECT_TRUE(res.ok());
  } else {
    auto insertNode = res.get();
    EXPECT_EQ(insertNode.type, "InsertNode");
    // inVariable
    EXPECT_EQ(insertNode.inVariable.id, 0ul);
    EXPECT_EQ(insertNode.inVariable.name, "u");
    EXPECT_TRUE(insertNode.inVariable.isDataFromCollection);
    EXPECT_TRUE(insertNode.inVariable.isFullDocumentFromCollection);
  }
}