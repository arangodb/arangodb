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

#include <VPackDeserialize/deserializer.h>

using namespace deserializer;


constexpr const char accumulatorType[] = "accumulatorType";
/*
constexpr const char valueType[] = "valueType";
constexpr const char storeSender[] = "storeSender";
constexpr const char neighborFilter[] = "neighborFilter";
constexpr const char updateExpression[] = "updateExpression";
*/

using accumulator_type_parameter =
  factory_deserialized_parameter<accumulatorType, values::value_deserializer<std::string>, true>;

using accumulator_options_plan = parameter_list<accumulator_type_parameter>;

using accumulator_options_deserializer =
    utilities::constructing_deserializer<AccumulatorOptions, accumulator_options_plan>;

result<AccumulatorOptions, error> parseAccumulatorOptions(VPackSlice slice) {
  return deserialize<accumulator_options_deserializer>(slice);
}

std::ostream& operator<<(std::ostream& os, AccumulatorOptions const& opt) {
  os << "VertexAccumulator:" << std::endl;
  os << accumulatorType << ": " << opt.type;
  return os;
}

