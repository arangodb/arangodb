////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
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

TEST_F(ValidateInspectorTest, validate_checks_invariant_regardless_of_condition) {
  {  // skipped field, invariant violated
    ConditionalWithInvariant c{.version = 1, .newField = 0};
    auto result = inspector.apply(c);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Field invariant failed", result.error());
    EXPECT_EQ("newField", result.path());
  }
  {  // skipped field, the member's own value satisfies the invariant
    ConditionalWithInvariant c{.version = 1, .newField = 5};
    auto result = inspector.apply(c);
    ASSERT_TRUE(result.ok()) << result.error();
  }
  {  // processed field, invariant violated
    ConditionalWithInvariant c{.version = 2, .newField = 0};
    auto result = inspector.apply(c);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Field invariant failed", result.error());
    EXPECT_EQ("newField", result.path());
  }
}

TEST_F(ValidateInspectorTest, validate_checks_inner_invariants_of_skipped_field) {
  ConditionalNested c{.version = 1, .inner = {.i = 0, .s = "x"}};
  auto result = inspector.apply(c);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Field invariant failed", result.error());
  EXPECT_EQ("inner.i", result.path());
}

TEST_F(ValidateInspectorTest, validate_also_checks_the_inactive_alternative) {
  // Conditions do not apply here, so both alternatives sharing an attribute
  // name are validated - the inactive one must hold a valid value too.
  AlternativeFields a{.useId = true, .id = 7, .name = ""};
  auto result = inspector.apply(a);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Field invariant failed", result.error());
  EXPECT_EQ("target", result.path());
}

TEST_F(ValidateInspectorTest, validate_does_not_apply_fallback_to_skipped_field) {
  // Loading applies the fallback before checking the invariant, so the same
  // object loads fine. Validation must not modify the object to "fix" it.
  ConditionalFallbackAndInvariant c{.version = 1, .newField = 0};
  auto result = inspector.apply(c);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Field invariant failed", result.error());
  EXPECT_EQ("newField", result.path());
  EXPECT_EQ(0, c.newField);
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
