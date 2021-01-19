////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_ENDIAN_H
#define ARANGODB_BASICS_ENDIAN_H 1

#include <cstdint>
#include <cstring>
#include <type_traits>

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#include <machine/endian.h>
#elif _WIN32
#include <stdlib.h>
static_assert(sizeof(uint16_t) == sizeof(unsigned short),
              "wrong size for ushort");
static_assert(sizeof(uint32_t) == sizeof(unsigned long),
              "wrong size for ulong");
#elif __linux__
#include <endian.h>
#else
#pragma messsage("unsupported os or compiler")
#endif

namespace arangodb {
namespace basics {

#ifdef __APPLE__
#if BYTE_ORDER == LITTLE_ENDIAN
static constexpr bool isLittleEndian() { return true; }
#elif BYTE_ORDER == BIG_ENDIAN
static constexpr bool isLittleEndian() { return false; }
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

template <typename T, size_t size>
struct EndianTraits;

template <typename T>
struct EndianTraits<T, 2> {
  typedef typename std::make_unsigned<T>::type type;

  inline static type htole(type in) {
#ifdef __APPLE__
    return OSSwapHostToLittleInt16(in);
#elif __linux__
    return htole16(in);
#elif _WIN32
    if (!isLittleEndian()) {
      return _byteswap_ushort(in);
    }
    return in;
#else
    return in;
#endif
  }

  inline static type letoh(type in) {
#ifdef __APPLE__
    return OSSwapLittleToHostInt16(in);
#elif __linux__
    return le16toh(in);
#elif _WIN32
    if (!isLittleEndian()) {
      return _byteswap_ushort(in);
    }
    return in;
#else
    return in;
#endif
  }

  inline static type htobe(type in) {
#ifdef __APPLE__
    return OSSwapHostToBigInt16(in);
#elif __linux__
    return htobe16(in);
#elif _WIN32
    if (isLittleEndian()) {
      return _byteswap_ushort(in);
    }
    return in;
#else
    return in;
#endif
  }
  inline static type betoh(type in) {
#ifdef __APPLE__
    return OSSwapBigToHostInt16(in);
#elif __linux__
    return be16toh(in);
#elif _WIN32
    if (isLittleEndian()) {
      return _byteswap_ushort(in);
    }
    return in;
#else
    return in;
#endif
  }
};

template <typename T>
struct EndianTraits<T, 4> {
  typedef typename std::make_unsigned<T>::type type;

  inline static type htole(type in) {
#ifdef __APPLE__
    return OSSwapHostToLittleInt32(in);
#elif __linux__
    return htole32(in);
#elif _WIN32
    if (!isLittleEndian()) {
      return _byteswap_ulong(in);
    }
    return in;
#else
    return in;
#endif
  }

  inline static type letoh(type in) {
#ifdef __APPLE__
    return OSSwapLittleToHostInt32(in);
#elif __linux__
    return le32toh(in);
#elif _WIN32
    if (!isLittleEndian()) {
      return _byteswap_ulong(in);
    }
    return in;
#else
    return in;
#endif
  }

  inline static type htobe(type in) {
#ifdef __APPLE__
    return OSSwapHostToBigInt32(in);
#elif __linux__
    return htobe32(in);
#elif _WIN32
    if (isLittleEndian()) {
      return _byteswap_ulong(in);
    }
    return in;
#else
    return in;
#endif
  }

  inline static type betoh(type in) {
#ifdef __APPLE__
    return OSSwapBigToHostInt32(in);
#elif __linux__
    return be32toh(in);
#elif _WIN32
    if (isLittleEndian()) {
      return _byteswap_ulong(in);
    }
    return in;
#else
    return in;
#endif
  }
};

template <typename T>
struct EndianTraits<T, 8> {
  typedef typename std::make_unsigned<T>::type type;

  inline static type htole(type in) {
#ifdef __APPLE__
    return OSSwapHostToLittleInt64(in);
#elif __linux__
    return htole64(in);
#elif _WIN32
    if (!isLittleEndian()) {
      return _byteswap_uint64(in);
    }
    return in;
#else
    return in;
#endif
  }

  inline static type letoh(type in) {
#ifdef __APPLE__
    return OSSwapLittleToHostInt64(in);
#elif __linux__
    return le64toh(in);
#elif _WIN32
    if (!isLittleEndian()) {
      return _byteswap_uint64(in);
    }
    return in;
#else
    return in;
#endif
  }

  inline static type htobe(type in) {
#ifdef __APPLE__
    return OSSwapHostToBigInt64(in);
#elif __linux__
    return htobe64(in);
#elif _WIN32
    if (isLittleEndian()) {
      return _byteswap_uint64(in);
    }
    return in;
#else
    return in;
#endif
  }

  inline static type betoh(type in) {
#ifdef __APPLE__
    return OSSwapBigToHostInt64(in);
#elif __linux__
    return be64toh(in);
#elif _WIN32
    if (isLittleEndian()) {
      return _byteswap_uint64(in);
    }
    return in;
#else
    return in;
#endif
  }
};

template <bool>
struct cp {
  template <typename T>
  inline static T mu(T t) {
    return t;
  }
  template <typename T>
  inline static T ms(T t) {
    return t;
  }
};

template <>
struct cp<true> {
  // make unsigned
  template <typename T>
  inline static T mu(T t) {
    typename std::make_unsigned<T>::type tmp;
    std::memcpy(&tmp, &t, sizeof(T));
    return tmp;
  }
  // revert back to signed
  template <typename T>
  inline static T ms(T t) {
    typename std::make_signed<T>::type tmp;
    std::memcpy(&tmp, &t, sizeof(T));
    return tmp;
  }
};

// hostToLittle
template <typename T>
inline T hostToLittle(T in) {
  return cp<std::is_signed<T>::value>::ms(
      EndianTraits<T, sizeof(T)>::htole(cp<std::is_signed<T>::value>::mu(in)));
}

// littleToHost
template <typename T>
inline T littleToHost(T in) {
  return cp<std::is_signed<T>::value>::ms(
      EndianTraits<T, sizeof(T)>::letoh(cp<std::is_signed<T>::value>::mu(in)));
}

// hostToBig
template <typename T>
inline T hostToBig(T in) {
  return cp<std::is_signed<T>::value>::ms(
      EndianTraits<T, sizeof(T)>::htobe(cp<std::is_signed<T>::value>::mu(in)));
}

// bigToHost
template <typename T>
inline T bigToHost(T in) {
  return cp<std::is_signed<T>::value>::ms(
      EndianTraits<T, sizeof(T)>::betoh(cp<std::is_signed<T>::value>::mu(in)));
}

}  // namespace basics
}  // namespace arangodb

#endif
