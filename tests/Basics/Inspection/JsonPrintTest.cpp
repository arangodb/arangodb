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

#include <ostream>

#include "Inspection/Format.h"
#include "Inspection/JsonPrintInspector.h"

#include "InspectionTestHelper.h"

namespace {
using namespace arangodb;

struct JsonPrintInspectorTest : public ::testing::Test {
  std::ostringstream stream;
  inspection::JsonPrintInspector<> inspector{
      stream, inspection::JsonPrintFormat::kPretty};
};

TEST_F(JsonPrintInspectorTest, store_empty_objecty) {
  auto empty = AnEmptyObject{};
  auto result = inspector.apply(empty);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("{\n}", stream.str());
}

TEST_F(JsonPrintInspectorTest, store_int) {
  int x = 42;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("42", stream.str());
}

TEST_F(JsonPrintInspectorTest, store_double) {
  double x = 123.456;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("123.456", stream.str());
}

TEST_F(JsonPrintInspectorTest, store_bool) {
  bool x = true;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("true", stream.str());
}

TEST_F(JsonPrintInspectorTest, store_string) {
  std::string x = "foobar";
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("\"foobar\"", stream.str());
}

TEST_F(JsonPrintInspectorTest, store_object) {
  Dummy const f{.i = 42, .d = 123.456, .b = true, .s = "foobar"};
  auto result = inspector.apply(f);
  EXPECT_TRUE(result.ok());

  auto expected = R"({
  "i": 42,
  "d": 123.456,
  "b": true,
  "s": "foobar"
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_nested_object) {
  Nested b{.dummy = {.i = 42, .d = 123.456, .b = true, .s = "foobar"}};
  auto result = inspector.apply(b);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  "dummy": {
    "i": 42,
    "d": 123.456,
    "b": true,
    "s": "foobar"
  }
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_nested_object_without_nesting) {
  Container c{.i = {.value = 42}};
  auto result = inspector.apply(c);
  ASSERT_TRUE(result.ok());

  auto expected = "{\n  \"i\": 42\n}";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_list) {
  List l{.vec = {{1}, {2}, {3}}, .list = {4, 5}};
  auto result = inspector.apply(l);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  "vec": [
    {
      "i": 1
    },
    {
      "i": 2
    },
    {
      "i": 3
    }
  ],
  "list": [
    4,
    5
  ]
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_map) {
  Map m{.map = {{"1", {1}}, {"2", {2}}, {"3", {3}}}, .unordered = {{"4", 4}}};
  auto result = inspector.apply(m);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  "map": {
    "1": {
      "i": 1
    },
    "2": {
      "i": 2
    },
    "3": {
      "i": 3
    }
  },
  "unordered": {
    "4": 4
  }
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_tuples) {
  Tuple t{.tuple = {"foo", 42, 12.34},
          .pair = {987, "bar"},
          .array1 = {"a", "b"},
          .array2 = {1, 2, 3}};
  auto result = inspector.apply(t);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  "tuple": [
    "foo",
    42,
    12.34
  ],
  "pair": [
    987,
    "bar"
  ],
  "array1": [
    "a",
    "b"
  ],
  "array2": [
    1,
    2,
    3
  ]
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_optional) {
  Optional o{.a = std::nullopt,
             .b = std::nullopt,
             .x = std::nullopt,
             .y = "blubb",
             .vec = {1, std::nullopt, 3},
             .map = {{"1", 1}, {"2", std::nullopt}, {"3", 3}}};
  auto result = inspector.apply(o);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  "a": null,
  "b": null,
  "y": "blubb",
  "vec": [
    1,
    null,
    3
  ],
  "map": {
    "1": 1,
    "2": null,
    "3": 3
  }
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_optional_pointer) {
  Pointer p{.a = nullptr,
            .b = std::make_shared<int>(42),
            .c = nullptr,
            .d = std::make_unique<Container>(Container{.i = {.value = 43}}),
            .vec = {},
            .x = {},
            .y = {}};
  p.vec.push_back(std::make_unique<int>(1));
  p.vec.push_back(nullptr);
  p.vec.push_back(std::make_unique<int>(2));
  auto result = inspector.apply(p);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  "b": 42,
  "d": {
    "i": 43
  },
  "vec": [
    1,
    null,
    2
  ],
  "x": null,
  "y": null
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_object_with_field_transform) {
  FieldTransform f{.x = 42};
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  "x": "42"
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_object_with_optional_field_transform) {
  OptionalFieldTransform f{.x = 1, .y = std::nullopt, .z = 3};
  auto result = inspector.apply(f);

  auto expected = R"({
  "x": "1",
  "z": "3"
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_type_with_custom_specialization) {
  Specialization s{.i = 42, .s = "foobar"};
  auto result = inspector.apply(s);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  "i": 42,
  "s": "foobar"
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_type_with_explicitly_ignored_fields) {
  ExplicitIgnore e{.s = "foobar"};
  auto result = inspector.apply(e);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  "s": "foobar"
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_type_with_unsafe_fields) {
  velocypack::Builder localBuilder;
  localBuilder.add(VPackValue("blubb"));
  std::string_view hashedString = "hashedString";
  Unsafe u{.view = "foobar",
           .slice = localBuilder.slice(),
           .hashed = {hashedString.data(),
                      static_cast<uint32_t>(hashedString.size())}};
  auto result = inspector.apply(u);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  "view": "foobar",
  "slice": "blubb",
  "hashed": "hashedString"
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_qualified_variant) {
  QualifiedVariant d{.a = {"foobar"},
                     .b = {42},
                     .c = {Struct1{1}},
                     .d = {Struct2{2}},
                     .e = {std::monostate{}}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  "a": "foobar",
  "b": {
    "t": "int",
    "v": 42
  },
  "c": {
    "t": "Struct1",
    "v": {
      "v": 1
    }
  },
  "d": {
    "t": "Struct2",
    "v": {
      "v": 2
    }
  },
  "e": {
    "t": "nil",
    "v": {
    }
  }
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_unqualified_variant) {
  UnqualifiedVariant d{.a = {"foobar"},
                       .b = {42},
                       .c = {Struct1{1}},
                       .d = {Struct2{2}},
                       .e = {std::monostate{}}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  "a": {
    "string": "foobar"
  },
  "b": 42,
  "c": {
    "Struct1": {
      "v": 1
    }
  },
  "d": {
    "Struct2": {
      "v": 2
    }
  },
  "e": {
    "nil": {
    }
  }
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_string_enum) {
  std::vector<MyStringEnum> enums{MyStringEnum::kValue1, MyStringEnum::kValue2,
                                  MyStringEnum::kValue3};
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());

  auto expected = R"([
  "value1",
  "value2",
  "value2"
])";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_int_enum) {
  std::vector<MyIntEnum> enums{MyIntEnum::kValue1, MyIntEnum::kValue2,
                               MyIntEnum::kValue3};
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());

  auto expected = R"([
  1,
  2,
  2
])";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_mixed_enum) {
  std::vector<MyMixedEnum> enums{MyMixedEnum::kValue1, MyMixedEnum::kValue2};
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());

  auto expected = R"([
  "value1",
  "value2"
])";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest,
       store_string_enum_returns_error_for_unknown_value) {
  MyStringEnum val = static_cast<MyStringEnum>(42);
  auto result = inspector.apply(val);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Unknown enum value 42", result.error());
}

TEST_F(JsonPrintInspectorTest, store_int_enum_returns_error_for_unknown_value) {
  MyIntEnum val = static_cast<MyIntEnum>(42);
  auto result = inspector.apply(val);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Unknown enum value 42", result.error());
}

TEST_F(JsonPrintInspectorTest,
       store_mixed_enum_returns_error_for_unknown_value) {
  MyMixedEnum val = static_cast<MyMixedEnum>(42);
  auto result = inspector.apply(val);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Unknown enum value 42", result.error());
}

TEST_F(JsonPrintInspectorTest, store_inline_variant) {
  InlineVariant d{.a = {"foobar"},
                  .b = {Struct1{.v = 42}},
                  .c = {std::vector<int>{1, 2, 3}},
                  .d = {TypedInt{.value = 123}},
                  .e = {std::tuple{"blubb", 987, true}}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  "a": "foobar",
  "b": {
    "v": 42
  },
  "c": [
    1,
    2,
    3
  ],
  "d": 123,
  "e": [
    "blubb",
    987,
    true
  ]
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_embedded_variant) {
  EmbeddedVariant d{.a = {Struct1{1}},
                    .b = {Struct2{2}},
                    .c = {Struct3{.a = 1, .b = 2}},
                    .d = {true}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  "a": {
    "t": "Struct1",
    "v": 1
  },
  "b": {
    "t": "Struct2",
    "v": 2
  },
  "c": {
    "t": "Struct3",
    "a": 1,
    "b": 2
  },
  "d": true
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorTest, store_embedded_fields) {
  NestedEmbedding const n{
      Embedded{.a = 1, .inner = {.i = 42, .s = "foobar"}, .b = 2}};
  auto result = inspector.apply(n);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  "a": 1,
  "i": 42,
  "s": "foobar",
  "b": 2
})";
  EXPECT_EQ(expected, stream.str());
}

struct JsonPrintInspectorCompactTest : public ::testing::Test {
  std::ostringstream stream;
  inspection::JsonPrintInspector<> inspector{
      stream, inspection::JsonPrintFormat::kCompact};
};

TEST_F(JsonPrintInspectorCompactTest, store_empty_objecty) {
  auto empty = AnEmptyObject{};
  auto result = inspector.apply(empty);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("{ }", stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_object) {
  Dummy const f{.i = 42, .d = 123.456, .b = true, .s = "foobar"};
  auto result = inspector.apply(f);
  EXPECT_TRUE(result.ok());

  auto expected = R"({ "i": 42, "d": 123.456, "b": true, "s": "foobar" })";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_nested_object) {
  Nested b{.dummy = {.i = 42, .d = 123.456, .b = true, .s = "foobar"}};
  auto result = inspector.apply(b);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({ "dummy": { "i": 42, "d": 123.456, "b": true, "s": "foobar" } })";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_nested_object_without_nesting) {
  Container c{.i = {.value = 42}};
  auto result = inspector.apply(c);
  ASSERT_TRUE(result.ok());

  auto expected = "{ \"i\": 42 }";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_list) {
  List l{.vec = {{1}, {2}, {3}}, .list = {4, 5}};
  auto result = inspector.apply(l);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({ "vec": [ { "i": 1 }, { "i": 2 }, { "i": 3 } ], "list": [ 4, 5 ] })";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_map) {
  Map m{.map = {{"1", {1}}, {"2", {2}}, {"3", {3}}}, .unordered = {{"4", 4}}};
  auto result = inspector.apply(m);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({ "map": { "1": { "i": 1 }, "2": { "i": 2 }, "3": { "i": 3 } }, "unordered": { "4": 4 } })";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_tuples) {
  Tuple t{.tuple = {"foo", 42, 12.34},
          .pair = {987, "bar"},
          .array1 = {"a", "b"},
          .array2 = {1, 2, 3}};
  auto result = inspector.apply(t);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({ "tuple": [ "foo", 42, 12.34 ], "pair": [ 987, "bar" ], "array1": [ "a", "b" ], "array2": [ 1, 2, 3 ] })";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_optional) {
  Optional o{.a = std::nullopt,
             .b = std::nullopt,
             .x = std::nullopt,
             .y = "blubb",
             .vec = {1, std::nullopt, 3},
             .map = {{"1", 1}, {"2", std::nullopt}, {"3", 3}}};
  auto result = inspector.apply(o);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({ "a": null, "b": null, "y": "blubb", "vec": [ 1, null, 3 ], "map": { "1": 1, "2": null, "3": 3 } })";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_optional_pointer) {
  Pointer p{.a = nullptr,
            .b = std::make_shared<int>(42),
            .c = nullptr,
            .d = std::make_unique<Container>(Container{.i = {.value = 43}}),
            .vec = {},
            .x = {},
            .y = {}};
  p.vec.push_back(std::make_unique<int>(1));
  p.vec.push_back(nullptr);
  p.vec.push_back(std::make_unique<int>(2));
  auto result = inspector.apply(p);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({ "b": 42, "d": { "i": 43 }, "vec": [ 1, null, 2 ], "x": null, "y": null })";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_object_with_field_transform) {
  FieldTransform f{.x = 42};
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());

  auto expected = R"({ "x": "42" })";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest,
       store_object_with_optional_field_transform) {
  OptionalFieldTransform f{.x = 1, .y = std::nullopt, .z = 3};
  auto result = inspector.apply(f);

  auto expected = R"({ "x": "1", "z": "3" })";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_type_with_custom_specialization) {
  Specialization s{.i = 42, .s = "foobar"};
  auto result = inspector.apply(s);
  ASSERT_TRUE(result.ok());

  auto expected = R"({ "i": 42, "s": "foobar" })";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest,
       store_type_with_explicitly_ignored_fields) {
  ExplicitIgnore e{.s = "foobar"};
  auto result = inspector.apply(e);
  ASSERT_TRUE(result.ok());

  auto expected = R"({ "s": "foobar" })";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_type_with_unsafe_fields) {
  velocypack::Builder localBuilder;
  localBuilder.add(VPackValue("blubb"));
  std::string_view hashedString = "hashedString";
  Unsafe u{.view = "foobar",
           .slice = localBuilder.slice(),
           .hashed = {hashedString.data(),
                      static_cast<uint32_t>(hashedString.size())}};
  auto result = inspector.apply(u);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({ "view": "foobar", "slice": "blubb", "hashed": "hashedString" })";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_qualified_variant) {
  QualifiedVariant d{.a = {"foobar"},
                     .b = {42},
                     .c = {Struct1{1}},
                     .d = {Struct2{2}},
                     .e = {std::monostate{}}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({ "a": "foobar", "b": { "t": "int", "v": 42 }, "c": { "t": "Struct1", "v": { "v": 1 } }, "d": { "t": "Struct2", "v": { "v": 2 } }, "e": { "t": "nil", "v": { } } })";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_unqualified_variant) {
  UnqualifiedVariant d{.a = {"foobar"},
                       .b = {42},
                       .c = {Struct1{1}},
                       .d = {Struct2{2}},
                       .e = {std::monostate{}}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({ "a": { "string": "foobar" }, "b": 42, "c": { "Struct1": { "v": 1 } }, "d": { "Struct2": { "v": 2 } }, "e": { "nil": { } } })";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_string_enum) {
  std::vector<MyStringEnum> enums{MyStringEnum::kValue1, MyStringEnum::kValue2,
                                  MyStringEnum::kValue3};
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());

  auto expected = R"([ "value1", "value2", "value2" ])";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_int_enum) {
  std::vector<MyIntEnum> enums{MyIntEnum::kValue1, MyIntEnum::kValue2,
                               MyIntEnum::kValue3};
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());

  auto expected = R"([ 1, 2, 2 ])";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_mixed_enum) {
  std::vector<MyMixedEnum> enums{MyMixedEnum::kValue1, MyMixedEnum::kValue2};
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());

  auto expected = R"([ "value1", "value2" ])";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_inline_variant) {
  InlineVariant d{.a = {"foobar"},
                  .b = {Struct1{.v = 42}},
                  .c = {std::vector<int>{1, 2, 3}},
                  .d = {TypedInt{.value = 123}},
                  .e = {std::tuple{"blubb", 987, true}}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({ "a": "foobar", "b": { "v": 42 }, "c": [ 1, 2, 3 ], "d": 123, "e": [ "blubb", 987, true ] })";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_embedded_variant) {
  EmbeddedVariant d{.a = {Struct1{1}},
                    .b = {Struct2{2}},
                    .c = {Struct3{.a = 1, .b = 2}},
                    .d = {true}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({ "a": { "t": "Struct1", "v": 1 }, "b": { "t": "Struct2", "v": 2 }, "c": { "t": "Struct3", "a": 1, "b": 2 }, "d": true })";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorCompactTest, store_embedded_fields) {
  NestedEmbedding const n{
      Embedded{.a = 1, .inner = {.i = 42, .s = "foobar"}, .b = 2}};
  auto result = inspector.apply(n);
  ASSERT_TRUE(result.ok());

  auto expected = R"({ "a": 1, "i": 42, "s": "foobar", "b": 2 })";
  EXPECT_EQ(expected, stream.str());
}

struct JsonPrintInspectorMinimalTest : public ::testing::Test {
  std::ostringstream stream;
  inspection::JsonPrintInspector<> inspector{
      stream, inspection::JsonPrintFormat::kMinimal};
};

TEST_F(JsonPrintInspectorMinimalTest, store_empty_objecty) {
  auto empty = AnEmptyObject{};
  auto result = inspector.apply(empty);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("{}", stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_object) {
  Dummy const f{.i = 42, .d = 123.456, .b = true, .s = "foobar"};
  auto result = inspector.apply(f);
  EXPECT_TRUE(result.ok());

  auto expected = R"({"i":42,"d":123.456,"b":true,"s":"foobar"})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_nested_object) {
  Nested b{.dummy = {.i = 42, .d = 123.456, .b = true, .s = "foobar"}};
  auto result = inspector.apply(b);
  ASSERT_TRUE(result.ok());

  auto expected = R"({"dummy":{"i":42,"d":123.456,"b":true,"s":"foobar"}})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_nested_object_without_nesting) {
  Container c{.i = {.value = 42}};
  auto result = inspector.apply(c);
  ASSERT_TRUE(result.ok());

  auto expected = "{\"i\":42}";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_list) {
  List l{.vec = {{1}, {2}, {3}}, .list = {4, 5}};
  auto result = inspector.apply(l);
  ASSERT_TRUE(result.ok());

  auto expected = R"({"vec":[{"i":1},{"i":2},{"i":3}],"list":[4,5]})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_map) {
  Map m{.map = {{"1", {1}}, {"2", {2}}, {"3", {3}}}, .unordered = {{"4", 4}}};
  auto result = inspector.apply(m);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({"map":{"1":{"i":1},"2":{"i":2},"3":{"i":3}},"unordered":{"4":4}})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_tuples) {
  Tuple t{.tuple = {"foo", 42, 12.34},
          .pair = {987, "bar"},
          .array1 = {"a", "b"},
          .array2 = {1, 2, 3}};
  auto result = inspector.apply(t);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({"tuple":["foo",42,12.34],"pair":[987,"bar"],"array1":["a","b"],"array2":[1,2,3]})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_optional) {
  Optional o{.a = std::nullopt,
             .b = std::nullopt,
             .x = std::nullopt,
             .y = "blubb",
             .vec = {1, std::nullopt, 3},
             .map = {{"1", 1}, {"2", std::nullopt}, {"3", 3}}};
  auto result = inspector.apply(o);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({"a":null,"b":null,"y":"blubb","vec":[1,null,3],"map":{"1":1,"2":null,"3":3}})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_optional_pointer) {
  Pointer p{.a = nullptr,
            .b = std::make_shared<int>(42),
            .c = nullptr,
            .d = std::make_unique<Container>(Container{.i = {.value = 43}}),
            .vec = {},
            .x = {},
            .y = {}};
  p.vec.push_back(std::make_unique<int>(1));
  p.vec.push_back(nullptr);
  p.vec.push_back(std::make_unique<int>(2));
  auto result = inspector.apply(p);
  ASSERT_TRUE(result.ok());

  auto expected = R"({"b":42,"d":{"i":43},"vec":[1,null,2],"x":null,"y":null})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_object_with_field_transform) {
  FieldTransform f{.x = 42};
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());

  auto expected = R"({"x":"42"})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest,
       store_object_with_optional_field_transform) {
  OptionalFieldTransform f{.x = 1, .y = std::nullopt, .z = 3};
  auto result = inspector.apply(f);

  auto expected = R"({"x":"1","z":"3"})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_type_with_custom_specialization) {
  Specialization s{.i = 42, .s = "foobar"};
  auto result = inspector.apply(s);
  ASSERT_TRUE(result.ok());

  auto expected = R"({"i":42,"s":"foobar"})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest,
       store_type_with_explicitly_ignored_fields) {
  ExplicitIgnore e{.s = "foobar"};
  auto result = inspector.apply(e);
  ASSERT_TRUE(result.ok());

  auto expected = R"({"s":"foobar"})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_type_with_unsafe_fields) {
  velocypack::Builder localBuilder;
  localBuilder.add(VPackValue("blubb"));
  std::string_view hashedString = "hashedString";
  Unsafe u{.view = "foobar",
           .slice = localBuilder.slice(),
           .hashed = {hashedString.data(),
                      static_cast<uint32_t>(hashedString.size())}};
  auto result = inspector.apply(u);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({"view":"foobar","slice":"blubb","hashed":"hashedString"})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_qualified_variant) {
  QualifiedVariant d{.a = {"foobar"},
                     .b = {42},
                     .c = {Struct1{1}},
                     .d = {Struct2{2}},
                     .e = {std::monostate{}}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({"a":"foobar","b":{"t":"int","v":42},"c":{"t":"Struct1","v":{"v":1}},"d":{"t":"Struct2","v":{"v":2}},"e":{"t":"nil","v":{}}})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_unqualified_variant) {
  UnqualifiedVariant d{.a = {"foobar"},
                       .b = {42},
                       .c = {Struct1{1}},
                       .d = {Struct2{2}},
                       .e = {std::monostate{}}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({"a":{"string":"foobar"},"b":42,"c":{"Struct1":{"v":1}},"d":{"Struct2":{"v":2}},"e":{"nil":{}}})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_string_enum) {
  std::vector<MyStringEnum> enums{MyStringEnum::kValue1, MyStringEnum::kValue2,
                                  MyStringEnum::kValue3};
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());

  auto expected = R"(["value1","value2","value2"])";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_int_enum) {
  std::vector<MyIntEnum> enums{MyIntEnum::kValue1, MyIntEnum::kValue2,
                               MyIntEnum::kValue3};
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());

  auto expected = R"([1,2,2])";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_mixed_enum) {
  std::vector<MyMixedEnum> enums{MyMixedEnum::kValue1, MyMixedEnum::kValue2};
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());

  auto expected = R"(["value1","value2"])";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_inline_variant) {
  InlineVariant d{.a = {"foobar"},
                  .b = {Struct1{.v = 42}},
                  .c = {std::vector<int>{1, 2, 3}},
                  .d = {TypedInt{.value = 123}},
                  .e = {std::tuple{"blubb", 987, true}}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({"a":"foobar","b":{"v":42},"c":[1,2,3],"d":123,"e":["blubb",987,true]})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_embedded_variant) {
  EmbeddedVariant d{.a = {Struct1{1}},
                    .b = {Struct2{2}},
                    .c = {Struct3{.a = 1, .b = 2}},
                    .d = {true}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  auto expected =
      R"({"a":{"t":"Struct1","v":1},"b":{"t":"Struct2","v":2},"c":{"t":"Struct3","a":1,"b":2},"d":true})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(JsonPrintInspectorMinimalTest, store_embedded_fields) {
  NestedEmbedding const n{
      Embedded{.a = 1, .inner = {.i = 42, .s = "foobar"}, .b = 2}};
  auto result = inspector.apply(n);
  ASSERT_TRUE(result.ok());

  auto expected = R"({"a":1,"i":42,"s":"foobar","b":2})";
  EXPECT_EQ(expected, stream.str());
}

TEST(JsonPrint, stream_output) {
  Dummy const f{.i = 42, .d = 123.456, .b = true, .s = "foobar"};
  std::ostringstream stream;
  {
    stream << inspection::json(f, inspection::JsonPrintFormat::kPretty);
    auto expected = R"({
  "i": 42,
  "d": 123.456,
  "b": true,
  "s": "foobar"
})";
    EXPECT_EQ(expected, stream.str());
  }

  {
    stream.str("");
    stream.clear();
    stream << inspection::json(f);
    auto expected = R"({ "i": 42, "d": 123.456, "b": true, "s": "foobar" })";
    EXPECT_EQ(expected, stream.str());
  }
}

TEST(JsonPrint, format_output) {
  Dummy const f{.i = 42, .d = 123.456, .b = true, .s = "foobar"};

  {
    auto expected = R"(Dummy - {
  "i": 42,
  "d": 123.456,
  "b": true,
  "s": "foobar"
})";
    auto actual = fmt::format("Dummy - {:p}", inspection::json(f));
    EXPECT_EQ(expected, actual);

    actual =
        fmt::format("Dummy - {}",
                    inspection::json(f, inspection::JsonPrintFormat::kPretty));
    EXPECT_EQ(expected, actual);
  }

  {
    auto expected =
        R"(Dummy - { "i": 42, "d": 123.456, "b": true, "s": "foobar" })";
    auto actual = fmt::format("Dummy - {}", inspection::json(f));
    EXPECT_EQ(expected, actual);
    actual = fmt::format("Dummy - {:c}", inspection::json(f));
    EXPECT_EQ(expected, actual);
  }

  {
    auto expected = R"(Dummy - {"i":42,"d":123.456,"b":true,"s":"foobar"})";
    auto actual = fmt::format("Dummy - {:m}", inspection::json(f));
    EXPECT_EQ(expected, actual);

    actual =
        fmt::format("Dummy - {}",
                    inspection::json(f, inspection::JsonPrintFormat::kMinimal));
    EXPECT_EQ(expected, actual);
  }
}

TEST(JsonPrint, format_output_with_unquoted_fields) {
  Dummy const f{.i = 42, .d = 123.456, .b = true, .s = "foobar"};

  {
    auto expected = R"(Dummy - {
  i: 42,
  d: 123.456,
  b: true,
  s: "foobar"
})";
    auto actual = fmt::format("Dummy - {:pu}", inspection::json(f));
    EXPECT_EQ(expected, actual);

    actual = fmt::format(
        "Dummy - {}",
        inspection::json(f, inspection::JsonPrintFormat::kPretty, false));
    EXPECT_EQ(expected, actual);
  }

  {
    auto expected = R"(Dummy - { i: 42, d: 123.456, b: true, s: "foobar" })";
    auto actual = fmt::format(
        "Dummy - {}",
        inspection::json(f, inspection::JsonPrintFormat::kCompact, false));
    EXPECT_EQ(expected, actual);
    actual = fmt::format("Dummy - {:cu}", inspection::json(f));
    EXPECT_EQ(expected, actual);
  }

  {
    auto expected = R"(Dummy - {i:42,d:123.456,b:true,s:"foobar"})";
    auto actual = fmt::format("Dummy - {:mu}", inspection::json(f));
    EXPECT_EQ(expected, actual);

    actual = fmt::format(
        "Dummy - {}",
        inspection::json(f, inspection::JsonPrintFormat::kMinimal, false));
    EXPECT_EQ(expected, actual);
  }
}

}  // namespace
