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
#include "Inspection/VPackInspection.h"
#include "InspectTestHelperMakros.h"
#include "velocypack/Collection.h"

#include "Aql/Optimizer2/Plan/VerbosePlan.h"
#include "Aql/Optimizer2/Plan/PlanRPCHandler.h"

#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::plan;

class Optimizer2VerbosePlan : public testing::Test {
 protected:
  SharedSlice createMinimumBody() {
    return R"({
      "plan": {
        "nodes": [
          {
          "type": "SingletonNode",
          "typeID": 1,
          "dependencies": [],
          "id": 1,
          "parents": [
            2
          ],
          "estimatedCost": 1,
          "estimatedNrItems": 1,
          "depth": 0,
          "varInfoList": [
            {
              "VariableId": 1,
              "depth": 0,
              "RegisterId": 65536
            }
          ],
          "nrRegs": [
            0
          ],
          "nrConstRegs": 1,
          "regsToClear": [],
          "varsUsedLaterStack": [
            [
              {
                "id": 1,
                "name": "0",
                "isFullDocumentFromCollection": false,
                "isDataFromCollection": false,
                "constantValue": 1
              }
            ]
          ],
          "regsToKeepStack": [
            []
          ],
          "varsValidStack": [
            []
          ],
          "isInSplicedSubquery": false,
          "isAsyncPrefetchEnabled": false,
          "isCallstackSplitEnabled": false
          },
          {
          "type": "CalculationNode",
          "typeID": 7,
          "dependencies": [
            1
          ],
          "id": 2,
          "parents": [
            3
          ],
          "estimatedCost": 2,
          "estimatedNrItems": 1,
          "depth": 0,
          "varInfoList": [
            {
              "VariableId": 1,
              "depth": 0,
              "RegisterId": 65536
            }
          ],
          "nrRegs": [
            0
          ],
          "nrConstRegs": 1,
          "regsToClear": [],
          "varsUsedLaterStack": [
            [
              {
                "id": 1,
                "name": "0",
                "isFullDocumentFromCollection": false,
                "isDataFromCollection": false,
                "constantValue": 1
              }
            ]
          ],
          "regsToKeepStack": [
            []
          ],
          "varsValidStack": [
            [
              {
                "id": 1,
                "name": "0",
                "isFullDocumentFromCollection": false,
                "isDataFromCollection": false,
                "constantValue": 1
              }
            ]
          ],
          "isInSplicedSubquery": false,
          "isAsyncPrefetchEnabled": false,
          "isCallstackSplitEnabled": false,
          "expression": {
            "type": "value",
            "typeID": 40,
            "value": 1,
            "vType": "int",
            "vTypeID": 2
          },
          "outVariable": {
            "id": 1,
            "name": "0",
            "isFullDocumentFromCollection": false,
            "isDataFromCollection": false,
            "constantValue": 1
          },
          "canThrow": false,
          "expressionType": "json",
          "functions": []
          },
          {
          "type": "ReturnNode",
          "typeID": 18,
          "dependencies": [
            2
          ],
          "id": 3,
          "parents": [],
          "estimatedCost": 3,
          "estimatedNrItems": 1,
          "depth": 0,
          "varInfoList": [
            {
              "VariableId": 1,
              "depth": 0,
              "RegisterId": 65536
            }
          ],
          "nrRegs": [
            0
          ],
          "nrConstRegs": 1,
          "regsToClear": [],
          "varsUsedLaterStack": [
            []
          ],
          "regsToKeepStack": [
            []
          ],
          "varsValidStack": [
            [
              {
                "id": 1,
                "name": "0",
                "isFullDocumentFromCollection": false,
                "isDataFromCollection": false,
                "constantValue": 1
              }
            ]
          ],
          "isInSplicedSubquery": false,
          "isAsyncPrefetchEnabled": false,
          "isCallstackSplitEnabled": false,
          "inVariable": {
            "id": 1,
            "name": "0",
            "isFullDocumentFromCollection": false,
            "isDataFromCollection": false,
            "constantValue": 1
          },
          "count": true
          }
          ],
          "rules": [],
          "collections": [],
          "variables": [
          {
          "id": 1,
          "name": "0",
          "isFullDocumentFromCollection": false,
          "isDataFromCollection": false,
          "constantValue": 1
          }
          ],
          "estimatedCost": 3,
          "estimatedNrItems": 1,
          "isModificationQuery": false
        },
        "cacheable": true,
        "warnings": [],
        "error": false,
        "code": 200
    })"_vpack;
  }

  template<typename T>
  VPackBuilder createMinimumBodyWithOneValue(std::string const& attributeName,
                                             T const& attributeValue) {
    SharedSlice VariableBuffer = createMinimumBody();

    VPackBuilder b;
    {
      VPackObjectBuilder guard{&b};
      if constexpr (std::is_same_v<T, VPackSlice>) {
        b.add(attributeName, attributeValue);
      } else {
        b.add(attributeName, VPackValue(attributeValue));
      }
    }

    return arangodb::velocypack::Collection::merge(VariableBuffer.slice(),
                                                   b.slice(), false);
  }

  // Tries to parse the given body and returns a ResulT of your Type under
  // test.
  StatusT<VerbosePlan> parse(SharedSlice body) {
    return deserializeWithStatus<VerbosePlan>(body);
  }
  // Tries to serialize the given object of your type and returns
  // a filled VPackBuilder
  StatusT<SharedSlice> serialize(VerbosePlan testee) {
    return serializeWithStatus<VerbosePlan>(testee);
  }
};

// Generic tests
// Currently no generic tests implemented

// Default test

TEST_F(Optimizer2VerbosePlan, construction) {
  auto PlanBuffer = createMinimumBody();

  auto res = deserializeWithStatus<VerbosePlan>(PlanBuffer);

  if (!res) {
    fmt::print("Something went wrong: {} {} ", res.error(), res.path());
    EXPECT_TRUE(res.ok());
  } else {
    auto verbosePlan = res.get();
    auto plan = verbosePlan.plan.slice();
    EXPECT_TRUE(plan.isObject());
    EXPECT_TRUE(plan.hasKey("nodes"));
    EXPECT_TRUE(plan.get("nodes").isArray());
  }
}
