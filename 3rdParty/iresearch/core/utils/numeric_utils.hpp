////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_NUMERIC_UTILS_H
#define IRESEARCH_NUMERIC_UTILS_H

#include "utils/string.hpp"

namespace {

// MSVC < v14.0 (Visual Studio >2015) does not support explicit initializer for arrays: error C2536
// GCC < v4.9 does not initialize the union array member with the specified value (initializes with {0,0})
// CLANG (at least 4.0.1) does not support constexpr reinterpret cast
// for versions @see https://github.com/thp/psmoveapi/blob/master/external/glm/glm/simd/platform.h
#if defined(__APPLE__) \
    || (defined(__clang__) && (__clang_major__ > 4)) \
    || (defined(_MSC_VER) && (_MSC_VER >= 1900)) \
    || (defined(__GNUC__) && (__GNUC__ >= 5)) \
    || (defined(__GNUC__) && (__GNUC__ == 4) && (__GNUC_MINOR__ >= 9))
  union big_endian_check {
    char raw[2]{ '\0', '\xff' };
    uint16_t num; // big endian: num < 0x100
    constexpr operator bool() const { return num < 0x100; }
  };
#endif

template <typename T, size_t N>
struct equal_size_type;

template<>
struct equal_size_type<long, 4> { typedef int32_t type; };

template<>
struct equal_size_type<long, 8> { typedef int64_t type; };

template<>
struct equal_size_type<unsigned long, 4> { typedef uint32_t type; };

template<>
struct equal_size_type<unsigned long, 8> { typedef uint64_t type; };

}

namespace iresearch {
namespace numeric_utils {


#if defined(__APPLE__) \
    || (defined(__clang__) && (__clang_major__ > 4)) \
    || (defined(_MSC_VER) && (_MSC_VER >= 1900)) \
    || (defined(__GNUC__) && (__GNUC__ >= 5)) \
    || (defined(__GNUC__) && (__GNUC__ == 4) && (__GNUC_MINOR__ >= 9))
  inline constexpr bool is_big_endian() { return big_endian_check(); }
#else
  inline constexpr bool is_big_endian() { return *(uint16_t*)"\0\xff" < 0x100; }
#endif

IRESEARCH_API const bytes_ref& mini64();
IRESEARCH_API const bytes_ref& maxi64();

IRESEARCH_API uint64_t decode64(const byte_type* out);
IRESEARCH_API size_t encode64(uint64_t value, byte_type* out, size_t shift = 0);
IRESEARCH_API uint64_t hton64(uint64_t value);
IRESEARCH_API uint64_t ntoh64(uint64_t value);
IRESEARCH_API const bytes_ref& minu64();
IRESEARCH_API const bytes_ref& maxu64();

IRESEARCH_API const bytes_ref& mini32();
IRESEARCH_API const bytes_ref& maxi32();

IRESEARCH_API uint32_t decode32(const byte_type* out);
IRESEARCH_API size_t encode32(uint32_t value, byte_type* out, size_t shift = 0);
IRESEARCH_API uint32_t hton32(uint32_t value);
IRESEARCH_API uint32_t ntoh32(uint32_t value);
IRESEARCH_API const bytes_ref& minu32();
IRESEARCH_API const bytes_ref& maxu32();

IRESEARCH_API size_t encodef32(uint32_t value, byte_type* out, size_t shift = 0);
IRESEARCH_API uint32_t decodef32(const byte_type* out);
IRESEARCH_API int32_t ftoi32(float_t value);
IRESEARCH_API float_t i32tof(int32_t value);
IRESEARCH_API const bytes_ref& minf32();
IRESEARCH_API const bytes_ref& maxf32();
IRESEARCH_API const bytes_ref& finf32();
IRESEARCH_API const bytes_ref& nfinf32();

IRESEARCH_API size_t encoded64(uint64_t value, byte_type* out, size_t shift = 0);
IRESEARCH_API uint64_t decoded64(const byte_type* out);
IRESEARCH_API int64_t dtoi64(double_t value);
IRESEARCH_API double_t i64tod(int64_t value);
IRESEARCH_API const bytes_ref& mind64();
IRESEARCH_API const bytes_ref& maxd64();
IRESEARCH_API const bytes_ref& dinf64();
IRESEARCH_API const bytes_ref& ndinf64();

template<typename T>
struct numeric_traits;

template<>
struct numeric_traits<int32_t> {
  typedef int32_t integral_t;
  static const bytes_ref& (min)() { return mini32(); } 
  static const bytes_ref& (max)() { return maxi32(); } 
  inline static integral_t integral(integral_t value) { return value; }
  constexpr static size_t size() { return sizeof(integral_t)+1; }
  static size_t encode(integral_t value, byte_type* out, size_t offset = 0) {
    return encode32(value, out, offset);
  }
  static integral_t decode(const byte_type* in) {
    return decode32(in);
  }
}; // numeric_traits

template<>
struct numeric_traits<uint32_t> {
  typedef uint32_t integral_t;
  static integral_t decode(const byte_type* in) { return decode32(in); }
  static size_t encode(integral_t value, byte_type* out, size_t offset = 0) {
    return encode32(value, out, offset);
  }
  static integral_t hton(integral_t value) { return hton32(value); }
  static integral_t ntoh(integral_t value) { return ntoh32(value); }
  inline static integral_t integral(integral_t value) { return value; }
  static const bytes_ref& (min)() { return minu32(); }
  static const bytes_ref& (max)() { return maxu32(); }
  static bytes_ref raw_ref(integral_t const& value) {
    return bytes_ref(
      reinterpret_cast<irs::byte_type const*>(&value),
      sizeof(value)
    );
  }
  constexpr static size_t size() { return sizeof(integral_t) + 1; }
};

template<>
struct numeric_traits<int64_t> {
  typedef int64_t integral_t;
  static const bytes_ref& (min)() { return mini64(); } 
  static const bytes_ref& (max)() { return maxi64(); } 
  inline static integral_t integral(integral_t value) { return value; }
  constexpr static size_t size() { return sizeof(integral_t)+1; }
  static size_t encode(integral_t value, byte_type* out, size_t offset = 0) {
    return encode64(value, out, offset);
  }
  static integral_t decode(const byte_type* in) {
    return decode64(in);
  }
}; // numeric_traits

template<>
struct numeric_traits<uint64_t> {
  typedef uint64_t integral_t;
  static integral_t decode(const byte_type* in) { return decode64(in); }
  static size_t encode(integral_t value, byte_type* out, size_t offset = 0) {
    return encode64(value, out, offset);
  }
  static integral_t hton(integral_t value) { return hton64(value); }
  static integral_t ntoh(integral_t value) { return ntoh64(value); }
  inline static integral_t integral(integral_t value) { return value; }
  static const bytes_ref& (max)() { return maxu64(); }
  static const bytes_ref& (min)() { return minu64(); }
  static bytes_ref raw_ref(integral_t const& value) {
    return bytes_ref(
      reinterpret_cast<irs::byte_type const*>(&value),
      sizeof(value)
    );
  }
  constexpr static size_t size() { return sizeof(integral_t) + 1; }
};

// MacOS 'unsigned long' is a different type from any of the above
// MSVC 'unsigned long' is a different type from any of the above
#if defined (__APPLE__) || defined(_MSC_VER)
  template<>
  struct numeric_traits<unsigned long>
    : public numeric_traits<equal_size_type<unsigned long, sizeof(unsigned long)>::type> {
  };
#endif

template<>
struct numeric_traits<float> {
  typedef int32_t integral_t;
  static const bytes_ref& ninf() { return nfinf32(); }
  static const bytes_ref& (min)() { return minf32(); } 
  static const bytes_ref& (max)() { return maxf32(); } 
  static const bytes_ref& inf() { return finf32(); }
  static float_t floating(integral_t value) { return i32tof(value); }
  static integral_t integral(float_t value) { return ftoi32(value); }
  constexpr static size_t size() { return sizeof(integral_t)+1; }
  static size_t encode(integral_t value, byte_type* out, size_t offset = 0) {
    return encodef32(value, out, offset);
  }
  static float_t decode(const byte_type* in) {
    return floating(decodef32(in));
  }
}; // numeric_traits

template<>
struct numeric_traits<double> {
  typedef int64_t integral_t;
  static const bytes_ref& ninf() { return ndinf64(); }
  static const bytes_ref& (min)() { return mind64(); } 
  static const bytes_ref& (max)() { return maxd64(); } 
  static const bytes_ref& inf() { return dinf64(); }
  static double_t floating(integral_t value) { return i64tod(value); }
  static integral_t integral(double_t value) { return dtoi64(value); }
  constexpr static size_t size() { return sizeof(integral_t)+1; }
  static size_t encode(integral_t value, byte_type* out, size_t offset = 0) {
    return encoded64(value, out, offset);
  }
  static double_t decode(const byte_type* in) {
    return floating(decoded64(in));
  }
}; // numeric_traits

template<>
struct numeric_traits<long double> {
}; // numeric_traits

} // numeric_utils
} // ROOT

#endif // IRESEARCH_NUMERIC_UTILS_H
