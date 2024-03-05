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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "Basics/Endian.h"
#include "RocksDBEngine/RocksDBTypes.h"

namespace arangodb::rocksutils {

// TODO(MBkkt) I don't see the reason for runtime switch endianes
//  So I think better to make this opton compile time
//  and avoid function pointer calls

extern RocksDBEndianness rocksDBEndianness;

/* function pointers to serialization implementation */
extern uint16_t (*uint16FromPersistent)(char const* p);
extern uint32_t (*uint32FromPersistent)(char const* p);
extern uint64_t (*uint64FromPersistent)(char const* p);

extern void (*uint16ToPersistent)(std::string& p, uint16_t value);
extern void (*uint32ToPersistent)(std::string& p, uint32_t value);
extern void (*uint64ToPersistent)(std::string& p, uint64_t value);

extern void (*uint64ToPersistentRaw)(char* p, uint64_t value);

/// Enable litte endian or big-endian key formats
void setRocksDBKeyFormatEndianess(RocksDBEndianness);

RocksDBEndianness getRocksDBKeyFormatEndianness() noexcept;

template<typename T>
inline T uintFromPersistentLittleEndian(char const* p) {
  static_assert(std::is_unsigned_v<T>, "type must be unsigned");
  T value;
  memcpy(&value, p, sizeof(T));
  return basics::littleToHost<T>(value);
}

template<typename T>
inline T uintFromPersistentBigEndian(char const* p) {
  static_assert(std::is_unsigned_v<T>, "type must be unsigned");
  T value;
  memcpy(&value, p, sizeof(T));
  return basics::bigToHost<T>(value);
}

template<typename T>
inline void uintToPersistentLittleEndian(std::string& p, T value) {
  static_assert(std::is_unsigned_v<T>, "type must be unsigned");
  value = basics::hostToLittle(value);
  p.append(reinterpret_cast<char const*>(&value), sizeof(T));
}

template<typename T>
inline void uintToPersistentBigEndian(std::string& p, T value) {
  static_assert(std::is_unsigned_v<T>, "type must be unsigned");
  value = basics::hostToBig(value);
  p.append(reinterpret_cast<char const*>(&value), sizeof(T));
}

template<typename T>
inline void uintToPersistentRawLE(char* p, T value) {
  static_assert(std::is_unsigned_v<T>, "type must be unsigned");
  value = basics::hostToLittle(value);
  memcpy(p, &value, sizeof(T));
}

template<typename T>
inline void uintToPersistentRawBE(char* p, T value) {
  static_assert(std::is_unsigned_v<T>, "type must be unsigned");
  value = basics::hostToBig(value);
  memcpy(p, &value, sizeof(T));
}

}  // namespace arangodb::rocksutils
