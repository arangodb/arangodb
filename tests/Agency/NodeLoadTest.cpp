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
#include <velocypack/SharedSlice.h>
#include <velocypack/Value.h>
#include <memory>

#include "Agency/NodeDeserialization.h"
#include "Agency/NodeLoadInspector.h"

#include "Inspection/InspectionTestHelper.h"

namespace {
using namespace arangodb;
using NodeLoadInspector = inspection::NodeLoadInspector<>;

struct NodeLoadInspectorTest : public ::testing::Test {};

template<class T>
void assign(consensus::Node& node, T value) {
  velocypack::Builder builder;
  builder.add(VPackValue(value));
  node = builder.slice();
}

consensus::Node& addChild(consensus::Node& node, std::string name) {
  auto result = std::make_shared<consensus::Node>(name);
  node.addChild(name, result);
  return *result;
}

TEST_F(NodeLoadInspectorTest, load_empty_object) {
  consensus::Node node{""};
  NodeLoadInspector inspector{&node};

  auto d = AnEmptyObject{};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());
}

TEST_F(NodeLoadInspectorTest, load_int) {
  consensus::Node node{""};
  assign(node, 42);
  NodeLoadInspector inspector{&node};

  int x = 0;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(42, x);
}

TEST_F(NodeLoadInspectorTest, load_double) {
  consensus::Node node{""};
  assign(node, 123.456);
  NodeLoadInspector inspector{&node};

  double x = 0;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(123.456, x);
}

TEST_F(NodeLoadInspectorTest, load_bool) {
  consensus::Node node{""};
  assign(node, true);
  NodeLoadInspector inspector{&node};

  bool x = false;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(true, x);
}

TEST_F(NodeLoadInspectorTest, load_string) {
  consensus::Node node{""};
  assign(node, "foobar");
  NodeLoadInspector inspector{&node};

  std::string x;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("foobar", x);
}

TEST_F(NodeLoadInspectorTest, load_object) {
  consensus::Node node{""};
  assign(addChild(node, "i"), 42);
  assign(addChild(node, "d"), 123.456);
  assign(addChild(node, "b"), true);
  assign(addChild(node, "s"), "foobar");
  NodeLoadInspector inspector{&node};

  Dummy d;
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, d.i);
  EXPECT_EQ(123.456, d.d);
  EXPECT_EQ(true, d.b);
  EXPECT_EQ("foobar", d.s);
}

TEST_F(NodeLoadInspectorTest, load_nested_object) {
  consensus::Node parent{""};
  auto& node = addChild(parent, "dummy");
  assign(addChild(node, "i"), 42);
  assign(addChild(node, "d"), 123.456);
  assign(addChild(node, "b"), true);
  assign(addChild(node, "s"), "foobar");
  NodeLoadInspector inspector{&parent};

  Nested n;
  auto result = inspector.apply(n);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, n.dummy.i);
  EXPECT_EQ(123.456, n.dummy.d);
  EXPECT_EQ(true, n.dummy.b);
  EXPECT_EQ("foobar", n.dummy.s);
}

TEST_F(NodeLoadInspectorTest, load_nested_object_without_nesting) {
  consensus::Node node{""};
  assign(addChild(node, "i"), 42);
  NodeLoadInspector inspector{&node};

  Container c;
  auto result = inspector.apply(c);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, c.i.value);
}

TEST_F(NodeLoadInspectorTest, load_list) {
  consensus::Node node{""};
  {
    velocypack::Builder builder;
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
    addChild(node, "vec") = builder.slice();
  }
  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue(4));
    builder.add(VPackValue(5));
    builder.close();
    addChild(node, "list") = builder.slice();
  }
  NodeLoadInspector inspector{&node};

  List l;
  auto result = inspector.apply(l);
  ASSERT_TRUE(result.ok());

  EXPECT_EQ(3u, l.vec.size());
  EXPECT_EQ(1, l.vec[0].i.value);
  EXPECT_EQ(2, l.vec[1].i.value);
  EXPECT_EQ(3, l.vec[2].i.value);
  EXPECT_EQ((std::list<int>{4, 5}), l.list);
}

TEST_F(NodeLoadInspectorTest, load_map) {
  consensus::Node parent{""};
  {
    auto& node = addChild(parent, "map");
    assign(addChild(addChild(node, "1"), "i"), 1);
    assign(addChild(addChild(node, "2"), "i"), 2);
    assign(addChild(addChild(node, "3"), "i"), 3);
  }

  {
    auto& node = addChild(parent, "unordered");
    assign(addChild(node, "4"), 4);
    assign(addChild(node, "5"), 5);
  }
  NodeLoadInspector inspector{&parent};

  Map m;
  auto result = inspector.apply(m);
  ASSERT_TRUE(result.ok());

  EXPECT_EQ(
      (std::map<std::string, Container>{{"1", {1}}, {"2", {2}}, {"3", {3}}}),
      m.map);
  EXPECT_EQ((std::unordered_map<std::string, int>{{"4", 4}, {"5", 5}}),
            m.unordered);
}

TEST_F(NodeLoadInspectorTest, load_tuples) {
  consensus::Node node{""};

  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue("foo"));
    builder.add(VPackValue(42));
    builder.add(VPackValue(12.34));
    builder.close();
    addChild(node, "tuple") = builder.slice();
  }

  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue(987));
    builder.add(VPackValue("bar"));
    builder.close();
    addChild(node, "pair") = builder.slice();
  }

  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue("a"));
    builder.add(VPackValue("b"));
    builder.close();
    addChild(node, "array1") = builder.slice();
  }

  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue(1));
    builder.add(VPackValue(2));
    builder.add(VPackValue(3));
    builder.close();
    addChild(node, "array2") = builder.slice();
  }

  NodeLoadInspector inspector{&node};

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

TEST_F(NodeLoadInspectorTest, load_slice) {
  {
    consensus::Node parent{""};
    auto& node = addChild(parent, "dummy");
    assign(addChild(node, "i"), 42);
    assign(addChild(node, "b"), true);
    assign(addChild(node, "s"), "foobar");
    NodeLoadInspector inspector{&parent};

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
    consensus::Node node{""};
    assign(node, VPackValue("foobar"));
    NodeLoadInspector inspector{&node};

    velocypack::SharedSlice slice;
    auto result = inspector.apply(slice);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ("foobar", slice.stringView());
  }

  {
    consensus::Node node{""};
    assign(node, VPackValue("foobar"));
    inspection::NodeUnsafeLoadInspector<> inspector{&node};

    velocypack::Slice slice;
    auto result = inspector.apply(slice);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ("foobar", slice.stringView());
  }
}

TEST_F(NodeLoadInspectorTest, load_builder) {
  {
    consensus::Node parent{""};
    auto& node = addChild(parent, "dummy");
    assign(addChild(node, "i"), 42);
    assign(addChild(node, "b"), true);
    assign(addChild(node, "s"), "foobar");
    NodeLoadInspector inspector{&parent};

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
    consensus::Node node{""};
    assign(node, VPackValue("foobar"));
    NodeLoadInspector inspector{&node};

    velocypack::Builder builder;
    auto result = inspector.apply(builder);
    auto slice = builder.slice();
    ASSERT_TRUE(result.ok());
    EXPECT_EQ("foobar", slice.stringView());
  }
}

TEST_F(NodeLoadInspectorTest, load_optional) {
  consensus::Node node{""};
  assign(addChild(node, "y"), "blubb");

  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue(1));
    builder.add(VPackValue(velocypack::ValueType::Null));
    builder.add(VPackValue(3));
    builder.close();
    addChild(node, "vec") = builder.slice();
  }

  {
    auto& child = addChild(node, "map");
    assign(addChild(child, "1"), 1);
    assign(addChild(child, "2"), velocypack::ValueType::Null);
    assign(addChild(child, "3"), 3);
  }

  assign(addChild(node, "a"), velocypack::ValueType::Null);
  NodeLoadInspector inspector{&node};

  Optional o{.a = 1, .b = 2, .x = 42, .y = {}, .vec = {}, .map = {}};
  auto result = inspector.apply(o);
  ASSERT_TRUE(result.ok()) << result.error();

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

TEST_F(NodeLoadInspectorTest, load_optional_pointer) {
  consensus::Node node{""};
  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue(1));
    builder.add(VPackValue(VPackValueType::Null));
    builder.add(VPackValue(2));
    builder.close();
    addChild(node, "vec") = builder.slice();
  }

  assign(addChild(node, "a"), VPackValueType::Null);
  assign(addChild(node, "b"), 42);

  {
    auto& child = addChild(node, "d");
    assign(addChild(child, "i"), 43);
  }

  assign(addChild(node, "x"), VPackValueType::Null);

  NodeLoadInspector inspector{&node};

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

TEST_F(NodeLoadInspectorTest, error_expecting_int) {
  consensus::Node node{""};
  assign(node, "foo");
  NodeLoadInspector inspector{&node};

  int i{};
  auto result = inspector.apply(i);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Int", result.error());
}

TEST_F(NodeLoadInspectorTest, error_expecting_int16) {
  consensus::Node node{""};
  assign(node, 123456789);
  NodeLoadInspector inspector{&node};

  std::int16_t i{};
  auto result = inspector.apply(i);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Number out of range", result.error());
}

TEST_F(NodeLoadInspectorTest, error_expecting_double) {
  consensus::Node node{""};
  assign(node, "foo");
  NodeLoadInspector inspector{&node};

  double d{};
  auto result = inspector.apply(d);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting numeric type", result.error());
}

TEST_F(NodeLoadInspectorTest, error_expecting_bool) {
  consensus::Node node{""};
  assign(node, 42);
  NodeLoadInspector inspector{&node};

  bool b{};
  auto result = inspector.apply(b);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Bool", result.error());
}

TEST_F(NodeLoadInspectorTest, error_expecting_string) {
  consensus::Node node{""};
  assign(node, 42);
  NodeLoadInspector inspector{&node};

  std::string s;
  auto result = inspector.apply(s);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type String", result.error());
}

TEST_F(NodeLoadInspectorTest, error_expecting_array) {
  consensus::Node node{""};
  assign(node, 42);
  NodeLoadInspector inspector{&node};

  std::vector<int> v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Array", result.error());
}

TEST_F(NodeLoadInspectorTest, error_expecting_object) {
  consensus::Node node{""};
  assign(node, 42);
  NodeLoadInspector inspector{&node};

  Dummy d;
  auto result = inspector.apply(d);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Object", result.error());
}

TEST_F(NodeLoadInspectorTest, error_expecting_type_on_path) {
  consensus::Node node{""};
  assign(addChild(addChild(node, "dummy"), "i"), "foo");
  NodeLoadInspector inspector{&node};

  Nested n;
  auto result = inspector.apply(n);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("dummy.i", result.path());
}

TEST_F(NodeLoadInspectorTest, error_expecting_type_on_path_with_array) {
  consensus::Node node{""};

  {
    velocypack::Builder builder;
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
    addChild(node, "vec") = builder.slice();
  }
  NodeLoadInspector inspector{&node};

  List l;
  auto result = inspector.apply(l);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ("vec[2].i", result.path());
}

TEST_F(NodeLoadInspectorTest, error_expecting_type_on_path_with_map) {
  consensus::Node node{""};
  {
    auto& child = addChild(node, "map");
    assign(addChild(addChild(child, "1"), "i"), 1);
    assign(addChild(addChild(child, "2"), "i"), 2);
    assign(addChild(addChild(child, "3"), "i"), "foobar");
  }

  NodeLoadInspector inspector{&node};

  Map m;
  auto result = inspector.apply(m);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ("map['3'].i", result.path());
}

TEST_F(NodeLoadInspectorTest, error_missing_field) {
  consensus::Node node{""};
  assign(addChild(addChild(node, "dummy"), "s"), "foo");
  NodeLoadInspector inspector{&node};

  Nested n;
  auto result = inspector.apply(n);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Missing required attribute 'i'", result.error());
  EXPECT_EQ("dummy.i", result.path());
}

TEST_F(NodeLoadInspectorTest, error_found_unexpected_attribute) {
  consensus::Node node{""};
  assign(addChild(node, "i"), 42);
  assign(addChild(node, "should_not_be_here"), 123);
  NodeLoadInspector inspector{&node};

  Container c;
  auto result = inspector.apply(c);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Found unexpected attribute 'should_not_be_here'", result.error());
}

TEST_F(NodeLoadInspectorTest, load_object_ignoring_unknown_attributes) {
  consensus::Node node{""};
  assign(addChild(node, "i"), 42);
  assign(addChild(node, "ignore_me"), 123);
  NodeLoadInspector inspector{&node, {.ignoreUnknownFields = true}};

  Container c;
  auto result = inspector.apply(c);
  ASSERT_TRUE(result.ok()) << "Error: " << result.error()
                           << "\nPath: " << result.path();
}

TEST_F(NodeLoadInspectorTest, load_object_with_fallbacks) {
  consensus::Node node{""};
  NodeLoadInspector inspector{&node};

  Fallback f;
  Dummy expected = f.d;
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, f.i);
  EXPECT_EQ("foobar", f.s);
  EXPECT_EQ(expected, f.d);
  EXPECT_EQ(84, f.dynamic);  // f.i * 2
}

TEST_F(NodeLoadInspectorTest, load_object_with_fallback_reference) {
  consensus::Node node{""};
  assign(addChild(node, "x"), 42);
  NodeLoadInspector inspector{&node};

  FallbackReference f;
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, f.x);
  EXPECT_EQ(42, f.y);
}

TEST_F(NodeLoadInspectorTest, load_object_ignoring_missing_fields) {
  consensus::Node node{""};
  NodeLoadInspector inspector{&node, {.ignoreMissingFields = true}};

  FallbackReference f{.x = 1, .y = 2};
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(1, f.x);
  EXPECT_EQ(1, f.y);
}

TEST_F(NodeLoadInspectorTest, load_object_with_invariant_fulfilled) {
  consensus::Node node{""};
  assign(addChild(node, "i"), 42);
  assign(addChild(node, "s"), "foobar");
  NodeLoadInspector inspector{&node};

  Invariant i;
  auto result = inspector.apply(i);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, i.i);
  EXPECT_EQ("foobar", i.s);
}

TEST_F(NodeLoadInspectorTest, load_object_with_invariant_not_fulfilled) {
  {
    consensus::Node node{""};
    assign(addChild(node, "i"), 0);
    assign(addChild(node, "s"), "foobar");
    NodeLoadInspector inspector{&node};

    Invariant i;
    auto result = inspector.apply(i);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Field invariant failed", result.error());
    EXPECT_EQ("i", result.path());
  }

  {
    consensus::Node node{""};
    assign(addChild(node, "i"), 42);
    assign(addChild(node, "s"), "");
    NodeLoadInspector inspector{&node};

    Invariant i;
    auto result = inspector.apply(i);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Field invariant failed", result.error());
    EXPECT_EQ("s", result.path());
  }
}

TEST_F(NodeLoadInspectorTest, load_object_with_invariant_Result_not_fulfilled) {
  {
    consensus::Node node{""};
    assign(addChild(node, "i"), 0);
    NodeLoadInspector inspector{&node};

    InvariantWithResult i;
    auto result = inspector.apply(i);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Must not be zero", result.error());
    EXPECT_EQ("i", result.path());
  }

  {
    consensus::Node node{""};
    assign(addChild(node, "i"), 42);
    assign(addChild(node, "s"), "");
    NodeLoadInspector inspector{&node};

    Invariant i;
    auto result = inspector.apply(i);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Field invariant failed", result.error());
    EXPECT_EQ("s", result.path());
  }
}

TEST_F(NodeLoadInspectorTest, load_object_with_invariant_and_fallback) {
  consensus::Node node{""};
  NodeLoadInspector inspector{&node};

  InvariantAndFallback i;
  auto result = inspector.apply(i);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, i.i);
  EXPECT_EQ("foobar", i.s);
}

TEST_F(NodeLoadInspectorTest, load_object_with_object_invariant) {
  consensus::Node node{""};
  assign(addChild(node, "i"), 42);
  assign(addChild(node, "s"), "");
  NodeLoadInspector inspector{&node};

  ObjectInvariant o;
  auto result = inspector.apply(o);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Object invariant failed", result.error());
}

TEST_F(NodeLoadInspectorTest, load_object_with_field_transform) {
  consensus::Node node{""};
  assign(addChild(node, "x"), "42");
  NodeLoadInspector inspector{&node};

  FieldTransform f;
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, f.x);
}

TEST_F(NodeLoadInspectorTest, load_object_with_field_transform_and_fallback) {
  consensus::Node node{""};
  assign(addChild(node, "x"), "42");
  NodeLoadInspector inspector{&node};

  FieldTransformWithFallback f;
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, f.x);
  EXPECT_EQ(2, f.y);
}

TEST_F(NodeLoadInspectorTest, load_object_with_optional_field_transform) {
  consensus::Node node{""};
  assign(addChild(node, "x"), "42");
  NodeLoadInspector inspector{&node};

  OptionalFieldTransform f{.x = 1, .y = 2, .z = 3};
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, *f.x);
  EXPECT_FALSE(f.y.has_value());
  EXPECT_EQ(123, *f.z);
}

TEST_F(NodeLoadInspectorTest, load_type_with_custom_specialization) {
  consensus::Node node{""};
  assign(addChild(node, "i"), 42);
  assign(addChild(node, "s"), "foobar");
  NodeLoadInspector inspector{&node};

  Specialization s;
  auto result = inspector.apply(s);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, s.i);
  EXPECT_EQ("foobar", s.s);
}

TEST_F(NodeLoadInspectorTest, load_type_with_explicitly_ignored_fields) {
  consensus::Node node{""};
  assign(addChild(node, "s"), "foobar");
  assign(addChild(node, "ignore"), "something");
  NodeLoadInspector inspector{&node};

  ExplicitIgnore e;
  auto result = inspector.apply(e);
  ASSERT_TRUE(result.ok());
}

TEST_F(NodeLoadInspectorTest, load_qualified_variant) {
  consensus::Node node{""};
  assign(addChild(node, "a"), "foobar");

  {
    auto& child = addChild(node, "b");
    assign(addChild(child, "t"), "int");
    assign(addChild(child, "v"), 42);
  }

  {
    auto& child = addChild(node, "c");
    assign(addChild(child, "t"), "Struct1");
    assign(addChild(addChild(child, "v"), "v"), 1);
  }

  {
    auto& child = addChild(node, "d");
    assign(addChild(child, "t"), "Struct2");
    assign(addChild(addChild(child, "v"), "v"), 2);
  }

  {
    auto& child = addChild(node, "e");
    assign(addChild(child, "t"), "nil");
    addChild(child, "v");
  }

  NodeLoadInspector inspector{&node};

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

TEST_F(NodeLoadInspectorTest,
       error_unknown_type_tag_when_loading_qualified_variant) {
  consensus::Node node{""};
  {
    auto& child = addChild(node, "a");
    assign(addChild(child, "t"), "blubb");
    assign(addChild(child, "v"), "");
  }
  NodeLoadInspector inspector{&node};

  QualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Found invalid type: blubb", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_expecting_string_when_parsing_qualified_variant_value) {
  consensus::Node node{""};
  {
    auto& child = addChild(node, "a");
    assign(addChild(child, "t"), "int");
    assign(addChild(child, "v"), "blubb");
  }
  NodeLoadInspector inspector{&node};

  QualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Int", result.error());
  EXPECT_EQ("a.v", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_missing_tag_when_parsing_qualified_variant) {
  consensus::Node node{""};
  {
    auto& child = addChild(node, "a");
    assign(addChild(child, "v"), 42);
  }
  NodeLoadInspector inspector{&node};

  QualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Variant type field \"t\" is missing", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_invalid_tag_type_when_parsing_qualified_variant) {
  consensus::Node node{""};
  {
    auto& child = addChild(node, "a");
    assign(addChild(child, "t"), 42);
  }
  NodeLoadInspector inspector{&node};

  QualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Variant type field \"t\" must be a string", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_missing_value_when_parsing_qualified_variant) {
  consensus::Node node{""};
  {
    auto& child = addChild(node, "a");
    assign(addChild(child, "t"), "int");
  }
  NodeLoadInspector inspector{&node};

  QualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Variant value field \"v\" is missing", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest, load_unqualified_variant) {
  consensus::Node node{""};
  assign(addChild(addChild(node, "a"), "string"), "foobar");
  assign(addChild(node, "b"), 42);
  assign(addChild(addChild(addChild(node, "c"), "Struct1"), "v"), 1);
  assign(addChild(addChild(addChild(node, "d"), "Struct2"), "v"), 2);
  addChild(addChild(node, "e"), "nil");
  NodeLoadInspector inspector{&node};

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

TEST_F(NodeLoadInspectorTest,
       error_unknown_type_tag_when_loading_unqualified_variant) {
  consensus::Node node{""};
  assign(addChild(addChild(node, "a"), "blubb"), "");
  NodeLoadInspector inspector{&node};

  UnqualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Found invalid type: blubb", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_expecting_string_when_parsing_unqualified_variant_value) {
  consensus::Node node{""};
  assign(addChild(addChild(node, "a"), "string"), 42);
  NodeLoadInspector inspector{&node};

  UnqualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type String", result.error());
  EXPECT_EQ("a.string", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_missing_data_when_parsing_unqualified_variant) {
  consensus::Node node{""};
  addChild(node, "a");
  NodeLoadInspector inspector{&node};

  UnqualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Missing unqualified variant data", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_when_parsing_unqualified_variant_with_more_than_one_field) {
  consensus::Node node{""};
  {
    auto& child = addChild(node, "a");
    assign(addChild(child, "string"), "foobar");
    assign(addChild(child, "blubb"), "blubb");
  }
  NodeLoadInspector inspector{&node};

  UnqualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Unqualified variant data has too many fields", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest, load_inline_variant) {
  consensus::Node node{""};
  assign(addChild(node, "a"), "foobar");

  assign(addChild(addChild(node, "b"), "v"), 42);

  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue(1));
    builder.add(VPackValue(2));
    builder.add(VPackValue(3));
    builder.close();
    addChild(node, "c") = builder.slice();
  }

  assign(addChild(node, "d"), 123);

  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue("blubb"));
    builder.add(VPackValue(987));
    builder.add(VPackValue(true));
    builder.close();
    addChild(node, "e") = builder.slice();
  }

  NodeLoadInspector inspector{&node};

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

TEST_F(NodeLoadInspectorTest, error_unknown_type_when_loading_inline_variant) {
  consensus::Node node{""};
  addChild(node, "a");
  NodeLoadInspector inspector{&node};

  InlineVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Could not find matching inline type", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest, load_embedded_variant) {
  consensus::Node node{""};
  {
    auto& child = addChild(node, "a");
    assign(addChild(child, "t"), "Struct1");
    assign(addChild(child, "v"), 1);
  }
  {
    auto& child = addChild(node, "b");
    assign(addChild(child, "t"), "Struct2");
    assign(addChild(child, "v"), 2);
  }
  {
    auto& child = addChild(node, "c");
    assign(addChild(child, "t"), "Struct3");
    assign(addChild(child, "a"), 1);
    assign(addChild(child, "b"), 2);
  }
  assign(addChild(node, "d"), true);
  NodeLoadInspector inspector{&node};

  EmbeddedVariant v{.a = {}, .b = {}, .c = {}, .d = {}};
  auto result = inspector.apply(v);
  ASSERT_TRUE(result.ok()) << result.error();
  EXPECT_EQ(1, std::get<Struct1>(v.a).v);
  EXPECT_EQ(2, std::get<Struct2>(v.b).v);
  EXPECT_EQ(1, std::get<Struct3>(v.c).a);
  EXPECT_EQ(2, std::get<Struct3>(v.c).b);
  EXPECT_EQ(true, std::get<bool>(v.d));
}

TEST_F(NodeLoadInspectorTest,
       error_unknown_type_tag_when_loading_embedded_variant) {
  consensus::Node node{""};
  {
    auto& child = addChild(node, "a");
    assign(addChild(child, "t"), "blubb");
    assign(addChild(child, "v"), "");
  }
  NodeLoadInspector inspector{&node};

  EmbeddedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Found invalid type: blubb", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_expecting_int_when_parsing_embedded_variant_value) {
  consensus::Node node{""};
  {
    auto& child = addChild(node, "a");
    assign(addChild(child, "t"), "Struct1");
    assign(addChild(child, "v"), "blubb");
  }
  NodeLoadInspector inspector{&node};

  EmbeddedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Int", result.error());
  EXPECT_EQ("a.v", result.path());
}

TEST_F(NodeLoadInspectorTest, error_missing_tag_when_parsing_embedded_variant) {
  consensus::Node node{""};
  assign(addChild(addChild(node, "a"), "v"), 42);
  NodeLoadInspector inspector{&node};

  EmbeddedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Variant type field \"t\" is missing", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_invalid_tag_type_when_parsing_embedded_variant) {
  consensus::Node node{""};
  assign(addChild(addChild(node, "a"), "t"), 42);
  NodeLoadInspector inspector{&node};

  EmbeddedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Variant type field \"t\" must be a string", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_missing_value_when_parsing_embedded_variant) {
  consensus::Node node{""};
  {
    auto& child = addChild(node, "a");
    assign(addChild(child, "t"), "Struct3");
    assign(addChild(child, "a"), 1);
  }
  NodeLoadInspector inspector{&node};

  EmbeddedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Missing required attribute 'b'", result.error());
  EXPECT_EQ("a.b", result.path());
}

TEST_F(NodeLoadInspectorTest, load_type_with_unsafe_fields) {
  consensus::Node node{""};
  assign(addChild(node, "view"), "foobar");
  assign(addChild(node, "slice"), "blubb");
  assign(addChild(node, "hashed"), "hashedString");
  arangodb::inspection::NodeUnsafeLoadInspector<> inspector{&node};

  Unsafe u;
  auto result = inspector.apply(u);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(node.get("view")->getStringView().value(), u.view);
  EXPECT_EQ(node.get("view")->getStringView().value().data(), u.view.data());
  EXPECT_EQ(node.get("slice")->slice().start(), u.slice.start());
  EXPECT_EQ(node.get("hashed")->getStringView().value(), u.hashed.stringView());
  EXPECT_EQ(node.get("hashed")->getStringView().value().data(),
            u.hashed.data());
}

TEST_F(NodeLoadInspectorTest, load_string_enum) {
  consensus::Node node{""};
  MyStringEnum myEnum;
  {
    assign(node, "value1");
    arangodb::inspection::NodeLoadInspector<> inspector{&node};
    auto result = inspector.apply(myEnum);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(MyStringEnum::kValue1, myEnum);
  }

  {
    assign(node, "value2");
    arangodb::inspection::NodeLoadInspector<> inspector{&node};
    auto result = inspector.apply(myEnum);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(MyStringEnum::kValue2, myEnum);
  }
}

TEST_F(NodeLoadInspectorTest, load_int_enum) {
  consensus::Node node{""};
  MyIntEnum myEnum;
  {
    assign(node, 1);
    arangodb::inspection::NodeLoadInspector<> inspector{&node};
    auto result = inspector.apply(myEnum);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(MyIntEnum::kValue1, myEnum);
  }

  {
    assign(node, 2);
    arangodb::inspection::NodeLoadInspector<> inspector{&node};
    auto result = inspector.apply(myEnum);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(MyIntEnum::kValue2, myEnum);
  }
}

TEST_F(NodeLoadInspectorTest, load_mixed_enum) {
  consensus::Node node{""};
  MyMixedEnum myEnum;
  {
    assign(node, "value1");
    arangodb::inspection::NodeLoadInspector<> inspector{&node};
    auto result = inspector.apply(myEnum);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(MyMixedEnum::kValue1, myEnum);
  }
  {
    assign(node, 1);
    arangodb::inspection::NodeLoadInspector<> inspector{&node};
    auto result = inspector.apply(myEnum);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(MyMixedEnum::kValue1, myEnum);
  }
  {
    assign(node, "value2");
    arangodb::inspection::NodeLoadInspector<> inspector{&node};
    auto result = inspector.apply(myEnum);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(MyMixedEnum::kValue2, myEnum);
  }
  {
    assign(node, 2);
    arangodb::inspection::NodeLoadInspector<> inspector{&node};
    auto result = inspector.apply(myEnum);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(MyMixedEnum::kValue2, myEnum);
  }
}

TEST_F(NodeLoadInspectorTest, load_string_enum_returns_error_when_not_string) {
  consensus::Node node{""};
  assign(node, 42);
  arangodb::inspection::NodeLoadInspector<> inspector{&node};

  MyStringEnum myEnum;
  auto result = inspector.apply(myEnum);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ("Expecting type String", result.error());
}

TEST_F(NodeLoadInspectorTest, load_int_enum_returns_error_when_not_int) {
  consensus::Node node{""};
  assign(node, "foobar");
  arangodb::inspection::NodeLoadInspector<> inspector{&node};

  MyIntEnum myEnum;
  auto result = inspector.apply(myEnum);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ("Expecting type UInt", result.error());
}

TEST_F(NodeLoadInspectorTest,
       load_mixed_enum_returns_error_when_not_string_or_int) {
  consensus::Node node{""};
  assign(node, false);
  arangodb::inspection::NodeLoadInspector<> inspector{&node};

  MyMixedEnum myEnum;
  auto result = inspector.apply(myEnum);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ("Expecting type String or Int", result.error());
}

TEST_F(NodeLoadInspectorTest,
       load_string_enum_returns_error_when_value_is_unknown) {
  consensus::Node node{""};
  assign(node, "unknownValue");
  arangodb::inspection::NodeLoadInspector<> inspector{&node};

  MyStringEnum myEnum;
  auto result = inspector.apply(myEnum);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ("Unknown enum value unknownValue", result.error());
}

TEST_F(NodeLoadInspectorTest,
       load_int_enum_returns_error_when_value_is_unknown) {
  consensus::Node node{""};
  assign(node, 42);
  arangodb::inspection::NodeLoadInspector<> inspector{&node};

  MyIntEnum myEnum;
  auto result = inspector.apply(myEnum);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ("Unknown enum value 42", result.error());
}

TEST_F(NodeLoadInspectorTest,
       load_mixed_enum_returns_error_when_value_is_unknown) {
  {
    consensus::Node node{""};
    assign(node, "unknownValue");
    arangodb::inspection::NodeLoadInspector<> inspector{&node};

    MyMixedEnum myEnum;
    auto result = inspector.apply(myEnum);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ("Unknown enum value unknownValue", result.error());
  }
  {
    consensus::Node node{""};
    assign(node, 42);
    arangodb::inspection::NodeLoadInspector<> inspector{&node};

    MyMixedEnum myEnum;
    auto result = inspector.apply(myEnum);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ("Unknown enum value 42", result.error());
  }
}

TEST_F(NodeLoadInspectorTest, load_embedded_object) {
  consensus::Node node{""};
  assign(addChild(node, "a"), 1);
  assign(addChild(node, "b"), 2);
  NodeLoadInspector inspector{&node};

  NestedEmbedding n;
  auto result = inspector.apply(n);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(1, n.a);
  EXPECT_EQ(42, n.inner.i);
  EXPECT_EQ("foobar", n.inner.s);
  EXPECT_EQ(2, n.b);
}

TEST_F(NodeLoadInspectorTest,
       load_embedded_object_with_invariant_not_fulfilled) {
  consensus::Node node{""};
  assign(addChild(node, "a"), 1);
  assign(addChild(node, "b"), 2);
  assign(addChild(node, "i"), 0);
  NodeLoadInspector inspector{&node};

  NestedEmbedding n;
  auto result = inspector.apply(n);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Field invariant failed", result.error());
  EXPECT_EQ("i", result.path());
}

TEST_F(NodeLoadInspectorTest,
       load_embedded_object_with_object_invariant_not_fulfilled) {
  consensus::Node node{""};
  assign(addChild(node, "a"), 1);
  assign(addChild(node, "b"), 2);
  assign(addChild(node, "i"), 42);
  assign(addChild(node, "s"), "");
  NodeLoadInspector inspector{&node};

  NestedEmbeddingWithObjectInvariant o;
  auto result = inspector.apply(o);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Object invariant failed", result.error());
}

TEST(NodeLoadInspectorContextTest, deserialize_with_context) {
  struct Context {
    int defaultInt;
    int minInt;
    std::string defaultString;
  };

  consensus::Node node{""};

  {
    Context ctxt{.defaultInt = 42, .minInt = 0, .defaultString = "foobar"};
    auto data = consensus::deserialize<WithContext>(node, {}, ctxt);
    EXPECT_EQ(42, data.i);
    EXPECT_EQ("foobar", data.s);
  }

  {
    Context ctxt{.defaultInt = -1, .minInt = -2, .defaultString = "blubb"};
    auto data = consensus::deserialize<WithContext>(node, {}, ctxt);
    EXPECT_EQ(-1, data.i);
    EXPECT_EQ("blubb", data.s);
  }
}

}  // namespace
