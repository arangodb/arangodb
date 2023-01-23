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
#include "Aql/Optimizer2/PlanNodeTypes/Variable.h"
#include "Inspection/VPackInspection.h"
#include "InspectTestHelperMakros.h"
#include "velocypack/Collection.h"

#include <fmt/core.h>

using namespace arangodb::inspection;
using namespace arangodb::velocypack;
using namespace arangodb::aql::optimizer2::types;

class Optimizer2Variable : public testing::Test {
 protected:
  SharedSlice createMinimumBody() {
    return R"({
      "id": 2,
      "name": "1",
      "isFullDocumentFromCollection": false,
      "isDataFromCollection": false
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
  StatusT<Variable> parse(SharedSlice body) {
    return deserializeWithStatus<Variable>(body);
  }
  // Tries to serialize the given object of your type and returns
  // a filled VPackBuilder
  StatusT<SharedSlice> serialize(Variable testee) {
    return serializeWithStatus<Variable>(testee);
  }
};

// Generic tests

GenerateIntegerAttributeTest(Optimizer2Variable, id);
GenerateStringAttributeTest(Optimizer2Variable, name);
GenerateBoolAttributeTest(Optimizer2Variable, isFullDocumentFromCollection);
GenerateBoolAttributeTest(Optimizer2Variable, isDataFromCollection);

// Default test

TEST_F(Optimizer2Variable, construction) {
  auto VariableBuffer = createMinimumBody();

  auto res = deserializeWithStatus<Variable>(VariableBuffer);

  if (!res) {
    fmt::print("Something went wrong: {}", res.error());
    EXPECT_TRUE(res.ok());
  } else {
    auto variable = res.get();
    EXPECT_EQ(variable.id, 2u);
    EXPECT_EQ(variable.name, "1");
    EXPECT_FALSE(variable.isDataFromCollection);
    EXPECT_FALSE(variable.isFullDocumentFromCollection);
  }
}

TEST_F(Optimizer2Variable, constructionWithConstValue) {
  auto ConstantValueBuffer = R"([1, 2, 3])"_vpack;
  auto VariableBuffer = createMinimumBodyWithOneValue(
      "constantValue", ConstantValueBuffer.slice());
  auto res = deserializeWithStatus<Variable>(VariableBuffer.sharedSlice());

  if (!res) {
    fmt::print("Something went wrong: {}", res.error());
    EXPECT_TRUE(res.ok());
  } else {
    auto variable = res.get();
    EXPECT_EQ(variable.id, 2u);
    EXPECT_EQ(variable.name, "1");
    EXPECT_FALSE(variable.isDataFromCollection);
    EXPECT_FALSE(variable.isFullDocumentFromCollection);
    fmt::print("", variable.constantValue->slice().toString());
    EXPECT_TRUE(variable.constantValue->slice().isArray());
    EXPECT_EQ(variable.constantValue->slice().length(), 3u);
    EXPECT_EQ(variable.constantValue->slice().at(0).getInt(), 1);
    EXPECT_EQ(variable.constantValue->slice().at(1).getInt(), 2);
    EXPECT_EQ(variable.constantValue->slice().at(2).getInt(), 3);
  }
}