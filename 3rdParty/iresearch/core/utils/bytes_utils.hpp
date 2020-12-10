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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_BYTES_UTILS_H
#define IRESEARCH_BYTES_UTILS_H

#include "shared.hpp"
#include "utils/bit_utils.hpp"
#include "utils/math_utils.hpp"
#include "utils/numeric_utils.hpp"

namespace iresearch {

template<typename T, size_t N = sizeof(T)>
struct bytes_io;

template<typename T>
struct bytes_io<T, sizeof(uint8_t)> {
  static const T const_max_vsize = 1;

  template<typename InputIterator>
  static T read(InputIterator& in, std::input_iterator_tag) {
    T out = static_cast<T>(*in);       ++in;

    return out;
  }

  template<typename InputIterator>
  static T vread(InputIterator& in, std::input_iterator_tag) {
    // read direct same as writen in vwrite(...)
    return read(in, typename std::iterator_traits<InputIterator>::iterator_category());
  }

  template<typename OutputIterator>
  static void write(OutputIterator& out, T value) {
    *out = static_cast<irs::byte_type>(value);       ++out;
  }

  template<typename OutputIterator>
  static void vwrite(OutputIterator& out, T value) {
    // write direct since no benefit from variable-size encoding
    write(out, value);
  }
}; // bytes_io<T, sizeof(uint8_t)>

template<typename T>
struct bytes_io<T, sizeof(uint16_t)> {
  static const T const_max_vsize = 2;

  template<typename InputIterator>
  static T read(InputIterator& in, std::input_iterator_tag) {
    T out = static_cast<T>(*in) << 8; ++in;
    out |= static_cast<T>(*in);       ++in;

    return out;
  }

  template<typename InputIterator>
  static T vread(InputIterator& in, std::input_iterator_tag) {
    // read direct same as writen in vwrite(...)
    return read(in, typename std::iterator_traits<InputIterator>::iterator_category());
  }

  template<typename OutputIterator>
  static void write(OutputIterator& out, T value) {
    *out = static_cast<irs::byte_type>(value >> 8);  ++out;
    *out = static_cast<irs::byte_type>(value);       ++out;
  }

  template<typename OutputIterator>
  static void vwrite(OutputIterator& out, T value) {
    // write direct since no benefit from variable-size encoding
    write(out, value);
  }
}; // bytes_io<T, sizeof(uint16_t)>

template<typename T>
struct bytes_io<T, sizeof(uint32_t)> {
  static const T const_max_vsize = 5;

  template<typename OutputIterator>
  static void vwrite(OutputIterator& out, T in) {
    while (in >= 0x80) {
      *out = static_cast<irs::byte_type>(in | 0x80); ++out;
      in >>= 7;
    }

    *out = static_cast<irs::byte_type>(in); ++out;
  }

  template<typename OutputIterator>
  static void write(OutputIterator& out, T in) {
    *out = static_cast<irs::byte_type>(in >> 24); ++out;
    *out = static_cast<irs::byte_type>(in >> 16); ++out;
    *out = static_cast<irs::byte_type>(in >> 8);  ++out;
    *out = static_cast<irs::byte_type>(in);       ++out;
  }

  static void write(byte_type*& out, T in) {
    if (!numeric_utils::is_big_endian()) {
      in = numeric_utils::hton32(in);
    }

    *reinterpret_cast<T*>(out) = in;
    out += sizeof(T);
  }

  template<typename InputIterator>
  static T vread(InputIterator& in, std::input_iterator_tag) {
    T out = *in; ++in; if (!(out & 0x80)) return out;

    T b;
    out -= 0x80;
    b = *in; ++in; out += b << 7; if (!(b & 0x80)) return out;
    out -= 0x80 << 7;
    b = *in; ++in; out += b << 14; if (!(b & 0x80)) return out;
    out -= 0x80 << 14;
    b = *in; ++in; out += b << 21; if (!(b & 0x80)) return out;
    out -= 0x80 << 21;
    b = *in; ++in; out += b << 28;
    // last byte always has MSB == 0, so we don't need to check and subtract 0x80

    return out;
  }

  template<typename InputIterator>
  static T read(InputIterator& in, std::input_iterator_tag) {
    T out = static_cast<T>(*in) << 24; ++in;
    out |= static_cast<T>(*in) << 16;  ++in;
    out |= static_cast<T>(*in) << 8;   ++in;
    out |= static_cast<T>(*in);        ++in;

    return out;
  }

  static T read(byte_type*& in) {
    auto value = *reinterpret_cast<T*>(in);

    if (!numeric_utils::is_big_endian()) {
      value = numeric_utils::ntoh32(value);
    }

    in += sizeof(uint32_t);
    return value;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @returns number of bytes required to store value in variable length format
  ////////////////////////////////////////////////////////////////////////////////
  FORCE_INLINE static uint32_t vsize(uint32_t value) {
    // compute 0 == value ? 1 : 1 + floor(log2(value)) / 7

    // OR 0x1 since log2_floor_32 does not accept 0
    const uint32_t log2 = math::log2_floor_32(value | 0x1);

    // division within range [1;31]
    return (73 + 9*log2) >> 6;
  }

  template<typename InputIterator>
  static int32_t zvread(InputIterator& in, std::input_iterator_tag) {
    return irs::zig_zag_decode32(vread(
      in, typename std::iterator_traits<InputIterator>::iterator_category()
    ));
  }

  template<typename OutputIterator>
  static void zvwrite(OutputIterator& out, int32_t value) {
    vwrite(out, zig_zag_encode32(value));
  }
}; // bytes_io<T, sizeof(uint32_t)>

template<typename T>
struct bytes_io<T, sizeof(uint64_t)> {
  static const T const_max_vsize = 10;

  template<typename OutputIterator>
  static void vwrite(OutputIterator& out, T in) {
    while (in >= T(0x80)) {
      *out = static_cast<irs::byte_type>(in | T(0x80)); ++out;
      in >>= 7;
    }

    *out = static_cast<irs::byte_type>(in); ++out;
  }

  template<typename OutputIterator>
  static void write(OutputIterator& out, T in) {
    typedef bytes_io<uint32_t, sizeof(uint32_t)> bytes_io_t;

    bytes_io_t::write(out, static_cast<uint32_t>(in >> 32));
    bytes_io_t::write(out, static_cast<uint32_t>(in));
  }

  static void write(byte_type*& out, T in) {
    if (!numeric_utils::is_big_endian()) {
      in = numeric_utils::hton64(in);
    }

    *reinterpret_cast<T*>(out) = in;
    out += sizeof(T);
  }

  template<typename InputIterator>
  static T vread(InputIterator& in, std::input_iterator_tag) {
    const T MASK = 0x80;
    T out = *in; ++in; if (!(out & MASK)) return out;

    T b;
    out -= MASK;
    b = *in; ++in; out += b << 7; if (!(b & MASK)) return out;
    out -= MASK << 7;
    b = *in; ++in; out += b << 14; if (!(b & MASK)) return out;
    out -= MASK << 14;
    b = *in; ++in; out += b << 21; if (!(b & MASK)) return out;
    out -= MASK << 21;
    b = *in; ++in; out += b << 28; if (!(b & MASK)) return out;
    out -= MASK << 28;
    b = *in; ++in; out += b << 35; if (!(b & MASK)) return out;
    out -= MASK << 35;
    b = *in; ++in; out += b << 42; if (!(b & MASK)) return out;
    out -= MASK << 42;
    b = *in; ++in; out += b << 49; if (!(b & MASK)) return out;
    out -= MASK << 49;
    b = *in; ++in; out += b << 56; if (!(b & MASK)) return out;
    out -= MASK << 56;
    b = *in; ++in; out += b << 63;
    // last byte always has MSB == 0, so we don't need to check and subtract 0x80

    return out;
  }

  template<typename InputIterator>
  static T read(InputIterator& in, std::input_iterator_tag) {
    typedef bytes_io<uint32_t, sizeof(uint32_t)> bytes_io_t;

    T out = static_cast<T>(bytes_io_t::read(in, std::input_iterator_tag{})) << 32;
    return out | static_cast<T>(bytes_io_t::read(in, std::input_iterator_tag{}));
  }

  static T read(byte_type*& in) {
    auto value = *reinterpret_cast<T*>(in);

    if (!numeric_utils::is_big_endian()) {
      value = numeric_utils::ntoh64(value);
    }

    in += sizeof(uint64_t);
    return value;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @returns number of bytes required to store value in variable length format
  ////////////////////////////////////////////////////////////////////////////////
  FORCE_INLINE static uint64_t vsize(uint64_t value) {
    // compute 0 == value ? 1 : 1 + floor(log2(value)) / 7

    // OR 0x1 since log2_floor_64 does not accept 0
    const uint64_t log2 = math::log2_floor_64(value | 0x1);

    // division within range [1;63]
    return (73 + 9*log2) >> 6;
  }

  template<typename InputIterator>
  static int64_t zvread(InputIterator& in, std::input_iterator_tag) {
    return zig_zag_decode64(vread(
      in, typename std::iterator_traits<InputIterator>::iterator_category()
    ));
  }

  template<typename OutputIterator>
  static void zvwrite(OutputIterator& out, int64_t value) {
    vwrite(out, zig_zag_encode64(value));
  }
}; // bytes_io<T, sizeof(uint64_t)>

// -----------------------------------------------------------------------------
// --SECTION--                              exported functions for reading bytes
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief read a raw value of type T from 'in'
///        will increment 'in' to position after the end of the read value
////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Iterator>
inline T read(Iterator& in) {
  return bytes_io<T, sizeof(T)>::read(in, typename std::iterator_traits<Iterator>::iterator_category());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read a variable-size encoded value of type T from 'in'
///        will increment 'in' to position after the end of the read value
///        variable-size encoding allows using less bytes for small values
////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Iterator>
inline T vread(Iterator& in) {
  return bytes_io<T, sizeof(T)>::vread(in, typename std::iterator_traits<Iterator>::iterator_category());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read a variable-size zig-zag encoded value of type T from 'in'
///        will increment 'in' to position after the end of the read value
///        variable-size encoding allows using less bytes for small values
////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Iterator>
inline T zvread(Iterator& in) {
  return bytes_io<T, sizeof(T)>::zvread(in, typename std::iterator_traits<Iterator>::iterator_category());
}

// -----------------------------------------------------------------------------
// --SECTION--                              exported functions for writing bytes
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief write a raw value 'value' to 'out'
///        will increment 'out' to position after the end of the written value
////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Iterator>
inline void write(Iterator& out, T value) {
  bytes_io<T, sizeof(T)>::write(out, value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write a variable-size encoded value 'value' to 'out'
///        will increment 'out' to position after the end of the written value
////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Iterator>
inline void vwrite(Iterator& out, T value) {
  bytes_io<T, sizeof(T)>::vwrite(out, value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write a variable-size zig-zag encoded value 'value' to 'out'
///        will increment 'out' to position after the end of the written value
////////////////////////////////////////////////////////////////////////////////
template<typename T, typename Iterator>
inline void zvwrite(Iterator& out, T value) {
  bytes_io<T, sizeof(T)>::zvwrite(out, value);
}

}

#endif // IRESEARCH_BYTES_UTILS_H
