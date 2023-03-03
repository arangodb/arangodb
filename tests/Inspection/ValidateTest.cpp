////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Inspection/ValidateInspector.h"
#include "Inspection/InspectionTestHelper.h"

namespace {
using namespace arangodb;

struct ValidateInspectorTest : public ::testing::Test {
  arangodb::inspection::ValidateInspector<> inspector;
};

TEST_F(ValidateInspectorTest, validate_object_with_invariant_fulfilled) {
  Invariant i{.i = 42, .s = "foobar"};
  auto result = inspector.apply(i);
  ASSERT_TRUE(result.ok());
}

TEST_F(ValidateInspectorTest, validate_object_with_invariant_not_fulfilled) {
  {
    Invariant i{.i = 0, .s = "foobar"};
    auto result = inspector.apply(i);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Field invariant failed", result.error());
    EXPECT_EQ("i", result.path());
  }

  {
    Invariant i{.i = 42, .s = ""};
    auto result = inspector.apply(i);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Field invariant failed", result.error());
    EXPECT_EQ("s", result.path());
  }
}

TEST_F(ValidateInspectorTest,
       validate_object_with_invariant_Result_not_fulfilled) {
  {
    InvariantWithResult i{.i = 0, .s = ""};
    auto result = inspector.apply(i);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Must not be zero", result.error());
    EXPECT_EQ("i", result.path());
  }

  {
    Invariant i{.i = 42, .s = ""};
    auto result = inspector.apply(i);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Field invariant failed", result.error());
    EXPECT_EQ("s", result.path());
  }
}

TEST_F(ValidateInspectorTest, validate_object_with_object_invariant) {
  ObjectInvariant o{.i = 42, .s = ""};
  auto result = inspector.apply(o);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Object invariant failed", result.error());
}

TEST_F(ValidateInspectorTest, validate_object_with_nested_invariant) {
  {
    NestedInvariant n{.i = {.i = 0, .s = "x"}, .o = {.i = 42, .s = "x"}};
    auto result = inspector.apply(n);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Field invariant failed", result.error());
    EXPECT_EQ("i.i", result.path());
  }

  {
    NestedInvariant n{.i = {.i = 42, .s = "x"}, .o = {.i = 0, .s = "x"}};
    auto result = inspector.apply(n);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Object invariant failed", result.error());
    EXPECT_EQ("o", result.path());
  }
}

TEST_F(ValidateInspectorTest, validate_embedded_object) {
  NestedEmbedding n{
      Embedded{.a = 1, .inner = {.i = 42, .s = "foobar"}, .b = 2}};
  auto result = inspector.apply(n);
  ASSERT_TRUE(result.ok());
}

TEST_F(ValidateInspectorTest,
       validate_embedded_object_with_invariant_not_fulfilled) {
  NestedEmbedding n{Embedded{.a = 1, .inner = {.i = 0, .s = "foobar"}, .b = 2}};
  auto result = inspector.apply(n);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Field invariant failed", result.error());
  EXPECT_EQ("i", result.path());
}

TEST_F(ValidateInspectorTest,
       validate_embedded_object_with_object_invariant_not_fulfilled) {
  NestedEmbeddingWithObjectInvariant o{
      EmbeddedObjectInvariant{.a = 1, .inner = {.i = 42, .s = ""}, .b = 2}};
  auto result = inspector.apply(o);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Object invariant failed", result.error());
}

TEST(ValidateInspectorContext, validate_with_context) {
  struct Context {
    int defaultInt;
    int minInt;
    std::string defaultString;
  };
  Context ctxt{.defaultInt = 0, .minInt = 42, .defaultString = ""};

  {
    inspection::ValidateInspector<Context> inspector(ctxt);
    WithContext data{.i = 43, .s = ""};
    auto result = inspector.apply(data);
    EXPECT_TRUE(result.ok());
  }

  {
    inspection::ValidateInspector inspector(ctxt);
    WithContext data{.i = 42, .s = ""};
    auto result = inspector.apply(data);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Field invariant failed", result.error());
    EXPECT_EQ("i", result.path());
  }
}

}  // namespace
