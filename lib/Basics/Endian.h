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
/// @author Kaveh Vahedipour
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

#include <endian.h>

namespace arangodb {
namespace basics {

#if __BYTE_ORDER == __LITTLE_ENDIAN
static constexpr bool isLittleEndian() { return true; }
#elif __BYTE_ORDER == __BIG_ENDIAN
static constexpr bool isLittleEndian() { return false; }
#endif

template<typename T, size_t size>
struct EndianTraits;

template<typename T>
struct EndianTraits<T, 2> {
  typedef typename std::make_unsigned<T>::type type;

  inline static type htole(type in) { return htole16(in); }

  inline static type letoh(type in) { return le16toh(in); }

  inline static type htobe(type in) { return htobe16(in); }

  inline static type betoh(type in) { return be16toh(in); }
};

template<typename T>
struct EndianTraits<T, 4> {
  typedef typename std::make_unsigned<T>::type type;

  inline static type htole(type in) { return htole32(in); }

  inline static type letoh(type in) { return le32toh(in); }

  inline static type htobe(type in) { return htobe32(in); }

  inline static type betoh(type in) { return be32toh(in); }
};

template<typename T>
struct EndianTraits<T, 8> {
  typedef typename std::make_unsigned<T>::type type;

  inline static type htole(type in) { return htole64(in); }

  inline static type letoh(type in) { return le64toh(in); }

  inline static type htobe(type in) { return htobe64(in); }

  inline static type betoh(type in) { return be64toh(in); }
};

template<bool>
struct cp {
  template<typename T>
  inline static T mu(T t) {
    return t;
  }
  template<typename T>
  inline static T ms(T t) {
    return t;
  }
};

template<>
struct cp<true> {
  // make unsigned
  template<typename T>
  inline static T mu(T t) {
    typename std::make_unsigned<T>::type tmp;
    std::memcpy(&tmp, &t, sizeof(T));
    return tmp;
  }
  // revert back to signed
  template<typename T>
  inline static T ms(T t) {
    typename std::make_signed<T>::type tmp;
    std::memcpy(&tmp, &t, sizeof(T));
    return tmp;
  }
};

// hostToLittle
template<typename T>
inline T hostToLittle(T in) {
  return cp<std::is_signed<T>::value>::ms(
      EndianTraits<T, sizeof(T)>::htole(cp<std::is_signed<T>::value>::mu(in)));
}

// littleToHost
template<typename T>
inline T littleToHost(T in) {
  return cp<std::is_signed<T>::value>::ms(
      EndianTraits<T, sizeof(T)>::letoh(cp<std::is_signed<T>::value>::mu(in)));
}

// hostToBig
template<typename T>
inline T hostToBig(T in) {
  return cp<std::is_signed<T>::value>::ms(
      EndianTraits<T, sizeof(T)>::htobe(cp<std::is_signed<T>::value>::mu(in)));
}

// bigToHost
template<typename T>
inline T bigToHost(T in) {
  return cp<std::is_signed<T>::value>::ms(
      EndianTraits<T, sizeof(T)>::betoh(cp<std::is_signed<T>::value>::mu(in)));
}

}  // namespace basics
}  // namespace arangodb
