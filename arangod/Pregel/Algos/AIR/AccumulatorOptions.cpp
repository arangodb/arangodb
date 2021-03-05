////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "AccumulatorOptions.h"
#include <unordered_set>

#include <VPackDeserializer/deserializer.h>

using namespace arangodb::velocypack::deserializer;

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {


template<typename D, template <typename> typename C>
using non_empty_array_deserializer = validate<
    array_deserializer<D, C>, utilities::not_empty_validator>;

template <typename K, typename V>
using my_map = std::unordered_map<K, V>;
template <typename V>
using my_vector = std::vector<V>;
template <typename V>
using my_unordered_set = std::unordered_set<V>;

bool isValidAccumulatorOptions(AccumulatorOptions const& options);

/* clang-format off */

constexpr const char accumulatorType_max[] = "max";
constexpr const char accumulatorType_min[] = "min";
constexpr const char accumulatorType_sum[] = "sum";
constexpr const char accumulatorType_and[] = "and";
constexpr const char accumulatorType_or[] = "or";
constexpr const char accumulatorType_store[] = "store";
constexpr const char accumulatorType_list[] = "list";
constexpr const char accumulatorType_custom[] = "custom";

using accumulator_type_deserializer = enum_deserializer<AccumulatorType,
    enum_member<AccumulatorType::MIN, values::string_value<accumulatorType_min>>,
    enum_member<AccumulatorType::MAX, values::string_value<accumulatorType_max>>,
    enum_member<AccumulatorType::SUM, values::string_value<accumulatorType_sum>>,
    enum_member<AccumulatorType::AND, values::string_value<accumulatorType_and>>,
    enum_member<AccumulatorType::OR, values::string_value<accumulatorType_or>>,
    enum_member<AccumulatorType::STORE, values::string_value<accumulatorType_store>>,
    enum_member<AccumulatorType::LIST, values::string_value<accumulatorType_list>>,
    enum_member<AccumulatorType::CUSTOM, values::string_value<accumulatorType_custom>>
>;

constexpr const char accumulatorValueType_double[] = "double";
constexpr const char accumulatorValueType_int[] = "int";
constexpr const char accumulatorValueType_string[] = "string";
constexpr const char accumulatorValueType_bool[] = "bool";
constexpr const char accumulatorValueType_any[] = "any";

using accumulator_value_type_deserializer = enum_deserializer<AccumulatorValueType,
    enum_member<AccumulatorValueType::DOUBLE, values::string_value<accumulatorValueType_double>>,
    enum_member<AccumulatorValueType::INT, values::string_value<accumulatorValueType_int>>,
    enum_member<AccumulatorValueType::STRING, values::string_value<accumulatorValueType_string>>,
    enum_member<AccumulatorValueType::BOOL, values::string_value<accumulatorValueType_bool>>,
    enum_member<AccumulatorValueType::ANY, values::string_value<accumulatorValueType_any>>
>;

constexpr const char accumulatorType[] = "accumulatorType";
constexpr const char valueType[] = "valueType";
constexpr const char parameters[] = "parameters";
constexpr const char customType[] = "customType";

using accumulator_options_plan = parameter_list<
    factory_deserialized_parameter<accumulatorType, accumulator_type_deserializer, true>,
    factory_deserialized_parameter<valueType, accumulator_value_type_deserializer, true>,
    factory_optional_value_parameter<customType, std::string>,
    factory_optional_builder_parameter<parameters>
>;

using accumulator_options_deserializer_base =
    utilities::constructing_deserializer<AccumulatorOptions, accumulator_options_plan>;

/* clang-format on */

struct accumulator_options_validator {
  std::optional<deserialize_error> operator()(AccumulatorOptions const& opts) {
    if (!isValidAccumulatorOptions(opts)) {
      return deserialize_error{"bad combination of accumulator and value type"};
    }
    if (opts.type == AccumulatorType::CUSTOM && !opts.customType) {
      return deserialize_error{"missing customType for custom accumulator"};
    }
    if (opts.customType && opts.type != AccumulatorType::CUSTOM) {
      return deserialize_error{"customType must not be set for this type"};
    }
    return {};
  }
};

/* clang-format off */

using accumulator_options_deserializer = validator::validate<accumulator_options_deserializer_base, accumulator_options_validator>;

constexpr const char clearProgram[] = "clearProgram";
constexpr const char setProgram[] = "setProgram";
constexpr const char getProgram[] = "getProgram";
constexpr const char updateProgram[] = "updateProgram";

constexpr const char setStateProgram[] = "setStateProgram";
constexpr const char getStateProgram[] = "getStateProgram";
constexpr const char getStateUpdateProgram[] = "getStateUpdateProgram";
constexpr const char aggregateStateProgram[] = "aggregateStateProgram";

constexpr const char finalizeProgram[] = "finalizeProgram";

using custom_accumulator_definition_plan = parameter_list<
    factory_builder_parameter<clearProgram, true>,
    factory_builder_parameter<setProgram, false>,
    factory_builder_parameter<getProgram, false>,
    factory_builder_parameter<updateProgram, true>,

    factory_builder_parameter<setStateProgram, false>,
    factory_builder_parameter<getStateProgram, false>,
    factory_builder_parameter<getStateUpdateProgram, false>,
    factory_builder_parameter<aggregateStateProgram, false>,

    factory_builder_parameter<finalizeProgram, false>
>;

using custom_accumulator_definition_deserializer =
    utilities::constructing_deserializer<CustomAccumulatorDefinition, custom_accumulator_definition_plan>;

/* Data Access */

constexpr const char writeVertex[] = "writeVertex";
constexpr const char readVertex[] = "readVertex";
constexpr const char readEdge[] = "readEdge";

using path_deserializer = non_empty_array_deserializer<values::value_deserializer<std::string>, my_vector>;

using key_or_path_deserializer = conditional_deserializer<
    condition_deserializer_pair<is_array_condition, path_deserializer>,
    conditional_default<values::value_deserializer<std::string>>
>;

using key_path_list = non_empty_array_deserializer<key_or_path_deserializer, my_vector>;

using data_access_options_plan = parameter_list<
    factory_optional_builder_parameter<writeVertex>,
    factory_optional_deserialized_parameter<readVertex, key_path_list>,
    factory_optional_deserialized_parameter<readEdge, key_path_list>
>;

using data_access_options_deserializer = utilities::constructing_deserializer<DataAccessDefinition, data_access_options_plan>;

/* Algorithm Phase */

constexpr const char name[] = "name";
constexpr const char onHalt[] = "onHalt";
constexpr const char onPreStep[] = "onPreStep";
constexpr const char onPostStep[] = "onPostStep";
constexpr const char initProgram[] = "initProgram";

using algorithm_phase_plan = parameter_list<
    factory_deserialized_parameter<name, values::value_deserializer<std::string>, true>,
    factory_builder_parameter<initProgram, false>,
    factory_builder_parameter<updateProgram, true>,
    factory_builder_parameter<onHalt, false>,
    factory_builder_parameter<onPreStep, false>,
    factory_builder_parameter<onPostStep, false>
>;

using algorithm_phase_deserializer = utilities::constructing_deserializer<AlgorithmPhase, algorithm_phase_plan>;

/* Debug */

using identifier_list_deserializer = array_deserializer<values::value_deserializer<std::string>, my_unordered_set>;

constexpr const char bySender[] = "bySender";
constexpr const char byAccumulator[] = "byAccumulator";

using trace_messages_filter_options_deserializer_plan = parameter_list<
    factory_deserialized_parameter<bySender, identifier_list_deserializer, false>,
    factory_deserialized_parameter<byAccumulator, identifier_list_deserializer, false>
>;

using trace_messages_filter_options_deserializer =
  utilities::constructing_deserializer<TraceMessagesFilterOptions, trace_messages_filter_options_deserializer_plan>;

constexpr const char filter[] = "filter";

using trace_messages_options_deserializer_plan = parameter_list<
    factory_optional_deserialized_parameter<filter, trace_messages_filter_options_deserializer>
>;

using trace_messages_options_deserializer =
  utilities::constructing_deserializer<TraceMessagesOptions, trace_messages_options_deserializer_plan>;

using trace_messages_vertex_list_deserializer = map_deserializer<trace_messages_options_deserializer, my_map>;

constexpr const char traceMessages[] = "traceMessages";

using debug_information_deserializer_plan = parameter_list<
    factory_deserialized_parameter<traceMessages, trace_messages_vertex_list_deserializer, false>
>;

using debug_information_deserializer = utilities::constructing_deserializer<DebugInformation, debug_information_deserializer_plan>;

/* Algorithm */

constexpr const char resultField[] = "resultField";
constexpr const char parallelism[] = "parallelism";
constexpr const char vertexAccumulators[] = "vertexAccumulators";
constexpr const char globalAccumulators[] = "globalAccumulators";
constexpr const char customAccumulators[] = "customAccumulators";
constexpr const char dataAccess[] = "dataAccess";
constexpr const char bindings[] = "bindings";
constexpr const char maxGSS[] = "maxGSS";
constexpr const char phases[] = "phases";
constexpr const char debug[] = "debug";



using accumulators_map_deserializer = map_deserializer<accumulator_options_deserializer, my_map>;
using custom_accumulators_map_deserializer = map_deserializer<custom_accumulator_definition_deserializer, my_map>;
using bindings_map_deserializer = map_deserializer<values::vpack_builder_deserializer, my_map>;
using phases_deserializer = non_empty_array_deserializer<algorithm_phase_deserializer, my_vector>;

using vertex_accumulator_options_plan = parameter_list<
    factory_deserialized_parameter<resultField, values::value_deserializer<std::string>, false>,
    factory_simple_parameter<parallelism, size_t, false>,
    factory_deserialized_parameter<vertexAccumulators, accumulators_map_deserializer, false>,
    factory_deserialized_parameter<globalAccumulators, accumulators_map_deserializer, false>,
    factory_deserialized_parameter<customAccumulators, custom_accumulators_map_deserializer, false>,
    factory_deserialized_parameter<dataAccess, data_access_options_deserializer, false>,
    factory_deserialized_parameter<bindings, bindings_map_deserializer, /* required */ false>, // will be default constructed as empty map
    factory_deserialized_parameter<phases, phases_deserializer, true>,
    factory_simple_parameter<maxGSS, uint64_t, false, values::numeric_value<uint64_t, 500>>,
    factory_optional_deserialized_parameter<debug, debug_information_deserializer>
>;

/* clang-format on */

// TODO: we could of course collect all parsing problems and return
//       them in bulk
struct vertex_accumulator_options_validator {

  bool isDefinedCustomAccumulatorType(VertexAccumulatorOptions const& opts, std::string const& customTypeName) {
    // C++2020 will have contains...
    return opts.customAccumulators.find(customTypeName) != std::end(opts.customAccumulators);
  }

  std::optional<deserialize_error> validate(VertexAccumulatorOptions const& opts, accumulators_map_deserializer::constructed_type::value_type const& acc) {
    if (acc.second.type == AccumulatorType::CUSTOM) {
      // Custom accumulators have to have type set, this
      // is ensured by the validator.
      auto const& customTypeName = acc.second.customType.value();
      if (!isDefinedCustomAccumulatorType(opts, customTypeName)) {
        return deserialize_error{"unknown custom accumulator type `"
          + customTypeName + "` for `" + acc.first + "`."};
      }
    }
    return {};
  }

  std::optional<deserialize_error> operator()(VertexAccumulatorOptions const& opts) {
    for (auto const& acc : opts.globalAccumulators) {
      if (auto err = validate(opts, acc); err) {
        return err->wrap("validating global accumulator");
      }
    }

    for (auto const& acc : opts.vertexAccumulators) {
      if (auto err = validate(opts, acc); err) {
        return err->wrap("validating vertex accumulator");
      }
    }
    return {};
  }
};

using vertex_accumulator_options_deserializer_base =
  utilities::constructing_deserializer<VertexAccumulatorOptions, vertex_accumulator_options_plan>;

using vertex_accumulator_options_deserializer = validator::validate<vertex_accumulator_options_deserializer_base, vertex_accumulator_options_validator>;

result<AccumulatorOptions, error> parseAccumulatorOptions(VPackSlice slice) {
  return deserialize<accumulator_options_deserializer>(slice);
}

result<VertexAccumulatorOptions, error> parseVertexAccumulatorOptions(VPackSlice slice) {
  return deserialize<vertex_accumulator_options_deserializer>(slice);
}

result<DataAccessDefinition, error> parseDataAccessOptions(VPackSlice slice) {
  return deserialize<data_access_options_deserializer>(slice);
}

std::ostream& operator<<(std::ostream& os, AccumulatorType const& type) {
  switch (type) {
    case AccumulatorType::MIN:
      os << accumulatorType_min;
      break;
    case AccumulatorType::MAX:
      os << accumulatorType_max;
      break;
    case AccumulatorType::SUM:
      os << accumulatorType_sum;
      break;
    case AccumulatorType::AND:
      os << accumulatorType_and;
      break;
    case AccumulatorType::OR:
      os << accumulatorType_or;
      break;
    case AccumulatorType::STORE:
      os << accumulatorType_store;
      break;
    case AccumulatorType::LIST:
      os << accumulatorType_list;
      break;
    case AccumulatorType::CUSTOM:
      os << accumulatorType_custom;
      break;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, AccumulatorValueType const& type) {
  switch (type) {
    case AccumulatorValueType::DOUBLE:
      os << accumulatorValueType_double;
      break;
    case AccumulatorValueType::INT:
      os << accumulatorValueType_int;
      break;
    case AccumulatorValueType::STRING:
      os << accumulatorValueType_string;
      break;
    case AccumulatorValueType::BOOL:
      os << accumulatorValueType_bool;
      break;
    case AccumulatorValueType::ANY:
      os << accumulatorValueType_any;
      break;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, AccumulatorOptions const& opt) {
  os << "VertexAccumulator:" << std::endl;
  os << accumulatorType << ": " << opt.type << ", ";
  os << valueType << ": " << opt.valueType;
  return os;
}

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
