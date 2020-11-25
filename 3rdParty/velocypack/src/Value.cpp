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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "velocypack/Exception.h"
#include "velocypack/StringRef.h"
#include "velocypack/Value.h"

using namespace arangodb::velocypack;
  
// creates a Value with the specified type Array or Object
Value::Value(ValueType t, bool allowUnindexed)
      : _valueType(t), _cType(CType::None) {
  if (allowUnindexed &&
      (_valueType != ValueType::Array && _valueType != ValueType::Object)) {
    throw Exception(Exception::InvalidValueType, "Expecting compound type");
  }
  // we use the boolean part to store the allowUnindexed value
  _value.b = allowUnindexed;
}

bool Value::unindexed() const {
  if (_valueType != ValueType::Array && _valueType != ValueType::Object) {
    throw Exception(Exception::InvalidValueType, "Expecting compound type");
  }
  return _value.b;
}
  
ValuePair::ValuePair(StringRef const& value, ValueType type) noexcept
      : ValuePair(value.data(), value.size(), type) {}
