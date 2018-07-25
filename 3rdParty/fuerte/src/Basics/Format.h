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
#include "operating-system.h"

// enable unalgined little-endian data access
#undef TRI_USE_FAST_UNALIGNED_DATA_ACCESS
#ifdef TRI_UNALIGNED_ACCESS
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define TRI_USE_FAST_UNALIGNED_DATA_ACCESS
#endif
#endif

namespace arangodb {
namespace basics {
  
/*
 * Alignment aware serialization and desirialization functions
 */

template<typename T> 
inline T uintFromPersistentLittleEndian(uint8_t const* ptr) {
  static_assert(std::is_unsigned<T>::value, "type must be unsigned");
#ifdef TRI_USE_FAST_UNALIGNED_DATA_ACCESS
  static_assert(basics::isLittleEndian(), "");
  return *reinterpret_cast<T const*>(ptr);
#else
  T value = 0;
  T x = 0;
  uint8_t const* end = ptr + sizeof(T);
  do {
    value += static_cast<T>(*ptr++) << x;
    x += 8;
  } while (ptr < end);
  return value;
#endif 
}

/*template<typename T>
inline T uintFromPersistentBigEndian(uint8_t const* p) {
  //return basics::bigToHost(uintFromPersistentLittleEndian<T>(p));
  static_assert(std::is_unsigned<T>::value, "type must be unsigned");
#ifdef TRI_USE_FAST_UNALIGNED_DATA_ACCESS
  return basics::bigToHost(*reinterpret_cast<T const*>(p));
#else
  T value = 0;
  T x = sizeof(T) * 8;
  uint8_t const* end = ptr + sizeof(T);
  do {
    x -= 8;
    value += static_cast<T>(*ptr++) << x;
  } while (ptr < end);
  TRI_ASSERT(x == 0);
  return value;
#endif
}*/
  
template<typename T>
inline void uintToPersistentLittleEndian(uint8_t* ptr, T value) {
  static_assert(std::is_unsigned<T>::value, "type must be unsigned");
#ifdef TRI_USE_FAST_UNALIGNED_DATA_ACCESS
  static_assert(basics::isLittleEndian(), "");
  *reinterpret_cast<T*>(ptr) = value;
#else
  uint8_t* end = ptr + sizeof(T);
  do {
    *ptr++ = static_cast<uint8_t>(value & 0xffU);
    value >>= 8;
  } while (ptr < end);
#endif
}

/*template<typename T>
inline void uintToPersistentBigEndian(std::string& p, T value) {
  //uintToPersistentLittleEndian<T>(p, basics::hostToBig(value));
  static_assert(std::is_unsigned<T>::value, "type must be unsigned");
#ifdef TRI_USE_FAST_UNALIGNED_DATA_ACCESS
  value = basics::hostToBig(value);
  p.append(reinterpret_cast<const char*>(&value), sizeof(T));
#else
  size_t len = sizeof(T) * 8;
  do {
    len -= 8;
    p.push_back(static_cast<char>((value >> len) & 0xFF));
  } while (len != 0);
#endif
}*/

}  // namespace rocksutils
}  // namespace arangodb

#endif
