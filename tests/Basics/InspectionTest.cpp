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

#include <iostream>
#include <list>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

#include <velocypack/Value.h>
#include <velocypack/ValueType.h>
#include <velocypack/velocypack-memory.h>

#include "Inspection/Access.h"
#include "Inspection/VPackLoadInspector.h"
#include "Inspection/VPackSaveInspector.h"
#include "Inspection/VPack.h"
#include "velocypack/Builder.h"

#include "Logger/LogMacros.h"

namespace {

struct Dummy {
  int i;
  double d;
  bool b;
  std::string s;
  bool operator==(Dummy const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, Dummy& x) {
  return f.object(x).fields(f.field("i", x.i), f.field("d", x.d),
                            f.field("b", x.b), f.field("s", x.s));
}

struct Nested {
  Dummy dummy;
};

template<class Inspector>
auto inspect(Inspector& f, Nested& x) {
  return f.object(x).fields(f.field("dummy", x.dummy));
}

struct TypedInt {
  int value;
  int getValue() { return value; }
  bool operator==(TypedInt const& r) const { return value == r.value; };
};

struct Container {
  TypedInt i{.value = 0};
  bool operator==(Container const& r) const { return i == r.i; };
};

template<class Inspector>
auto inspect(Inspector& f, TypedInt& x) {
  if constexpr (Inspector::isLoading) {
    int v;
    auto res = f.apply(v);
    if (res.ok()) {
      x = TypedInt{.value = v};
    }
    return res;
  } else {
    return f.apply(x.getValue());
  }
}

template<class Inspector>
auto inspect(Inspector& f, Container& x) {
  return f.object(x).fields(f.field("i", x.i));
}
struct List {
  std::vector<Container> vec;
  std::list<int> list;
};

template<class Inspector>
auto inspect(Inspector& f, List& x) {
  return f.object(x).fields(f.field("vec", x.vec), f.field("list", x.list));
}

struct Map {
  std::map<std::string, Container> map;
  std::unordered_map<std::string, int> unordered;
};

template<class Inspector>
auto inspect(Inspector& f, Map& x) {
  return f.object(x).fields(f.field("map", x.map),
                            f.field("unordered", x.unordered));
}

struct Tuple {
  std::tuple<std::string, int, double> tuple;
  std::pair<int, std::string> pair;
  std::string array1[2];
  std::array<int, 3> array2;
};

template<class Inspector>
auto inspect(Inspector& f, Tuple& x) {
  return f.object(x).fields(f.field("tuple", x.tuple), f.field("pair", x.pair),
                            f.field("array1", x.array1),
                            f.field("array2", x.array2));
}

struct Optional {
  std::optional<int> a;
  std::optional<int> b;
  std::optional<int> x;
  std::optional<std::string> y;
  std::vector<std::optional<int>> vec;
  std::map<std::string, std::optional<int>> map;
};

template<class Inspector>
auto inspect(Inspector& f, Optional& x) {
  return f.object(x).fields(f.field("a", x.a).fallback(123),
                            f.field("b", x.b).fallback(456), f.field("x", x.x),
                            f.field("y", x.y), f.field("vec", x.vec),
                            f.field("map", x.map));
}

struct Pointer {
  std::shared_ptr<int> a;
  std::shared_ptr<int> b;
  std::unique_ptr<int> c;
  std::unique_ptr<Container> d;
  std::vector<std::unique_ptr<int>> vec;
  std::shared_ptr<int> x;
  std::shared_ptr<int> y;
};

template<class Inspector>
auto inspect(Inspector& f, Pointer& x) {
  return f.object(x).fields(
      f.field("a", x.a), f.field("b", x.b), f.field("c", x.c),
      f.field("d", x.d), f.field("vec", x.vec),
      f.field("x", x.x).fallback(std::make_shared<int>(123)),
      f.field("y", x.y).fallback(std::make_shared<int>(456)));
}

struct Fallback {
  int i;
  std::string s;
  Dummy d = {.i = 1, .d = 4.2, .b = true, .s = "2"};
};

template<class Inspector>
auto inspect(Inspector& f, Fallback& x) {
  return f.object(x).fields(f.field("i", x.i).fallback(42),
                            f.field("s", x.s).fallback("foobar"),
                            f.field("d", x.d).fallback(f.keep()));
}

struct Invariant {
  int i;
  std::string s;
};

template<class Inspector>
auto inspect(Inspector& f, Invariant& x) {
  return f.object(x).fields(
      f.field("i", x.i).invariant([](int v) { return v != 0; }),
      f.field("s", x.s).invariant(
          [](std::string const& v) { return !v.empty(); }));
}

struct InvariantWithResult {
  int i;
  std::string s;
};

template<class Inspector>
auto inspect(Inspector& f, InvariantWithResult& x) {
  return f.object(x).fields(
      f.field("i", x.i).invariant([](int v) -> arangodb::inspection::Status {
        if (v == 0) {
          return {"Must not be zero"};
        }
        return {};
      }));
}

struct InvariantAndFallback {
  int i;
  std::string s;
};

template<class Inspector>
auto inspect(Inspector& f, InvariantAndFallback& x) {
  return f.object(x).fields(
      f.field("i", x.i).fallback(42).invariant([](int v) { return v != 0; }),
      f.field("s", x.s)
          .invariant([](std::string const& v) { return !v.empty(); })
          .fallback("foobar"));
}

struct ObjectInvariant {
  int i;
  std::string s;
};

template<class Inspector>
auto inspect(Inspector& f, ObjectInvariant& x) {
  return f.object(x)
      .fields(f.field("i", x.i), f.field("s", x.s))
      .invariant([](ObjectInvariant& o) { return o.i != 0 && !o.s.empty(); });
}

struct FallbackReference {
  int x;
  int y;
};

template<class Inspector>
auto inspect(Inspector& f, FallbackReference& x) {
  return f.object(x).fields(f.field("x", x.x),
                            f.field("y", x.y).fallback(std::ref(x.x)));
}

struct MyTransformer {
  using MemoryType = int;
  using SerializedType = std::string;

  arangodb::inspection::Status toSerialized(MemoryType v,
                                            SerializedType& result) const {
    result = std::to_string(v);
    return {};
  }
  arangodb::inspection::Status fromSerialized(SerializedType const& v,
                                              MemoryType& result) const {
    result = std::stoi(v);
    return {};
  }
};

struct FieldTransform {
  int x;
};

template<class Inspector>
auto inspect(Inspector& f, FieldTransform& x) {
  return f.object(x).fields(f.field("x", x.x).transformWith(MyTransformer{}));
}

struct FieldTransformWithFallback {
  int x;
  int y;
};

template<class Inspector>
auto inspect(Inspector& f, FieldTransformWithFallback& x) {
  return f.object(x).fields(
      f.field("x", x.x).fallback(1).transformWith(MyTransformer{}),
      f.field("y", x.y).transformWith(MyTransformer{}).fallback(2));
}

struct OptionalFieldTransform {
  std::optional<int> x;
  std::optional<int> y;
  std::optional<int> z;
};

template<class Inspector>
auto inspect(Inspector& f, OptionalFieldTransform& x) {
  return f.object(x).fields(
      f.field("x", x.x).transformWith(MyTransformer{}),
      f.field("y", x.y).transformWith(MyTransformer{}),
      f.field("z", x.z).transformWith(MyTransformer{}).fallback(123));
}

struct Specialization {
  int i;
  std::string s;
};

enum class AnEnumClass { Option1, Option2, Option3 };

auto to_string(AnEnumClass e) -> std::string_view {
  switch (e) {
    case AnEnumClass::Option1:
      return "Option1";
    case AnEnumClass::Option2:
      return "Option2";
    case AnEnumClass::Option3:
      return "Option3";
  }
  return "invalid.";
}

template<typename Enum>
struct EnumStorage {
  using MemoryType = Enum;

  std::underlying_type_t<Enum> code;
  std::string message;

  explicit EnumStorage(Enum e)
      : code(static_cast<std::underlying_type_t<Enum>>(e)),
        message(to_string(e)){};
  explicit EnumStorage() {}

  operator Enum() const { return Enum(code); }
};

template<class Inspector, class Enum>
auto inspect(Inspector& f, EnumStorage<Enum>& e) {
  if constexpr (Inspector::isLoading) {
    return f.object(e).fields(f.field("code", e.code),
                              f.ignoreField("message"));
  } else {
    return f.object(e).fields(f.field("code", e.code),
                              f.field("message", e.message));
  }
}
}  // namespace

namespace arangodb::inspection {
template<>
struct Access<Specialization> : AccessBase<Specialization> {
  template<class Inspector>
  [[nodiscard]] static Status apply(Inspector& f, Specialization& x) {
    return f.object(x).fields(f.field("i", x.i), f.field("s", x.s));
  }
};
template<>
struct Access<AnEnumClass>
    : StorageTransformerAccess<AnEnumClass, EnumStorage<AnEnumClass>> {};

}  // namespace arangodb::inspection

namespace {

struct ExplicitIgnore {
  std::string s;
};

template<class Inspector>
auto inspect(Inspector& f, ExplicitIgnore& x) {
  return f.object(x).fields(f.field("s", x.s), f.ignoreField("ignore"));
}

struct Unsafe {
  std::string_view view;
  arangodb::velocypack::Slice slice;
  arangodb::velocypack::HashedStringRef hashed;
};

template<class Inspector>
auto inspect(Inspector& f, Unsafe& x) {
  return f.object(x).fields(f.field("view", x.view), f.field("slice", x.slice),
                            f.field("hashed", x.hashed));
}

struct Struct1 {
  int v;
};
struct Struct2 {
  int v;
};

struct MyQualifiedVariant
    : std::variant<std::string, int, Struct1, Struct2, std::monostate> {};

struct QualifiedVariant {
  MyQualifiedVariant a;
  MyQualifiedVariant b;
  MyQualifiedVariant c;
  MyQualifiedVariant d;
  MyQualifiedVariant e;
};

template<class Inspector>
auto inspect(Inspector& f, MyQualifiedVariant& x) {
  namespace insp = arangodb::inspection;
  return f.variant(x).qualified("t", "v").alternatives(
      insp::type<std::string>("string"),   //
      insp::type<int>("int"),              //
      insp::type<Struct1>("Struct1"),      //
      insp::type<Struct2>("Struct2"),      //
      insp::type<std::monostate>("nil"));  //
}

template<class Inspector>
auto inspect(Inspector& f, QualifiedVariant& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b),
                            f.field("c", x.c), f.field("d", x.d),
                            f.field("e", x.e));
}

struct MyUnqualifiedVariant
    : std::variant<std::string, int, Struct1, Struct2, std::monostate> {};

struct UnqualifiedVariant {
  MyUnqualifiedVariant a;
  MyUnqualifiedVariant b;
  MyUnqualifiedVariant c;
  MyUnqualifiedVariant d;
  MyUnqualifiedVariant e;
};

template<class Inspector>
auto inspect(Inspector& f, MyUnqualifiedVariant& x) {
  namespace insp = arangodb::inspection;
  return f.variant(x).unqualified().alternatives(
      insp::type<std::string>("string"),   //
      insp::type<int>("int"),              //
      insp::type<Struct1>("Struct1"),      //
      insp::type<Struct2>("Struct2"),      //
      insp::type<std::monostate>("nil"));  //
}

template<class Inspector>
auto inspect(Inspector& f, UnqualifiedVariant& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b),
                            f.field("c", x.c), f.field("d", x.d),
                            f.field("e", x.e));
}

template<class Inspector>
auto inspect(Inspector& f, Struct1& x) {
  return f.object(x).fields(f.field("v", x.v));
}

template<class Inspector>
auto inspect(Inspector& f, Struct2& x) {
  return f.object(x).fields(f.field("v", x.v));
}
}  // namespace

namespace {
using namespace arangodb;
using VPackLoadInspector = inspection::VPackLoadInspector;
using VPackSaveInspector = inspection::VPackSaveInspector;

struct VPackSaveInspectorTest : public ::testing::Test {
  velocypack::Builder builder;
  VPackSaveInspector inspector{builder};
};

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
  ASSERT_EQ(3, list.length());
  EXPECT_EQ(l.vec[0].i.value, list[0]["i"].getInt());
  EXPECT_EQ(l.vec[1].i.value, list[1]["i"].getInt());
  EXPECT_EQ(l.vec[2].i.value, list[2]["i"].getInt());

  list = slice["list"];
  ASSERT_TRUE(list.isArray());
  ASSERT_EQ(2, list.length());
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
  ASSERT_EQ(3, obj.length());
  EXPECT_EQ(m.map["1"].i.value, obj["1"]["i"].getInt());
  EXPECT_EQ(m.map["2"].i.value, obj["2"]["i"].getInt());
  EXPECT_EQ(m.map["3"].i.value, obj["3"]["i"].getInt());

  obj = slice["unordered"];
  ASSERT_TRUE(obj.isObject());
  ASSERT_EQ(2, obj.length());
  EXPECT_EQ(m.unordered["4"], obj["4"].getInt());
  EXPECT_EQ(m.unordered["5"], obj["5"].getInt());
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
  ASSERT_EQ(3, list.length());
  EXPECT_EQ(std::get<0>(t.tuple), list[0].copyString());
  EXPECT_EQ(std::get<1>(t.tuple), list[1].getInt());
  EXPECT_EQ(std::get<2>(t.tuple), list[2].getDouble());

  list = slice["pair"];
  ASSERT_EQ(2, list.length());
  EXPECT_EQ(std::get<0>(t.pair), list[0].getInt());
  EXPECT_EQ(std::get<1>(t.pair), list[1].copyString());

  list = slice["array1"];
  ASSERT_EQ(2, list.length());
  EXPECT_EQ(t.array1[0], list[0].copyString());
  EXPECT_EQ(t.array1[1], list[1].copyString());

  list = slice["array2"];
  ASSERT_EQ(3, list.length());
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
  EXPECT_EQ(5, slice.length());
  // a and b have fallbacks, so we need to serialize them explicitly as null
  EXPECT_TRUE(slice["a"].isNull());
  EXPECT_TRUE(slice["b"].isNull());
  EXPECT_EQ("blubb", slice["y"].copyString());

  auto vec = slice["vec"];
  ASSERT_TRUE(vec.isArray());
  ASSERT_EQ(3, vec.length());
  EXPECT_EQ(1, vec[0].getInt());
  EXPECT_TRUE(vec[1].isNull());
  EXPECT_EQ(3, vec[2].getInt());

  auto map = slice["map"];
  ASSERT_TRUE(map.isObject());
  ASSERT_EQ(3, map.length());
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
  EXPECT_EQ(5, slice.length());
  EXPECT_EQ(42, slice["b"].getInt());
  EXPECT_EQ(43, slice["d"]["i"].getInt());
  auto vec = slice["vec"];
  EXPECT_TRUE(vec.isArray());
  EXPECT_EQ(3, vec.length());
  EXPECT_EQ(1, vec[0].getInt());
  EXPECT_TRUE(vec[1].isNull());
  EXPECT_EQ(2, vec[2].getInt());
  // x and y have fallbacks, so we need to serialize them explicitly as null
  EXPECT_TRUE(slice["x"].isNull());
  EXPECT_TRUE(slice["y"].isNull());
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
  EXPECT_EQ(2, slice.length());
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
  EXPECT_EQ(1, slice.length());
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
  EXPECT_EQ("string", slice["a"]["t"].stringView());
  EXPECT_EQ("foobar", slice["a"]["v"].stringView());

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
  EXPECT_EQ(1, slice["a"].length());
  EXPECT_EQ("foobar", slice["a"]["string"].stringView());

  EXPECT_EQ(1, slice["b"].length());
  EXPECT_EQ(42, slice["b"]["int"].getInt());

  EXPECT_EQ(1, slice["c"].length());
  EXPECT_EQ(1, slice["c"]["Struct1"]["v"].getInt());

  EXPECT_EQ(1, slice["d"].length());
  EXPECT_EQ(2, slice["d"]["Struct2"]["v"].getInt());

  EXPECT_EQ(1, slice["e"].length());
  EXPECT_TRUE(slice["e"]["nil"].isEmptyObject());
}

struct VPackLoadInspectorTest : public ::testing::Test {
  velocypack::Builder builder;
};

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

TEST_F(VPackLoadInspectorTest, store_string) {
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

  EXPECT_EQ(3, l.vec.size());
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

  ASSERT_EQ(3, p.vec.size());
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
  builder.add(VPackValue("a"));
  builder.openObject();
  builder.add("t", "string");
  builder.add("v", "foobar");
  builder.close();

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
  builder.add("t", "string");
  builder.add("v", 42);
  builder.close();
  builder.close();
  VPackLoadInspector inspector{builder};

  QualifiedVariant v;
  auto result = inspector.apply(v);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Expecting type String", result.error());
  EXPECT_EQ("a.value", result.path());
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

  builder.add(VPackValue("b"));
  builder.openObject();
  builder.add("int", 42);
  builder.close();

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
  EXPECT_EQ("a.value", result.path());
}

TEST_F(VPackLoadInspectorTest,
       error_missing_data_when_parsing_qualified_variant) {
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

TEST_F(VPackLoadInspectorTest, load_type_with_unsafe_fields) {
  builder.openObject();
  builder.add("view", VPackValue("foobar"));
  builder.add("slice", VPackValue("blubb"));
  builder.add("hashed", VPackValue("hashedString"));
  builder.close();
  arangodb::inspection::VPackUnsafeLoadInspector inspector{builder};

  Unsafe u;
  auto result = inspector.apply(u);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(builder.slice()["view"].stringView(), u.view);
  EXPECT_EQ(builder.slice()["view"].stringView().data(), u.view.data());
  EXPECT_EQ(builder.slice()["slice"].start(), u.slice.start());
  EXPECT_EQ(builder.slice()["hashed"].stringView(), u.hashed.stringView());
  EXPECT_EQ(builder.slice()["hashed"].stringView().data(), u.hashed.data());
}

struct VPackInspectionTest : public ::testing::Test {};

TEST_F(VPackInspectionTest, serialize) {
  velocypack::Builder builder;
  Dummy const d{.i = 42, .d = 123.456, .b = true, .s = "foobar"};
  arangodb::velocypack::serialize(builder, d);

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());
  EXPECT_EQ(d.i, slice["i"].getInt());
  EXPECT_EQ(d.d, slice["d"].getDouble());
  EXPECT_EQ(d.b, slice["b"].getBool());
  EXPECT_EQ(d.s, slice["s"].copyString());
}

TEST_F(VPackInspectionTest, deserialize) {
  velocypack::Builder builder;
  builder.openObject();
  builder.add("i", VPackValue(42));
  builder.add("d", VPackValue(123.456));
  builder.add("b", VPackValue(true));
  builder.add("s", VPackValue("foobar"));
  builder.close();

  Dummy d = arangodb::velocypack::deserialize<Dummy>(builder.slice());
  EXPECT_EQ(42, d.i);
  EXPECT_EQ(123.456, d.d);
  EXPECT_EQ(true, d.b);
  EXPECT_EQ("foobar", d.s);
}

TEST_F(VPackInspectionTest, deserialize_throws) {
  velocypack::Builder builder;
  builder.openObject();
  builder.close();

  EXPECT_THROW(
      {
        try {
          arangodb::velocypack::deserialize<Dummy>(builder.slice());
        } catch (arangodb::basics::Exception& e) {
          EXPECT_TRUE(std::string(e.what()).starts_with(
              "Error while parsing VelocyPack: Missing required attribute"))
              << "Actual error message: " << e.what();
          throw;
        }
      },
      arangodb::basics::Exception);
}

TEST_F(VPackInspectionTest, GenericEnumClass) {
  {
    velocypack::Builder builder;

    AnEnumClass const d = AnEnumClass::Option1;
    arangodb::velocypack::serialize(builder, d);

    velocypack::Slice slice = builder.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_EQ(static_cast<std::underlying_type_t<AnEnumClass>>(d),
              slice["code"].getInt());
    EXPECT_EQ(to_string(d), slice["message"].copyString());
  }

  {
    auto const expected = AnEnumClass::Option3;
    velocypack::Builder builder;

    builder.openObject();
    builder.add(
        "code",
        VPackValue(static_cast<std::underlying_type_t<AnEnumClass>>(expected)));
    builder.add("message", VPackValue(to_string(expected)));
    builder.close();

    auto d = arangodb::velocypack::deserialize<AnEnumClass>(builder.slice());

    EXPECT_EQ(d, expected);
  }
}

}  // namespace
