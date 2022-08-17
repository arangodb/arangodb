////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Basics/Exceptions.h"
#include "Logger/LogMacros.h"
#include "VocBase/Properties/PlanCollection.h"
#include <velocypack/Builder.h>

namespace arangodb {
namespace tests {

class PlanCollectionUserAPITest : public ::testing::Test {
 protected:
  /// @brief this generates the minimal required body, only exchanging one
  /// attribute with the given value should work on all basic types
  template<typename T>
  VPackBuilder createMinimumBodyWithOneValue(std::string const& attributeName,
                                             T const& attributeValue) {
    std::string colName = "test";
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      body.add("name", VPackValue(colName));
      if constexpr (std::is_same_v<T, VPackSlice>) {
        body.add(attributeName, attributeValue);
      } else {
        body.add(attributeName, VPackValue(attributeValue));
      }
    }
    return body;
  }

  void assertParsingThrows(VPackBuilder const& body) {
    EXPECT_THROW(PlanCollection::fromCreateAPIBody(body.slice()),
                 arangodb::basics::Exception);
  }
};

TEST_F(PlanCollectionUserAPITest, test_requires_some_input) {
  VPackBuilder body;
  { VPackObjectBuilder guard(&body); }
  assertParsingThrows(body);
}

TEST_F(PlanCollectionUserAPITest, test_minimal_user_input) {
  std::string colName = "test";
  VPackBuilder body;
  {
    VPackObjectBuilder guard(&body);
    body.add("name", VPackValue(colName));
  }
  auto testee = PlanCollection::fromCreateAPIBody(body.slice());
  EXPECT_EQ(testee.name, colName);
  // Test Default values
  EXPECT_FALSE(testee.waitForSync);
  EXPECT_FALSE(testee.isSystem);
  EXPECT_EQ(testee.type, TRI_col_type_e::TRI_COL_TYPE_DOCUMENT);

  // TODO: this is just rudimentary
  // does not test internals yet
  EXPECT_TRUE(testee.schema.slice().isObject());
  EXPECT_TRUE(testee.keyOptions.slice().isObject());
}

TEST_F(PlanCollectionUserAPITest, test_collection_type) {
  auto shouldBeEvaluatedToType = [&](VPackBuilder body, TRI_col_type_e type) {
    auto testee = PlanCollection::fromCreateAPIBody(body.slice());
    EXPECT_EQ(testee.type, type) << "Parsing error in " << body.toJson();
  };

  // Edge types, we only have two valid ways to get edges
  shouldBeEvaluatedToType(createMinimumBodyWithOneValue("type", 3),
                          TRI_col_type_e::TRI_COL_TYPE_EDGE);

  shouldBeEvaluatedToType(createMinimumBodyWithOneValue("type", 2),
                          TRI_col_type_e::TRI_COL_TYPE_DOCUMENT);

  /// The following defaulted to edge before (not mentioned in doc since 3.3)
  assertParsingThrows(createMinimumBodyWithOneValue("type", "edge"));
  /// The following defaulted to document before (not mentioned in doc
  /// since 3.3):
  assertParsingThrows(createMinimumBodyWithOneValue("type", 0));
  assertParsingThrows(createMinimumBodyWithOneValue("type", 1));
  assertParsingThrows(createMinimumBodyWithOneValue("type", 4));
  assertParsingThrows(createMinimumBodyWithOneValue("type", "document"));

  assertParsingThrows(createMinimumBodyWithOneValue("type", "dogfather"));
  assertParsingThrows(
      createMinimumBodyWithOneValue("type", VPackSlice::emptyObjectSlice()));
  assertParsingThrows(
      createMinimumBodyWithOneValue("type", VPackSlice::emptyArraySlice()));
}

}  // namespace tests
}  // namespace arangodb
