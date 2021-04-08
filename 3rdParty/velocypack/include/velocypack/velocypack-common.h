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

#ifndef VELOCYPACK_COMMON_H
#define VELOCYPACK_COMMON_H 1

#include <cstdint>
// for size_t:
#include <cstring>
#include <type_traits>

#if defined(__GNUC__) || defined(__GNUG__)
#define VELOCYPACK_LIKELY(v) __builtin_expect(!!(v), 1)
#define VELOCYPACK_UNLIKELY(v) __builtin_expect(!!(v), 0)
#else
#define VELOCYPACK_LIKELY(v) v
#define VELOCYPACK_UNLIKELY(v) v
#endif

// debug mode
#ifndef NDEBUG
#ifndef VELOCYPACK_DEBUG
// turn on assertions when not compiled with -DNDEBUG
#define VELOCYPACK_DEBUG
#endif
#endif

#ifdef VELOCYPACK_DEBUG
#include <cassert>
#define VELOCYPACK_ASSERT(x) assert(x)

#else

#define VELOCYPACK_ASSERT(x)
#endif

// check for environment type (32 or 64 bit)
// if the environment type cannot be determined reliably, then this will
// abort compilation. this will abort on systems that neither have 32 bit
// nor 64 bit pointers!
#if INTPTR_MAX == INT32_MAX
#define VELOCYPACK_32BIT

#elif INTPTR_MAX == INT64_MAX
#define VELOCYPACK_64BIT

#else
#error "Could not determine environment type (32 or 64 bits)"
#endif

// attribute used to tag potentially unused functions (used mostly in tests/)
#ifdef __GNUC__
#define VELOCYPACK_UNUSED __attribute__((unused))
#else
#define VELOCYPACK_UNUSED /* unused */
#endif

// attribute used to force inlining of functions
#if defined(__GNUC__) || defined(__clang__)
#define VELOCYPACK_FORCE_INLINE inline __attribute__((__always_inline__))
#elif _WIN32
#define VELOCYPACK_FORCE_INLINE __forceinline
#endif

#ifndef VELOCYPACK_XXHASH
#ifndef VELOCYPACK_WYHASH
#ifndef VELOCYPACK_FASTHASH
// default to xxhash if no hash define is set
#define VELOCYPACK_XXHASH
#endif
#endif
#endif

#include "velocypack/velocypack-memory.h"

#ifdef VELOCYPACK_XXHASH
// forward for XXH functions declared elsewhere
#include "velocypack/velocypack-xxhash.h"

#define VELOCYPACK_HASH(mem, size, seed) XXH64(mem, size, seed)
#define VELOCYPACK_HASH32(mem, size, seed) XXH32(mem, size, seed)
#endif

#ifdef VELOCYPACK_WYHASH
// forward for wy hash functions declared elsewhere
#include "velocypack/velocypack-wyhash.h"

//the default secret parameters
static constexpr uint64_t _wyp[4] = {0xa0761d6478bd642full, 0xe7037ed1a0b428dbull, 0x8ebc6af09c88c6e3ull, 0x589965cc75374cc3ull};

#define VELOCYPACK_HASH(mem, size, seed) wyhash(mem, size, seed, _wyp)
#define VELOCYPACK_HASH32(mem, size, seed) static_cast<uint32_t>(wyhash(mem, size, seed, _wyp))
#endif

#ifdef VELOCYPACK_FASTHASH
// forward for fasthash functions declared elsewhere
uint64_t fasthash64(void const*, std::size_t, uint64_t);
uint64_t fasthash32(void const*, std::size_t, uint32_t);

#define VELOCYPACK_HASH(mem, size, seed) fasthash64(mem, size, seed)
#define VELOCYPACK_HASH32(mem, size, seed) fasthash32(mem, size, seed)
#endif

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#include <machine/endian.h>
#elif _WIN32
#include <stdlib.h>
#elif __linux__
#include <endian.h>
#else
#pragma messsage("unsupported os or compiler")
#endif

namespace arangodb {
namespace velocypack {

#ifdef __APPLE__
#define bswap_16(x) OSSwapInt16(x)
#define bswap_32(x) OSSwapInt32(x)
#define bswap_64(x) OSSwapInt64(x)
#if BYTE_ORDER == LITTLE_ENDIAN
static constexpr bool isLittleEndian() { return true; }
#elif BYTE_ORDER == BIG_ENDIAN
static constexpr bool isLittleEndian() { return false; }
#include <libkern/OSByteOrder.h>
#endif
#elif _WIN32
static constexpr bool isLittleEndian() { return true; }
#elif __linux__
#if __BYTE_ORDER == __LITTLE_ENDIAN
static constexpr bool isLittleEndian() { return true; }
#elif __BYTE_ORDER == __BIG_ENDIAN
static constexpr bool isLittleEndian() { return false; }
#endif
#else
#pragma messsage("unsupported os or compiler")
#endif


template<typename T>
VELOCYPACK_FORCE_INLINE T hostToLittle(T in) noexcept {
  static_assert(sizeof(T) == 8 || sizeof(T) == 4 || sizeof(T) == 2 || sizeof(T) == 1,
    "Type size is not supported");
  if constexpr (sizeof(T) == 8) {
#ifdef __APPLE__
    return OSSwapHostToLittleInt64(in);
#elif __linux__
    return htole64(in);
#elif _WIN32
    if constexpr (!isLittleEndian()) {
      return _byteswap_uint64(in);
    }
#endif
  }
  if constexpr (sizeof(T) == 4) {
#ifdef __APPLE__
    return OSSwapHostToLittleInt32(in);
#elif __linux__
    return htole32(in);
#elif _WIN32
    if constexpr (!isLittleEndian()) {
      return _byteswap_ulong(in);
    }
#endif
  }
  if constexpr (sizeof(T) == 2) {
#ifdef __APPLE__
    return OSSwapHostToLittleInt16(in);
#elif __linux__
    return htole16(in);
#elif _WIN32
    if constexpr (!isLittleEndian()) {
      return _byteswap_ushort(in);
    }
#endif
  }
  return in;
}

template<typename T>
VELOCYPACK_FORCE_INLINE T littleToHost(T in) noexcept {
  static_assert(sizeof(T) == 8 || sizeof(T) == 4 || sizeof(T) == 2 || sizeof(T) == 1,
    "Type size is not supported");
  if constexpr (sizeof(T) == 8) {
#ifdef __APPLE__
    return OSSwapLittleToHostInt64(in);
#elif __linux__
    return le64toh(in);
#elif _WIN32
    if constexpr (!isLittleEndian()) {
      return _byteswap_uint64(in);
    }
#endif
  }
  if constexpr (sizeof(T) == 4) {
#ifdef __APPLE__
    return OSSwapLittleToHostInt32(in);
#elif __linux__
    return le32toh(in);
#elif _WIN32
    if constexpr (!isLittleEndian()) {
      return _byteswap_ulong(in);
    }
#endif
  }
  if constexpr (sizeof(T) == 2) {
#ifdef __APPLE__
    return OSSwapLittleToHostInt16(in);
#elif __linux__
    return le16toh(in);
#elif _WIN32
    if constexpr (!isLittleEndian()) {
      return _byteswap_ushort(in);
    }
#endif
  }
  return in;
}


// unified size type for VPack, can be used on 32 and 64 bit
// though no VPack values can exceed the bounds of 32 bit on a 32 bit OS
typedef uint64_t ValueLength;

// disable hand-coded SSE4_2 functions for JSON parsing
// this must be called before the JSON parser is used
void disableAssemblerFunctions();

// whether or not the SSE4_2 functions are disabled
bool assemblerFunctionsEnabled();
bool assemblerFunctionsDisabled();

#ifndef VELOCYPACK_64BIT
// check if the length is beyond the size of a SIZE_MAX on this platform
std::size_t checkOverflow(ValueLength);
#else
// on a 64 bit platform, the following function is probably a no-op
static VELOCYPACK_FORCE_INLINE constexpr std::size_t checkOverflow(ValueLength length) noexcept {
  return static_cast<std::size_t>(length);
}
#endif

// calculate the length of a variable length integer in unsigned LEB128 format
static inline ValueLength getVariableValueLength(ValueLength value) noexcept {
  ValueLength len = 1;
  while (value >= 0x80U) {
    value >>= 7;
    ++len;
  }
  return len;
}

// read a variable length integer in unsigned LEB128 format
template <bool reverse>
static inline ValueLength readVariableValueLength(uint8_t const* source) noexcept {
  ValueLength len = 0;
  uint8_t v;
  ValueLength p = 0;
  do {
    v = *source;
    len += static_cast<ValueLength>(v & 0x7fU) << p;
    p += 7;
    if (reverse) {
      --source;
    } else {
      ++source;
    }
  } while (v & 0x80U);
  return len;
}

// store a variable length integer in unsigned LEB128 format
template <bool reverse>
static inline void storeVariableValueLength(uint8_t* dst, ValueLength value) noexcept {
  VELOCYPACK_ASSERT(value > 0);

  if (reverse) {
    while (value >= 0x80U) {
      *dst-- = static_cast<uint8_t>(value | 0x80U);
      value >>= 7;
    }
    *dst-- = static_cast<uint8_t>(value & 0x7fU);
  } else {
    while (value >= 0x80U) {
      *dst++ = static_cast<uint8_t>(value | 0x80U);
      value >>= 7;
    }
    *dst++ = static_cast<uint8_t>(value & 0x7fU);
  }
}

// returns current value for UTCDate
int64_t currentUTCDateValue();

static inline uint64_t toUInt64(int64_t v) noexcept {
  // If v is negative, we need to add 2^63 to make it positive,
  // before we can cast it to an uint64_t:
  uint64_t shift2 = 1ULL << 63;
  int64_t shift = static_cast<int64_t>(shift2 - 1);
  return v >= 0 ? static_cast<uint64_t>(v)
                : static_cast<uint64_t>((v + shift) + 1) + shift2;
  // Note that g++ and clang++ with -O3 compile this away to
  // nothing. Further note that a plain cast from int64_t to
  // uint64_t is not guaranteed to work for negative values!
}

static inline int64_t toInt64(uint64_t v) noexcept {
  uint64_t shift2 = 1ULL << 63;
  int64_t shift = static_cast<int64_t>(shift2 - 1);
  return v >= shift2 ? (static_cast<int64_t>(v - shift2) - shift) - 1
                     : static_cast<int64_t>(v);
}


#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4554)
#endif
template <typename T, unsigned Bytes, unsigned Shift = 0>
static inline T readIntFixedHelper(uint8_t const* p) noexcept {
  // bailout if nothing to shift or target type is too small for shift
  // to avoid compiler warning
  if constexpr (Bytes == 0 || sizeof(T) * 8 <= Shift) {
      return 0;
  } else {
    return readIntFixedHelper<T, Bytes - 1, Shift + 8>(p + 1) | (static_cast<T>(*p) << Shift);
    // for some reason MSVC detects possible operator precedence error here ~~^                                                                          ^
  }
}
#ifdef _WIN32
#pragma warning(pop)
#endif

// read an unsigned little endian integer value of the
// specified length, starting at the specified byte offset
template <typename T, ValueLength length>
static inline T readIntegerFixed(uint8_t const* start) noexcept {
  static_assert(std::is_unsigned<T>::value, "result type must be unsigned");
  static_assert(length > 0, "length must be > 0");
  static_assert(length <= sizeof(T), "length must be <= sizeof(T)");
  static_assert(length <=8);
  switch (length) {
    case 1:
      return *start;
    case 2:
      return readIntFixedHelper<T, 2>(start);
    case 3:
      return readIntFixedHelper<T, 3>(start);
    case 4:
      return readIntFixedHelper<T, 4>(start);
    case 5: // starting with 5 bytes memcpy shows better results than shifts. But
            // for big-endian we leave shifts as this saves some cpu cyles on byteswapping
      if constexpr (!isLittleEndian()) {
        return readIntFixedHelper<T, 5>(start);
      } else {
        T v{};
        memcpy(&v, start, 5);
        return v;
      }
    case 6:
      if constexpr (!isLittleEndian()) {
        return readIntFixedHelper<T, 6>(start);
      } else {
        T v{};
        memcpy(&v, start, 6);
        return v;
      }
    case 7:
      if constexpr (!isLittleEndian()) {
        return readIntFixedHelper<T, 7>(start);
      } else {
        T v{};
        memcpy(&v, start, 7);
        return v;
      }
    case 8: {
      T v;
      memcpy(&v, start, 8);
      if constexpr (!isLittleEndian()) {
        v = littleToHost(v);
      }
      return v;
    }
  }
  return 0;
}

// read an unsigned little endian integer value of the
// specified, non-0 length, starting at the specified byte offset
template <typename T>
static inline T readIntegerNonEmpty(uint8_t const* start, ValueLength length) noexcept {
  static_assert(std::is_unsigned<T>::value, "result type must be unsigned");
  VELOCYPACK_ASSERT(length > 0);
  VELOCYPACK_ASSERT(length <= sizeof(T));
  VELOCYPACK_ASSERT(length <= 8);
  switch (length) {
    case 1:
      return *start;
    case 2:
      return readIntegerFixed<T, 2>(start);
    case 3:
      return readIntegerFixed<T, 3>(start);
    case 4:
      return readIntegerFixed<T, 4>(start);
    case 5:
      return readIntegerFixed<T, 5>(start);
    case 6:
      return readIntegerFixed<T, 6>(start);
    case 7:
      return readIntegerFixed<T, 7>(start);
    case 8:
      return readIntegerFixed<T, 8>(start);
  }
  VELOCYPACK_ASSERT(false);
  return 0;
}

static inline uint64_t readUInt64(uint8_t const* start) noexcept {
  return readIntegerFixed<uint64_t, 8>(start);
}

static inline void storeUInt64(uint8_t* start, uint64_t value) noexcept {
  if constexpr (!isLittleEndian()) {
    value = hostToLittle(value);
  }
  memcpy(start, &value, sizeof(value));
}

}  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
