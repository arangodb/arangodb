////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_ENCODING_H
#define ARANGODB_BASICS_ENCODING_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace encoding {

/// @brief returns the 8-byte aligned size for the value
template <typename T, size_t alignment = 8>
static constexpr inline T alignedSize(T value) {
  return (value + (alignment - 1)) - ((value + (alignment - 1)) & (alignment - 1));
}

/// @brief portably and safely reads a number from little endian storage
template <typename T>
static inline T readNumber(uint8_t const* source, uint32_t length) {
  T value = 0;
  uint64_t x = 0;
  uint8_t const* end = source + length;
  do {
    value += static_cast<T>(*source++) << x;
    x += 8;
  } while (source < end);
  return value;
}

/// @brief portably and safely stores a number in little endian format
template <typename T>
static inline void storeNumber(uint8_t* dest, T value, uint32_t length) {
  uint8_t* end = dest + length;
  do {
    *dest++ = static_cast<uint8_t>(value & 0xff);
    value >>= 8;
  } while (dest < end);
}

}  // namespace encoding
}  // namespace arangodb

#endif
