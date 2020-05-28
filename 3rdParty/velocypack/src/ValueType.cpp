////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <ostream>

#include "velocypack/ValueType.h"

using namespace arangodb::velocypack;

char const* arangodb::velocypack::valueTypeName(ValueType type) {
  switch (type) {
    case ValueType::None:
      return "none";
    case ValueType::Illegal:
      return "illegal";
    case ValueType::Null:
      return "null";
    case ValueType::Bool:
      return "bool";
    case ValueType::Array:
      return "array";
    case ValueType::Object:
      return "object";
    case ValueType::Double:
      return "double";
    case ValueType::UTCDate:
      return "utc-date";
    case ValueType::External:
      return "external";
    case ValueType::MinKey:
      return "min-key";
    case ValueType::MaxKey:
      return "max-key";
    case ValueType::Int:
      return "int";
    case ValueType::UInt:
      return "uint";
    case ValueType::SmallInt:
      return "smallint";
    case ValueType::String:
      return "string";
    case ValueType::Binary:
      return "binary";
    case ValueType::BCD:
      return "bcd";
    case ValueType::Tagged:
      return "tagged";
    case ValueType::Custom:
      return "custom";
  }

  return "unknown";
}

ValueType arangodb::velocypack::valueTypeGroup(ValueType type) {
  // numbers are all the same, upcast them to double
  if (type == ValueType::Double || type == ValueType::Int || type == ValueType::UInt || type == ValueType::SmallInt) {
    return ValueType::Double; 
  }
  return type;
} 

std::ostream& operator<<(std::ostream& stream, ValueType type) {
  stream << valueTypeName(type);
  return stream;
}
