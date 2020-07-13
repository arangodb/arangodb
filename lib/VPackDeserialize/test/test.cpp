#include <map>

#define DESERIALIZER_NO_VPACK_TYPES
#define DESERIALIZER_SET_TEST_TYPES

#include "test-types.h"

#include "deserializer.h"

#include "velocypack/Buffer.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "velocypack/Slice.h"

using namespace arangodb::velocypack;

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

void test01() {
  auto buffer = R"=(["hello", true, 123.4])="_vpack;
  auto slice = deserializer::test::recording_slice::from_buffer(buffer);

  using deserial =
      deserializer::tuple_deserializer<deserializer::values::value_deserializer<std::string>,
                                       deserializer::values::value_deserializer<bool>,
                                       deserializer::values::value_deserializer<double>>;

  auto result = deserializer::deserialize<deserial>(slice);
  static_assert(
      std::is_same_v<decltype(result)::value_type, std::tuple<std::string, bool, double>>);
  std::cout << *slice.tape << std::endl;
}

void test02() {
  auto buffer = R"=([{"op":"bar"}, {"op":"foo"}])="_vpack;
  auto slice = deserializer::test::recording_slice::from_buffer(buffer);

  constexpr static const char op_name[] = "op";
  constexpr static const char bar_name[] = "bar";
  constexpr static const char foo_name[] = "foo";

  using op_deserial =
      deserializer::attribute_deserializer<op_name, deserializer::values::value_deserializer<std::string>>;

  using prec_deserial_pair =
      deserializer::value_deserializer_pair<deserializer::values::string_value<bar_name>, op_deserial>;
  using prec_deserial_pair_foo =
      deserializer::value_deserializer_pair<deserializer::values::string_value<foo_name>, op_deserial>;

  using deserial =
      deserializer::array_deserializer<deserializer::field_value_dependent_deserializer<op_name, prec_deserial_pair, prec_deserial_pair_foo>, my_vector>;

  auto result = deserializer::deserialize<deserial>(slice);

  static_assert(
      std::is_same_v<decltype(result)::value_type, std::vector<std::variant<std::string, std::string>>>);
  if (!result) {
    std::cerr << result.error().as_string() << std::endl;
  }
  std::cout << *slice.tape << std::endl;
}

void test03() {
  struct deserialized_type {
    my_map<std::string, std::variant<std::unique_ptr<deserialized_type>, std::string>> value;
  };

  struct recursive_deserializer {
    using plan = deserializer::map_deserializer<
        deserializer::conditional_deserializer<
            deserializer::condition_deserializer_pair<deserializer::is_object_condition, deserializer::unpack_proxy<recursive_deserializer, deserialized_type>>,
            deserializer::conditional_default<deserializer::values::value_deserializer<std::string>>>,
        my_map>;
    using factory = deserializer::utilities::constructor_factory<deserialized_type>;
    using constructed_type = deserialized_type;
  };

  auto buffer = R"=({"a":"b", "c":{"d":{"e":"false"}}})="_vpack;
  auto slice = deserializer::test::recording_slice::from_buffer(buffer);

  auto result = deserializer::deserialize<recursive_deserializer>(slice);

  if (!result) {
    std::cerr << result.error().as_string() << std::endl;
  }
  std::cout << *slice.tape << std::endl;
}

void test04() {
  struct non_default_constructible_type {
    explicit non_default_constructible_type(double){};
  };

  struct non_copyable_type {
    explicit non_copyable_type(double) {}
    non_copyable_type(non_copyable_type const&) = delete;
    non_copyable_type(non_copyable_type&&) noexcept = default;
  };

  using deserial = deserializer::tuple_deserializer<
      deserializer::utilities::constructing_deserializer<non_default_constructible_type, deserializer::values::value_deserializer<double>>,
      deserializer::utilities::constructing_deserializer<non_copyable_type, deserializer::values::value_deserializer<double>>>;

  auto buffer = R"=([12, 11, 13])="_vpack;
  auto slice = deserializer::test::recording_slice::from_buffer(buffer);

  auto result = deserializer::deserialize<deserial>(slice);

  if (!result) {
    std::cerr << result.error().as_string() << std::endl;
  }
  std::cout << *slice.tape << std::endl;
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

using graph_options_deserializer = deserializer::utilities::constructing_deserializer<graph_options, deserializer::parameter_list<
    deserializer::factory_optional_parameter<str_smart_graph_attribute, std::string_view>,
    deserializer::factory_simple_parameter<str_number_of_shards, uint32_t, false, deserializer::values::numeric_value<uint32_t, 1>>,
    deserializer::factory_simple_parameter<str_replication_factor, uint32_t, false, deserializer::values::numeric_value<uint32_t, 1>>,
    deserializer::factory_simple_parameter<str_min_replication_factor, uint32_t, false, deserializer::values::numeric_value<uint32_t, 1>>
>>;

using graph_options_validating_deserializer = deserializer::validate<graph_options_deserializer, graph_options_validator>;

struct graph_edge_definition {
  std::string_view collection;
  std::vector<std::string_view> from;
  std::vector<std::string_view> to;
};

constexpr const char str_collection[] = "collection";
constexpr const char str_from[] = "from";
constexpr const char str_to[] = "to";

template<typename D, template <typename> typename C>
using non_empty_array_deserializer = deserializer::validate<
    deserializer::array_deserializer<D, C>,
    deserializer::utilities::not_empty_validator>;

using non_empty_string_view_array_deserializer = non_empty_array_deserializer<
    deserializer::values::value_deserializer<std::string_view>, my_vector>;

template<typename S>
using non_empty_string_container = deserializer::validate<
    deserializer::values::value_deserializer<S>,
    deserializer::utilities::not_empty_validator>;

using non_empty_string_view = non_empty_string_container<std::string_view>;

using graph_edge_definition_deserializer = deserializer::utilities::constructing_deserializer<graph_edge_definition, deserializer::parameter_list<
    deserializer::factory_deserialized_parameter<str_collection, non_empty_string_view, true>,
    deserializer::factory_deserialized_parameter<str_from, non_empty_string_view_array_deserializer, true>,
    deserializer::factory_deserialized_parameter<str_to, non_empty_string_view_array_deserializer, true>
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

using graph_definition_deserializer = deserializer::utilities::constructing_deserializer<graph_definition, deserializer::parameter_list<
    deserializer::factory_deserialized_parameter<str_name, non_empty_string_view, true>,
    deserializer::factory_simple_parameter<str_is_smart, bool, false, deserializer::values::numeric_value<bool, false>>,
    deserializer::factory_deserialized_parameter<str_edge_definitions, graph_edge_definition_list_deserializer, true>,
    deserializer::factory_deserialized_parameter<str_options, graph_options_validating_deserializer, false>
>>;

/* clang-format on */

void test05() {
  auto buffer = R"=({"name":"myGraph","edgeDefinitions":[{"collection":"edges","from":["startVertices"],"to":["endVertices"]},{"collection":"edges","from":[],"to":["bla"]}],"options":{"replicationFactor":2,"minReplicationFactor":2}})="_vpack;
  auto slice = deserializer::test::recording_slice::from_buffer(buffer);

  graph_options_validator::context_type ctx = {2, 3};

  auto result =
      deserializer::deserialize_with_context<graph_definition_deserializer>(slice, ctx);

  if (!result) {
    std::cerr << result.error().as_string() << std::endl;
  }
  std::cout << *slice.tape << std::endl;
}

enum class MyEnum {
  MIN, MAX, SUM
};

constexpr const char MyEnum_min[] = "min";
constexpr const char MyEnum_max[] = "max";
constexpr const char MyEnum_sum[] = "sum";

using MyEnum_deserializer = deserializer::enum_deserializer<
    MyEnum, deserializer::enum_member<MyEnum::MIN, deserializer::values::string_value<MyEnum_min>>,
    deserializer::enum_member<MyEnum::MAX, deserializer::values::string_value<MyEnum_max>>,
    deserializer::enum_member<MyEnum::SUM, deserializer::values::string_value<MyEnum_sum>>>;

void test06() {
  auto buffer = R"=("mox")="_vpack;
  auto slice = deserializer::test::recording_slice::from_buffer(buffer);

  auto result =
      deserializer::deserialize<MyEnum_deserializer>(slice);

  if (!result) {
    std::cerr << result.error().as_string() << std::endl;
  }
  std::cout << *slice.tape << std::endl;
}

int main(int argc, char* argv[]) {
  test01();
  test02();
  test03();
  test04();
  test05();
  test06();
}
