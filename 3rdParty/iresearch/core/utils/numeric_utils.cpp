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

#include "shared.hpp"
#include "numeric_utils.hpp"
#include "bit_utils.hpp"

#include <cmath>

#if defined(_MSC_VER)
  #include <Winsock2.h>
  #pragma comment(lib,"Ws2_32.lib")
#elif defined(__APPLE__)
  #include <arpa/inet.h>
  #include <machine/endian.h>
#else
  #include <arpa/inet.h>
  #include <endian.h>

  #define htonll htobe64
  #define ntohll be64toh
#endif // _WIN32

namespace {

// ----------------------------------------------------------------------------
// static buffers
// ----------------------------------------------------------------------------

enum class buf_id_t { NINF, MIN, MAX, INF };

template<typename T, buf_id_t ID>
iresearch::bstring& static_buf() {
  static iresearch::bstring buf;
  return buf;
}

}

namespace iresearch {
namespace numeric_utils {

template<typename T>
struct encode_traits;

template<>
struct encode_traits<uint64_t> {
  typedef uint64_t type;
  static const byte_type TYPE_MAGIC = 0x60;
  static type hton(type value) { return htonll(value); }
  static type ntoh(type value) { return ntohll(value); }
}; // encode_traits

template<>
struct encode_traits<uint32_t> {
  typedef uint32_t type;
  static const byte_type TYPE_MAGIC = 0;
  static type hton(type value) { return htonl(value); }
  static type ntoh(type value) { return ntohl(value); }
}; // encode_traits

#ifndef FLOAT_T_IS_DOUBLE_T
template<>
struct encode_traits<float_t> : encode_traits<uint32_t> {
  static const byte_type TYPE_MAGIC = 0x20;
}; // encode_traits
#endif

template<>
struct encode_traits<double_t> : encode_traits<uint64_t> {
  static const byte_type TYPE_MAGIC = 0xA0;
}; // encode_traits

// returns number of bytes required to store 
// value of type T with the specified offset
template<typename T>
size_t encoded_size(size_t shift) {
  const size_t bits = bits_required<T>(); // number of bits required to store T
  return bits > shift ? 1 + (bits - 1 - shift) / 8 : 0; 
}

template<typename T>
bstring& encode(bstring& buf, T value, size_t offset = 0) {
  typedef numeric_traits<T> traits_t;
  buf.resize(traits_t::size());
  buf.resize(traits_t::encode(traits_t::integral(value), &(buf[0]), offset));
  return buf;
}

template<typename T, typename EncodeTraits = encode_traits<T>>
size_t encode(typename EncodeTraits::type value, byte_type* out, size_t shift) {
  typedef typename EncodeTraits::type type;
  
  value ^= type(1) << (bits_required<type>() - 1);
  value &= std::numeric_limits<type>::max() ^ ((type(1) << shift) - 1);
  if (!is_big_endian()) {
    value = EncodeTraits::hton(value);
  } 

  const size_t size = encoded_size<type>(shift);
  *out = static_cast<byte_type>(shift) + EncodeTraits::TYPE_MAGIC;
  std::memcpy(out+1, reinterpret_cast<const void*>(&value), size);
  return size+1;
}

template<typename T, typename EncodeTraits = encode_traits<T>>
typename EncodeTraits::type decode(const byte_type* in) {
  typedef typename EncodeTraits::type type;
  type value{};

  const size_t size = encoded_size<type>(*in - EncodeTraits::TYPE_MAGIC);
  if (size) {
    std::memcpy(reinterpret_cast<void*>(&value), in + 1, size);
    if (!is_big_endian()) {
      value = EncodeTraits::ntoh(value);
    }
    value ^= type(1) << (bits_required<type>() - 1);
  }

  return value;
}

inline int32_t make_sortable32(int32_t value) {
  return value ^ ((value >> 31) & INT32_C(0x7FFFFFFF));
}

inline int64_t make_sortable64(int64_t value) {
  return value ^ ((value >> 63) & INT64_C(0x7FFFFFFFFFFFFFFF));
}

size_t encode64(uint64_t value, byte_type* out, size_t shift /* = 0 */) {
  return encode<uint64_t>(value, out, shift);
}

uint64_t decode64(const byte_type* in) {
  return decode<uint64_t>(in);
}

size_t encode32(uint32_t value, byte_type* out, size_t shift /* = 0 */) {
  return encode<uint32_t>(value, out, shift);
}

uint32_t decode32(const byte_type* in) {
  return decode<uint32_t>(in);
}

size_t encodef32(uint32_t value, byte_type* out, size_t shift /* = 0 */) {
  return encode<float_t>(value, out, shift); 
}

uint32_t decodef32(const byte_type* in) {
  return decode<float_t>(in);
}

size_t encoded64(uint64_t value, byte_type* out, size_t shift /* = 0 */) {
  return encode<double_t>(value, out, shift); 
}

uint64_t decoded64(const byte_type* in) {
  return decode<double_t>(in);
}

const bytes_ref& mini32() {
  static bytes_ref data = encode(static_buf<int32_t, buf_id_t::MIN>(), std::numeric_limits<int32_t>::min());
  return data; 
}

const bytes_ref& maxi32() {
  static bytes_ref data = encode(static_buf<int32_t, buf_id_t::MAX>(), std::numeric_limits<int32_t>::max());
  return data;
}

const bytes_ref& minu32() {
  static bytes_ref data = encode(static_buf<uint32_t, buf_id_t::MIN>(), std::numeric_limits<uint32_t>::min());
  return data; 
}

const bytes_ref& maxu32() {
  static bytes_ref data = encode(static_buf<uint32_t, buf_id_t::MAX>(), std::numeric_limits<uint32_t>::max());
  return data;
}

const bytes_ref& mini64() {
  static bytes_ref data = encode(static_buf<int64_t, buf_id_t::MIN>(), std::numeric_limits<int64_t>::min());
  return data;
}

const bytes_ref& maxi64() {
  static bytes_ref data = encode(static_buf<int64_t, buf_id_t::MAX>(), std::numeric_limits<int64_t>::max());
  return data;
}

const bytes_ref& minu64() {
  static bytes_ref data = encode(static_buf<uint64_t, buf_id_t::MIN>(), std::numeric_limits<uint64_t>::min());
  return data;
}

const bytes_ref& maxu64() {
  static bytes_ref data = encode(static_buf<uint64_t, buf_id_t::MAX>(), std::numeric_limits<uint64_t>::max());
  return data;
}

uint32_t hton32(uint32_t value) {
  return encode_traits<uint32_t>::hton(value);
}

uint32_t ntoh32(uint32_t value) {
  return encode_traits<uint32_t>::ntoh(value);
}

uint64_t hton64(uint64_t value) {
  return encode_traits<uint64_t>::hton(value);
}

uint64_t ntoh64(uint64_t value) {
  return encode_traits<uint64_t>::ntoh(value);
}

int32_t ftoi32(float_t value) {
  static_assert(std::numeric_limits<float_t>::is_iec559,
                "compiler does not support ieee754 (float)");

  union {
    float_t in;
    int32_t out;
  } conv;

  conv.in = value;
  return make_sortable32(conv.out);
}

float_t i32tof(int32_t value) {
  static_assert(std::numeric_limits<float_t>::is_iec559,
                "compiler does not support ieee754 (float)");

  union {
    float_t out;
    int32_t in;
  } conv;

  conv.in = make_sortable32(value);
  return conv.out;

}

int64_t dtoi64(double_t value) {
  static_assert(std::numeric_limits<double_t>::is_iec559,
                "compiler does not support ieee754 (double)");

  union {
    double_t in;
    int64_t out;
  } conv;

  conv.in = value;
  return make_sortable64(conv.out);
}

double_t i64tod(int64_t value) {
  static_assert(std::numeric_limits<double_t>::is_iec559,
                "compiler does not support ieee754 (double)");

  union {
    double_t out;
    int64_t in;
  } conv;

  conv.in = make_sortable64(value);
  return conv.out;
}

const bytes_ref& finf32() {
  static_assert(std::numeric_limits<double_t>::is_iec559,
                "compiler does not support ieee754 (float)");
  static bytes_ref data = encode(static_buf<float_t, buf_id_t::INF>(), std::numeric_limits<float_t>::infinity());
  return data; 
}

const bytes_ref& nfinf32() {
  static_assert(std::numeric_limits<double_t>::is_iec559,
                "compiler does not support ieee754 (float)");
  static bytes_ref data = encode(static_buf<float_t, buf_id_t::NINF>(), -1*std::numeric_limits<float_t>::infinity());
  return data; 
}
const bytes_ref& minf32() {
  static bytes_ref data = encode(static_buf<float_t, buf_id_t::MIN>(), (std::numeric_limits<float_t>::min)());
  return data; 
}

const bytes_ref& maxf32() {
  static bytes_ref data = encode(static_buf<float_t, buf_id_t::MAX>(), (std::numeric_limits<float_t>::max)());
  return data; 
}

const bytes_ref& dinf64() {
  static_assert(std::numeric_limits<double_t>::is_iec559,
                "compiler does not support ieee754 (double)");
  static bytes_ref data = encode(static_buf<double_t, buf_id_t::INF>(), std::numeric_limits<double_t>::infinity());
  return data; 
}

const bytes_ref& ndinf64() {
  static_assert(std::numeric_limits<double_t>::is_iec559,
                "compiler does not support ieee754 (double)");
  static bytes_ref data = encode(static_buf<double_t, buf_id_t::NINF>(), -1*std::numeric_limits<double_t>::infinity());
  return data; 
}

const bytes_ref& mind64() {
  static bytes_ref data = encode(static_buf<double_t, buf_id_t::MIN>(), (std::numeric_limits<double_t>::min)());
  return data; 
}

const bytes_ref& maxd64(){
  static bytes_ref data = encode(static_buf<double_t, buf_id_t::MAX>(), (std::numeric_limits<double_t>::max)());
  return data; 
}

} // numeric_utils
} // ROOT
