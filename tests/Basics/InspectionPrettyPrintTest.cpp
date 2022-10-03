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

#include "Inspection/Access.h"
#include "Inspection/Format.h"
#include "Inspection/PrettyPrintInspector.h"
#include "velocypack/Builder.h"

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
  int dynamic;
};

template<class Inspector>
auto inspect(Inspector& f, Fallback& x) {
  return f.object(x).fields(
      f.field("i", x.i).fallback(42), f.field("s", x.s).fallback("foobar"),
      f.field("d", x.d).fallback(f.keep()),
      f.field("dynamic", x.dynamic).fallbackFactory([&x]() {
        return x.i * 2;
      }));
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

struct NestedInvariant {
  Invariant i;
  ObjectInvariant o;
};

template<class Inspector>
auto inspect(Inspector& f, NestedInvariant& x) {
  return f.object(x).fields(f.field("i", x.i), f.field("o", x.o));
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

struct AnEmptyObject {};
template<class Inspector>
auto inspect(Inspector& f, AnEmptyObject& x) {
  return f.object(x).fields();
}

}  // namespace

template<>
struct fmt::formatter<Dummy> : arangodb::inspection::inspection_formatter {};

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

struct Struct3 {
  int a;
  int b;
};

template<class Inspector>
auto inspect(Inspector& f, Struct1& x) {
  return f.object(x).fields(f.field("v", x.v));
}

template<class Inspector>
auto inspect(Inspector& f, Struct2& x) {
  return f.object(x).fields(f.field("v", x.v));
}

template<class Inspector>
auto inspect(Inspector& f, Struct3& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
}

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

struct MyEmbeddedVariant : std::variant<Struct1, Struct2, Struct3, bool> {};

struct EmbeddedVariant {
  MyEmbeddedVariant a;
  MyEmbeddedVariant b;
  MyEmbeddedVariant c;
  MyEmbeddedVariant d;
};

template<class Inspector>
auto inspect(Inspector& f, MyEmbeddedVariant& x) {
  namespace insp = arangodb::inspection;
  return f.variant(x).embedded("t").alternatives(
      insp::inlineType<bool>(),         //
      insp::type<Struct1>("Struct1"),   //
      insp::type<Struct2>("Struct2"),   //
      insp::type<Struct3>("Struct3"));  //
}

template<class Inspector>
auto inspect(Inspector& f, EmbeddedVariant& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b),
                            f.field("c", x.c), f.field("d", x.d));
}

struct MyInlineVariant
    : std::variant<std::string, Struct1, std::vector<int>, TypedInt,
                   std::tuple<std::string, int, bool>> {};

struct InlineVariant {
  MyInlineVariant a;
  MyInlineVariant b;
  MyInlineVariant c;
  MyInlineVariant d;
  MyInlineVariant e;
};

template<class Inspector>
auto inspect(Inspector& f, MyInlineVariant& x) {
  namespace insp = arangodb::inspection;
  return f.variant(x).unqualified().alternatives(
      insp::inlineType<std::string>(),                          //
      insp::inlineType<Struct1>(),                              //
      insp::inlineType<std::vector<int>>(),                     //
      insp::inlineType<TypedInt>(),                             //
      insp::inlineType<std::tuple<std::string, int, bool>>());  //
}

template<class Inspector>
auto inspect(Inspector& f, InlineVariant& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b),
                            f.field("c", x.c), f.field("d", x.d),
                            f.field("e", x.e));
}

enum class MyStringEnum {
  kValue1,
  kValue2,
  kValue3 = kValue2,
};

template<class Inspector>
auto inspect(Inspector& f, MyStringEnum& x) {
  return f.enumeration(x).values(MyStringEnum::kValue1, "value1",  //
                                 MyStringEnum::kValue2, "value2");
}

enum class MyIntEnum {
  kValue1,
  kValue2,
  kValue3 = kValue2,
};

template<class Inspector>
auto inspect(Inspector& f, MyIntEnum& x) {
  return f.enumeration(x).values(MyIntEnum::kValue1, 1,  //
                                 MyIntEnum::kValue2, 2);
}

enum class MyMixedEnum {
  kValue1,
  kValue2,
};

template<class Inspector>
auto inspect(Inspector& f, MyMixedEnum& x) {
  return f.enumeration(x).values(MyMixedEnum::kValue1, "value1",  //
                                 MyMixedEnum::kValue1, 1,         //
                                 MyMixedEnum::kValue2, "value2",  //
                                 MyMixedEnum::kValue2, 2);
}

struct Embedded {
  int a;
  InvariantAndFallback inner;
  int b;
};

template<class Inspector>
auto inspect(Inspector& f, Embedded& v) {
  return f.object(v).fields(f.field("a", v.a), f.embedFields(v.inner),
                            f.field("b", v.b));
}

struct NestedEmbedding : Embedded {};

template<class Inspector>
auto inspect(Inspector& f, NestedEmbedding& v) {
  return f.object(v).fields(f.embedFields(static_cast<Embedded&>(v)));
}

struct EmbeddedObjectInvariant {
  int a;
  ObjectInvariant inner;
  int b;
};

template<class Inspector>
auto inspect(Inspector& f, EmbeddedObjectInvariant& v) {
  return f.object(v).fields(f.field("a", v.a), f.embedFields(v.inner),
                            f.field("b", v.b));
}

struct NestedEmbeddingWithObjectInvariant : EmbeddedObjectInvariant {};

template<class Inspector>
auto inspect(Inspector& f, NestedEmbeddingWithObjectInvariant& v) {
  return f.object(v).fields(
      f.embedFields(static_cast<EmbeddedObjectInvariant&>(v)));
}

}  // namespace

namespace {
using namespace arangodb;

struct PrettyPrintInspectorTest : public ::testing::Test {
  std::ostringstream stream;
  inspection::PrettyPrintInspector<> inspector{stream, ""};
};

TEST_F(PrettyPrintInspectorTest, store_empty_object) {
  auto empty = AnEmptyObject{};
  auto result = inspector.apply(empty);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("{\n}", stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_int) {
  int x = 42;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("42", stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_double) {
  double x = 123.456;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("123.456", stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_bool) {
  bool x = true;
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("true", stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_string) {
  std::string x = "foobar";
  auto result = inspector.apply(x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("\"foobar\"", stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_object) {
  Dummy const f{.i = 42, .d = 123.456, .b = true, .s = "foobar"};
  auto result = inspector.apply(f);
  EXPECT_TRUE(result.ok());

  auto expected = R"({
  i: 42,
  d: 123.456,
  b: true,
  s: "foobar"
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_nested_object) {
  Nested b{.dummy = {.i = 42, .d = 123.456, .b = true, .s = "foobar"}};
  auto result = inspector.apply(b);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  dummy: {
    i: 42,
    d: 123.456,
    b: true,
    s: "foobar"
  }
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_nested_object_without_nesting) {
  Container c{.i = {.value = 42}};
  auto result = inspector.apply(c);
  ASSERT_TRUE(result.ok());

  auto expected = "{\n  i: 42\n}";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_list) {
  List l{.vec = {{1}, {2}, {3}}, .list = {4, 5}};
  auto result = inspector.apply(l);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  vec: [
    {
      i: 1
    },
    {
      i: 2
    },
    {
      i: 3
    }
  ],
  list: [
    4,
    5
  ]
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_map) {
  Map m{.map = {{"1", {1}}, {"2", {2}}, {"3", {3}}},
        .unordered = {{"4", 4}, {"5", 5}}};
  auto result = inspector.apply(m);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  map: {
    "1": {
      i: 1
    },
    "2": {
      i: 2
    },
    "3": {
      i: 3
    }
  },
  unordered: {
    "5": 5,
    "4": 4
  }
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_tuples) {
  Tuple t{.tuple = {"foo", 42, 12.34},
          .pair = {987, "bar"},
          .array1 = {"a", "b"},
          .array2 = {1, 2, 3}};
  auto result = inspector.apply(t);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  tuple: [
    "foo",
    42,
    12.34
  ],
  pair: [
    987,
    "bar"
  ],
  array1: [
    "a",
    "b"
  ],
  array2: [
    1,
    2,
    3
  ]
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_optional) {
  Optional o{.a = std::nullopt,
             .b = std::nullopt,
             .x = std::nullopt,
             .y = "blubb",
             .vec = {1, std::nullopt, 3},
             .map = {{"1", 1}, {"2", std::nullopt}, {"3", 3}}};
  auto result = inspector.apply(o);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  a: null,
  b: null,
  y: "blubb",
  vec: [
    1,
    null,
    3
  ],
  map: {
    "1": 1,
    "2": null,
    "3": 3
  }
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_optional_pointer) {
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
  b: 42,
  d: {
    i: 43
  },
  vec: [
    1,
    null,
    2
  ],
  x: null,
  y: null
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_object_with_field_transform) {
  FieldTransform f{.x = 42};
  auto result = inspector.apply(f);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  x: "42"
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_object_with_optional_field_transform) {
  OptionalFieldTransform f{.x = 1, .y = std::nullopt, .z = 3};
  auto result = inspector.apply(f);

  auto expected = R"({
  x: "1",
  z: "3"
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_type_with_custom_specialization) {
  Specialization s{.i = 42, .s = "foobar"};
  auto result = inspector.apply(s);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  i: 42,
  s: "foobar"
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_type_with_explicitly_ignored_fields) {
  ExplicitIgnore e{.s = "foobar"};
  auto result = inspector.apply(e);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  s: "foobar"
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_type_with_unsafe_fields) {
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
  view: "foobar",
  slice: "blubb",
  hashed: "hashedString"
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_qualified_variant) {
  QualifiedVariant d{.a = {"foobar"},
                     .b = {42},
                     .c = {Struct1{1}},
                     .d = {Struct2{2}},
                     .e = {std::monostate{}}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  a: {
    t: "string",
    v: "foobar"
  },
  b: {
    t: "int",
    v: 42
  },
  c: {
    t: "Struct1",
    v: {
      v: 1
    }
  },
  d: {
    t: "Struct2",
    v: {
      v: 2
    }
  },
  e: {
    t: "nil",
    v: {
    }
  }
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_unqualified_variant) {
  UnqualifiedVariant d{.a = {"foobar"},
                       .b = {42},
                       .c = {Struct1{1}},
                       .d = {Struct2{2}},
                       .e = {std::monostate{}}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  a: {
    string: "foobar"
  },
  b: {
    int: 42
  },
  c: {
    Struct1: {
      v: 1
    }
  },
  d: {
    Struct2: {
      v: 2
    }
  },
  e: {
    nil: {
    }
  }
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_string_enum) {
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

TEST_F(PrettyPrintInspectorTest, store_int_enum) {
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

TEST_F(PrettyPrintInspectorTest, store_mixed_enum) {
  std::vector<MyMixedEnum> enums{MyMixedEnum::kValue1, MyMixedEnum::kValue2};
  auto result = inspector.apply(enums);
  ASSERT_TRUE(result.ok());

  auto expected = R"([
  "value1",
  "value2"
])";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest,
       store_string_enum_returns_error_for_unknown_value) {
  MyStringEnum val = static_cast<MyStringEnum>(42);
  auto result = inspector.apply(val);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Unknown enum value 42", result.error());
}

TEST_F(PrettyPrintInspectorTest,
       store_int_enum_returns_error_for_unknown_value) {
  MyIntEnum val = static_cast<MyIntEnum>(42);
  auto result = inspector.apply(val);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Unknown enum value 42", result.error());
}

TEST_F(PrettyPrintInspectorTest,
       store_mixed_enum_returns_error_for_unknown_value) {
  MyMixedEnum val = static_cast<MyMixedEnum>(42);
  auto result = inspector.apply(val);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ("Unknown enum value 42", result.error());
}

TEST_F(PrettyPrintInspectorTest, store_inline_variant) {
  InlineVariant d{.a = {"foobar"},
                  .b = {Struct1{.v = 42}},
                  .c = {std::vector<int>{1, 2, 3}},
                  .d = {TypedInt{.value = 123}},
                  .e = {std::tuple{"blubb", 987, true}}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  a: "foobar",
  b: {
    v: 42
  },
  c: [
    1,
    2,
    3
  ],
  d: 123,
  e: [
    "blubb",
    987,
    true
  ]
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_embedded_variant) {
  EmbeddedVariant d{.a = {Struct1{1}},
                    .b = {Struct2{2}},
                    .c = {Struct3{.a = 1, .b = 2}},
                    .d = {true}};
  auto result = inspector.apply(d);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  a: {
    t: "Struct1",
    v: 1
  },
  b: {
    t: "Struct2",
    v: 2
  },
  c: {
    t: "Struct3",
    a: 1,
    b: 2
  },
  d: true
})";
  EXPECT_EQ(expected, stream.str());
}

TEST_F(PrettyPrintInspectorTest, store_embedded_fields) {
  NestedEmbedding const n{
      Embedded{.a = 1, .inner = {.i = 42, .s = "foobar"}, .b = 2}};
  auto result = inspector.apply(n);
  ASSERT_TRUE(result.ok());

  auto expected = R"({
  a: 1,
  i: 42,
  s: "foobar",
  b: 2
})";
  EXPECT_EQ(expected, stream.str());
}

}  // namespace
