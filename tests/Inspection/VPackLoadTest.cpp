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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include <velocypack/Builder.h>

#include "Inspection/VPack.h"
#include "Inspection/VPackLoadInspector.h"
#include "Inspection/InspectionTestHelper.h"

namespace {
using namespace arangodb;
using VPackLoadInspector = inspection::VPackLoadInspector<>;

struct VPackLoadInspectorTest : public ::testing::Test {
  velocypack::Builder builder;
};

TEST_F(VPackLoadInspectorTest, load_empty_object) {
  builder.openObject();
  builder.close();
  VPackLoadInspector inspector{builder};

  auto d = AnEmptyObject{};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());
}

TEST_F(VPackLoadInspectorTest, load_int) {
  builder.add(VPackValue(42));
  VPackLoadInspector inspector{builder};

  int x = 0;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(42, x);
}

TEST_F(VPackLoadInspectorTest, load_double) {
  builder.add(VPackValue(123.456));
  VPackLoadInspector inspector{builder};

  double x = 0;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(123.456, x);
}

TEST_F(VPackLoadInspectorTest, load_bool) {
  builder.add(VPackValue(true));
  VPackLoadInspector inspector{builder};

  bool x = false;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(true, x);
}

TEST_F(VPackLoadInspectorTest, load_string) {
  builder.add(VPackValue("foobar"));
  VPackLoadInspector inspector{builder};

  std::string x;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("foobar", x);
}

TEST_F(VPackLoadInspectorTest, load_object) {
  builder.openObject();
  builder.add("i", VPackValue(42));
  builder.add("d", VPackValue(123.456));
  builder.add("b", VPackValue(true));
  builder.add("s", VPackValue("foobar"));
  builder.close();
  VPackLoadInspector inspector{builder};

  Dummy d;
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, d.i);
  EXPECT_EQ(123.456, d.d);
  EXPECT_EQ(true, d.b);
  EXPECT_EQ("foobar", d.s);
}

TEST_F(VPackLoadInspectorTest, load_nested_object) {
  builder.openObject();
  builder.add(VPackValue("dummy"));
  builder.openObject();
  builder.add("i", VPackValue(42));
  builder.add("d", VPackValue(123));
  builder.add("b", VPackValue(true));
  builder.add("s", VPackValue("foobar"));
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  Nested n;
  auto result = inspector.apply(n);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, n.dummy.i);
  EXPECT_EQ(123, n.dummy.d);
  EXPECT_EQ(true, n.dummy.b);
  EXPECT_EQ("foobar", n.dummy.s);
}

TEST_F(VPackLoadInspectorTest, load_nested_object_without_nesting) {
  builder.openObject();
  builder.add("i", VPackValue(42));
  builder.close();
  VPackLoadInspector inspector{builder};

  Container c;
  auto result = inspector.apply(c);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, c.i.value);
}

TEST_F(VPackLoadInspectorTest, load_list) {
  builder.openObject();
  builder.add(VPackValue("vec"));
  builder.openArray();
  builder.openObject();
  builder.add("i", VPackValue(1));
  builder.close();
  builder.openObject();
  builder.add("i", VPackValue(2));
  builder.close();
  builder.openObject();
  builder.add("i", VPackValue(3));
  builder.close();
  builder.close();
  builder.add(VPackValue("list"));
  builder.openArray();
  builder.add(VPackValue(4));
  builder.add(VPackValue(5));
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  List l;
  auto result = inspector.apply(l);
  ASSERT_TRUE(result.ok());

  EXPECT_EQ(3u, l.vec.size());
  EXPECT_EQ(1, l.vec[0].i.value);
  EXPECT_EQ(2, l.vec[1].i.value);
  EXPECT_EQ(3, l.vec[2].i.value);
  EXPECT_EQ((std::list<int>{4, 5}), l.list);
}

TEST_F(VPackLoadInspectorTest, load_map) {
  builder.openObject();
  builder.add(VPackValue("map"));
  builder.openObject();
  builder.add(VPackValue("1"));
  builder.openObject();
  builder.add("i", VPackValue(1));
  builder.close();
  builder.add(VPackValue("2"));
  builder.openObject();
  builder.add("i", VPackValue(2));
  builder.close();
  builder.add(VPackValue("3"));
  builder.openObject();
  builder.add("i", VPackValue(3));
  builder.close();
  builder.close();
  builder.add(VPackValue("unordered"));
  builder.openObject();
  builder.add("4", VPackValue(4));
  builder.add("5", VPackValue(5));
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  Map m;
  auto result = inspector.apply(m);
  ASSERT_TRUE(result.ok());

  EXPECT_EQ(
      (std::map<std::string, Container>{{"1", {1}}, {"2", {2}}, {"3", {3}}}),
      m.map);
  EXPECT_EQ((std::unordered_map<std::string, int>{{"4", 4}, {"5", 5}}),
            m.unordered);
}

TEST_F(VPackLoadInspectorTest, load_transformed_map) {
  builder.openObject();
  builder.add(VPackValue("map"));
  builder.openArray();
  builder.openObject();
  builder.add("key", 1);
  builder.add(VPackValue("value"));
  builder.openObject();
  builder.add("i", VPackValue(1));
  builder.close();
  builder.close();
  builder.openObject();
  builder.add("key", 2);
  builder.add(VPackValue("value"));
  builder.openObject();
  builder.add("i", VPackValue(2));
  builder.close();
  builder.close();
  builder.openObject();
  builder.add("key", 3);
  builder.add(VPackValue("value"));
  builder.openObject();
  builder.add("i", VPackValue(3));
  builder.close();
  builder.close();
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  TransformedMap m;
  auto result = inspector.apply(m);
  ASSERT_TRUE(result.ok());

  EXPECT_EQ((std::map<int, Container>{{1, {1}}, {2, {2}}, {3, {3}}}), m.map);
}

TEST_F(VPackLoadInspectorTest, load_set) {
  builder.openObject();
  builder.add(VPackValue("set"));
  builder.openArray();
  builder.openObject();
  builder.add("i", VPackValue(1));
  builder.close();
  builder.openObject();
  builder.add("i", VPackValue(2));
  builder.close();
  builder.openObject();
  builder.add("i", VPackValue(3));
  builder.close();
  builder.close();
  builder.add(VPackValue("unordered"));
  builder.openArray();
  builder.add(VPackValue(4));
  builder.add(VPackValue(5));
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  Set s;
  auto result = inspector.apply(s);
  ASSERT_TRUE(result.ok()) << result.error();

  EXPECT_EQ((std::set<Container>{{1}, {2}, {3}}), s.set);
  EXPECT_EQ((std::unordered_set<int>{4, 5}), s.unordered);
}

TEST_F(VPackLoadInspectorTest, load_tuples) {
  builder.openObject();

  builder.add(VPackValue("tuple"));
  builder.openArray();
  builder.add(VPackValue("foo"));
  builder.add(VPackValue(42));
  builder.add(VPackValue(12.34));
  builder.close();

  builder.add(VPackValue("pair"));
  builder.openArray();
  builder.add(VPackValue(987));
  builder.add(VPackValue("bar"));
  builder.close();

  builder.add(VPackValue("array1"));
  builder.openArray();
  builder.add(VPackValue("a"));
  builder.add(VPackValue("b"));
  builder.close();

  builder.add(VPackValue("array2"));
  builder.openArray();
  builder.add(VPackValue(1));
  builder.add(VPackValue(2));
  builder.add(VPackValue(3));
  builder.close();

  builder.close();
  VPackLoadInspector inspector{builder};

  Tuple t;
  auto result = inspector.apply(t);
  ASSERT_TRUE(result.ok());

  Tuple expected{.tuple = {"foo", 42, 12.34},
                 .pair = {987, "bar"},
                 .array1 = {"a", "b"},
                 .array2 = {1, 2, 3}};
  EXPECT_EQ(expected.tuple, t.tuple);
  EXPECT_EQ(expected.pair, t.pair);
  EXPECT_EQ(expected.array1[0], t.array1[0]);
  EXPECT_EQ(expected.array1[1], t.array1[1]);
  EXPECT_EQ(expected.array2, t.array2);
}

TEST_F(VPackLoadInspectorTest, load_slice) {
  {
    builder.openObject();
    builder.add(VPackValue("dummy"));
    builder.openObject();
    builder.add("i", VPackValue(42));
    builder.add("b", VPackValue(true));
    builder.add("s", VPackValue("foobar"));
    builder.close();
    builder.close();
    VPackLoadInspector inspector{builder};

    velocypack::SharedSlice slice;
    auto result = inspector.apply(slice);
    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(slice.isObject());
    slice = slice["dummy"];
    ASSERT_TRUE(slice.isObject());
    EXPECT_EQ(42, slice["i"].getInt());
    EXPECT_EQ(true, slice["b"].getBoolean());
    EXPECT_EQ("foobar", slice["s"].stringView());
  }

  {
    builder.clear();
    builder.add(VPackValue("foobar"));
    VPackLoadInspector inspector{builder};

    velocypack::SharedSlice slice;
    auto result = inspector.apply(slice);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ("foobar", slice.stringView());
  }

  {
    builder.clear();
    builder.add(VPackValue("foobar"));
    inspection::VPackUnsafeLoadInspector<> inspector{builder};

    velocypack::Slice slice;
    auto result = inspector.apply(slice);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ("foobar", slice.stringView());
  }
}

TEST_F(VPackLoadInspectorTest, load_builder) {
  {
    builder.openObject();
    builder.add(VPackValue("dummy"));
    builder.openObject();
    builder.add("i", VPackValue(42));
    builder.add("b", VPackValue(true));
    builder.add("s", VPackValue("foobar"));
    builder.close();
    builder.close();
    VPackLoadInspector inspector{builder};

    velocypack::Builder builder;
    auto result = inspector.apply(builder);
    ASSERT_TRUE(result.ok());
    auto slice = builder.slice();
    ASSERT_TRUE(slice.isObject());
    slice = slice["dummy"];
    ASSERT_TRUE(slice.isObject());
    EXPECT_EQ(42, slice["i"].getInt());
    EXPECT_EQ(true, slice["b"].getBoolean());
    EXPECT_EQ("foobar", slice["s"].stringView());
  }

  {
    builder.clear();
    builder.add(VPackValue("foobar"));
    inspection::VPackUnsafeLoadInspector<> inspector{builder};

    velocypack::Builder builder;
    auto result = inspector.apply(builder);
    auto slice = builder.slice();
    ASSERT_TRUE(result.ok());
    EXPECT_EQ("foobar", slice.stringView());
  }
}

TEST_F(VPackLoadInspectorTest, load_optional) {
  builder.openObject();
  builder.add("y", VPackValue("blubb"));

  builder.add(VPackValue("vec"));
  builder.openArray();
  builder.add(VPackValue(1));
  builder.add(VPackValue(velocypack::ValueType::Null));
  builder.add(VPackValue(3));
  builder.close();

  builder.add(VPackValue("map"));
  builder.openObject();
  builder.add("1", VPackValue(1));
  builder.add("2", VPackValue(velocypack::ValueType::Null));
  builder.add("3", VPackValue(3));
  builder.close();

  builder.add("a", VPackValue(velocypack::ValueType::Null));
  builder.close();
  VPackLoadInspector inspector{builder};

  Optional o{.a = 1, .b = 2, .x = 42, .y = {}, .vec = {}, .map = {}};
  auto result = inspector.apply(o);
  ASSERT_TRUE(result.ok());

  Optional expected{.a = std::nullopt,
                    .b = 456,
                    .x = std::nullopt,
                    .y = "blubb",
                    .vec = {1, std::nullopt, 3},
                    .map = {{"1", 1}, {"2", std::nullopt}, {"3", 3}}};
  EXPECT_EQ(expected.a, o.a);
  EXPECT_EQ(expected.b, o.b);
  EXPECT_EQ(expected.x, o.x);
  EXPECT_EQ(expected.y, o.y);
  ASSERT_EQ(expected.vec, o.vec);
  EXPECT_EQ(expected.map, o.map);
}

TEST_F(VPackLoadInspectorTest, load_non_default_constructible_type_vec) {
  builder.openArray();
  builder.add(VPackValue(42));
  builder.close();

  VPackLoadInspector inspector{builder};

  auto vec = std::vector<NonDefaultConstructibleIntLike>();
  auto result = inspector.apply(vec);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(vec, decltype(vec){NonDefaultConstructibleIntLike{42}});
}

TEST_F(VPackLoadInspectorTest, load_non_default_constructible_type_map) {
  builder.openObject();
  builder.add("foo", VPackValue(42));
  builder.close();

  VPackLoadInspector inspector{builder};

  auto map = std::map<std::string, NonDefaultConstructibleIntLike>();
  auto result = inspector.apply(map);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(map, (decltype(map){{"foo", NonDefaultConstructibleIntLike{42}}}));
}

TEST_F(VPackLoadInspectorTest, load_non_default_constructible_type_optional) {
  builder.add(VPackValue(42));

  VPackLoadInspector inspector{builder};

  auto x = std::optional<NonDefaultConstructibleIntLike>();
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(x, (decltype(x){NonDefaultConstructibleIntLike{42}}));
}

TEST_F(VPackLoadInspectorTest, load_non_default_constructible_type_unique_ptr) {
  builder.add(VPackValue(42));

  VPackLoadInspector inspector{builder};

  auto x = std::unique_ptr<NonDefaultConstructibleIntLike>();
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*x, NonDefaultConstructibleIntLike{42});
}

TEST_F(VPackLoadInspectorTest, load_non_default_constructible_type_shared_ptr) {
  builder.add(VPackValue(42));

  VPackLoadInspector inspector{builder};

  auto x = std::shared_ptr<NonDefaultConstructibleIntLike>();
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*x, NonDefaultConstructibleIntLike{42});
}

TEST_F(VPackLoadInspectorTest,
       load_non_default_constructible_type_inline_variant) {
  builder.add(VPackValue(42));

  VPackLoadInspector inspector{builder};

  auto x = InlineVariantWithNonDefaultConstructible{};
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(x, (decltype(x){NonDefaultConstructibleIntLike{42}}));
}

TEST_F(VPackLoadInspectorTest,
       load_non_default_constructible_type_qualified_variant) {
  builder.openObject();
  builder.add("t", "nondc_type");
  builder.add("v", VPackValue(42));
  builder.close();

  VPackLoadInspector inspector{builder};

  auto x = QualifiedVariantWithNonDefaultConstructible{};
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(x, (decltype(x){NonDefaultConstructibleIntLike{42}}));
}

TEST_F(VPackLoadInspectorTest, load_optional_pointer) {
  builder.openObject();
  builder.add(VPackValue("vec"));
  builder.openArray();
  builder.add(VPackValue(1));
  builder.add(VPackValue(VPackValueType::Null));
  builder.add(VPackValue(2));
  builder.close();

  builder.add("a", VPackValue(VPackValueType::Null));

  builder.add("b", VPackValue(42));

  builder.add(VPackValue("d"));
  builder.openObject();
  builder.add("i", VPackValue(43));
  builder.close();

  builder.add("x", VPackValue(VPackValueType::Null));

  builder.close();
  VPackLoadInspector inspector{builder};

  Pointer p{.a = std::make_shared<int>(0),
            .b = std::make_shared<int>(0),
            .c = std::make_unique<int>(0),
            .d = std::make_unique<Container>(Container{.i = {.value = 0}}),
            .vec = {},
            .x = std::make_shared<int>(0),
            .y = std::make_shared<int>(0)};
  auto result = inspector.apply(p);
  ASSERT_TRUE(result.ok()) << result.error() << "; " << result.path();

  EXPECT_EQ(nullptr, p.a);
  ASSERT_NE(nullptr, p.b);
  EXPECT_EQ(42, *p.b);
  EXPECT_EQ(nullptr, p.c);
  ASSERT_NE(nullptr, p.d);
  EXPECT_EQ(43, p.d->i.value);

  ASSERT_EQ(3u, p.vec.size());
  ASSERT_NE(nullptr, p.vec[0]);
  EXPECT_EQ(1, *p.vec[0]);
  EXPECT_EQ(nullptr, p.vec[1]);
  ASSERT_NE(nullptr, p.vec[2]);
  EXPECT_EQ(2, *p.vec[2]);

  EXPECT_EQ(nullptr, p.x);
  ASSERT_NE(nullptr, p.y);
  EXPECT_EQ(456, *p.y);
}

TEST_F(VPackLoadInspectorTest, error_expecting_int) {
  builder.add(VPackValue("foo"));
  VPackLoadInspector inspector{builder};

  int i{};
  auto result = inspector.apply(i);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Int", result.error());
}

TEST_F(VPackLoadInspectorTest, error_expecting_int16) {
  builder.add(VPackValue(123456789));
  VPackLoadInspector inspector{builder};

  std::int16_t i{};
  auto result = inspector.apply(i);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Number out of range", result.error());
}

TEST_F(VPackLoadInspectorTest, error_expecting_double) {
  builder.add(VPackValue("foo"));
  VPackLoadInspector inspector{builder};

  double d{};
  auto result = inspector.apply(d);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting numeric type", result.error());
}

TEST_F(VPackLoadInspectorTest, error_expecting_bool) {
  builder.add(VPackValue(42));
  VPackLoadInspector inspector{builder};

  bool b{};
  auto result = inspector.apply(b);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Bool", result.error());
}

TEST_F(VPackLoadInspectorTest, error_expecting_string) {
  builder.add(VPackValue(42));
  VPackLoadInspector inspector{builder};

  std::string s;
  auto result = inspector.apply(s);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type String", result.error());
}

TEST_F(VPackLoadInspectorTest, error_expecting_array) {
  builder.add(VPackValue(42));
  VPackLoadInspector inspector{builder};

  std::vector<int> v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Array", result.error());
}

TEST_F(VPackLoadInspectorTest, error_expecting_object) {
  builder.add(VPackValue(42));
  VPackLoadInspector inspector{builder};

  Dummy d;
  auto result = inspector.apply(d);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Object", result.error());
}

TEST_F(VPackLoadInspectorTest, error_tuple_array_too_short) {
  builder.openArray();
  builder.add(VPackValue("foo"));
  builder.add(VPackValue(42));
  builder.close();
  VPackLoadInspector inspector{builder};

  std::tuple<std::string, int, double> t;
  auto result = inspector.apply(t);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expected array of length 3", result.error());
}

TEST_F(VPackLoadInspectorTest, error_tuple_array_too_large) {
  builder.openArray();
  builder.add(VPackValue("foo"));
  builder.add(VPackValue(42));
  builder.add(VPackValue(123.456));
  builder.close();
  VPackLoadInspector inspector{builder};

  std::tuple<std::string, int> t;
  auto result = inspector.apply(t);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expected array of length 2", result.error());
}

TEST_F(VPackLoadInspectorTest, error_c_style_array_too_short) {
  builder.openArray();
  builder.add(VPackValue(1));
  builder.add(VPackValue(2));
  builder.close();
  VPackLoadInspector inspector{builder};

  int a[4];
  auto result = inspector.apply(a);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expected array of length 4", result.error());
}

TEST_F(VPackLoadInspectorTest, error_c_style_array_too_long) {
  builder.openArray();
  builder.add(VPackValue(1));
  builder.add(VPackValue(2));
  builder.add(VPackValue(3));
  builder.add(VPackValue(4));
  builder.close();
  VPackLoadInspector inspector{builder};

  int a[3];
  auto result = inspector.apply(a);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expected array of length 3", result.error());
}

TEST_F(VPackLoadInspectorTest, error_expecting_type_on_path) {
  builder.openObject();
  builder.add(VPackValue("dummy"));
  builder.openObject();
  builder.add("i", VPackValue("foo"));
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  Nested n;
  auto result = inspector.apply(n);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("dummy.i", result.path());
}

TEST_F(VPackLoadInspectorTest, error_expecting_type_on_path_with_array) {
  builder.openObject();
  builder.add(VPackValue("vec"));
  builder.openArray();
  builder.openObject();
  builder.add("i", VPackValue(1));
  builder.close();
  builder.openObject();
  builder.add("i", VPackValue(2));
  builder.close();
  builder.openObject();
  builder.add("i", VPackValue("foobar"));
  builder.close();
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  List l;
  auto result = inspector.apply(l);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ("vec[2].i", result.path());
}

TEST_F(VPackLoadInspectorTest, error_expecting_type_on_path_with_map) {
  builder.openObject();
  builder.add(VPackValue("map"));
  builder.openObject();
  builder.add(VPackValue("1"));
  builder.openObject();
  builder.add("i", VPackValue(1));
  builder.close();
  builder.add(VPackValue("2"));
  builder.openObject();
  builder.add("i", VPackValue(2));
  builder.close();
  builder.add(VPackValue("3"));
  builder.openObject();
  builder.add("i", VPackValue("foobar"));
  builder.close();
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  Map m;
  auto result = inspector.apply(m);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ("map['3'].i", result.path());
}

TEST_F(VPackLoadInspectorTest, error_expecting_type_on_path_with_tuple) {
  builder.openObject();

  builder.add(VPackValue("tuple"));
  builder.openArray();
  builder.add(VPackValue("foo"));
  builder.add(VPackValue(42));
  builder.add(VPackValue("bar"));
  builder.close();

  builder.close();
  VPackLoadInspector inspector{builder};

  Tuple l;
  auto result = inspector.apply(l);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ("tuple[2]", result.path());
}

TEST_F(VPackLoadInspectorTest,
       error_expecting_type_on_path_with_c_style_array) {
  builder.openObject();

  builder.add(VPackValue("tuple"));
  builder.openArray();
  builder.add(VPackValue("foo"));
  builder.add(VPackValue(42));
  builder.add(VPackValue(0));
  builder.close();

  builder.add(VPackValue("pair"));
  builder.openArray();
  builder.add(VPackValue(987));
  builder.add(VPackValue("bar"));
  builder.close();

  builder.add(VPackValue("array1"));
  builder.openArray();
  builder.add(VPackValue("a"));
  builder.add(VPackValue(42));
  builder.close();

  builder.close();
  VPackLoadInspector inspector{builder};

  Tuple l;
  auto result = inspector.apply(l);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ("array1[1]", result.path());
}

TEST_F(VPackLoadInspectorTest, error_expecting_type_on_path_with_std_array) {
  builder.openObject();

  builder.add(VPackValue("tuple"));
  builder.openArray();
  builder.add(VPackValue("foo"));
  builder.add(VPackValue(42));
  builder.add(VPackValue(0));
  builder.close();

  builder.add(VPackValue("pair"));
  builder.openArray();
  builder.add(VPackValue(987));
  builder.add(VPackValue("bar"));
  builder.close();

  builder.add(VPackValue("array1"));
  builder.openArray();
  builder.add(VPackValue("a"));
  builder.add(VPackValue("b"));
  builder.close();

  builder.add(VPackValue("array2"));
  builder.openArray();
  builder.add(VPackValue(1));
  builder.add(VPackValue(2));
  builder.add(VPackValue("foo"));
  builder.close();

  builder.close();
  VPackLoadInspector inspector{builder};

  Tuple l;
  auto result = inspector.apply(l);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ("array2[2]", result.path());
}

TEST_F(VPackLoadInspectorTest, error_missing_field) {
  builder.openObject();
  builder.add(VPackValue("dummy"));
  builder.openObject();
  builder.add("s", VPackValue("foo"));
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  Nested n;
  auto result = inspector.apply(n);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Missing required attribute 'i'", result.error());
  EXPECT_EQ("dummy.i", result.path());
}

TEST_F(VPackLoadInspectorTest, error_found_unexpected_attribute) {
  builder.openObject();
  builder.add("i", VPackValue(42));
  builder.add("should_not_be_here", VPackValue(123));
  builder.close();
  VPackLoadInspector inspector{builder};

  Container c;
  auto result = inspector.apply(c);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Found unexpected attribute 'should_not_be_here'", result.error());
}

TEST_F(VPackLoadInspectorTest, load_object_ignoring_unknown_attributes) {
  builder.openObject();
  builder.add("i", VPackValue(42));
  builder.add("ignore_me", VPackValue(123));
  builder.close();
  VPackLoadInspector inspector{builder, {.ignoreUnknownFields = true}};

  Container c;
  auto result = inspector.apply(c);
  ASSERT_TRUE(result.ok()) << "Error: " << result.error()
                           << "\nPath: " << result.path();
}

TEST_F(VPackLoadInspectorTest, load_object_with_fallbacks) {
  builder.openObject();
  builder.close();
  VPackLoadInspector inspector{builder};

  Fallback f;
  Dummy expected = f.d;
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, f.i);
  EXPECT_EQ("foobar", f.s);
  EXPECT_EQ(expected, f.d);
  EXPECT_EQ(84, f.dynamic);  // f.i * 2
}

TEST_F(VPackLoadInspectorTest, load_object_with_fallback_reference) {
  builder.openObject();
  builder.add("x", VPackValue(42));
  builder.close();
  VPackLoadInspector inspector{builder};

  FallbackReference f;
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, f.x);
  EXPECT_EQ(42, f.y);
}

TEST_F(VPackLoadInspectorTest, load_object_ignoring_missing_fields) {
  builder.openObject();
  builder.close();
  VPackLoadInspector inspector{builder, {.ignoreMissingFields = true}};

  FallbackReference f{.x = 1, .y = 2};
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(1, f.x);
  EXPECT_EQ(1, f.y);
}

TEST_F(VPackLoadInspectorTest, load_object_with_invariant_fulfilled) {
  builder.openObject();
  builder.add("i", VPackValue(42));
  builder.add("s", VPackValue("foobar"));
  builder.close();
  VPackLoadInspector inspector{builder};

  Invariant i;
  auto result = inspector.apply(i);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, i.i);
  EXPECT_EQ("foobar", i.s);
}

TEST_F(VPackLoadInspectorTest, load_object_with_invariant_not_fulfilled) {
  {
    builder.openObject();
    builder.add("i", VPackValue(0));
    builder.add("s", VPackValue("foobar"));
    builder.close();
    VPackLoadInspector inspector{builder};

    Invariant i;
    auto result = inspector.apply(i);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Field invariant failed", result.error());
    EXPECT_EQ("i", result.path());
  }

  {
    builder.clear();
    builder.openObject();
    builder.add("i", VPackValue(42));
    builder.add("s", VPackValue(""));
    builder.close();
    VPackLoadInspector inspector{builder};

    Invariant i;
    auto result = inspector.apply(i);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Field invariant failed", result.error());
    EXPECT_EQ("s", result.path());
  }
}

TEST_F(VPackLoadInspectorTest,
       load_object_with_invariant_Result_not_fulfilled) {
  {
    builder.openObject();
    builder.add("i", VPackValue(0));
    builder.close();
    VPackLoadInspector inspector{builder};

    InvariantWithResult i;
    auto result = inspector.apply(i);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Must not be zero", result.error());
    EXPECT_EQ("i", result.path());
  }

  {
    builder.clear();
    builder.openObject();
    builder.add("i", VPackValue(42));
    builder.add("s", VPackValue(""));
    builder.close();
    VPackLoadInspector inspector{builder};

    Invariant i;
    auto result = inspector.apply(i);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Field invariant failed", result.error());
    EXPECT_EQ("s", result.path());
  }
}

TEST_F(VPackLoadInspectorTest, load_object_with_invariant_and_fallback) {
  builder.openObject();
  builder.close();
  VPackLoadInspector inspector{builder};

  InvariantAndFallback i;
  auto result = inspector.apply(i);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, i.i);
  EXPECT_EQ("foobar", i.s);
}

TEST_F(VPackLoadInspectorTest, load_object_with_object_invariant) {
  builder.openObject();
  builder.add("i", VPackValue(42));
  builder.add("s", VPackValue(""));
  builder.close();
  VPackLoadInspector inspector{builder};

  ObjectInvariant o;
  auto result = inspector.apply(o);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Object invariant failed", result.error());
}

TEST_F(VPackLoadInspectorTest, load_object_with_field_transform) {
  builder.openObject();
  builder.add("x", VPackValue("42"));
  builder.close();
  VPackLoadInspector inspector{builder};

  FieldTransform f;
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, f.x);
}

TEST_F(VPackLoadInspectorTest, load_object_with_field_transform_and_fallback) {
  builder.openObject();
  builder.add("x", VPackValue("42"));
  builder.close();
  VPackLoadInspector inspector{builder};

  FieldTransformWithFallback f;
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, f.x);
  EXPECT_EQ(2, f.y);
}

TEST_F(VPackLoadInspectorTest, load_object_with_optional_field_transform) {
  builder.openObject();
  builder.add("x", VPackValue("42"));
  builder.close();
  VPackLoadInspector inspector{builder};

  OptionalFieldTransform f{.x = 1, .y = 2, .z = 3};
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, *f.x);
  EXPECT_FALSE(f.y.has_value());
  EXPECT_EQ(123, *f.z);
}

TEST_F(VPackLoadInspectorTest, load_type_with_custom_specialization) {
  builder.openObject();
  builder.add("i", VPackValue(42));
  builder.add("s", VPackValue("foobar"));
  builder.close();
  VPackLoadInspector inspector{builder};

  Specialization s;
  auto result = inspector.apply(s);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, s.i);
  EXPECT_EQ("foobar", s.s);
}

TEST_F(VPackLoadInspectorTest, load_type_with_explicitly_ignored_fields) {
  builder.openObject();
  builder.add("s", VPackValue("foobar"));
  builder.add("ignore", VPackValue("something"));
  builder.close();
  VPackLoadInspector inspector{builder};

  ExplicitIgnore e;
  auto result = inspector.apply(e);
  ASSERT_TRUE(result.ok());
}

TEST_F(VPackLoadInspectorTest, load_qualified_variant) {
  builder.openObject();
  builder.add("a", VPackValue("foobar"));

  builder.add(VPackValue("b"));
  builder.openObject();
  builder.add("t", "int");
  builder.add("v", 42);
  builder.close();

  builder.add(VPackValue("c"));
  builder.openObject();
  builder.add("t", "Struct1");
  builder.add(VPackValue("v"));
  builder.openObject();
  builder.add("v", 1);
  builder.close();
  builder.close();

  builder.add(VPackValue("d"));
  builder.openObject();
  builder.add("t", "Struct2");
  builder.add(VPackValue("v"));
  builder.openObject();
  builder.add("v", 2);
  builder.close();
  builder.close();

  builder.add(VPackValue("e"));
  builder.openObject();
  builder.add("t", "nil");
  builder.add(VPackValue("v"));
  builder.openObject();
  builder.close();
  builder.close();

  builder.close();
  VPackLoadInspector inspector{builder};

  QualifiedVariant v{.a = {std::monostate{}},
                     .b = {std::monostate{}},
                     .c = {std::monostate{}},
                     .d = {std::monostate{}},
                     .e = {0}};
  auto result = inspector.apply(v);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ("foobar", std::get<std::string>(v.a));
  EXPECT_EQ(42, std::get<int>(v.b));
  EXPECT_EQ(1, std::get<Struct1>(v.c).v);
  EXPECT_EQ(2, std::get<Struct2>(v.d).v);
  EXPECT_TRUE(std::holds_alternative<std::monostate>(v.e));
}

TEST_F(VPackLoadInspectorTest,
       error_unknown_type_tag_when_loading_qualified_variant) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.add("t", "blubb");
  builder.add("v", "");
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  QualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Found invalid type: blubb", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(VPackLoadInspectorTest,
       error_expecting_string_when_parsing_qualified_variant_value) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.add("t", "int");
  builder.add("v", "blubb");
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  QualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Int", result.error());
  EXPECT_EQ("a.v", result.path());
}

TEST_F(VPackLoadInspectorTest,
       error_missing_tag_when_parsing_qualified_variant) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.add("v", 42);
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  QualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Variant type field \"t\" is missing", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(VPackLoadInspectorTest,
       error_invalid_tag_type_when_parsing_qualified_variant) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.add("t", 42);
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  QualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Variant type field \"t\" must be a string", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(VPackLoadInspectorTest,
       error_missing_value_when_parsing_qualified_variant) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.add("t", "int");
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  QualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Variant value field \"v\" is missing", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(VPackLoadInspectorTest, load_unqualified_variant) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.add("string", "foobar");
  builder.close();

  builder.add("b", VPackValue(42));

  builder.add(VPackValue("c"));
  builder.openObject();
  builder.add(VPackValue("Struct1"));
  builder.openObject();
  builder.add("v", 1);
  builder.close();
  builder.close();

  builder.add(VPackValue("d"));
  builder.openObject();
  builder.add(VPackValue("Struct2"));
  builder.openObject();
  builder.add("v", 2);
  builder.close();
  builder.close();

  builder.add(VPackValue("e"));
  builder.openObject();
  builder.add(VPackValue("nil"));
  builder.openObject();
  builder.close();
  builder.close();

  builder.close();
  VPackLoadInspector inspector{builder};

  UnqualifiedVariant v{.a = {std::monostate{}},
                       .b = {std::monostate{}},
                       .c = {std::monostate{}},
                       .d = {std::monostate{}},
                       .e = {0}};
  auto result = inspector.apply(v);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ("foobar", std::get<std::string>(v.a));
  EXPECT_EQ(42, std::get<int>(v.b));
  EXPECT_EQ(1, std::get<Struct1>(v.c).v);
  EXPECT_EQ(2, std::get<Struct2>(v.d).v);
  EXPECT_TRUE(std::holds_alternative<std::monostate>(v.e));
}

TEST_F(VPackLoadInspectorTest,
       error_unknown_type_tag_when_loading_unqualified_variant) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.add("blubb", "");
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  UnqualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Found invalid type: blubb", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(VPackLoadInspectorTest,
       error_expecting_string_when_parsing_unqualified_variant_value) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.add("string", 42);
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  UnqualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type String", result.error());
  EXPECT_EQ("a.string", result.path());
}

TEST_F(VPackLoadInspectorTest,
       error_missing_data_when_parsing_unqualified_variant) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  UnqualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Missing unqualified variant data", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(VPackLoadInspectorTest,
       error_when_parsing_unqualified_variant_with_more_than_one_field) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.add("string", VPackValue("foobar"));
  builder.add("blubb", VPackValue("blubb"));
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  UnqualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Unqualified variant data has too many fields", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(VPackLoadInspectorTest, load_inline_variant) {
  builder.openObject();
  builder.add("a", "foobar");

  builder.add(VPackValue("b"));
  builder.openObject();
  builder.add("v", VPackValue(42));
  builder.close();

  builder.add(VPackValue("c"));
  builder.openArray();
  builder.add(VPackValue(1));
  builder.add(VPackValue(2));
  builder.add(VPackValue(3));
  builder.close();

  builder.add("d", VPackValue(123));

  builder.add(VPackValue("e"));
  builder.openArray();
  builder.add(VPackValue("blubb"));
  builder.add(VPackValue(987));
  builder.add(VPackValue(true));
  builder.close();

  builder.close();
  VPackLoadInspector inspector{builder};

  InlineVariant v{.a = {}, .b = {}, .c = {}, .d{}, .e = {}};
  auto result = inspector.apply(v);
  ASSERT_TRUE(result.ok()) << result.error();
  EXPECT_EQ("foobar", std::get<std::string>(v.a));
  EXPECT_EQ(42, std::get<Struct1>(v.b).v);
  EXPECT_EQ(std::vector<int>({1, 2, 3}), std::get<std::vector<int>>(v.c));
  EXPECT_EQ(123, std::get<TypedInt>(v.d).value);
  EXPECT_EQ(std::make_tuple("blubb", 987, true),
            (std::get<std::tuple<std::string, int, bool>>(v.e)));
}

TEST_F(VPackLoadInspectorTest, error_unknown_type_when_loading_inline_variant) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  InlineVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Could not find matching inline type", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(VPackLoadInspectorTest, load_embedded_variant) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.add("t", "Struct1");
  builder.add("v", 1);
  builder.close();

  builder.add(VPackValue("b"));
  builder.openObject();
  builder.add("t", "Struct2");
  builder.add("v", 2);
  builder.close();

  builder.add(VPackValue("c"));
  builder.openObject();
  builder.add("t", "Struct3");
  builder.add("a", 1);
  builder.add("b", 2);
  builder.close();

  builder.add("d", VPackValue(true));
  builder.close();
  VPackLoadInspector inspector{builder};

  EmbeddedVariant v{.a = {}, .b = {}, .c = {}, .d = {}};
  auto result = inspector.apply(v);
  ASSERT_TRUE(result.ok()) << result.error();
  EXPECT_EQ(1, std::get<Struct1>(v.a).v);
  EXPECT_EQ(2, std::get<Struct2>(v.b).v);
  EXPECT_EQ(1, std::get<Struct3>(v.c).a);
  EXPECT_EQ(2, std::get<Struct3>(v.c).b);
  EXPECT_EQ(true, std::get<bool>(v.d));
}

TEST_F(VPackLoadInspectorTest,
       error_unknown_type_tag_when_loading_embedded_variant) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.add("t", "blubb");
  builder.add("v", "");
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  EmbeddedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Found invalid type: blubb", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(VPackLoadInspectorTest,
       error_expecting_int_when_parsing_embedded_variant_value) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.add("t", "Struct1");
  builder.add("v", "blubb");
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  EmbeddedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Int", result.error());
  EXPECT_EQ("a.v", result.path());
}

TEST_F(VPackLoadInspectorTest,
       error_missing_tag_when_parsing_embedded_variant) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.add("v", 42);
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  EmbeddedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Variant type field \"t\" is missing", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(VPackLoadInspectorTest,
       error_invalid_tag_type_when_parsing_embedded_variant) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.add("t", 42);
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  EmbeddedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Variant type field \"t\" must be a string", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(VPackLoadInspectorTest,
       error_missing_value_when_parsing_embedded_variant) {
  builder.openObject();
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.add("t", "Struct3");
  builder.add("a", 1);
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  EmbeddedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Missing required attribute 'b'", result.error());
  EXPECT_EQ("a.b", result.path());
}

TEST_F(VPackLoadInspectorTest, load_type_with_unsafe_fields) {
  builder.openObject();
  builder.add("view", VPackValue("foobar"));
  builder.add("slice", VPackValue("blubb"));
  builder.add("hashed", VPackValue("hashedString"));
  builder.close();
  arangodb::inspection::VPackUnsafeLoadInspector<> inspector{builder};

  Unsafe u;
  auto result = inspector.apply(u);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(builder.slice()["view"].stringView(), u.view);
  EXPECT_EQ(builder.slice()["view"].stringView().data(), u.view.data());
  EXPECT_EQ(builder.slice()["slice"].start(), u.slice.start());
  EXPECT_EQ(builder.slice()["hashed"].stringView(), u.hashed.stringView());
  EXPECT_EQ(builder.slice()["hashed"].stringView().data(), u.hashed.data());
}

TEST_F(VPackLoadInspectorTest, load_string_enum) {
  builder.openArray();
  builder.add(VPackValue("value1"));
  builder.add(VPackValue("value2"));
  builder.close();
  arangodb::inspection::VPackLoadInspector<> inspector{builder};

  std::vector<MyStringEnum> enums;
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(2u, enums.size());
  EXPECT_EQ(MyStringEnum::kValue1, enums[0]);
  EXPECT_EQ(MyStringEnum::kValue2, enums[1]);
}

TEST_F(VPackLoadInspectorTest, load_transformed_string_enum) {
  builder.openArray();
  builder.add(VPackValue("Value1"));
  builder.add(VPackValue("value2"));
  builder.close();
  arangodb::inspection::VPackLoadInspector<> inspector{builder};

  std::vector<MyTransformedStringEnum> enums;
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(2u, enums.size());
  EXPECT_EQ(MyTransformedStringEnum::kValue1, enums[0]);
  EXPECT_EQ(MyTransformedStringEnum::kValue2, enums[1]);
}

TEST_F(VPackLoadInspectorTest, load_int_enum) {
  builder.openArray();
  builder.add(VPackValue(1));
  builder.add(VPackValue(2));
  builder.close();
  arangodb::inspection::VPackLoadInspector<> inspector{builder};

  std::vector<MyIntEnum> enums;
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(2u, enums.size());
  EXPECT_EQ(MyIntEnum::kValue1, enums[0]);
  EXPECT_EQ(MyIntEnum::kValue2, enums[1]);
}

TEST_F(VPackLoadInspectorTest, load_mixed_enum) {
  builder.openArray();
  builder.add(VPackValue("value1"));
  builder.add(VPackValue(1));
  builder.add(VPackValue("value2"));
  builder.add(VPackValue(2));
  builder.close();
  arangodb::inspection::VPackLoadInspector<> inspector{builder};

  std::vector<MyMixedEnum> enums;
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(4u, enums.size());
  EXPECT_EQ(MyMixedEnum::kValue1, enums[0]);
  EXPECT_EQ(MyMixedEnum::kValue1, enums[1]);
  EXPECT_EQ(MyMixedEnum::kValue2, enums[2]);
  EXPECT_EQ(MyMixedEnum::kValue2, enums[3]);
}

TEST_F(VPackLoadInspectorTest, load_string_enum_returns_error_when_not_string) {
  builder.add(VPackValue(42));
  arangodb::inspection::VPackLoadInspector<> inspector{builder};

  MyStringEnum myEnum;
  auto result = inspector.apply(myEnum);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ("Expecting type String", result.error());
}

TEST_F(VPackLoadInspectorTest, load_int_enum_returns_error_when_not_int) {
  builder.add(VPackValue("foobar"));
  arangodb::inspection::VPackLoadInspector<> inspector{builder};

  MyIntEnum myEnum;
  auto result = inspector.apply(myEnum);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ("Expecting type UInt", result.error());
}

TEST_F(VPackLoadInspectorTest,
       load_mixed_enum_returns_error_when_not_string_or_int) {
  builder.add(VPackValue(false));
  arangodb::inspection::VPackLoadInspector<> inspector{builder};

  MyMixedEnum myEnum;
  auto result = inspector.apply(myEnum);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ("Expecting type String or Int", result.error());
}

TEST_F(VPackLoadInspectorTest,
       load_string_enum_returns_error_when_value_is_unknown) {
  builder.add(VPackValue("unknownValue"));
  arangodb::inspection::VPackLoadInspector<> inspector{builder};

  MyStringEnum myEnum;
  auto result = inspector.apply(myEnum);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ("Unknown enum value unknownValue", result.error());
}

TEST_F(VPackLoadInspectorTest,
       load_int_enum_returns_error_when_value_is_unknown) {
  builder.add(VPackValue(42));
  arangodb::inspection::VPackLoadInspector<> inspector{builder};

  MyIntEnum myEnum;
  auto result = inspector.apply(myEnum);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ("Unknown enum value 42", result.error());
}

TEST_F(VPackLoadInspectorTest,
       load_mixed_enum_returns_error_when_value_is_unknown) {
  {
    builder.add(VPackValue("unknownValue"));
    arangodb::inspection::VPackLoadInspector<> inspector{builder};

    MyMixedEnum myEnum;
    auto result = inspector.apply(myEnum);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ("Unknown enum value unknownValue", result.error());
  }
  {
    builder.clear();
    builder.add(VPackValue(42));
    arangodb::inspection::VPackLoadInspector<> inspector{builder};

    MyMixedEnum myEnum;
    auto result = inspector.apply(myEnum);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ("Unknown enum value 42", result.error());
  }
}

TEST_F(VPackLoadInspectorTest, load_embedded_object) {
  builder.openObject();
  builder.add("a", 1);
  builder.add("b", 2);
  builder.close();
  VPackLoadInspector inspector{builder};

  NestedEmbedding n;
  auto result = inspector.apply(n);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(1, n.a);
  EXPECT_EQ(42, n.inner.i);
  EXPECT_EQ("foobar", n.inner.s);
  EXPECT_EQ(2, n.b);
}

TEST_F(VPackLoadInspectorTest,
       load_embedded_object_with_invariant_not_fulfilled) {
  builder.openObject();
  builder.add("a", 1);
  builder.add("b", 2);
  builder.add("i", 0);
  builder.close();
  VPackLoadInspector inspector{builder};

  NestedEmbedding n;
  auto result = inspector.apply(n);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Field invariant failed", result.error());
  EXPECT_EQ("i", result.path());
}

TEST_F(VPackLoadInspectorTest,
       load_embedded_object_with_object_invariant_not_fulfilled) {
  builder.openObject();
  builder.add("a", 1);
  builder.add("b", 2);
  builder.add("i", 42);
  builder.add("s", "");
  builder.close();
  VPackLoadInspector inspector{builder};

  NestedEmbeddingWithObjectInvariant o;
  auto result = inspector.apply(o);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Object invariant failed", result.error());
}

TEST(VPackLoadInspectorContext, deserialize_with_context) {
  struct Context {
    int defaultInt;
    int minInt;
    std::string defaultString;
  };

  velocypack::Builder builder;
  builder.openObject();
  builder.close();

  {
    Context ctxt{.defaultInt = 42, .minInt = 0, .defaultString = "foobar"};
    auto data = velocypack::deserialize<WithContext>(builder.slice(), {}, ctxt);
    EXPECT_EQ(42, data.i);
    EXPECT_EQ("foobar", data.s);
  }

  {
    Context ctxt{.defaultInt = -1, .minInt = -2, .defaultString = "blubb"};
    auto data = velocypack::deserialize<WithContext>(builder.slice(), {}, ctxt);
    EXPECT_EQ(-1, data.i);
    EXPECT_EQ("blubb", data.s);
  }
}

}  // namespace
