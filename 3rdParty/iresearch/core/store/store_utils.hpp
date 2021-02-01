////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///

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

#ifndef IRESEARCH_STORE_UTILS_H
#define IRESEARCH_STORE_UTILS_H

#include "shared.hpp"

#include "directory.hpp"
#include "data_output.hpp"
#include "data_input.hpp"

#include "utils/string.hpp"
#include "utils/bit_utils.hpp"
#include "utils/bytes_utils.hpp"
#include "utils/bit_packing.hpp"
#include "utils/numeric_utils.hpp"
#include "utils/attributes.hpp"
#include "utils/std.hpp"

#include <unordered_set>

namespace {

using iresearch::data_input;
using iresearch::data_output;

template<typename T, size_t N = sizeof(T)>
struct read_write_helper{
  static T read(data_input& in);
  static T write(data_output& out, T size);
};

template<typename T>
struct read_write_helper<T, sizeof(uint32_t)> {
  inline static T read(data_input& in) {
    return in.read_vint();
  }

  inline static void write(data_output& out, T in) {
    out.write_vint(in);
  }
};

template<typename T>
struct read_write_helper<T, sizeof(uint64_t)> {
  inline static T read(data_input& in) {
    return in.read_vlong();
  }

  inline static void write(data_output& out, T in) {
    out.write_vlong(in);
  }
};

} // LOCAL

namespace iresearch {


template<
  typename StringType,
  typename TraitsType = typename StringType::traits_type
> StringType to_string(const byte_type* begin) {
  typedef typename TraitsType::char_type char_type;

  const auto size = irs::vread<uint32_t>(begin);

  return StringType(
    reinterpret_cast<const char_type*>(begin),
    size
  );
}

// ----------------------------------------------------------------------------
// --SECTION--                                               read/write helpers
// ----------------------------------------------------------------------------

struct enum_hash {
  template<typename T>
  size_t operator()(T value) const {
    typedef typename std::enable_if<
      std::is_enum<T>::value,
      typename std::underlying_type<T>::type
    >::type underlying_type_t;

    return static_cast<underlying_type_t>(value);
  }
};

template<typename T>
void write_enum(data_output& out, T value) {
  typedef typename std::enable_if<
    std::is_enum<T>::value,
    typename std::underlying_type<T>::type
  >::type underlying_type_t;

  ::read_write_helper<underlying_type_t>::write(
    out, static_cast<underlying_type_t>(value)
  );
}

template<typename T>
T read_enum(data_input& in) {
  typedef typename std::enable_if<
    std::is_enum<T>::value,
    typename std::underlying_type<T>::type
  >::type underlying_type_t;

  return static_cast<T>(
    ::read_write_helper<underlying_type_t>::read(in)
  );
}

inline void write_size(data_output& out, size_t size) {
  ::read_write_helper<size_t>::write(out, size);
}

inline size_t read_size(data_input& in) {
  return ::read_write_helper<size_t>::read(in);
}

void IRESEARCH_API write_zvfloat(data_output& out, float_t v);

float_t IRESEARCH_API read_zvfloat(data_input& in);

void IRESEARCH_API write_zvdouble(data_output& out, double_t v);

double_t IRESEARCH_API read_zvdouble(data_input& in);

inline void write_zvint(data_output& out, int32_t v) {
  out.write_vint(zig_zag_encode32(v));
}

inline int32_t read_zvint(data_input& in) {
  return zig_zag_decode32(in.read_vint());
}

inline void write_zvlong(data_output& out, int64_t v) {
  out.write_vlong(zig_zag_encode64(v));
}

inline int64_t read_zvlong(data_input& in) {
  return zig_zag_decode64(in.read_vlong());
}

inline void write_string(data_output& out, const char* s, size_t len) {
  assert(len < integer_traits<uint32_t>::const_max);
  out.write_vint(uint32_t(len));
  out.write_bytes(reinterpret_cast<const byte_type*>(s), len);
}

inline void write_string(data_output& out, const byte_type* s, size_t len) {
  assert(len < integer_traits<uint32_t>::const_max);
  out.write_vint(uint32_t(len));
  out.write_bytes(s, len);
}

template<typename StringType>
inline void write_string(data_output& out, const StringType& str) {
  write_string(out, str.c_str(), str.size());
}

template<typename ContType>
inline data_output& write_strings(data_output& out, const ContType& c) {
  write_size(out, c.size());
  for (const auto& s : c) {
    write_string<decltype(s)>(out, s);
  }

  return out;
}

template<typename StringType>
inline StringType read_string(data_input& in) {
  const size_t len = in.read_vint();

  StringType str(len, 0);
#ifdef IRESEARCH_DEBUG
  const size_t read = in.read_bytes(reinterpret_cast<byte_type*>(&str[0]), str.size());
  assert(read == str.size());
  UNUSED(read);
#else
  in.read_bytes(reinterpret_cast<byte_type*>(&str[0]), str.size());
#endif // IRESEARCH_DEBUG
  return str;
}

template< typename ContType >
inline ContType read_strings(data_input& in) {
  ContType c;

  const size_t size = read_size(in);
  c.reserve(size);

  for (size_t i = 0; i < size; ++i) {
    c.emplace(read_string<typename ContType::value_type>(in));
  }

  return c;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                    bytes helpers
// ----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief write to 'out' array of data pointed by 'value' of length 'size'
/// @return bytes written
////////////////////////////////////////////////////////////////////////////////
template<typename OutputIterator, typename T>
size_t write_bytes(OutputIterator& out, const T* value, size_t size) {
  auto* data = reinterpret_cast<const byte_type*>(value);

  size = sizeof(T) * size;

  // write data out byte-by-byte
  for (auto i = size; i; --i) {
    *out = *data;
    ++out;
    ++data;
  }

  return size;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write to 'out' raw byte representation of data in 'value'
/// @return bytes written
////////////////////////////////////////////////////////////////////////////////
template<typename OutputIterator, typename T>
size_t write_bytes(OutputIterator& out, const T& value) {
  return write_bytes(out, &value, 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read a value of the specified type from 'in'
////////////////////////////////////////////////////////////////////////////////
template<typename T>
T& read_ref(const byte_type*& in) {
  auto& data = reinterpret_cast<T&>(*in);

  in += sizeof(T); // increment past value

  return data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read an array of the specified type and length of 'size' from 'in'
////////////////////////////////////////////////////////////////////////////////
template<typename T>
T* read_ref(const byte_type*& in, size_t size) {
  auto* data = reinterpret_cast<T*>(&(*in));

  in += sizeof(T) * size; // increment past value

  return data;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                   string helpers
// ----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief write to 'out' size + data pointed by 'value' of length 'size'
////////////////////////////////////////////////////////////////////////////////
template<typename OutputIterator, typename CharType>
void vwrite_string(OutputIterator& out, const CharType* value, size_t size) {
  vwrite<uint64_t>(out, size);
  write_bytes(out, value, size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write to 'out' data in 'value'
////////////////////////////////////////////////////////////////////////////////
template<typename OutputIterator, typename StringType>
void vwrite_string(OutputIterator& out, const StringType& value) {
  vwrite_string(out, value.c_str(), value.size());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read a string + size into a value of type 'StringType' from 'in'
////////////////////////////////////////////////////////////////////////////////
template<
  typename StringType,
  typename TraitsType = typename StringType::traits_type
>
StringType vread_string(const byte_type*& in) {
  typedef typename TraitsType::char_type char_type;
  const auto size = vread<uint64_t>(in);

  return StringType(read_ref<const char_type>(in, size), size);
}

// ----------------------------------------------------------------------------
// --SECTION--                                              bit packing helpers
// ----------------------------------------------------------------------------

FORCE_INLINE uint64_t shift_pack_64(uint64_t val, bool b) noexcept {
  assert(val <= UINT64_C(0x7FFFFFFFFFFFFFFF));
  return (val << 1) | uint64_t(b);
}

FORCE_INLINE uint32_t shift_pack_32(uint32_t val, bool b) noexcept {
  assert(val <= UINT32_C(0x7FFFFFFF));
  return (val << 1) | uint32_t(b);
}

FORCE_INLINE bool shift_unpack_64(uint64_t in, uint64_t& out) noexcept {
  out = in >> 1;
  return in & 1;
}

FORCE_INLINE bool shift_unpack_32(uint32_t in, uint32_t& out) noexcept {
  out = in >> 1;
  return in & 1;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                      I/O streams
// ----------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////////
/// @class bytes_output
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API bytes_output final : public data_output {
 public:
  explicit bytes_output(bstring& buf) noexcept
    : buf_(&buf) {
  }

  virtual void write_byte(byte_type b) override {
    (*buf_) += b;
  }

  virtual void write_bytes(const byte_type* b, size_t size) override {
    buf_->append(b, size);
  }

  virtual void close() override { }

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  bstring* buf_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // bytes_output

//////////////////////////////////////////////////////////////////////////////
/// @class bytes_ref_input
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API bytes_ref_input : public index_input {
 public:
  bytes_ref_input() = default;
  explicit bytes_ref_input(const bytes_ref& data);

  void skip(size_t size) noexcept {
    assert(pos_ + size <= data_.end());
    pos_ += size;
  }

  virtual void seek(size_t pos) noexcept override {
    assert(data_.begin() + pos <= data_.end());
    pos_ = data_.begin() + pos;
  }

  virtual size_t file_pointer() const noexcept override {
    return std::distance(data_.begin(), pos_);
  }

  virtual size_t length() const noexcept override {
    return data_.size();
  }

  virtual bool eof() const noexcept override {
    return pos_ >= data_.end();
  }

  virtual byte_type read_byte() override final {
    assert(pos_ < data_.end());
    return *pos_++;
  }

  virtual const byte_type* read_buffer(size_t size, BufferHint /*hint*/) noexcept final {
    const auto* pos = pos_ + size;

    if (pos > data_.end()) {
      return nullptr;
    }

    std::swap(pos, pos_);
    return pos;
  }

  virtual size_t read_bytes(byte_type* b, size_t size) override final;

  // append to buf
  void read_bytes(bstring& buf, size_t size);

  void reset(const byte_type* data, size_t size) noexcept {
    data_ = bytes_ref(data, size);
    pos_ = data;
  }

  void reset(const bytes_ref& ref) noexcept {
    reset(ref.c_str(), ref.size());
  }

  virtual ptr dup() const override {
    return memory::make_unique<bytes_ref_input>(*this);
  }

  virtual ptr reopen() const override {
    return dup();
  }

  virtual int32_t read_int() override final {
    return irs::read<uint32_t>(pos_);
  }

  virtual int64_t read_long() override final {
    return irs::read<uint64_t>(pos_);
  }

  virtual uint64_t read_vlong() override final {
    return irs::vread<uint64_t>(pos_);
  }

  virtual uint32_t read_vint() override final {
    return irs::vread<uint32_t>(pos_);
  }

  virtual int64_t checksum(size_t offset) const override final;

 private:
  bytes_ref data_;
  const byte_type* pos_{ data_.begin() };
}; // bytes_ref_input

namespace encode {

// ----------------------------------------------------------------------------
// --SECTION--                                bit packing encode/decode helpers
// ----------------------------------------------------------------------------
//
// Normal packed block has the following structure:
//   <BlockHeader>
//     </NumberOfBits>
//   </BlockHeader>
//   </PackedData>
//
// In case if all elements in a block are equal:
//   <BlockHeader>
//     <ALL_EQUAL>
//   </BlockHeader>
//   </PackedData>
//
// ----------------------------------------------------------------------------

namespace bitpack {

const uint32_t ALL_EQUAL = 0U;

// returns true if one can use run length encoding for the specified numberof bits
inline bool rl(const uint32_t bits) {
  return ALL_EQUAL == bits;
}

// skip block of the specified size that was previously
// written with the corresponding 'write_block' function
IRESEARCH_API void skip_block32(index_input& in, uint32_t size);

// skip block of the specified size that was previously
// written with the corresponding 'write_block' function
IRESEARCH_API void skip_block64(index_input& in, uint64_t size);

// reads block of the specified size from the stream
// that was previously encoded with the corresponding
// 'write_block' funcion
IRESEARCH_API void read_block(
  data_input& in,
  uint32_t size,
  uint32_t* RESTRICT encoded,
  uint32_t* RESTRICT decoded);

// reads block of 128 integers from the stream
// that was previously encoded with the corresponding
// 'write_block' funcion
IRESEARCH_API void read_block(
  data_input& in,
  uint32_t* RESTRICT encoded,
  uint32_t* RESTRICT decoded);

// reads block of the specified size from the stream
// that was previously encoded with the corresponding
// 'write_block' funcion
IRESEARCH_API void read_block(
  data_input& in,
  uint32_t size,
  uint64_t* RESTRICT encoded,
  uint64_t* RESTRICT decoded);

// reads block of 128 integers from the stream
// that was previously encoded with the corresponding
// 'write_block' funcion
IRESEARCH_API void read_block(
  data_input& in,
  uint64_t* RESTRICT encoded,
  uint64_t* RESTRICT decoded);

// writes block of 128 integers to a stream
//   all values are equal -> RL encoding,
//   otherwise            -> bit packing
// returns number of bits used to encoded the block (0 == RL)
IRESEARCH_API uint32_t write_block(
  data_output& out,
  const uint32_t* RESTRICT decoded,
  uint32_t* RESTRICT encoded);

// writes block of the specified size to stream
//   all values are equal -> RL encoding,
//   otherwise            -> bit packing
// returns number of bits used to encoded the block (0 == RL)
IRESEARCH_API uint32_t write_block(
  data_output& out,
  const uint32_t* RESTRICT decoded,
  uint32_t size,
  uint32_t* RESTRICT encoded);

// writes block of the specified size to stream
//   all values are equal -> RL encoding,
//   otherwise            -> bit packing
// returns number of bits used to encoded the block (0 == RL)
IRESEARCH_API uint32_t write_block(
  data_output& out,
  const uint64_t* RESTRICT decoded,
  uint64_t size, // same type as 'decoded'/'encoded'
  uint64_t* RESTRICT encoded);

// writes block of 128 integers to a stream
//   all values are equal -> RL encoding,
//   otherwise            -> bit packing
// returns number of bits used to encoded the block (0 == RL)
IRESEARCH_API uint32_t write_block(
  data_output& out,
  const uint64_t* RESTRICT decoded,
  uint64_t* RESTRICT encoded);

}

// ----------------------------------------------------------------------------
// --SECTION--                                      delta encode/decode helpers
// ----------------------------------------------------------------------------

namespace delta {

template<typename Iterator>
inline void decode(Iterator begin, Iterator end) {
  assert(std::distance(begin, end) > 0);

  typedef typename std::iterator_traits<Iterator>::value_type value_type;
  const auto second = begin+1;

  std::transform(second, end, begin, second, std::plus<value_type>());
}

template<typename Iterator>
inline void encode(Iterator begin, Iterator end) {
  assert(std::distance(begin, end) > 0);

  typedef typename std::iterator_traits<Iterator>::value_type value_type;
  const auto rend = irstd::make_reverse_iterator(begin);
  const auto rbegin = irstd::make_reverse_iterator(end);

  std::transform(
    rbegin + 1, rend, rbegin, rbegin,
    [](const value_type& lhs, const value_type& rhs) {
      return rhs - lhs;
  });
}

} // delta

// ----------------------------------------------------------------------------
// --SECTION--                                        avg encode/decode helpers
// ----------------------------------------------------------------------------

namespace avg {

// Encodes block denoted by [begin;end) using average encoding algorithm
// Returns block std::pair{ base, average }
inline std::pair<uint64_t, uint64_t> encode(uint64_t* begin, uint64_t* end) {
  assert(std::distance(begin, end) > 0 && std::is_sorted(begin, end));
  --end;

  const uint64_t base = *begin;

  const std::ptrdiff_t distance[] { 1, std::distance(begin, end) }; // prevent division by 0
  const uint64_t avg = std::lround(
    static_cast<double_t>(*end - base) / distance[distance[1] > 0]
  );

  *begin++ = 0; // zig_zag_encode64(*begin - base - avg*0) == 0
  for (uint64_t avg_base = base; begin <= end; ++begin) {
    *begin = zig_zag_encode64(*begin - (avg_base += avg));
  }

  return std::make_pair(base, avg);
}

// Encodes block denoted by [begin;end) using average encoding algorithm
// Returns block std::pair{ base, average }
inline std::pair<uint32_t, uint32_t> encode(uint32_t* begin, uint32_t* end) {
  assert(std::distance(begin, end) > 0 && std::is_sorted(begin, end));
  --end;

  const uint32_t base = *begin;

  const std::ptrdiff_t distance[] { 1, std::distance(begin, end) }; // prevent division by 0
  const uint32_t avg = std::lround(
    static_cast<float_t>(*end - base) / distance[distance[1] > 0]
  );

  *begin++ = 0; // zig_zag_encode32(*begin - base - avg*0) == 0
  for (uint32_t avg_base = base; begin <= end; ++begin) {
    *begin = zig_zag_encode32(*begin - (avg_base += avg));
  }

  return std::make_pair(base, avg);
}

// Visit average compressed block denoted by [begin;end) with the
// specified 'visitor'
template<typename Visitor>
inline void visit(
    uint64_t base, const uint64_t avg,
    uint64_t* begin, uint64_t* end,
    Visitor visitor) {
  for (; begin != end; ++begin, base += avg) {
    visitor(base + zig_zag_decode64(*begin));
  }
}

// Visit average compressed block denoted by [begin;end) with the
// specified 'visitor'
template<typename Visitor>
inline void visit(
    uint32_t base, const uint32_t avg,
    uint32_t* begin, uint32_t* end,
    Visitor visitor) {
  for (; begin != end; ++begin, base += avg) {
    visitor(base + zig_zag_decode32(*begin));
  }
}

// Visit average compressed, bit packed block denoted
// by [begin;begin+size) with the specified 'visitor'
template<typename Visitor>
inline void visit_packed(
    uint64_t base,  const uint64_t avg,
    uint64_t* begin, size_t size,
    const uint32_t bits, Visitor visitor) {
  for (size_t i = 0; i < size; ++i, base += avg) {
    visitor(base + zig_zag_decode64(packed::at(begin, i, bits)));
  }
}

// Visit average compressed, bit packed block denoted
// by [begin;begin+size) with the specified 'visitor'
template<typename Visitor>
inline void visit_packed(
    uint32_t base,  const uint32_t avg,
    uint32_t* begin, size_t size,
    const uint32_t bits, Visitor visitor) {
  for (size_t i = 0; i < size; ++i, base += avg) {
    visitor(base + zig_zag_decode32(packed::at(begin, i, bits)));
  }
}

// Decodes average compressed block denoted by [begin;end)
inline void decode(
    const uint64_t base, const uint64_t avg,
    uint64_t* begin, uint64_t* end) {
  visit(base, avg, begin, end, [begin](uint64_t decoded) mutable {
    *begin++ = decoded;
  });
}

// Decodes average compressed block denoted by [begin;end)
inline void decode(
    const uint32_t base, const uint32_t avg,
    uint32_t* begin, uint32_t* end) {
  visit(base, avg, begin, end, [begin](uint32_t decoded) mutable {
    *begin++ = decoded;
  });
}

inline uint32_t write_block(
    data_output& out,
    const uint64_t base,
    const uint64_t avg,
    const uint64_t* RESTRICT decoded,
    const uint64_t size, // same type as 'read_block'/'write_block'
    uint64_t* RESTRICT encoded) {
  out.write_vlong(base);
  out.write_vlong(avg);
  return bitpack::write_block(out, decoded, size, encoded);
}

inline uint32_t write_block(
    data_output& out,
    const uint32_t base,
    const uint32_t avg,
    const uint32_t* RESTRICT decoded,
    const uint32_t size, // same type as 'read_block'/'write_block'
    uint32_t* RESTRICT encoded) {
  out.write_vint(base);
  out.write_vint(avg);
  return bitpack::write_block(out, decoded, size, encoded);
}

// Skips average encoded 64-bit block
inline void skip_block64(index_input& in, size_t size) {
  in.read_vlong(); // skip base
  in.read_vlong(); // skip avg
  bitpack::skip_block64(in, size);
}

// Skips average encoded 64-bit block
inline void skip_block32(index_input& in, uint32_t size) {
  in.read_vint(); // skip base
  in.read_vint(); // skip avg
  bitpack::skip_block32(in, size);
}

template<typename Visitor>
inline void visit_block_rl64(
    data_input& in,
    uint64_t base,
    const uint64_t avg,
    size_t size,
    Visitor visitor) {
  base += in.read_vlong();
  for (; size; --size, base += avg) {
    visitor(base);
  }
}

template<typename Visitor>
inline void visit_block_rl32(
    data_input& in,
    uint32_t base,
    const uint32_t avg,
    size_t size,
    Visitor visitor) {
  base += in.read_vint();
  for (; size; --size, base += avg) {
    visitor(base);
  }
}

inline bool check_block_rl64(
    data_input& in,
    uint64_t expected_avg) {
  in.read_vlong(); // skip base
  const uint64_t avg = in.read_vlong();
  const uint32_t bits = in.read_vint();
  const uint64_t value = in.read_vlong();

  return expected_avg == avg
    && bitpack::ALL_EQUAL == bits
    && 0 == value; // delta
}

inline bool check_block_rl32(
    data_input& in,
    uint32_t expected_avg) {
  in.read_vint(); // skip base
  const uint32_t avg = in.read_vint();
  const uint32_t bits = in.read_vint();
  const uint32_t value = in.read_vint();

  return expected_avg == avg
    && bitpack::ALL_EQUAL == bits
    && 0 == value; // delta
}

inline bool read_block_rl64(
    irs::data_input& in,
    uint64_t& base,
    uint64_t& avg) {
  base = in.read_vlong();
  avg = in.read_vlong();
  const uint32_t bits = in.read_vint();
  const uint64_t value = in.read_vlong();

  return bitpack::ALL_EQUAL == bits
    && 0 == value; // delta
}

inline bool read_block_rl32(
    irs::data_input& in,
    uint32_t& base,
    uint32_t& avg) {
  base = in.read_vint();
  avg = in.read_vint();
  const uint32_t bits = in.read_vint();
  const uint32_t value = in.read_vint();

  return bitpack::ALL_EQUAL == bits
    && 0 == value; // delta
}

template<typename Visitor>
inline void visit_block_packed_tail(
    data_input& in,
    size_t size,
    uint64_t* packed,
    Visitor visitor) {
  const uint64_t base = in.read_vlong();
  const uint64_t avg = in.read_vlong();
  const uint32_t bits = in.read_vint();

  if (bitpack::ALL_EQUAL == bits) {
    visit_block_rl64(in, base, avg, size, visitor);
    return;
  }

  const size_t block_size = math::ceil64(size, packed::BLOCK_SIZE_64);

  in.read_bytes(
    reinterpret_cast<byte_type*>(packed),
    sizeof(uint64_t)*packed::blocks_required_64(block_size, bits)
  );

  visit_packed(base, avg, packed, size, bits, visitor);
}

template<typename Visitor>
inline void visit_block_packed_tail(
    data_input& in,
    uint32_t size,
    uint32_t* packed,
    Visitor visitor) {
  const uint32_t base = in.read_vint();
  const uint32_t avg = in.read_vint();
  const uint32_t bits = in.read_vint();

  if (bitpack::ALL_EQUAL == bits) {
    visit_block_rl32(in, base, avg, size, visitor);
    return;
  }

  const uint32_t block_size = math::ceil32(size, packed::BLOCK_SIZE_32);

  in.read_bytes(
    reinterpret_cast<byte_type*>(packed),
    sizeof(uint32_t)*packed::blocks_required_32(block_size, bits)
  );

  visit_packed(base, avg, packed, size, bits, visitor);
}

template<typename Visitor>
inline void visit_block_packed(
    data_input& in,
    size_t size,
    uint64_t* packed,
    Visitor visitor) {
  const uint64_t base = in.read_vlong();
  const uint64_t avg = in.read_vlong();
  const uint32_t bits = in.read_vint();

  if (bitpack::ALL_EQUAL == bits) {
    visit_block_rl64(in, base, avg, size, visitor);
    return;
  }

  in.read_bytes(
    reinterpret_cast<byte_type*>(packed),
    sizeof(uint64_t)*packed::blocks_required_64(size, bits)
  );

  visit_packed(base, avg, packed, size, bits, visitor);
}

template<typename Visitor>
inline void visit_block_packed(
    data_input& in,
    uint32_t size,
    uint32_t* packed,
    Visitor visitor) {
  const uint32_t base = in.read_vint();
  const uint32_t avg = in.read_vint();
  const uint32_t bits = in.read_vint();

  if (bitpack::ALL_EQUAL == bits) {
    visit_block_rl32(in, base, avg, size, visitor);
    return;
  }

  in.read_bytes(
    reinterpret_cast<byte_type*>(packed),
    sizeof(uint32_t)*packed::blocks_required_32(size, bits)
  );

  visit_packed(base, avg, packed, size, bits, visitor);
}

} // avg

} // encode

} // ROOT

#endif
