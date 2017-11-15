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

#ifndef VELOCYPACK_VALUETYPE_H
#define VELOCYPACK_VALUETYPE_H

#include <iosfwd>

#include "velocypack-common.h"

namespace arangodb {
namespace velocypack {

enum class ValueType {
  None,    // not yet initialized
  Illegal, // illegal value
  Null,    // JSON null
  Bool,
  Array,
  Object,
  Double,
  UTCDate,
  External,
  MinKey,
  MaxKey,
  Int,
  UInt,
  SmallInt,
  String,
  Binary,
  BCD,
  Custom
};

char const* valueTypeName(ValueType);

ValueType valueTypeGroup(ValueType type);

}  // namespace arangodb::velocypack
}  // namespace arangodb

std::ostream& operator<<(std::ostream&, arangodb::velocypack::ValueType);

#endif
