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

#ifndef ARANGO_ROCKSDB_ROCKSDB_FORMAT_H
#define ARANGO_ROCKSDB_ROCKSDB_FORMAT_H 1

#include "Basics/Common.h"
#include "Basics/Endian.h"
#include "RocksDBEngine/RocksDBTypes.h"

namespace arangodb {

namespace rocksutils {
extern RocksDBEndianness rocksDBEndianness;

/* function pointers to serialization implementation */
extern uint16_t (*uint16FromPersistent)(char const* p);
extern uint32_t (*uint32FromPersistent)(char const* p);
extern uint64_t (*uint64FromPersistent)(char const* p);

extern void (*uint16ToPersistent)(std::string& p, uint16_t value);
extern void (*uint32ToPersistent)(std::string& p, uint32_t value);
extern void (*uint64ToPersistent)(std::string& p, uint64_t value);

/// Enable litte endian or big-endian key formats
void setRocksDBKeyFormatEndianess(RocksDBEndianness);

inline uint64_t doubleToInt(double d) {
  uint64_t i;
  std::memcpy(&i, &d, sizeof(i));
  return i;
}

inline double intToDouble(uint64_t i) {
  double d;
  std::memcpy(&d, &i, sizeof(i));
  return d;
}

template <typename T>
inline T uintFromPersistentLittleEndian(char const* p) {
  static_assert(std::is_unsigned<T>::value, "type must be unsigned");
  T value;
  memcpy(&value, p, sizeof(T));
  return basics::littleToHost<T>(value);
}

template <typename T>
inline T uintFromPersistentBigEndian(char const* p) {
  static_assert(std::is_unsigned<T>::value, "type must be unsigned");
  T value;
  memcpy(&value, p, sizeof(T));
  return basics::bigToHost<T>(value);
}

template <typename T>
inline void uintToPersistentLittleEndian(std::string& p, T value) {
  static_assert(std::is_unsigned<T>::value, "type must be unsigned");
  value = basics::hostToLittle(value);
  p.append(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
inline void uintToPersistentBigEndian(std::string& p, T value) {
  // uintToPersistentLittleEndian<T>(p, basics::hostToBig(value));
  static_assert(std::is_unsigned<T>::value, "type must be unsigned");
  value = basics::hostToBig(value);
  p.append(reinterpret_cast<const char*>(&value), sizeof(T));
}

}  // namespace rocksutils
}  // namespace arangodb

#endif
