////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_CXX_DRIVER_FORMAT_H
#define ARANGO_CXX_DRIVER_FORMAT_H 1

#include "Endian.h"

namespace arangodb { namespace fuerte { namespace basics {
  
/*
 * Alignment aware serialization and deserialization functions
 */

template <typename T>
inline T uintFromPersistentLE(uint8_t const* p) {
  static_assert(std::is_unsigned<T>::value, "type must be unsigned");
  T value;
  memcpy(&value, p, sizeof(T));
  return basics::littleToHost<T>(value);
}

template <typename T>
inline void uintToPersistentLE(uint8_t* p, T value) {
  static_assert(std::is_unsigned<T>::value, "type must be unsigned");
  value = basics::hostToLittle(value);
  memcpy(p, &value, sizeof(T));
}

}}} 

#endif
