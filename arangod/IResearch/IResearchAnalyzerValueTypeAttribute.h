////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include <utils/attributes.hpp>
#include <utils/bit_utils.hpp>

namespace arangodb {
namespace iresearch {

enum class AnalyzerValueType : uint64_t {
  Undefined = 0,
  // Primitive types
  String = 1,
  Number = 1 << 1,
  Bool = 1 << 2,
  Null = 1 << 3,
  // Complex types
  Array = 1 << 4,
  Object = 1 << 5,
};

ENABLE_BITMASK_ENUM(AnalyzerValueType);

struct AnalyzerValueTypeAttribute final : irs::attribute {
  static constexpr std::string_view type_name() noexcept {
    return "value_type_attribute";
  }
  explicit AnalyzerValueTypeAttribute(
      AnalyzerValueType t = AnalyzerValueType::Undefined)
      : value(t) {}
  AnalyzerValueType value;
};

constexpr const char ANALYZER_VALUE_TYPE_STRING[] = "string";
constexpr const char ANALYZER_VALUE_TYPE_NUMBER[] = "number";
constexpr const char ANALYZER_VALUE_TYPE_BOOL[] = "bool";
constexpr const char ANALYZER_VALUE_TYPE_NULL[] = "null";
constexpr const char ANALYZER_VALUE_TYPE_ARRAY[] = "array";
constexpr const char ANALYZER_VALUE_TYPE_OBJECT[] = "object";

}  // namespace iresearch
}  // namespace arangodb
