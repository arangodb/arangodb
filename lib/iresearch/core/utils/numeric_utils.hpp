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

#pragma once

#include "utils/string.hpp"

namespace {

template<typename T, size_t N>
struct equal_size_type;

template<>
struct equal_size_type<long, 4> {
  typedef int32_t type;
};

template<>
struct equal_size_type<long, 8> {
  typedef int64_t type;
};

template<>
struct equal_size_type<unsigned long, 4> {
  typedef uint32_t type;
};

template<>
struct equal_size_type<unsigned long, 8> {
  typedef uint64_t type;
};

}  // namespace

namespace irs {
namespace numeric_utils {

bytes_view mini64();
bytes_view maxi64();

uint64_t decode64(const byte_type* out);
size_t encode64(uint64_t value, byte_type* out, size_t shift = 0);
uint64_t hton64(uint64_t value);
uint64_t ntoh64(uint64_t value);
bytes_view minu64();
bytes_view maxu64();

bytes_view mini32();
bytes_view maxi32();

uint32_t decode32(const byte_type* out);
size_t encode32(uint32_t value, byte_type* out, size_t shift = 0);
uint32_t hton32(uint32_t value);
uint32_t ntoh32(uint32_t value);
bytes_view minu32();
bytes_view maxu32();

size_t encodef32(uint32_t value, byte_type* out, size_t shift = 0);
uint32_t decodef32(const byte_type* out);
int32_t ftoi32(float_t value);
float_t i32tof(int32_t value);
bytes_view minf32();
bytes_view maxf32();
bytes_view finf32();
bytes_view nfinf32();

size_t encoded64(uint64_t value, byte_type* out, size_t shift = 0);
uint64_t decoded64(const byte_type* out);
int64_t dtoi64(double_t value);
double_t i64tod(int64_t value);
bytes_view mind64();
bytes_view maxd64();
bytes_view dinf64();
bytes_view ndinf64();

template<typename T>
struct numeric_traits;

template<>
struct numeric_traits<int32_t> {
  typedef int32_t integral_t;
  static bytes_view min() { return mini32(); }
  static bytes_view max() { return maxi32(); }
  inline static integral_t integral(integral_t value) { return value; }
  constexpr static size_t size() { return sizeof(integral_t) + 1; }
  static size_t encode(integral_t value, byte_type* out, size_t offset = 0) {
    return encode32(value, out, offset);
  }
  static integral_t decode(const byte_type* in) { return decode32(in); }
};

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
  static bytes_view min() { return minu32(); }
  static bytes_view max() { return maxu32(); }
  static bytes_view raw_ref(const integral_t& value) {
    return bytes_view(reinterpret_cast<const irs::byte_type*>(&value),
                      sizeof(value));
  }
  constexpr static size_t size() { return sizeof(integral_t) + 1; }
};

template<>
struct numeric_traits<int64_t> {
  typedef int64_t integral_t;
  static bytes_view min() { return mini64(); }
  static bytes_view max() { return maxi64(); }
  inline static integral_t integral(integral_t value) { return value; }
  constexpr static size_t size() { return sizeof(integral_t) + 1; }
  static size_t encode(integral_t value, byte_type* out, size_t offset = 0) {
    return encode64(value, out, offset);
  }
  static integral_t decode(const byte_type* in) { return decode64(in); }
};

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
  static bytes_view max() { return maxu64(); }
  static bytes_view min() { return minu64(); }
  static bytes_view raw_ref(const integral_t& value) {
    return bytes_view(reinterpret_cast<const irs::byte_type*>(&value),
                      sizeof(value));
  }
  constexpr static size_t size() { return sizeof(integral_t) + 1; }
};

// MacOS 'unsigned long' is a different type from any of the above
// MSVC 'unsigned long' is a different type from any of the above
#if defined(__APPLE__) || defined(_MSC_VER)
template<>
struct numeric_traits<unsigned long>
  : public numeric_traits<
      equal_size_type<unsigned long, sizeof(unsigned long)>::type> {};
#endif

template<>
struct numeric_traits<float> {
  typedef int32_t integral_t;
  static bytes_view ninf() { return nfinf32(); }
  static bytes_view min() { return minf32(); }
  static bytes_view max() { return maxf32(); }
  static bytes_view inf() { return finf32(); }
  static float_t floating(integral_t value) { return i32tof(value); }
  static integral_t integral(float_t value) { return ftoi32(value); }
  constexpr static size_t size() { return sizeof(integral_t) + 1; }
  static size_t encode(integral_t value, byte_type* out, size_t offset = 0) {
    return encodef32(value, out, offset);
  }
  static float_t decode(const byte_type* in) { return floating(decodef32(in)); }
};

template<>
struct numeric_traits<double> {
  typedef int64_t integral_t;
  static bytes_view ninf() { return ndinf64(); }
  static bytes_view min() { return mind64(); }
  static bytes_view max() { return maxd64(); }
  static bytes_view inf() { return dinf64(); }
  static double_t floating(integral_t value) { return i64tod(value); }
  static integral_t integral(double_t value) { return dtoi64(value); }
  constexpr static size_t size() { return sizeof(integral_t) + 1; }
  static size_t encode(integral_t value, byte_type* out, size_t offset = 0) {
    return encoded64(value, out, offset);
  }
  static double_t decode(const byte_type* in) {
    return floating(decoded64(in));
  }
};

template<>
struct numeric_traits<long double> {};  // numeric_traits

}  // namespace numeric_utils
}  // namespace irs
