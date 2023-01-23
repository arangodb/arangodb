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

#include "Aql/Optimizer2/Plan/QueryPostBody.h"

#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::plan;

class Optimizer2QueryPostBody : public testing::Test {
 protected:
  SharedSlice createMinimumBody() {
    return R"({
      "query": "RETURN 1",
      "bindVars": {},
      "options": {
        "verbosePlans": true
      }
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
  StatusT<QueryPostBody> parse(SharedSlice body) {
    return deserializeWithStatus<QueryPostBody>(body);
  }
  // Tries to serialize the given object of your type and returns
  // a filled VPackBuilder
  StatusT<SharedSlice> serialize(QueryPostBody testee) {
    return serializeWithStatus<QueryPostBody>(testee);
  }
};

// Generic tests
// Currently no generic tests implemented

// Default test

TEST_F(Optimizer2QueryPostBody, construction) {
  auto PlanBuffer = createMinimumBody();

  auto res = deserializeWithStatus<QueryPostBody>(PlanBuffer);

  if (!res) {
    fmt::print("Something went wrong: {} {} ", res.error(), res.path());
    EXPECT_TRUE(res.ok());
  } else {
    auto query = res.get();
    EXPECT_EQ(query.query, "RETURN 1");
    EXPECT_TRUE(query.bindVars.has_value());
    EXPECT_TRUE(query.bindVars.value().slice().isEmptyObject());
    EXPECT_TRUE(query.options.has_value());
    EXPECT_TRUE(query.options.value().verbosePlans);
  }
}
