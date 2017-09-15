//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_STORE_UTILS_H
#define IRESEARCH_STORE_UTILS_H

#include "shared.hpp"

#include "directory.hpp"
#include "data_output.hpp"
#include "data_input.hpp"

#include "utils/string.hpp"
#include "utils/bit_utils.hpp"
#include "utils/bit_packing.hpp"
#include "utils/numeric_utils.hpp"
#include "utils/attributes.hpp"
#include "utils/std.hpp"

#include <unordered_set>

NS_LOCAL

using iresearch::data_input;
using iresearch::data_output;

template<typename T>
struct read_write_helper {
  static T read(data_input& in);
  static T write(data_output& out, T size);
};

template<>
struct read_write_helper<uint32_t> {
  inline static uint32_t read( data_input& in ) { 
    return in.read_vint();
  }

  inline static void write( data_output& out, uint32_t size ) {
    out.write_vint(size);
  }
};

template<>
struct read_write_helper<uint64_t> {
  inline static uint64_t read( data_input& in ) {
    return in.read_vlong();
  }

  inline static void write( data_output& out, uint64_t size ) {
    out.write_vlong(size);
  }
};

NS_END // LOCAL

NS_BEGIN(detail)

template<class T, T max_size_val>
struct vencode_traits_base {
    static const T const_max_size = max_size_val;
};

NS_END // detail

NS_ROOT

inline byte_type* write_vint(uint32_t v, byte_type* begin) {
  while (v >= 0x80) {
    *begin++ = static_cast<byte_type>(v | 0x80);
    v >>= 7;
  }

  *begin = static_cast<byte_type>(v);
  return begin + 1;
}

inline byte_type* write_vlong(uint64_t v, byte_type* begin) {
  while (v >= uint64_t(0x80)) {
    *begin++ = static_cast<byte_type>(v | uint64_t(0x80));
    v >>= 7;
  }

  *begin = static_cast<byte_type>(v);
  return begin + 1;
}

inline std::pair<uint32_t, const byte_type*> read_vint(const byte_type* begin) {
  uint32_t out = *begin++; if (!(out & 0x80)) return std::make_pair(out, begin);

  uint32_t b;
  out -= 0x80;
  b = *begin++; out += b << 7; if (!(b & 0x80)) return std::make_pair(out, begin);
  out -= 0x80 << 7;
  b = *begin++; out += b << 14; if (!(b & 0x80)) return std::make_pair(out, begin);
  out -= 0x80 << 14;
  b = *begin++; out += b << 21; if (!(b & 0x80)) return std::make_pair(out, begin);
  out -= 0x80 << 21;
  b = *begin++; out += b << 28;
  // last byte always has MSB == 0, so we don't need to subtract 0x80

  return std::make_pair(out, begin);
}

inline std::pair<uint64_t, const byte_type*> read_vlong(const byte_type* begin) {
  const uint64_t MASK = 0x80;
  uint64_t out = *begin++; if (!(out & MASK)) return std::make_pair(out, begin);

  uint64_t b;
  out -= MASK;
  b = *begin++; out += b << 7; if (!(b & MASK)) return std::make_pair(out, begin);
  out -= MASK << 7;
  b = *begin++; out += b << 14; if (!(b & MASK)) return std::make_pair(out, begin);
  out -= MASK << 14;
  b = *begin++; out += b << 21; if (!(b & MASK)) return std::make_pair(out, begin);
  out -= MASK << 21;
  b = *begin++; out += b << 28; if (!(b & MASK)) return std::make_pair(out, begin);
  out -= MASK << 28;
  b = *begin++; out += b << 35; if (!(b & MASK)) return std::make_pair(out, begin);
  out -= MASK << 35;
  b = *begin++; out += b << 42; if (!(b & MASK)) return std::make_pair(out, begin);
  out -= MASK << 42;
  b = *begin++; out += b << 49; if (!(b & MASK)) return std::make_pair(out, begin);
  out -= MASK << 49;
  b = *begin++; out += b << 56; if (!(b & MASK)) return std::make_pair(out, begin);
  out -= MASK << 56;
  b = *begin++; out += b << 63;
  // last byte always has MSB == 0, so we don't need to subtract MASK

  return std::make_pair(out, begin);
}

template<typename StringType>
StringType to_string(const byte_type* const begin) {
  const auto res = read_vint(begin);
  return StringType(reinterpret_cast<const char*>(res.second), res.first);
}

////////////////////////////////////////////////////////////////////////////////
/// @returns number of bytes required to store value in variable length format
////////////////////////////////////////////////////////////////////////////////
FORCE_INLINE uint32_t vencode_size_32(uint32_t value) {
  // compute 0 == value ? 1 : 1 + floor(log2(value)) / 7

  // OR 0x1 since log2_floor_32 does not accept 0
  const uint32_t log2 = math::log2_floor_32(value | 0x1);

  // division within range [1;31]
  return (73 + 9*log2) >> 6;
}

////////////////////////////////////////////////////////////////////////////////
/// @returns number of bytes required to store value in variable length format
////////////////////////////////////////////////////////////////////////////////
FORCE_INLINE uint64_t vencode_size_64(uint64_t value) {
  // compute 0 == value ? 1 : 1 + floor(log2(value)) / 7

  // OR 0x1 since log2_floor_64 does not accept 0
  const uint64_t log2 = math::log2_floor_64(value | 0x1);

  // division within range [1;63]
  return (73 + 9*log2) >> 6;
}

template<typename T>
struct vencode_traits {
  static size_t size(T value);
  CONSTEXPR static size_t max_size();
  static byte_type* write(T value, byte_type* dst);
  static std::pair<T, const byte_type*> read(const byte_type* begin);
}; // vencode_traits

template<>
struct vencode_traits<uint32_t>: ::detail::vencode_traits_base<size_t, 5> {
  typedef uint32_t type;

  static size_t size(type v) {
    return vencode_size_32(v);
  }

  CONSTEXPR static size_t max_size() {
    return const_max_size; // may take up to 5 bytes
  }

  static byte_type* write(type v, byte_type* begin) {
    return write_vint(v, begin);
  }

  static std::pair<type, const byte_type*> read(const byte_type* begin) {
    return read_vint(begin);
  }
}; // vencode_traits<uint32_t>

template<>
struct vencode_traits<uint64_t>: ::detail::vencode_traits_base<size_t, 10> {
  typedef uint64_t type;

  static size_t size(type v) {
    return vencode_size_64(v);
  }

  CONSTEXPR static size_t max_size() {
    return const_max_size; // may take up to 10 bytes
  }

  static byte_type* write(type v, byte_type* begin) {
    return write_vlong(v, begin);
  }

  static std::pair<type, const byte_type*> read(const byte_type* begin) {
    return read_vlong(begin);
  }
}; // vencode_traits<uint64_t>

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

inline void write_size( data_output& out, size_t size ) {
  ::read_write_helper<size_t>::write( out, size );
}

inline size_t read_size( data_input& in ) {
  return ::read_write_helper<size_t>::read( in );
}

void IRESEARCH_API write_zvfloat(data_output& out, float_t v);

float_t IRESEARCH_API read_zvfloat(data_input& in);

void IRESEARCH_API write_zvdouble(data_output& out, double_t v);

double_t IRESEARCH_API read_zvdouble(data_input& in);

inline void write_zvint( data_output& out, int32_t v ) {
  out.write_vint( zig_zag_encode32( v ) );
}

inline int32_t read_zvint( data_input& in ) {
  return zig_zag_decode32( in.read_vint() );
}

inline void write_zvlong( data_output& out, int64_t v ) {
  out.write_vlong( zig_zag_encode64( v ) );
}

inline int64_t read_zvlong( data_input& in ) {
  return zig_zag_decode64( in.read_vlong() );
}

inline void write_string( data_output& out, const char* s, size_t len ) {
  assert(len < integer_traits<uint32_t>::const_max);
  out.write_vint(uint32_t(len));
  out.write_bytes(reinterpret_cast<const byte_type*>(s), len);
}

inline void write_string(data_output& out, const byte_type* s, size_t len) {
  assert(len < integer_traits<uint32_t>::const_max);
  out.write_vint(uint32_t(len));
  out.write_bytes(s, len);
}

template< typename StringType >
inline void write_string(data_output& out, const StringType& str) {
  write_string(out, str.c_str(), str.size());
}

template< typename ContType >
inline data_output& write_strings(data_output& out, const ContType& c) {
  write_size(out, c.size());
  for (const auto& s : c) {
    write_string< decltype(s) >(out, s);
  }

  return out;
}

template< typename StringType >
inline StringType read_string(data_input& in) {
  const size_t len = in.read_vint();

  StringType str(len, 0);
#ifdef IRESEARCH_DEBUG
  const size_t read = in.read_bytes(reinterpret_cast<byte_type*>(&str[0]), len);
  assert(read == len);
#else
  in.read_bytes(reinterpret_cast<byte_type*>(&str[0]), len);
#endif // IRESEARCH_DEBUG

  return str;
}

template< typename ContType >
inline ContType read_strings(data_input& in) {
  ContType c;

  const size_t size = read_size(in);
  c.reserve(size);

  for (size_t i = 0; i < size; ++i) {
    c.emplace(read_string< typename ContType::value_type >(in));
  }

  return c;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                     skip helpers 
// ----------------------------------------------------------------------------

const uint64_t SKIP_BUFFER_SIZE = 1024U;

IRESEARCH_API void skip(
  data_input& in, size_t to_skip,
  byte_type* skip_buf, size_t skip_buf_size
);

// ----------------------------------------------------------------------------
// --SECTION--                                              bit packing helpers
// ----------------------------------------------------------------------------

FORCE_INLINE uint64_t shift_pack_64(uint64_t val, bool b) {
  assert(val <= 0x7FFFFFFFFFFFFFFFLL);
  return (val << 1) | (b ? 1 : 0);
}

FORCE_INLINE uint32_t shift_pack_32(uint32_t val, bool b) {
  assert(val <= 0x7FFFFFFF);
  return (val << 1) | (b ? 1 : 0);
}

FORCE_INLINE bool shift_unpack_64(uint64_t in, uint64_t& out) {
  out = in >> 1;
  return in & 1;
}

FORCE_INLINE bool shift_unpack_32(uint32_t in, uint32_t& out) {
  out = in >> 1;
  return in & 1;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                      I/O streams
// ----------------------------------------------------------------------------

class IRESEARCH_API bytes_output final: public data_output, public bytes_ref {
 public:
  bytes_output() = default;
  explicit bytes_output( size_t capacity );
  bytes_output(bytes_output&& rhs) NOEXCEPT;
  bytes_output& operator=(bytes_output&& rhs) NOEXCEPT;

  void reset(size_t size = 0) { 
    this->size_ = size; 
  }

  virtual void write_byte( byte_type b ) override {
    oversize(buf_, this->size() + 1).replace(this->size(), 1, 1, b);
    this->data_ = buf_.data();
    ++this->size_;
  }

  virtual void write_bytes( const byte_type* b, size_t size ) override {
    oversize(buf_, this->size() + size).replace(this->size(), size, b, size);
    this->data_ = buf_.data();
    this->size_ += size;
  }

  virtual void close() override { }

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  bstring buf_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

class IRESEARCH_API bytes_ref_input : public index_input {
 public:
  bytes_ref_input();
  explicit bytes_ref_input(const bytes_ref& data);

  void skip(size_t size) NOEXCEPT {
    assert(pos_ + size < data_.size());
    pos_ += size;
  }

  virtual void seek(size_t pos) NOEXCEPT override {
    assert(pos < data_.size());
    pos_ = pos;
  }

  virtual size_t file_pointer() const NOEXCEPT override {
    return pos_;
  }

  virtual size_t length() const NOEXCEPT override {
    return data_.size();
  }

  virtual bool eof() const NOEXCEPT override {
    return pos_ >= data_.size();
  }

  virtual byte_type read_byte() override;
  virtual size_t read_bytes(byte_type* b, size_t size) override;
  void read_bytes(bstring& buf, size_t size); // append to buf

  void reset(const byte_type* data, size_t size) NOEXCEPT {
    data_ = bytes_ref(data, size);
    pos_ = 0;
  }

  void reset(const bytes_ref& ref) NOEXCEPT {
    reset(ref.c_str(), ref.size());
  }

  virtual ptr dup() const NOEXCEPT {
    try {
      return index_input::make<bytes_ref_input>(*this);
    } catch (...) {
      return nullptr;
    }
  }

  virtual ptr reopen() const NOEXCEPT {
    return dup();
  }

 private:
  bytes_ref data_;
  size_t pos_;
}; // bytes_ref_input

class IRESEARCH_API bytes_input final: public data_input, public bytes_ref {
 public:
  bytes_input();
  explicit bytes_input(const bytes_ref& data);
  bytes_input(bytes_input&& rhs) NOEXCEPT;
  bytes_input& operator=(bytes_input&& rhs) NOEXCEPT;
  bytes_input& operator=(const bytes_ref& data);

  void read_from(data_input& in, size_t size);

  void skip(size_t size) {
    assert(pos_ + size <= this->size());
    pos_ += size;
  }

  void seek(size_t pos) {
    assert(pos <= this->size());
    pos_ = pos;
  }

  virtual size_t file_pointer() const override { 
    return pos_; 
  }

  virtual size_t length() const override { 
    return this->size(); 
  }

  virtual bool eof() const override {
    return pos_ >= this->size();
  }

  virtual byte_type read_byte() override;
  virtual size_t read_bytes(byte_type* b, size_t size) override;
  void read_bytes(bstring& buf, size_t size); // append to buf

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  bstring buf_;
  size_t pos_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

NS_BEGIN(encode)

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

NS_BEGIN(bitpack)

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
  uint32_t* RESTRICT decoded
);

// reads block of the specified size from the stream
// that was previously encoded with the corresponding
// 'write_block' funcion
IRESEARCH_API void read_block(
  data_input& in,
  uint32_t size,
  uint64_t* RESTRICT encoded,
  uint64_t* RESTRICT decoded
);

// writes block of the specified size to stream
//   all values are equal -> RL encoding,
//   otherwise            -> bit packing
// returns number of bits used to encoded the block (0 == RL)
IRESEARCH_API uint32_t write_block(
  data_output& out,
  const uint32_t* RESTRICT decoded,
  uint32_t size,
  uint32_t* RESTRICT encoded
);

// writes block of the specified size to stream
//   all values are equal -> RL encoding,
//   otherwise            -> bit packing
// returns number of bits used to encoded the block (0 == RL)
IRESEARCH_API uint32_t write_block(
  data_output& out,
  const uint64_t* RESTRICT decoded,
  uint32_t size,
  uint64_t* RESTRICT encoded
);


NS_END

// ----------------------------------------------------------------------------
// --SECTION--                                      delta encode/decode helpers
// ----------------------------------------------------------------------------

NS_BEGIN(delta)

template<typename Iterator>
inline void decode(Iterator begin, Iterator end) {
  assert(std::distance(begin, end) > 0);

  typedef typename std::iterator_traits<Iterator>::value_type value_type;
  const auto second = begin+1;

  std::transform(second, end, begin, second, std::plus<value_type>());

  assert(std::is_sorted(begin, end));
}

template<typename Iterator>
inline void encode(Iterator begin, Iterator end) {
  assert(std::distance(begin, end) > 0 && std::is_sorted(begin, end));

  typedef typename std::iterator_traits<Iterator>::value_type value_type;
  const auto rend = irstd::make_reverse_iterator(begin);
  const auto rbegin = irstd::make_reverse_iterator(end);

  std::transform(
    rbegin + 1, rend, rbegin, rbegin,
    [](const value_type& lhs, const value_type& rhs) {
      return rhs - lhs;
  });
}

NS_END // delta

// ----------------------------------------------------------------------------
// --SECTION--                                        avg encode/decode helpers
// ----------------------------------------------------------------------------

NS_BEGIN(avg)

typedef std::pair<
  uint64_t, // base
  uint64_t // avg
> stats;

// Encodes block denoted by [begin;end) using average encoding algorithm
// Returns block base & average
inline stats encode(uint64_t* begin, uint64_t* end) {
  assert(std::distance(begin, end) > 0 && std::is_sorted(begin, end));
  --end;

  const uint64_t base = *begin;

  const std::ptrdiff_t distance[] { 1, std::distance(begin, end) }; // prevent division by 0
  const uint64_t avg = std::lround(
    static_cast<double_t>(*end - base) / distance[distance[1] > 0]
  );

  *begin++ = 0; // zig_zag_encode64(*begin - base - avg*0) == 0
  for (size_t avg_base = base; begin <= end; ++begin) {
    *begin = zig_zag_encode64(*begin - (avg_base += avg));
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

// Decodes average compressed block denoted by [begin;end)
inline void decode(
    const uint64_t base, const uint64_t avg,
    uint64_t* begin, uint64_t* end) {
  visit(base, avg, begin, end, [begin](uint64_t decoded) mutable {
    *begin++ = decoded;
  });
}

inline uint32_t write_block(
    data_output& out,
    const uint64_t base,
    const uint64_t avg,
    const uint64_t* RESTRICT decoded,
    const size_t size,
    uint64_t* RESTRICT encoded) {
  out.write_vlong(base);
  out.write_vlong(avg);
  return bitpack::write_block(out, decoded, size, encoded);
}

// Skips average encoded 64-bit block
inline void skip_block64(index_input& in, size_t size) {
  in.read_vlong(); // skip base
  in.read_vlong(); // skip avg
  bitpack::skip_block64(in, size);
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

NS_END // avg

NS_END // encode

NS_END // ROOT

#endif
