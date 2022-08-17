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
  /// if your attribute is "name" you will get a body back only with the name,
  /// otherwise there will be a body with a valid name + your given value.
  template<typename T>
  VPackBuilder createMinimumBodyWithOneValue(std::string const& attributeName,
                                             T const& attributeValue) {
    std::string colName = "test";
    VPackBuilder body;
    {
      VPackObjectBuilder guard(&body);
      if (attributeName != "name") {
        body.add("name", VPackValue(colName));
      }
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
                 arangodb::basics::Exception)
        << " On body " << body.toJson();
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
  EXPECT_FALSE(testee.allowSystem);
  EXPECT_FALSE(testee.doCompact);
  EXPECT_FALSE(testee.isVolatile);
  EXPECT_FALSE(testee.cacheEnabled);
  EXPECT_EQ(testee.type, TRI_col_type_e::TRI_COL_TYPE_DOCUMENT);

  EXPECT_EQ(testee.numberOfShards, 1);
  EXPECT_EQ(testee.replicationFactor, 1);
  EXPECT_EQ(testee.writeConcern, 1);

  EXPECT_EQ(testee.distributeShardsLike, "");
  EXPECT_EQ(testee.smartJoinAttribute, "");

  // TODO: this is just rudimentary
  // does not test internals yet
  EXPECT_TRUE(testee.computedValues.slice().isEmptyArray());
  EXPECT_TRUE(testee.schema.slice().isEmptyObject());
  EXPECT_TRUE(testee.keyOptions.slice().isEmptyObject());
}

TEST_F(PlanCollectionUserAPITest, test_illegal_names) {
  // The empty string
  assertParsingThrows(createMinimumBodyWithOneValue("name", ""));

  // Non String types
  assertParsingThrows(createMinimumBodyWithOneValue("name", 0));
  assertParsingThrows(
      createMinimumBodyWithOneValue("name", VPackSlice::emptyObjectSlice()));
  assertParsingThrows(
      createMinimumBodyWithOneValue("name", VPackSlice::emptyArraySlice()));
}

TEST_F(PlanCollectionUserAPITest, test_collection_type) {
  auto shouldBeEvaluatedToType = [&](VPackBuilder const& body,
                                     TRI_col_type_e type) {
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

// Basic test makros for the parser

// This macro generates a basic bool value test, checking if we get true/false
// through and other basic types are rejected.

#define GenerateBoolAttributeTest(attributeName)                              \
  TEST_F(PlanCollectionUserAPITest, test_##attributeName) {                   \
    auto shouldBeEvaluatedTo = [&](VPackBuilder const& body, bool expected) { \
      auto testee = PlanCollection::fromCreateAPIBody(body.slice());          \
      EXPECT_EQ(testee.attributeName, expected)                               \
          << "Parsing error in " << body.toJson();                            \
    };                                                                        \
    shouldBeEvaluatedTo(createMinimumBodyWithOneValue(#attributeName, true),  \
                        true);                                                \
    shouldBeEvaluatedTo(createMinimumBodyWithOneValue(#attributeName, false), \
                        false);                                               \
    assertParsingThrows(                                                      \
        createMinimumBodyWithOneValue(#attributeName, "test"));               \
    assertParsingThrows(createMinimumBodyWithOneValue(#attributeName, 1));    \
    assertParsingThrows(createMinimumBodyWithOneValue(                        \
        #attributeName, VPackSlice::emptyObjectSlice()));                     \
    assertParsingThrows(createMinimumBodyWithOneValue(                        \
        #attributeName, VPackSlice::emptyArraySlice()));                      \
  }

GenerateBoolAttributeTest(waitForSync);
GenerateBoolAttributeTest(doCompact);
GenerateBoolAttributeTest(isSystem);
GenerateBoolAttributeTest(isVolatile);
GenerateBoolAttributeTest(cacheEnabled);

// This macro generates a basic integer value test, checking if we get 2 and 42
// through and other basic types are rejected.
// NOTE: we also test 4.5 (double) right now this passes the validator, need to
// discuss if this is correct

#define GenerateIntegerAttributeTest(attributeName)                           \
  TEST_F(PlanCollectionUserAPITest, test_##attributeName) {                   \
    auto shouldBeEvaluatedTo = [&](VPackBuilder const& body,                  \
                                   uint64_t expected) {                       \
      auto testee = PlanCollection::fromCreateAPIBody(body.slice());          \
      EXPECT_EQ(testee.attributeName, expected)                               \
          << "Parsing error in " << body.toJson();                            \
    };                                                                        \
    shouldBeEvaluatedTo(createMinimumBodyWithOneValue(#attributeName, 2), 2); \
    shouldBeEvaluatedTo(createMinimumBodyWithOneValue(#attributeName, 42),    \
                        42);                                                  \
    shouldBeEvaluatedTo(createMinimumBodyWithOneValue(#attributeName, 4.5),   \
                        4);                                                   \
    assertParsingThrows(                                                      \
        createMinimumBodyWithOneValue(#attributeName, "test"));               \
    assertParsingThrows(createMinimumBodyWithOneValue(#attributeName, true)); \
    assertParsingThrows(createMinimumBodyWithOneValue(                        \
        #attributeName, VPackSlice::emptyObjectSlice()));                     \
    assertParsingThrows(createMinimumBodyWithOneValue(                        \
        #attributeName, VPackSlice::emptyArraySlice()));                      \
  }

GenerateIntegerAttributeTest(numberOfShards);
GenerateIntegerAttributeTest(replicationFactor);
GenerateIntegerAttributeTest(writeConcern);

#define GenerateStringAttributeTest(attributeName)                             \
  TEST_F(PlanCollectionUserAPITest, test_##attributeName) {                    \
    auto shouldBeEvaluatedTo = [&](VPackBuilder const& body,                   \
                                   std::string expected) {                     \
      auto testee = PlanCollection::fromCreateAPIBody(body.slice());           \
      EXPECT_EQ(testee.attributeName, expected)                                \
          << "Parsing error in " << body.toJson();                             \
    };                                                                         \
    shouldBeEvaluatedTo(createMinimumBodyWithOneValue(#attributeName, "test"), \
                        "test");                                               \
    shouldBeEvaluatedTo(                                                       \
        createMinimumBodyWithOneValue(#attributeName, "unknown"), "unknown");  \
    assertParsingThrows(createMinimumBodyWithOneValue(#attributeName, 2));     \
    assertParsingThrows(createMinimumBodyWithOneValue(#attributeName, 4.5));   \
    assertParsingThrows(createMinimumBodyWithOneValue(#attributeName, true));  \
    assertParsingThrows(createMinimumBodyWithOneValue(                         \
        #attributeName, VPackSlice::emptyObjectSlice()));                      \
    assertParsingThrows(createMinimumBodyWithOneValue(                         \
        #attributeName, VPackSlice::emptyArraySlice()));                       \
  }

GenerateStringAttributeTest(distributeShardsLike);
GenerateStringAttributeTest(smartJoinAttribute);

}  // namespace tests
}  // namespace arangodb
