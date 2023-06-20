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

using namespace arangodb::velocypack;

namespace {
using namespace arangodb;
using NodeLoadInspector = inspection::NodeLoadInspector<>;

struct NodeLoadInspectorTest : public ::testing::Test {};

TEST_F(NodeLoadInspectorTest, load_empty_object) {
  auto node = consensus::Node::create();
  NodeLoadInspector inspector{node};

  EXPECT_TRUE(node->isObject());

  auto d = AnEmptyObject{};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok()) << result.error();
}

TEST_F(NodeLoadInspectorTest, load_int) {
  auto node = consensus::Node::create(42);
  NodeLoadInspector inspector{node.get()};

  int x = 0;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(42, x);
}

TEST_F(NodeLoadInspectorTest, load_double) {
  auto node = consensus::Node::create(123.456);
  NodeLoadInspector inspector{node};

  double x = 0;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(123.456, x);
}

TEST_F(NodeLoadInspectorTest, load_bool) {
  auto node = consensus::Node::create(true);
  NodeLoadInspector inspector{node};

  bool x = false;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(true, x);
}

TEST_F(NodeLoadInspectorTest, load_string) {
  auto node = consensus::Node::create("foobar");
  NodeLoadInspector inspector{node};

  std::string x;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("foobar", x);
}

TEST_F(NodeLoadInspectorTest, load_object) {
  auto node = consensus::Node::create();
  node = node->placeAt("i", 42);
  node = node->placeAt("d", 123.456);
  node = node->placeAt("b", true);
  node = node->placeAt("s", "foobar");
  NodeLoadInspector inspector{node};

  Dummy d;
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok()) << result.error();
  EXPECT_EQ(42, d.i);
  EXPECT_EQ(123.456, d.d);
  EXPECT_EQ(true, d.b);
  EXPECT_EQ("foobar", d.s);
}

TEST_F(NodeLoadInspectorTest, load_nested_object) {
  auto node = consensus::Node::create();
  node = node->placeAt("dummy/i", 42);
  node = node->placeAt("dummy/d", 123.456);
  node = node->placeAt("dummy/b", true);
  node = node->placeAt("dummy/s", "foobar");
  NodeLoadInspector inspector{node};

  Nested n;
  auto result = inspector.apply(n);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, n.dummy.i);
  EXPECT_EQ(123.456, n.dummy.d);
  EXPECT_EQ(true, n.dummy.b);
  EXPECT_EQ("foobar", n.dummy.s);
}

TEST_F(NodeLoadInspectorTest, load_nested_object_without_nesting) {
  auto node = consensus::Node::create();
  node = node->placeAt("i", 42);
  NodeLoadInspector inspector{node};

  Container c;
  auto result = inspector.apply(c);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, c.i.value);
}

TEST_F(NodeLoadInspectorTest, load_list) {
  auto node = consensus::Node::create();
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
    node = node->placeAt("vec", builder.slice());
  }
  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue(4));
    builder.add(VPackValue(5));
    builder.close();
    node = node->placeAt("list", builder.slice());
  }
  NodeLoadInspector inspector{node.get()};

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
  auto node = consensus::Node::create();
  {
    node = node->placeAt("map/1/i", 1);
    node = node->placeAt("map/2/i", 2);
    node = node->placeAt("map/3/i", 3);
  }

  {
    node = node->placeAt("unordered/4", 4);
    node = node->placeAt("unordered/5", 5);
  }
  NodeLoadInspector inspector{node};

  Map m;
  auto result = inspector.apply(m);
  ASSERT_TRUE(result.ok()) << result.error() << node->toJson();

  EXPECT_EQ(
      (std::map<std::string, Container>{{"1", {1}}, {"2", {2}}, {"3", {3}}}),
      m.map);
  EXPECT_EQ((std::unordered_map<std::string, int>{{"4", 4}, {"5", 5}}),
            m.unordered);
}

TEST_F(NodeLoadInspectorTest, load_tuples) {
  auto node = consensus::Node::create();

  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue("foo"));
    builder.add(VPackValue(42));
    builder.add(VPackValue(12.34));
    builder.close();
    node = node->placeAt("tuple", builder.slice());
  }

  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue(987));
    builder.add(VPackValue("bar"));
    builder.close();
    node = node->placeAt("pair", builder.slice());
  }

  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue("a"));
    builder.add(VPackValue("b"));
    builder.close();
    node = node->placeAt("array1", builder.slice());
  }

  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue(1));
    builder.add(VPackValue(2));
    builder.add(VPackValue(3));
    builder.close();
    node = node->placeAt("array2", builder.slice());
  }

  NodeLoadInspector inspector{node};

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
    auto parent = consensus::Node::create();
    parent = parent->placeAt("dummy/i", 42);
    parent = parent->placeAt("dummy/b", true);
    parent = parent->placeAt("dummy/s", "foobar");
    NodeLoadInspector inspector{parent};

    velocypack::SharedSlice slice;
    auto result = inspector.apply(slice);
    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(slice.isObject());
    slice = slice["dummy"];
    ASSERT_TRUE(slice.isObject()) << slice.toJson();
    EXPECT_EQ(42, slice["i"].getInt());
    EXPECT_EQ(true, slice["b"].getBoolean());
    EXPECT_EQ("foobar", slice["s"].stringView());
  }

  {
    auto node = consensus::Node::create("foobar");
    NodeLoadInspector inspector{node};

    velocypack::SharedSlice slice;
    auto result = inspector.apply(slice);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ("foobar", slice.stringView());
  }

  {
    auto node = consensus::Node::create("foobar");
    inspection::NodeUnsafeLoadInspector<> inspector{node};

    velocypack::Slice slice;
    auto result = inspector.apply(slice);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ("foobar", slice.stringView());
  }
}

TEST_F(NodeLoadInspectorTest, load_builder) {
  {
    auto node = consensus::Node::create();
    node = node->placeAt("dummy/i", 42);
    node = node->placeAt("dummy/b", true);
    node = node->placeAt("dummy/s", "foobar");
    NodeLoadInspector inspector{node};

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
    auto node = consensus::Node::create("foobar");
    NodeLoadInspector inspector{node};

    velocypack::Builder builder;
    auto result = inspector.apply(builder);
    auto slice = builder.slice();
    ASSERT_TRUE(result.ok());
    EXPECT_EQ("foobar", slice.stringView());
  }
}

TEST_F(NodeLoadInspectorTest, load_optional) {
  auto node = consensus::Node::create();
  node = node->placeAt("y", "blubb");

  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue(1));
    builder.add(VPackValue(velocypack::ValueType::Null));
    builder.add(VPackValue(3));
    builder.close();
    node = node->placeAt("vec", builder.slice());
  }

  {
    node = node->placeAt("map/1", 1);
    node = node->placeAt("map/2", velocypack::ValueType::Null);
    node = node->placeAt("map/3", 3);
  }

  node = node->placeAt("a", velocypack::ValueType::Null);
  NodeLoadInspector inspector{node};

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
  auto node = consensus::Node::create();
  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue(1));
    builder.add(VPackValue(VPackValueType::Null));
    builder.add(VPackValue(2));
    builder.close();
    node = node->placeAt("vec", builder.slice());
  }

  node = node->placeAt("a", velocypack::ValueType::Null);
  node = node->placeAt("b", 42);
  node = node->placeAt("d/i", 43);
  node = node->placeAt("x", velocypack::ValueType::Null);

  NodeLoadInspector inspector{node};

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
  auto node = consensus::Node::create("foo");
  NodeLoadInspector inspector{node};

  int i{};
  auto result = inspector.apply(i);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Int", result.error());
}

TEST_F(NodeLoadInspectorTest, error_expecting_int16) {
  auto node = consensus::Node::create(123456789);
  NodeLoadInspector inspector{node};

  std::int16_t i{};
  auto result = inspector.apply(i);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Number out of range", result.error());
}

TEST_F(NodeLoadInspectorTest, error_expecting_double) {
  auto node = consensus::Node::create("foo");
  NodeLoadInspector inspector{node};

  double d{};
  auto result = inspector.apply(d);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting numeric type", result.error());
}

TEST_F(NodeLoadInspectorTest, error_expecting_bool) {
  auto node = consensus::Node::create(42);
  NodeLoadInspector inspector{node};

  bool b{};
  auto result = inspector.apply(b);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Bool", result.error());
}

TEST_F(NodeLoadInspectorTest, error_expecting_string) {
  auto node = consensus::Node::create(42);
  NodeLoadInspector inspector{node};

  std::string s;
  auto result = inspector.apply(s);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type String", result.error());
}

TEST_F(NodeLoadInspectorTest, error_expecting_array) {
  auto node = consensus::Node::create(42);
  NodeLoadInspector inspector{node};

  std::vector<int> v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Array", result.error());
}

TEST_F(NodeLoadInspectorTest, error_expecting_object) {
  auto node = consensus::Node::create(42);
  NodeLoadInspector inspector{node};

  Dummy d;
  auto result = inspector.apply(d);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Object", result.error());
}

TEST_F(NodeLoadInspectorTest, error_expecting_type_on_path) {
  auto node = consensus::Node::create();
  node = node->placeAt("dummy/i", "foo");
  NodeLoadInspector inspector{node};

  Nested n;
  auto result = inspector.apply(n);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("dummy.i", result.path());
}

TEST_F(NodeLoadInspectorTest, error_expecting_type_on_path_with_array) {
  auto node = consensus::Node::create();

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
    node = node->placeAt("vec", builder.slice());
  }
  NodeLoadInspector inspector{node};

  List l;
  auto result = inspector.apply(l);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ("vec[2].i", result.path());
}

TEST_F(NodeLoadInspectorTest, error_expecting_type_on_path_with_map) {
  auto node = consensus::Node::create();
  {
    node = node->placeAt("map/1/i", 1);
    node = node->placeAt("map/2/i", 2);
    node = node->placeAt("map/3/i", "foobar");
  }

  NodeLoadInspector inspector{node};

  Map m;
  auto result = inspector.apply(m);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ("map['3'].i", result.path());
}

TEST_F(NodeLoadInspectorTest, error_missing_field) {
  auto node = consensus::Node::create();
  node = node->placeAt("dummy/s", "foo");
  NodeLoadInspector inspector{node};

  Nested n;
  auto result = inspector.apply(n);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Missing required attribute 'i'", result.error());
  EXPECT_EQ("dummy.i", result.path());
}

TEST_F(NodeLoadInspectorTest, error_found_unexpected_attribute) {
  auto node = consensus::Node::create();
  node = node->placeAt("i", 42);
  node = node->placeAt("should_not_be_here", 123);
  NodeLoadInspector inspector{node};

  Container c;
  auto result = inspector.apply(c);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Found unexpected attribute 'should_not_be_here'", result.error());
}

TEST_F(NodeLoadInspectorTest, load_object_ignoring_unknown_attributes) {
  auto node = consensus::Node::create();
  node = node->placeAt("i", 42);
  node = node->placeAt("ignore_me", 123);
  NodeLoadInspector inspector{node, {.ignoreUnknownFields = true}};

  Container c;
  auto result = inspector.apply(c);
  ASSERT_TRUE(result.ok()) << "Error: " << result.error()
                           << "\nPath: " << result.path();
}

TEST_F(NodeLoadInspectorTest, load_object_with_fallbacks) {
  auto node = consensus::Node::create();
  NodeLoadInspector inspector{node};

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
  auto node = consensus::Node::create();
  node = node->placeAt("x", 42);
  NodeLoadInspector inspector{node};

  FallbackReference f;
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, f.x);
  EXPECT_EQ(42, f.y);
}

TEST_F(NodeLoadInspectorTest, load_object_ignoring_missing_fields) {
  auto node = consensus::Node::create();
  NodeLoadInspector inspector{node, {.ignoreMissingFields = true}};

  FallbackReference f{.x = 1, .y = 2};
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(1, f.x);
  EXPECT_EQ(1, f.y);
}

TEST_F(NodeLoadInspectorTest, load_object_with_invariant_fulfilled) {
  auto node = consensus::Node::create();
  node = node->placeAt("i", 42);
  node = node->placeAt("s", "foobar");
  NodeLoadInspector inspector{node.get()};

  Invariant i;
  auto result = inspector.apply(i);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, i.i);
  EXPECT_EQ("foobar", i.s);
}

TEST_F(NodeLoadInspectorTest, load_object_with_invariant_not_fulfilled) {
  {
    auto node = consensus::Node::create();
    node = node->placeAt("i", 0);
    node = node->placeAt("s", "foobar");
    NodeLoadInspector inspector{node};

    Invariant i;
    auto result = inspector.apply(i);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Field invariant failed", result.error());
    EXPECT_EQ("i", result.path());
  }

  {
    auto node = consensus::Node::create();
    node = node->placeAt("i", 42);
    node = node->placeAt("s", "");
    NodeLoadInspector inspector{node.get()};

    Invariant i;
    auto result = inspector.apply(i);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Field invariant failed", result.error());
    EXPECT_EQ("s", result.path());
  }
}

TEST_F(NodeLoadInspectorTest, load_object_with_invariant_Result_not_fulfilled) {
  {
    auto node = consensus::Node::create();
    node = node->placeAt("i", 0);
    NodeLoadInspector inspector{node};

    InvariantWithResult i;
    auto result = inspector.apply(i);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Must not be zero", result.error());
    EXPECT_EQ("i", result.path());
  }

  {
    auto node = consensus::Node::create();
    node = node->placeAt("i", 42);
    node = node->placeAt("s", "");
    NodeLoadInspector inspector{node};

    Invariant i;
    auto result = inspector.apply(i);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ("Field invariant failed", result.error());
    EXPECT_EQ("s", result.path());
  }
}

TEST_F(NodeLoadInspectorTest, load_object_with_invariant_and_fallback) {
  auto node = consensus::Node::create();
  NodeLoadInspector inspector{node.get()};

  InvariantAndFallback i;
  auto result = inspector.apply(i);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, i.i);
  EXPECT_EQ("foobar", i.s);
}

TEST_F(NodeLoadInspectorTest, load_object_with_object_invariant) {
  auto node = consensus::Node::create();
  node = node->placeAt("i", 0);
  node = node->placeAt("s", "");
  NodeLoadInspector inspector{node.get()};

  ObjectInvariant o;
  auto result = inspector.apply(o);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Object invariant failed", result.error());
}

TEST_F(NodeLoadInspectorTest, load_object_with_field_transform) {
  auto node = consensus::Node::create();
  node = node->placeAt("x", "42");
  NodeLoadInspector inspector{node};

  FieldTransform f;
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, f.x);
}

TEST_F(NodeLoadInspectorTest, load_object_with_field_transform_and_fallback) {
  auto node = consensus::Node::create();
  node = node->placeAt("x", "42");
  NodeLoadInspector inspector{node.get()};

  FieldTransformWithFallback f;
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, f.x);
  EXPECT_EQ(2, f.y);
}

TEST_F(NodeLoadInspectorTest, load_object_with_optional_field_transform) {
  auto node = consensus::Node::create();
  node = node->placeAt("x", "42");
  NodeLoadInspector inspector{node.get()};

  OptionalFieldTransform f{.x = 1, .y = 2, .z = 3};
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, *f.x);
  EXPECT_FALSE(f.y.has_value());
  EXPECT_EQ(123, *f.z);
}

TEST_F(NodeLoadInspectorTest, load_type_with_custom_specialization) {
  auto node = consensus::Node::create();
  node = node->placeAt("i", 42);
  node = node->placeAt("s", "foobar");
  NodeLoadInspector inspector{node};

  Specialization s;
  auto result = inspector.apply(s);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(42, s.i);
  EXPECT_EQ("foobar", s.s);
}

TEST_F(NodeLoadInspectorTest, load_type_with_explicitly_ignored_fields) {
  auto node = consensus::Node::create();
  node = node->placeAt("s", "foobar");
  node = node->placeAt("ignore", "something");
  NodeLoadInspector inspector{node.get()};

  ExplicitIgnore e;
  auto result = inspector.apply(e);
  ASSERT_TRUE(result.ok());
}

TEST_F(NodeLoadInspectorTest, load_qualified_variant) {
  auto node = consensus::Node::create();
  node = node->placeAt("a", "foobar");

  {
    node = node->placeAt("b/t", "int");
    node = node->placeAt("b/v", 42);
  }

  {
    node = node->placeAt("c/t", "Struct1");
    node = node->placeAt("c/v/v", 1);
  }

  {
    node = node->placeAt("d/t", "Struct2");
    node = node->placeAt("d/v/v", 2);
  }

  {
    node = node->placeAt("e/t", "nil");
    node = node->placeAt("e/v", VPackSlice::emptyObjectSlice());
  }

  NodeLoadInspector inspector{node};

  QualifiedVariant v{.a = {std::monostate{}},
                     .b = {std::monostate{}},
                     .c = {std::monostate{}},
                     .d = {std::monostate{}},
                     .e = {0}};
  auto result = inspector.apply(v);
  ASSERT_TRUE(result.ok()) << result.error() << node->toJson();
  EXPECT_EQ("foobar", std::get<std::string>(v.a));
  EXPECT_EQ(42, std::get<int>(v.b));
  EXPECT_EQ(1, std::get<Struct1>(v.c).v);
  EXPECT_EQ(2, std::get<Struct2>(v.d).v);
  EXPECT_TRUE(std::holds_alternative<std::monostate>(v.e));
}

TEST_F(NodeLoadInspectorTest,
       error_unknown_type_tag_when_loading_qualified_variant) {
  auto node = consensus::Node::create();
  {
    node = node->placeAt("a/t", "blubb");
    node = node->placeAt("a/v", "");
  }
  NodeLoadInspector inspector{node};

  QualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Found invalid type: blubb", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_expecting_string_when_parsing_qualified_variant_value) {
  auto node = consensus::Node::create();
  {
    node = node->placeAt("a/t", "int");
    node = node->placeAt("a/v", "blubb");
  }
  NodeLoadInspector inspector{node};

  QualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Int", result.error());
  EXPECT_EQ("a.v", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_missing_tag_when_parsing_qualified_variant) {
  auto node = consensus::Node::create();
  { node = node->placeAt("a/v", 42); }
  NodeLoadInspector inspector{node};

  QualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Variant type field \"t\" is missing", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_invalid_tag_type_when_parsing_qualified_variant) {
  auto node = consensus::Node::create();
  node = node->placeAt("a/t", 42);
  NodeLoadInspector inspector{node.get()};

  QualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Variant type field \"t\" must be a string", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_missing_value_when_parsing_qualified_variant) {
  auto node = consensus::Node::create();
  node = node->placeAt("a/t", "int");
  NodeLoadInspector inspector{node.get()};

  QualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Variant value field \"v\" is missing", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest, load_unqualified_variant) {
  auto node = consensus::Node::create();
  node = node->placeAt("a/string", "foobar");
  node = node->placeAt("b", 42);
  node = node->placeAt("c/Struct1/v", 1);
  node = node->placeAt("d/Struct2/v", 2);
  node = node->placeAt("e/nil", VPackSlice::emptyObjectSlice());
  NodeLoadInspector inspector{node};

  UnqualifiedVariant v{.a = {std::monostate{}},
                       .b = {std::monostate{}},
                       .c = {std::monostate{}},
                       .d = {std::monostate{}},
                       .e = {0}};
  auto result = inspector.apply(v);
  ASSERT_TRUE(result.ok()) << result.error();
  EXPECT_EQ("foobar", std::get<std::string>(v.a));
  EXPECT_EQ(42, std::get<int>(v.b));
  EXPECT_EQ(1, std::get<Struct1>(v.c).v);
  EXPECT_EQ(2, std::get<Struct2>(v.d).v);
  EXPECT_TRUE(std::holds_alternative<std::monostate>(v.e));
}

TEST_F(NodeLoadInspectorTest,
       error_unknown_type_tag_when_loading_unqualified_variant) {
  auto node = consensus::Node::create();
  node = node->placeAt("a/blubb", "");
  NodeLoadInspector inspector{node};

  UnqualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Found invalid type: blubb", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_expecting_string_when_parsing_unqualified_variant_value) {
  auto node = consensus::Node::create();
  node = node->placeAt("a/string", 42);
  NodeLoadInspector inspector{node};

  UnqualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type String", result.error());
  EXPECT_EQ("a.string", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_missing_data_when_parsing_unqualified_variant) {
  auto node = consensus::Node::create();
  node = node->placeAt("a", VPackSlice::emptyObjectSlice());
  NodeLoadInspector inspector{node};

  UnqualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Missing unqualified variant data", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_when_parsing_unqualified_variant_with_more_than_one_field) {
  auto node = consensus::Node::create();
  node = node->placeAt("a/string", "foobar");
  node = node->placeAt("a/blubb", "blubb");
  NodeLoadInspector inspector{node};

  UnqualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Unqualified variant data has too many fields", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest, load_inline_variant) {
  auto node = consensus::Node::create();
  node = node->placeAt("a", "foobar");
  node = node->placeAt("b/v", 42);

  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue(1));
    builder.add(VPackValue(2));
    builder.add(VPackValue(3));
    builder.close();
    node = node->placeAt("c", builder.slice());
  }

  node = node->placeAt("d", 123);

  {
    velocypack::Builder builder;
    builder.openArray();
    builder.add(VPackValue("blubb"));
    builder.add(VPackValue(987));
    builder.add(VPackValue(true));
    builder.close();
    node = node->placeAt("e", builder.slice());
  }

  NodeLoadInspector inspector{node};

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
  auto node = consensus::Node::create();
  node = node->placeAt("a", VPackSlice::emptyObjectSlice());
  NodeLoadInspector inspector{node};

  InlineVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Could not find matching inline type", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest, load_embedded_variant) {
  auto node = consensus::Node::create();
  {
    node = node->placeAt("a/t", "Struct1");
    node = node->placeAt("a/v", 1);
  }
  {
    node = node->placeAt("b/t", "Struct2");
    node = node->placeAt("b/v", 2);
  }
  {
    node = node->placeAt("c/t", "Struct3");
    node = node->placeAt("c/a", 1);
    node = node->placeAt("c/b", 2);
  }
  node = node->placeAt("d", true);
  NodeLoadInspector inspector{node};

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
  auto node = consensus::Node::create();
  {
    node = node->placeAt("a/t", "blubb");
    node = node->placeAt("a/v", "");
  }
  NodeLoadInspector inspector{node.get()};

  EmbeddedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Found invalid type: blubb", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_expecting_int_when_parsing_embedded_variant_value) {
  auto node = consensus::Node::create();
  {
    node = node->placeAt("a/t", "Struct1");
    node = node->placeAt("a/v", "blubb");
  }
  NodeLoadInspector inspector{node};

  EmbeddedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type Int", result.error());
  EXPECT_EQ("a.v", result.path());
}

TEST_F(NodeLoadInspectorTest, error_missing_tag_when_parsing_embedded_variant) {
  auto node = consensus::Node::create();
  node = node->placeAt("a/v", 42);
  NodeLoadInspector inspector{node};

  EmbeddedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Variant type field \"t\" is missing", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_invalid_tag_type_when_parsing_embedded_variant) {
  auto node = consensus::Node::create();
  node = node->placeAt("a/t", 42);
  NodeLoadInspector inspector{node};

  EmbeddedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Variant type field \"t\" must be a string", result.error());
  EXPECT_EQ("a", result.path());
}

TEST_F(NodeLoadInspectorTest,
       error_missing_value_when_parsing_embedded_variant) {
  auto node = consensus::Node::create();
  node = node->placeAt("a/t", "Struct3");
  node = node->placeAt("a/a", 1);
  NodeLoadInspector inspector{node};

  EmbeddedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Missing required attribute 'b'", result.error());
  EXPECT_EQ("a.b", result.path());
}

TEST_F(NodeLoadInspectorTest, load_type_with_unsafe_fields) {
  auto node = consensus::Node::create();
  node = node->placeAt("view", "foobar");
  node = node->placeAt("slice", "blubb");
  node = node->placeAt("hashed", "hashedString");
  arangodb::inspection::NodeUnsafeLoadInspector<> inspector{node};

  Unsafe u;
  auto result = inspector.apply(u);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(node->get("view")->getStringView().value(), u.view);
  EXPECT_EQ(node->get("view")->getStringView().value().data(), u.view.data());
  EXPECT_EQ(node->get("slice")->slice().start(), u.slice.start());
  EXPECT_EQ(node->get("hashed")->getStringView().value(),
            u.hashed.stringView());
  EXPECT_EQ(node->get("hashed")->getStringView().value().data(),
            u.hashed.data());
}

TEST_F(NodeLoadInspectorTest, load_string_enum) {
  auto node = consensus::Node::create();
  MyStringEnum myEnum;
  {
    node = consensus::Node::create("value1");
    arangodb::inspection::NodeLoadInspector<> inspector{node};
    auto result = inspector.apply(myEnum);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(MyStringEnum::kValue1, myEnum);
  }

  {
    node = consensus::Node::create("value2");
    arangodb::inspection::NodeLoadInspector<> inspector{node};
    auto result = inspector.apply(myEnum);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(MyStringEnum::kValue2, myEnum);
  }
}

TEST_F(NodeLoadInspectorTest, load_int_enum) {
  auto node = consensus::Node::create();
  MyIntEnum myEnum;
  {
    node = consensus::Node::create(1);
    arangodb::inspection::NodeLoadInspector<> inspector{node};
    auto result = inspector.apply(myEnum);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(MyIntEnum::kValue1, myEnum);
  }

  {
    node = consensus::Node::create(2);
    arangodb::inspection::NodeLoadInspector<> inspector{node};
    auto result = inspector.apply(myEnum);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(MyIntEnum::kValue2, myEnum);
  }
}

TEST_F(NodeLoadInspectorTest, load_mixed_enum) {
  auto node = consensus::Node::create();
  MyMixedEnum myEnum;
  {
    node = consensus::Node::create("value1");
    arangodb::inspection::NodeLoadInspector<> inspector{node};
    auto result = inspector.apply(myEnum);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(MyMixedEnum::kValue1, myEnum);
  }
  {
    node = consensus::Node::create(1);
    arangodb::inspection::NodeLoadInspector<> inspector{node};
    auto result = inspector.apply(myEnum);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(MyMixedEnum::kValue1, myEnum);
  }
  {
    node = consensus::Node::create("value2");
    arangodb::inspection::NodeLoadInspector<> inspector{node};
    auto result = inspector.apply(myEnum);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(MyMixedEnum::kValue2, myEnum);
  }
  {
    node = consensus::Node::create(2);
    arangodb::inspection::NodeLoadInspector<> inspector{node};
    auto result = inspector.apply(myEnum);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(MyMixedEnum::kValue2, myEnum);
  }
}

TEST_F(NodeLoadInspectorTest, load_string_enum_returns_error_when_not_string) {
  auto node = consensus::Node::create(42);
  arangodb::inspection::NodeLoadInspector<> inspector{node};

  MyStringEnum myEnum;
  auto result = inspector.apply(myEnum);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ("Expecting type String", result.error());
}

TEST_F(NodeLoadInspectorTest, load_int_enum_returns_error_when_not_int) {
  auto node = consensus::Node::create("foobar");
  arangodb::inspection::NodeLoadInspector<> inspector{node};

  MyIntEnum myEnum;
  auto result = inspector.apply(myEnum);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ("Expecting type UInt", result.error());
}

TEST_F(NodeLoadInspectorTest,
       load_mixed_enum_returns_error_when_not_string_or_int) {
  auto node = consensus::Node::create(false);
  arangodb::inspection::NodeLoadInspector<> inspector{node};

  MyMixedEnum myEnum;
  auto result = inspector.apply(myEnum);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ("Expecting type String or Int", result.error());
}

TEST_F(NodeLoadInspectorTest,
       load_string_enum_returns_error_when_value_is_unknown) {
  auto node = consensus::Node::create("unknownValue");
  arangodb::inspection::NodeLoadInspector<> inspector{node};

  MyStringEnum myEnum;
  auto result = inspector.apply(myEnum);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ("Unknown enum value unknownValue", result.error());
}

TEST_F(NodeLoadInspectorTest,
       load_int_enum_returns_error_when_value_is_unknown) {
  auto node = consensus::Node::create(42);
  arangodb::inspection::NodeLoadInspector<> inspector{node};

  MyIntEnum myEnum;
  auto result = inspector.apply(myEnum);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ("Unknown enum value 42", result.error());
}

TEST_F(NodeLoadInspectorTest,
       load_mixed_enum_returns_error_when_value_is_unknown) {
  {
    auto node = consensus::Node::create("unknownValue");
    arangodb::inspection::NodeLoadInspector<> inspector{node};

    MyMixedEnum myEnum;
    auto result = inspector.apply(myEnum);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ("Unknown enum value unknownValue", result.error());
  }
  {
    auto node = consensus::Node::create(42);
    arangodb::inspection::NodeLoadInspector<> inspector{node};

    MyMixedEnum myEnum;
    auto result = inspector.apply(myEnum);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ("Unknown enum value 42", result.error());
  }
}

TEST_F(NodeLoadInspectorTest, load_embedded_object) {
  auto node = consensus::Node::create();
  node = node->placeAt("a", 1);
  node = node->placeAt("b", 2);
  NodeLoadInspector inspector{node};

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
  auto node = consensus::Node::create();
  node = node->placeAt("a", 1);
  node = node->placeAt("b", 2);
  node = node->placeAt("i", 0);
  NodeLoadInspector inspector{node.get()};

  NestedEmbedding n;
  auto result = inspector.apply(n);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Field invariant failed", result.error());
  EXPECT_EQ("i", result.path());
}

TEST_F(NodeLoadInspectorTest,
       load_embedded_object_with_object_invariant_not_fulfilled) {
  auto node = consensus::Node::create();
  node = node->placeAt("a", 1);
  node = node->placeAt("b", 2);
  node = node->placeAt("i", 42);
  node = node->placeAt("s", "");
  NodeLoadInspector inspector{node};

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

  auto node = consensus::Node::create();

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
