////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include "AccumulatorOptionsDeserializer.h"

#include <VPackDeserializer/deserializer.h>

using namespace arangodb::velocypack::deserializer;

/* clang-format off */

constexpr const char accumulatorType_max[] = "max";
constexpr const char accumulatorType_min[] = "min";
constexpr const char accumulatorType_sum[] = "sum";
constexpr const char accumulatorType_and[] = "and";
constexpr const char accumulatorType_or[] = "or";
constexpr const char accumulatorType_store[] = "store";
constexpr const char accumulatorType_list[] = "list";

using accumulator_type_deserializer = enum_deserializer<AccumulatorType,
    enum_member<AccumulatorType::MIN, values::string_value<accumulatorType_max>>,
    enum_member<AccumulatorType::MAX, values::string_value<accumulatorType_min>>,
    enum_member<AccumulatorType::SUM, values::string_value<accumulatorType_sum>>,
    enum_member<AccumulatorType::AND, values::string_value<accumulatorType_and>>,
    enum_member<AccumulatorType::OR, values::string_value<accumulatorType_or>>,
    enum_member<AccumulatorType::STORE, values::string_value<accumulatorType_store>>,
    enum_member<AccumulatorType::LIST, values::string_value<accumulatorType_list>>
>;

constexpr const char accumulatorValueType_doubles[] = "doubles";
constexpr const char accumulatorValueType_ints[] = "ints";
constexpr const char accumulatorValueType_strings[] = "strings";
constexpr const char accumulatorValueType_bool[] = "bool";
constexpr const char accumulatorValueType_slice[] = "slice";

using accumulator_value_type_deserializer = enum_deserializer<AccumulatorValueType,
    enum_member<AccumulatorValueType::DOUBLES, values::string_value<accumulatorValueType_doubles>>,
    enum_member<AccumulatorValueType::INTS, values::string_value<accumulatorValueType_ints>>,
    enum_member<AccumulatorValueType::STRINGS, values::string_value<accumulatorValueType_strings>>,
    enum_member<AccumulatorValueType::BOOL, values::string_value<accumulatorValueType_bool>>,
    enum_member<AccumulatorValueType::SLICE, values::string_value<accumulatorValueType_slice>>
>;

constexpr const char accumulatorType[] = "accumulatorType";
constexpr const char valueType[] = "valueType";
constexpr const char storeSender[] = "storeSender";

using accumulator_options_plan = parameter_list<
    factory_deserialized_parameter<accumulatorType, accumulator_type_deserializer, true>,
    factory_deserialized_parameter<valueType, accumulator_value_type_deserializer, true>,
    factory_simple_parameter<storeSender, bool, false>>;

using accumulator_options_deserializer =
    utilities::constructing_deserializer<AccumulatorOptions, accumulator_options_plan>;

/* VertexAccumulatorOption */

constexpr const char resultField[] = "resultField";
constexpr const char accumulatorsDeclaration[] = "accumulatorsDeclaration";
constexpr const char bindings[] = "bindings";
constexpr const char initProgram[] = "initProgram";
constexpr const char updateProgram[] = "updateProgram";

template<typename K, typename V>
using my_map = std::map<K, V>;

template<typename D, template <typename> typename C>
using non_empty_array_deserializer = validate<
    array_deserializer<D, C>, utilities::not_empty_validator>;

using accumulators_map_deserializer = map_deserializer<accumulator_options_deserializer, my_map>;
using bindings_map_deserializer = map_deserializer<values::vpack_builder_deserializer, my_map>;

using vertex_accumulator_options_plan = parameter_list<
  factory_deserialized_parameter<resultField, values::value_deserializer<std::string>, true>,
  factory_deserialized_parameter<accumulatorsDeclaration, accumulators_map_deserializer, true>,
  factory_deserialized_parameter<bindings, bindings_map_deserializer, /* required */ false>, // will be default constructed as empty map
  factory_slice_parameter<initProgram, true>,
  factory_slice_parameter<updateProgram, true>>;

using vertex_accumulator_options_deserializer =
    utilities::constructing_deserializer<VertexAccumulatorOptions, vertex_accumulator_options_plan>;

/* clang-format on */

result<AccumulatorOptions, error> parseAccumulatorOptions(VPackSlice slice) {
  return deserialize<accumulator_options_deserializer>(slice);
}

result<VertexAccumulatorOptions, error> parseVertexAccumulatorOptions(VPackSlice slice) {
  return deserialize<vertex_accumulator_options_deserializer>(slice);
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
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, AccumulatorValueType const& type) {
  switch (type) {
    case AccumulatorValueType::DOUBLES:
      os << accumulatorValueType_doubles;
      break;
    case AccumulatorValueType::INTS:
      os << accumulatorValueType_ints;
      break;
    case AccumulatorValueType::STRINGS:
      os << accumulatorValueType_strings;
      break;
    case AccumulatorValueType::BOOL:
      os << accumulatorValueType_bool;
      break;
    case AccumulatorValueType::SLICE:
      os << accumulatorValueType_slice;
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
