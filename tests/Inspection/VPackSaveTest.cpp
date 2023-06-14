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

#include <velocypack/Builder.h>

#include "Inspection/VPackSaveInspector.h"
#include "Inspection/InspectionTestHelper.h"

namespace {
using namespace arangodb;
using VPackSaveInspector = inspection::VPackSaveInspector<>;

struct VPackSaveInspectorTest : public ::testing::Test {
  velocypack::Builder builder;
  VPackSaveInspector inspector{builder};
};

TEST_F(VPackSaveInspectorTest, store_empty_object) {
  auto empty = AnEmptyObject{};
  auto result = inspector.apply(empty);
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(builder.slice().isObject());
  EXPECT_EQ(0u, builder.slice().length());
}

TEST_F(VPackSaveInspectorTest, store_int) {
  int x = 42;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(x, builder.slice().getInt());
}

TEST_F(VPackSaveInspectorTest, store_double) {
  double x = 123.456;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(x, builder.slice().getDouble());
}

TEST_F(VPackSaveInspectorTest, store_bool) {
  bool x = true;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(x, builder.slice().getBool());
}

TEST_F(VPackSaveInspectorTest, store_string) {
  std::string x = "foobar";
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(x, builder.slice().copyString());
}

TEST_F(VPackSaveInspectorTest, store_object) {
  static_assert(
      inspection::detail::HasInspectOverload<Dummy, VPackSaveInspector>::value);

  Dummy const f{.i = 42, .d = 123.456, .b = true, .s = "foobar"};
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  EXPECT_EQ(f.i, slice["i"].getInt());
  EXPECT_EQ(f.d, slice["d"].getDouble());
  EXPECT_EQ(f.b, slice["b"].getBool());
  EXPECT_EQ(f.s, slice["s"].copyString());
}

TEST_F(VPackSaveInspectorTest, store_nested_object) {
  static_assert(
      inspection::detail::HasInspectOverload<Nested,
                                             VPackSaveInspector>::value);

  Nested b{.dummy = {.i = 42, .d = 123.456, .b = true, .s = "foobar"}};
  auto result = inspector.apply(b);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  auto d = slice["dummy"];
  ASSERT_TRUE(d.isObject());
  EXPECT_EQ(b.dummy.i, d["i"].getInt());
  EXPECT_EQ(b.dummy.d, d["d"].getDouble());
  EXPECT_EQ(b.dummy.b, d["b"].getBool());
  EXPECT_EQ(b.dummy.s, d["s"].copyString());
}

TEST_F(VPackSaveInspectorTest, store_nested_object_without_nesting) {
  static_assert(
      inspection::detail::HasInspectOverload<Container,
                                             VPackSaveInspector>::value);

  Container c{.i = {.value = 42}};
  auto result = inspector.apply(c);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  EXPECT_EQ(c.i.value, slice["i"].getInt());
}

TEST_F(VPackSaveInspectorTest, store_list) {
  static_assert(
      inspection::detail::HasInspectOverload<List, VPackSaveInspector>::value);

  List l{.vec = {{1}, {2}, {3}}, .list = {4, 5}};
  auto result = inspector.apply(l);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  auto list = slice["vec"];
  ASSERT_TRUE(list.isArray());
  ASSERT_EQ(3u, list.length());
  EXPECT_EQ(l.vec[0].i.value, list[0]["i"].getInt());
  EXPECT_EQ(l.vec[1].i.value, list[1]["i"].getInt());
  EXPECT_EQ(l.vec[2].i.value, list[2]["i"].getInt());

  list = slice["list"];
  ASSERT_TRUE(list.isArray());
  ASSERT_EQ(2u, list.length());
  auto it = l.list.begin();
  EXPECT_EQ(*it++, list[0].getInt());
  EXPECT_EQ(*it++, list[1].getInt());
}

TEST_F(VPackSaveInspectorTest, store_map) {
  static_assert(
      inspection::detail::HasInspectOverload<Map, VPackSaveInspector>::value);

  Map m{.map = {{"1", {1}}, {"2", {2}}, {"3", {3}}},
        .unordered = {{"4", 4}, {"5", 5}}};
  auto result = inspector.apply(m);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  auto obj = slice["map"];
  ASSERT_TRUE(obj.isObject());
  ASSERT_EQ(3u, obj.length());
  EXPECT_EQ(m.map["1"].i.value, obj["1"]["i"].getInt());
  EXPECT_EQ(m.map["2"].i.value, obj["2"]["i"].getInt());
  EXPECT_EQ(m.map["3"].i.value, obj["3"]["i"].getInt());

  obj = slice["unordered"];
  ASSERT_TRUE(obj.isObject());
  ASSERT_EQ(2u, obj.length());
  EXPECT_EQ(m.unordered["4"], obj["4"].getInt());
  EXPECT_EQ(m.unordered["5"], obj["5"].getInt());
}

TEST_F(VPackSaveInspectorTest, store_set) {
  static_assert(
      inspection::detail::HasInspectOverload<Set, VPackSaveInspector>::value);

  Set s{.set = {{1}, {2}, {3}}, .unordered = {4, 5}};
  auto result = inspector.apply(s);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  auto list = slice["set"];
  ASSERT_TRUE(list.isArray());
  ASSERT_EQ(3u, list.length());

  auto s2 = std::set<int>();
  s2.insert(static_cast<int>(list[0]["i"].getInt()));
  s2.insert(static_cast<int>(list[1]["i"].getInt()));
  s2.insert(static_cast<int>(list[2]["i"].getInt()));

  ASSERT_EQ(s2, std::set<int>({1, 2, 3}));

  list = slice["unordered"];
  ASSERT_TRUE(list.isArray());
  ASSERT_EQ(2u, list.length());

  auto us = std::unordered_set<int>();
  us.insert(static_cast<int>(list[0].getInt()));
  us.insert(static_cast<int>(list[1].getInt()));
  ASSERT_EQ(us, s.unordered);
}

TEST_F(VPackSaveInspectorTest, store_tuples) {
  static_assert(
      inspection::detail::HasInspectOverload<Tuple, VPackSaveInspector>::value);

  Tuple t{.tuple = {"foo", 42, 12.34},
          .pair = {987, "bar"},
          .array1 = {"a", "b"},
          .array2 = {1, 2, 3}};
  auto result = inspector.apply(t);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  auto list = slice["tuple"];
  ASSERT_EQ(3u, list.length());
  EXPECT_EQ(std::get<0>(t.tuple), list[0].copyString());
  EXPECT_EQ(std::get<1>(t.tuple), list[1].getInt());
  EXPECT_EQ(std::get<2>(t.tuple), list[2].getDouble());

  list = slice["pair"];
  ASSERT_EQ(2u, list.length());
  EXPECT_EQ(std::get<0>(t.pair), list[0].getInt());
  EXPECT_EQ(std::get<1>(t.pair), list[1].copyString());

  list = slice["array1"];
  ASSERT_EQ(2u, list.length());
  EXPECT_EQ(t.array1[0], list[0].copyString());
  EXPECT_EQ(t.array1[1], list[1].copyString());

  list = slice["array2"];
  ASSERT_EQ(3u, list.length());
  EXPECT_EQ(t.array2[0], list[0].getInt());
  EXPECT_EQ(t.array2[1], list[1].getInt());
  EXPECT_EQ(t.array2[2], list[2].getInt());
}

TEST_F(VPackSaveInspectorTest, store_optional) {
  static_assert(
      inspection::detail::HasInspectOverload<Optional,
                                             VPackSaveInspector>::value);

  Optional o{.a = std::nullopt,
             .b = std::nullopt,
             .x = std::nullopt,
             .y = "blubb",
             .vec = {1, std::nullopt, 3},
             .map = {{"1", 1}, {"2", std::nullopt}, {"3", 3}}};
  auto result = inspector.apply(o);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  EXPECT_EQ(5u, slice.length());
  // a and b have fallbacks, so we need to serialize them explicitly as null
  EXPECT_TRUE(slice["a"].isNull());
  EXPECT_TRUE(slice["b"].isNull());
  EXPECT_EQ("blubb", slice["y"].copyString());

  auto vec = slice["vec"];
  ASSERT_TRUE(vec.isArray());
  ASSERT_EQ(3u, vec.length());
  EXPECT_EQ(1, vec[0].getInt());
  EXPECT_TRUE(vec[1].isNull());
  EXPECT_EQ(3, vec[2].getInt());

  auto map = slice["map"];
  ASSERT_TRUE(map.isObject());
  ASSERT_EQ(3u, map.length());
  EXPECT_EQ(1, map["1"].getInt());
  EXPECT_TRUE(map["2"].isNull());
  EXPECT_EQ(3, map["3"].getInt());
}

TEST_F(VPackSaveInspectorTest, store_optional_pointer) {
  static_assert(
      inspection::detail::HasInspectOverload<Pointer,
                                             VPackSaveInspector>::value);

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

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  EXPECT_EQ(5u, slice.length());
  EXPECT_EQ(42, slice["b"].getInt());
  EXPECT_EQ(43, slice["d"]["i"].getInt());
  auto vec = slice["vec"];
  EXPECT_TRUE(vec.isArray());
  EXPECT_EQ(3u, vec.length());
  EXPECT_EQ(1, vec[0].getInt());
  EXPECT_TRUE(vec[1].isNull());
  EXPECT_EQ(2, vec[2].getInt());
  // x and y have fallbacks, so we need to serialize them explicitly as null
  EXPECT_TRUE(slice["x"].isNull());
  EXPECT_TRUE(slice["y"].isNull());
}

TEST_F(VPackSaveInspectorTest, store_non_default_constructible_type_vec) {
  auto vec = std::vector<NonDefaultConstructibleIntLike>{
      NonDefaultConstructibleIntLike{42}};
  auto result = inspector.apply(vec);
  EXPECT_TRUE(result.ok());
  ASSERT_TRUE(builder.slice().isArray());
  EXPECT_EQ(vec[0].value, builder.slice().at(0).getInt());
}

TEST_F(VPackSaveInspectorTest, store_non_default_constructible_type_map) {
  auto vec = std::map<std::string, NonDefaultConstructibleIntLike>{
      {"foo", NonDefaultConstructibleIntLike{42}}};
  auto result = inspector.apply(vec);
  EXPECT_TRUE(result.ok());
  ASSERT_TRUE(builder.slice().isObject());
  EXPECT_EQ(vec.at("foo").value, builder.slice().get("foo").getInt());
}

TEST_F(VPackSaveInspectorTest, store_object_with_fallbacks) {
  Fallback f;
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());

  static_assert(sizeof(inspector.field("i", f.i)) ==
                sizeof(inspector.field("i", f.i).fallback(42)));
}

TEST_F(VPackSaveInspectorTest, store_object_with_invariant) {
  Invariant i;
  auto result = inspector.apply(i);
  ASSERT_TRUE(result.ok());

  auto invariant = [](auto) { return true; };
  static_assert(sizeof(inspector.field("i", i.i)) ==
                sizeof(inspector.field("i", i.i).invariant(invariant)));
}

TEST_F(VPackSaveInspectorTest, store_object_with_invariant_and_fallback) {
  InvariantAndFallback i;
  auto result = inspector.apply(i);
  ASSERT_TRUE(result.ok());

  auto invariant = [](auto) { return true; };
  static_assert(
      sizeof(inspector.field("i", i.i)) ==
      sizeof(inspector.field("i", i.i).invariant(invariant).fallback(42)));
  static_assert(
      sizeof(inspector.field("i", i.i)) ==
      sizeof(inspector.field("i", i.i).fallback(42).invariant(invariant)));
}

TEST_F(VPackSaveInspectorTest, store_object_with_field_transform) {
  FieldTransform f{.x = 42};
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  EXPECT_EQ("42", slice["x"].copyString());
}

TEST_F(VPackSaveInspectorTest, store_object_with_optional_field_transform) {
  OptionalFieldTransform f{.x = 1, .y = std::nullopt, .z = 3};
  auto result = inspector.apply(f);

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  EXPECT_EQ(2u, slice.length());
  EXPECT_EQ("1", slice["x"].copyString());
  EXPECT_EQ("3", slice["z"].copyString());
}

TEST_F(VPackSaveInspectorTest, store_type_with_custom_specialization) {
  Specialization s{.i = 42, .s = "foobar"};
  auto result = inspector.apply(s);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  EXPECT_EQ(s.i, slice["i"].getInt());
  EXPECT_EQ(s.s, slice["s"].copyString());
}

TEST_F(VPackSaveInspectorTest, store_type_with_explicitly_ignored_fields) {
  ExplicitIgnore e{.s = "foobar"};
  auto result = inspector.apply(e);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  EXPECT_EQ(1u, slice.length());
}

TEST_F(VPackSaveInspectorTest, store_type_with_unsafe_fields) {
  velocypack::Builder localBuilder;
  localBuilder.add(VPackValue("blubb"));
  std::string_view hashedString = "hashedString";
  Unsafe u{.view = "foobar",
           .slice = localBuilder.slice(),
           .hashed = {hashedString.data(),
                      static_cast<uint32_t>(hashedString.size())}};
  auto result = inspector.apply(u);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  EXPECT_EQ("foobar", slice["view"].copyString());
  EXPECT_EQ("blubb", slice["slice"].copyString());
  EXPECT_EQ(hashedString, slice["hashed"].copyString());
}

TEST_F(VPackSaveInspectorTest, store_qualified_variant) {
  QualifiedVariant d{.a = {"foobar"},
                     .b = {42},
                     .c = {Struct1{1}},
                     .d = {Struct2{2}},
                     .e = {std::monostate{}}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  EXPECT_EQ("foobar", slice["a"].stringView());

  EXPECT_EQ("int", slice["b"]["t"].stringView());
  EXPECT_EQ(42, slice["b"]["v"].getInt());

  EXPECT_EQ("Struct1", slice["c"]["t"].stringView());
  EXPECT_EQ(1, slice["c"]["v"]["v"].getInt());

  EXPECT_EQ("Struct2", slice["d"]["t"].stringView());
  EXPECT_EQ(2, slice["d"]["v"]["v"].getInt());

  EXPECT_EQ("nil", slice["e"]["t"].stringView());
  EXPECT_TRUE(slice["e"]["v"].isEmptyObject());
}

TEST_F(VPackSaveInspectorTest, store_unqualified_variant) {
  UnqualifiedVariant d{.a = {"foobar"},
                       .b = {42},
                       .c = {Struct1{1}},
                       .d = {Struct2{2}},
                       .e = {std::monostate{}}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  EXPECT_EQ(1u, slice["a"].length());
  EXPECT_EQ("foobar", slice["a"]["string"].stringView());

  EXPECT_EQ(42, slice["b"].getInt());

  EXPECT_EQ(1u, slice["c"].length());
  EXPECT_EQ(1, slice["c"]["Struct1"]["v"].getInt());

  EXPECT_EQ(1u, slice["d"].length());
  EXPECT_EQ(2, slice["d"]["Struct2"]["v"].getInt());

  EXPECT_EQ(1u, slice["e"].length());
  EXPECT_TRUE(slice["e"]["nil"].isEmptyObject());
}

TEST_F(VPackSaveInspectorTest, store_inline_variant) {
  InlineVariant d{.a = {"foobar"},
                  .b = {Struct1{.v = 42}},
                  .c = {std::vector<int>{1, 2, 3}},
                  .d = {TypedInt{.value = 123}},
                  .e = {std::tuple{"blubb", 987, true}}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  EXPECT_EQ("foobar", slice["a"].stringView());

  EXPECT_TRUE(slice["b"].isObject());
  EXPECT_EQ(1u, slice["b"].length());
  EXPECT_EQ(42, slice["b"]["v"].getInt());

  EXPECT_TRUE(slice["c"].isArray());
  EXPECT_EQ(3u, slice["c"].length());
  EXPECT_EQ(1, slice["c"][0].getInt());
  EXPECT_EQ(2, slice["c"][1].getInt());
  EXPECT_EQ(3, slice["c"][2].getInt());

  EXPECT_EQ(123, slice["d"].getInt());

  EXPECT_TRUE(slice["e"].isArray());
  EXPECT_EQ(3u, slice["e"].length());
  EXPECT_EQ("blubb", slice["e"][0].stringView());
  EXPECT_EQ(987, slice["e"][1].getInt());
  EXPECT_EQ(true, slice["e"][2].getBoolean());
}

TEST_F(VPackSaveInspectorTest, store_string_enum) {
  std::vector<MyStringEnum> enums{MyStringEnum::kValue1, MyStringEnum::kValue2,
                                  MyStringEnum::kValue3};
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(3u, slice.length());
  EXPECT_EQ("value1", slice[0].copyString());
  EXPECT_EQ("value2", slice[1].copyString());
  EXPECT_EQ("value2", slice[2].copyString());
}

TEST_F(VPackSaveInspectorTest, store_int_enum) {
  std::vector<MyIntEnum> enums{MyIntEnum::kValue1, MyIntEnum::kValue2,
                               MyIntEnum::kValue3};
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(3u, slice.length());
  EXPECT_EQ(1, slice[0].getInt());
  EXPECT_EQ(2, slice[1].getInt());
  EXPECT_EQ(2, slice[2].getInt());
}

TEST_F(VPackSaveInspectorTest, store_mixed_enum) {
  std::vector<MyMixedEnum> enums{MyMixedEnum::kValue1, MyMixedEnum::kValue2};
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(2u, slice.length());
  EXPECT_EQ("value1", slice[0].copyString());
  EXPECT_EQ("value2", slice[1].copyString());
}

TEST_F(VPackSaveInspectorTest,
       store_string_enum_returns_error_for_unknown_value) {
  MyStringEnum val = static_cast<MyStringEnum>(42);
  auto result = inspector.apply(val);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Unknown enum value 42", result.error());
}

TEST_F(VPackSaveInspectorTest, store_int_enum_returns_error_for_unknown_value) {
  MyIntEnum val = static_cast<MyIntEnum>(42);
  auto result = inspector.apply(val);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Unknown enum value 42", result.error());
}

TEST_F(VPackSaveInspectorTest,
       store_mixed_enum_returns_error_for_unknown_value) {
  MyMixedEnum val = static_cast<MyMixedEnum>(42);
  auto result = inspector.apply(val);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Unknown enum value 42", result.error());
}

TEST_F(VPackSaveInspectorTest, store_embedded_variant) {
  EmbeddedVariant d{.a = {Struct1{1}},
                    .b = {Struct2{2}},
                    .c = {Struct3{.a = 1, .b = 2}},
                    .d = {true}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());

  EXPECT_EQ("Struct1", slice["a"]["t"].stringView());
  EXPECT_EQ(1, slice["a"]["v"].getInt());

  EXPECT_EQ("Struct2", slice["b"]["t"].stringView());
  EXPECT_EQ(2, slice["b"]["v"].getInt());

  EXPECT_EQ("Struct3", slice["c"]["t"].stringView());
  EXPECT_EQ(1, slice["c"]["a"].getInt());
  EXPECT_EQ(2, slice["c"]["b"].getInt());

  EXPECT_EQ(true, slice["d"].getBoolean());
}

TEST_F(VPackSaveInspectorTest, store_embedded_fields) {
  NestedEmbedding const n{
      Embedded{.a = 1, .inner = {.i = 42, .s = "foobar"}, .b = 2}};
  auto result = inspector.apply(n);
  ASSERT_TRUE(result.ok());

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  EXPECT_EQ(n.a, slice["a"].getInt());
  EXPECT_EQ(n.inner.i, slice["i"].getInt());
  EXPECT_EQ(n.inner.s, slice["s"].copyString());
  EXPECT_EQ(n.b, slice["b"].getInt());
}

TEST(VPackSaveInspectorContext, serialize_with_context) {
  struct Context {
    int defaultInt;
    int minInt;
    std::string defaultString;
  };

  Context ctxt{};
  velocypack::Builder builder;
  inspection::VPackSaveInspector<Context> inspector(builder, ctxt);

  WithContext data{.i = 42, .s = "foobar"};
  auto res = inspector.apply(data);
  ASSERT_TRUE(res.ok());
  EXPECT_EQ(42, builder.slice()["i"].getInt());
  EXPECT_EQ("foobar", builder.slice()["s"].copyString());
}

}  // namespace
