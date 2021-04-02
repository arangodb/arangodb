////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////
#ifndef ARANGOD_IRESEARCH_ANALYZER_VALUE_TYPE
#define ARANGOD_IRESEARCH_ANALYZER_VALUE_TYPE 1

#include <utils/attributes.hpp>
#include <utils/bit_utils.hpp>
#include "VPackDeserializer/deserializer.h"

namespace arangodb {
namespace iresearch {

enum class AnalyzerValueType : uint64_t {
  Undefined = 0,
  // Primitive types
  String    = 1,
  Number    = 1 << 1,
  Bool      = 1 << 2,
  Null      = 1 << 3,
  // Complex types
  Array     = 1 << 4,
  Object    = 1 << 5,
};

ENABLE_BITMASK_ENUM(AnalyzerValueType);

struct AnalyzerValueTypeAttribute final : irs::attribute {
  static constexpr irs::string_ref type_name() noexcept { return "value_type_attribute"; }
  explicit AnalyzerValueTypeAttribute(AnalyzerValueType t = AnalyzerValueType::Undefined)
    : value(t) {}
  AnalyzerValueType value;
};

constexpr const char ANALYZER_VALUE_TYPE_STRING[] = "string";
constexpr const char ANALYZER_VALUE_TYPE_NUMBER[] = "number";
constexpr const char ANALYZER_VALUE_TYPE_BOOL[] = "bool";
constexpr const char ANALYZER_VALUE_TYPE_NULL[] = "null";
constexpr const char ANALYZER_VALUE_TYPE_ARRAY[] = "array";
constexpr const char ANALYZER_VALUE_TYPE_OBJECT[] = "object";

using AnalyzerValueTypeEnumDeserializer = arangodb::velocypack::deserializer::enum_deserializer<
  AnalyzerValueType,
  arangodb::velocypack::deserializer::enum_member<
      AnalyzerValueType::String,
      arangodb::velocypack::deserializer::values::string_value<ANALYZER_VALUE_TYPE_STRING>>,
  arangodb::velocypack::deserializer::enum_member<
      AnalyzerValueType::Number,
      arangodb::velocypack::deserializer::values::string_value<ANALYZER_VALUE_TYPE_NUMBER>>,
  arangodb::velocypack::deserializer::enum_member<
      AnalyzerValueType::Bool,
      arangodb::velocypack::deserializer::values::string_value<ANALYZER_VALUE_TYPE_BOOL>>,
  arangodb::velocypack::deserializer::enum_member<
      AnalyzerValueType::Null,
      arangodb::velocypack::deserializer::values::string_value<ANALYZER_VALUE_TYPE_NULL>>,
  arangodb::velocypack::deserializer::enum_member<
      AnalyzerValueType::Array,
      arangodb::velocypack::deserializer::values::string_value<ANALYZER_VALUE_TYPE_ARRAY>>,
  arangodb::velocypack::deserializer::enum_member<
      AnalyzerValueType::Object,
      arangodb::velocypack::deserializer::values::string_value<ANALYZER_VALUE_TYPE_OBJECT>>
>;

} // namespace iresearch
} // namespace arangodb
#endif // ARANGOD_IRESEARCH_ANALYZER_VALUE_TYPE
