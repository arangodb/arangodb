////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "test-types.h"

#include "VPackDeserializer/deserializer.h"

#include "velocypack/Buffer.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "velocypack/Slice.h"

namespace arangodb {
namespace tests {
namespace deserializer {

using namespace arangodb::velocypack;
using namespace arangodb::velocypack::deserializer;

class VPackDeserializerBasicTest : public ::testing::Test {
 protected:
  VPackDeserializerBasicTest() {}
  ~VPackDeserializerBasicTest() {}
};

using VPackBufferPtr = std::shared_ptr<Buffer<uint8_t>>;

static inline Buffer<uint8_t> vpackFromJsonString(char const* c) {
  Options options;
  options.checkAttributeUniqueness = true;
  Parser parser(&options);
  parser.parse(c);

  std::shared_ptr<Builder> builder = parser.steal();
  std::shared_ptr<Buffer<uint8_t>> buffer = builder->steal();
  Buffer<uint8_t> b = std::move(*buffer);
  return b;
}

static inline Buffer<uint8_t> operator"" _vpack(const char* json, size_t) {
  return vpackFromJsonString(json);
}

template <typename T>
using my_vector = std::vector<T>;
template <typename K, typename V>
using my_map = std::unordered_map<K, V>;

TEST_F(VPackDeserializerBasicTest, test01) {
  auto buffer = R"=(["hello", true, 123.4])="_vpack;
  auto slice = recording_slice::from_buffer(buffer);

  using deserial =
      tuple_deserializer<values::value_deserializer<std::string>,
                         values::value_deserializer<bool>, values::value_deserializer<double>>;

  auto result = deserialize<deserial>(slice.slice);
  static_assert(
      std::is_same_v<decltype(result)::value_type, std::tuple<std::string, bool, double>>);
  ASSERT_TRUE(result.ok()) << result.error().as_string();
}

TEST_F(VPackDeserializerBasicTest, test02) {
  auto buffer = R"=([{"op":"bar"}, {"op":"foo"}])="_vpack;
  auto slice = recording_slice::from_buffer(buffer);

  constexpr static const char op_name[] = "op";
  constexpr static const char bar_name[] = "bar";
  constexpr static const char foo_name[] = "foo";

  using op_deserial =
      attribute_deserializer<op_name, values::value_deserializer<std::string>>;

  using prec_deserial_pair =
      value_deserializer_pair<values::string_value<bar_name>, op_deserial>;
  using prec_deserial_pair_foo =
      value_deserializer_pair<values::string_value<foo_name>, op_deserial>;

  using deserial =
      array_deserializer<field_value_dependent_deserializer<op_name, prec_deserial_pair, prec_deserial_pair_foo>, my_vector>;

  auto result = deserialize<deserial>(slice.slice);

  static_assert(
      std::is_same_v<decltype(result)::value_type, std::vector<std::variant<std::string, std::string>>>);
  ASSERT_TRUE(result.ok()) << result.error().as_string();
}

TEST_F(VPackDeserializerBasicTest, test03) {
  struct deserialized_type {
    my_map<std::string, std::variant<std::unique_ptr<deserialized_type>, std::string>> value;
  };

  struct recursive_deserializer {
    using plan =
        map_deserializer<conditional_deserializer<condition_deserializer_pair<is_object_condition, unpack_proxy<recursive_deserializer, deserialized_type>>,
                                                  conditional_default<values::value_deserializer<std::string>>>,
                         my_map>;
    using factory = utilities::constructor_factory<deserialized_type>;
    using constructed_type = deserialized_type;
  };

  auto buffer = R"=({"a":"b", "c":{"d":{"e":"false"}}})="_vpack;
  auto slice = recording_slice::from_buffer(buffer);

  auto result = deserialize<recursive_deserializer>(slice.slice);

  ASSERT_TRUE(result.ok()) << result.error().as_string();
}

TEST_F(VPackDeserializerBasicTest, test04) {
  struct non_default_constructible_type {
    explicit non_default_constructible_type(double){};
  };

  struct non_copyable_type {
    explicit non_copyable_type(double) {}
    non_copyable_type(non_copyable_type const&) = delete;
    non_copyable_type(non_copyable_type&&) noexcept = default;
  };

  using deserial =
      tuple_deserializer<utilities::constructing_deserializer<non_default_constructible_type, values::value_deserializer<double>>,
                         utilities::constructing_deserializer<non_copyable_type, values::value_deserializer<double>>>;

  auto buffer = R"=([12, 11])="_vpack;
  auto slice = recording_slice::from_buffer(buffer);

  auto result = deserialize<deserial>(slice.slice);

  ASSERT_TRUE(result.ok()) << result.error().as_string();
}

/* clang-format off */

struct graph_options {
  std::optional<std::string_view> smartGraphAttribute;
  uint32_t numberOfShards; // 1
  uint32_t replicationFactor; // 1
  uint32_t minReplicationFactor; // default = 1
};

constexpr const char str_smart_graph_attribute[] = "smartGraphAttribute";
constexpr const char str_number_of_shards[] = "numberOfShards";
constexpr const char str_replication_factor[] = "replicationFactor";
constexpr const char str_min_replication_factor[] = "minReplicationFactor";

/* clang-format on */

struct graph_options_validator {
  struct context_type {
    context_type(context_type const&) = delete;

    uint32_t maxNumberOfShards;
    uint32_t maxReplicationFactor;
  };

  context_type const& ctx;

  auto operator()(graph_options const& t) -> std::optional<deserialize_error> {
    if (t.smartGraphAttribute && t.smartGraphAttribute->empty()) {
      return deserialize_error{"smart graph attribute must not be empty"};
    }
    if (ctx.maxNumberOfShards < t.numberOfShards) {
      return deserialize_error{"maximum number of shards exceeded"};
    }
    if (ctx.maxReplicationFactor < t.replicationFactor) {
      return deserialize_error{"maximum replication factor exceeded"};
    }

    return {};
  }
};

/* clang-format off */

using graph_options_deserializer = utilities::constructing_deserializer<graph_options, parameter_list<
    factory_optional_parameter<str_smart_graph_attribute, std::string_view>,
    factory_simple_parameter<str_number_of_shards, uint32_t, false, values::numeric_value<uint32_t, 1>>,
    factory_simple_parameter<str_replication_factor, uint32_t, false, values::numeric_value<uint32_t, 1>>,
    factory_simple_parameter<str_min_replication_factor, uint32_t, false, values::numeric_value<uint32_t, 1>>
>>;

using graph_options_validating_deserializer = validate<graph_options_deserializer, graph_options_validator>;

struct graph_edge_definition {
  std::string_view collection;
  std::vector<std::string_view> from;
  std::vector<std::string_view> to;
};

constexpr const char str_collection[] = "collection";
constexpr const char str_from[] = "from";
constexpr const char str_to[] = "to";

template<typename D, template <typename> typename C>
using non_empty_array_deserializer = validate<
    array_deserializer<D, C>,
    utilities::not_empty_validator>;

using non_empty_string_view_array_deserializer = non_empty_array_deserializer<
    values::value_deserializer<std::string_view>, my_vector>;

template<typename S>
using non_empty_string_container = validate<
    values::value_deserializer<S>,
    utilities::not_empty_validator>;

using non_empty_string_view = non_empty_string_container<std::string_view>;

using graph_edge_definition_deserializer = utilities::constructing_deserializer<graph_edge_definition, parameter_list<
    factory_deserialized_parameter<str_collection, non_empty_string_view, true>,
    factory_deserialized_parameter<str_from, non_empty_string_view_array_deserializer, true>,
    factory_deserialized_parameter<str_to, non_empty_string_view_array_deserializer, true>
>>;

using graph_edge_definition_list = std::vector<graph_edge_definition>;
using graph_edge_definition_list_deserializer = non_empty_array_deserializer<graph_edge_definition_deserializer, my_vector>;

struct graph_definition {
  std::string_view name;
  bool is_smart;
  graph_edge_definition_list edgeDefinitions;
  std::optional<graph_options> options;
};

constexpr const char str_name[] = "name";
constexpr const char str_is_smart[] = "isSmart";
constexpr const char str_edge_definitions[] = "edgeDefinitions";
constexpr const char str_options[] = "options";

using graph_definition_deserializer = utilities::constructing_deserializer<graph_definition, parameter_list<
    factory_deserialized_parameter<str_name, non_empty_string_view, true>,
    factory_simple_parameter<str_is_smart, bool, false, values::numeric_value<bool, false>>,
    factory_deserialized_parameter<str_edge_definitions, graph_edge_definition_list_deserializer, true>,
    factory_deserialized_parameter<str_options, graph_options_validating_deserializer, false>
>>;

/* clang-format on */

TEST_F(VPackDeserializerBasicTest, test05) {
  auto buffer = R"=({"name":"myGraph","edgeDefinitions":[{"collection":"edges","from":["startVertices"],"to":["endVertices"]},{"collection":"edges","from":[],"to":["bla"]}],"options":{"replicationFactor":2,"minReplicationFactor":2}})="_vpack;
  auto slice = recording_slice::from_buffer(buffer);

  graph_options_validator::context_type ctx = {2, 3};

  auto result = deserialize_with_context<graph_definition_deserializer>(slice.slice, ctx);

  ASSERT_FALSE(result.ok());
}

enum class MyEnum { MIN, MAX, SUM };

constexpr const char MyEnum_min[] = "min";
constexpr const char MyEnum_max[] = "max";
constexpr const char MyEnum_sum[] = "sum";

using MyEnum_deserializer =
    enum_deserializer<MyEnum, enum_member<MyEnum::MIN, values::string_value<MyEnum_min>>,
                      enum_member<MyEnum::MAX, values::string_value<MyEnum_max>>,
                      enum_member<MyEnum::MAX, values::string_value<MyEnum_sum>>,
                      enum_member<MyEnum::SUM, values::numeric_value<int, 12>>>;

TEST_F(VPackDeserializerBasicTest, test06) {
  auto buffer = R"=("mox")="_vpack;
  auto slice = recording_slice::from_buffer(buffer);

  auto result = deserialize<MyEnum_deserializer>(slice.slice);

  ASSERT_FALSE(result.ok());
}

constexpr const char field1_name[] = "field1";
constexpr const char field2_name[] = "field1";

TEST_F(VPackDeserializerBasicTest, test_ignore_unknown_hint) {
  {
    struct TestStruct {
      int field1;
      int field2;
    };
    using TestDeserializer = utilities::constructing_deserializer<
        TestStruct, parameter_list<factory_simple_parameter<field1_name, int, true, values::numeric_value<int, 0>>,
                                   factory_simple_parameter<field2_name, int, false, values::numeric_value<int, 0>>>>;

    {
      auto vPackWithUnknown = arangodb::velocypack::Parser::fromJson(
          "{\"unknown\":true, \"field1\":1, \"field2\":2}");
      {
        auto const res = deserialize<TestDeserializer>(vPackWithUnknown->slice());
        ASSERT_FALSE(res.ok());
      }
      {
        auto const res =
            deserialize<TestDeserializer, hints::hint_list<hints::ignore_unknown>>(
                vPackWithUnknown->slice());
        ASSERT_TRUE(res.ok());
      }
    }
    // missing of mandatory parameter should still fail!
    {
      auto vPackWithUnknown = arangodb::velocypack::Parser::fromJson(
          "{\"unknown\":true, \"field2\":2}");
      auto const res =
          deserialize<TestDeserializer, hints::hint_list<hints::ignore_unknown>>(
              vPackWithUnknown->slice());
      ASSERT_FALSE(res.ok());
    }
    // missing of optionsl parameter should be ok
    {
      auto vPackWithUnknown = arangodb::velocypack::Parser::fromJson(
          "{\"unknown\":true, \"field1\":2}");
      auto const res =
          deserialize<TestDeserializer, hints::hint_list<hints::ignore_unknown>>(
              vPackWithUnknown->slice());
      ASSERT_TRUE(res.ok());
    }

  }
}

static_assert(GTEST_HAS_TYPED_TEST, "We need typed tests for the following:");

template <typename T>
class VPackDeserializerArithmeticTest : public ::testing::Test {
  struct value {
    T v;
  };

 public:
  void checkWorks(T v) {
    Builder b;
    b.add(Value(v));

    auto result = deserialize<values::value_deserializer<T>>(b.slice());

    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.get(), v);
  }

  void checkDoesNotWork() {
    Builder b;
    b.add(Value("BANANAS"));

    auto result = deserialize<values::value_deserializer<T>>(b.slice());
    ASSERT_TRUE(result.error());
  }

 protected:
  VPackDeserializerArithmeticTest() {}
  ~VPackDeserializerArithmeticTest() {}
};

using TypesToTest = ::testing::Types<size_t, uint8_t, uint16_t, uint32_t, uint64_t,
                                     int8_t, int16_t, int32_t, int64_t, float, double>;

TYPED_TEST_CASE(VPackDeserializerArithmeticTest, TypesToTest);

TYPED_TEST(VPackDeserializerArithmeticTest, canRead) {
  this->checkWorks(TypeParam{0});
  this->checkWorks(5);
  this->checkWorks(-5);
};

TYPED_TEST(VPackDeserializerArithmeticTest, cannotRead) {
  this->checkDoesNotWork();
}

}  // namespace deserializer
}  // namespace tests
}  // namespace arangodb
