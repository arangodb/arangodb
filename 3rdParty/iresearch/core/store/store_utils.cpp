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

#include "shared.hpp"
#include "store_utils.hpp"

#include "utils/std.hpp"
#include "utils/memory.hpp"

NS_ROOT

// ----------------------------------------------------------------------------
// --SECTION--                                               read/write helpers
// ----------------------------------------------------------------------------

void write_zvfloat(data_output& out, float_t v) {
  const int32_t iv = numeric_utils::ftoi32(v);

  if (iv >= -1 && iv <= 125) {
    // small signed values between [-1 and 125]
    out.write_byte(static_cast<byte_type>(0x80 | (1 + iv)));
  } else if (!std::signbit(v)) {
    // positive value
    out.write_int(iv);
  } else {
    // negative value
    out.write_byte(0xFF);
    out.write_int(iv);
  }
};

float_t read_zvfloat(data_input& in) {
  const uint32_t b = in.read_byte();

  if (0xFF == b) {
    // negative value
    return numeric_utils::i32tof(in.read_int());
  } else if (0 != (b & 0x80)) {
    // small signed value
    return float_t((b & 0x7F) - 1);
  }

  // positive float
  const uint32_t iv = (b << 24U)
    | (static_cast<uint32_t>(static_cast<uint16_t>(in.read_short())) << 8U)
    | static_cast<uint32_t>(in.read_byte());

  return numeric_utils::i32tof(iv);
}

void write_zvdouble(data_output& out, double_t v) {
  const int64_t lv = numeric_utils::dtoi64(v);

  if (lv > -1 && lv <= 124) {
    // small signed values between [-1 and 124]
    out.write_byte(static_cast<byte_type>(0x80 | (1 + lv)));
  } else {
    const float_t fv = static_cast< float_t >(v);

    if (fv == static_cast< double_t >( v ) ) {
      out.write_byte(0xFE);
      out.write_int(numeric_utils::ftoi32(fv));
    } else if (!std::signbit(v)) {
      // positive value
      out.write_long(lv);
    } else {
      // negative value
      out.write_byte(0xFF);
      out.write_long(lv);
    }
  }
}

double_t read_zvdouble(data_input& in) {
  const uint64_t b = in.read_byte();

  if (0xFF == b) {
    // negative value
    return numeric_utils::i64tod(in.read_long());
  } else if (0xFE == b) {
    // float value
    return numeric_utils::i32tof(in.read_int());
  } else if (0 != (b & 0x80)) {
    // small signed value
    return double_t((b & 0x7F) - 1U);
  }

  // positive double
  const uint64_t lv = (b << 56U)
    | (static_cast<uint64_t>(static_cast<uint32_t>(in.read_int())) << 24U)
    | (static_cast<uint64_t>(static_cast<uint16_t>(in.read_short())) << 8U)
    | static_cast<uint64_t>(in.read_byte());

  return numeric_utils::i64tod(lv);
}

/* -------------------------------------------------------------------
* skip helpers 
* ------------------------------------------------------------------*/

void skip(data_input& in, size_t to_skip,
          byte_type* skip_buf, size_t skip_buf_size) {
  assert(skip_buf);

  while (to_skip) {
    const auto step = std::min(skip_buf_size, to_skip);

#ifdef IRESEARCH_DEBUG
    const auto read = in.read_bytes(skip_buf, step);
    assert(read == step);
#else 
    in.read_bytes(skip_buf, step);
#endif // IRESEARCH_DEBUG

    to_skip -= step;
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                                              bit packing helpers
// ----------------------------------------------------------------------------

NS_BEGIN(encode)
NS_BEGIN(bitpack)

void skip_block32(index_input& in, uint32_t size) {
  assert(size);

  const uint32_t bits = in.read_vint();
  if (ALL_EQUAL == bits) {
    in.read_vint();
  } else {
    in.seek(in.file_pointer() + packed::bytes_required_32(size, bits));
  }
}

void skip_block64(index_input& in, uint64_t size) {
  assert(size);

  const uint32_t bits = in.read_vint();
  if (ALL_EQUAL == bits) {
    in.read_vlong();
  } else {
    in.seek(in.file_pointer() + packed::bytes_required_64(size, bits));
  }
}

void read_block(
    data_input& in,
    uint32_t size,
    uint32_t* RESTRICT encoded,
    uint32_t* RESTRICT decoded) {
  assert(size);
  assert(encoded);
  assert(decoded);

  const uint32_t bits = in.read_vint();
  if (ALL_EQUAL == bits) {
    std::fill(decoded, decoded + size, in.read_vint());
  } else {
    const size_t reqiured = packed::bytes_required_32(size, bits);

#ifdef IRESEARCH_DEBUG
    const auto read = in.read_bytes(
      reinterpret_cast<byte_type*>(encoded),
      reqiured 
    );
    assert(read == reqiured);
#else 
    in.read_bytes(
      reinterpret_cast<byte_type*>(encoded),
      reqiured 
    );
#endif // IRESEARCH_DEBUG

    packed::unpack(decoded, decoded + size, encoded, bits);
  }
}

void read_block(
    data_input& in,
    uint32_t size,
    uint64_t* RESTRICT encoded,
    uint64_t* RESTRICT decoded) {
  assert(size);
  assert(encoded);
  assert(decoded);

  const uint32_t bits = in.read_vint();
  if (ALL_EQUAL == bits) {
    std::fill(decoded, decoded + size, in.read_vlong());
  } else {
    const auto reqiured = packed::bytes_required_64(size, bits);

#ifdef IRESEARCH_DEBUG
    const auto read = in.read_bytes(
      reinterpret_cast<byte_type*>(encoded),
      reqiured 
    );
    assert(read == reqiured);
#else 
    in.read_bytes(
      reinterpret_cast<byte_type*>(encoded),
      reqiured
    );
#endif // IRESEARCH_DEBUG

    packed::unpack(decoded, decoded + size, encoded, bits);
  }
}

uint32_t write_block(
    data_output& out,
    const uint32_t* RESTRICT decoded,
    uint32_t size,
    uint32_t* RESTRICT encoded) {
  assert(size);
  assert(encoded);
  assert(decoded);

  if (irstd::all_equal(decoded, decoded + size)) {
    out.write_vint(ALL_EQUAL);
    out.write_vint(*decoded);
    return ALL_EQUAL;
  }

  const auto bits = packed::bits_required_32(
    *std::max_element(decoded, decoded + size)
  );

  std::memset(encoded, 0, sizeof(uint32_t) * size);
  packed::pack(decoded, decoded + size, encoded, bits);

  out.write_vint(bits);
  out.write_bytes(
    reinterpret_cast<const byte_type*>(encoded),
    packed::bytes_required_32(size, bits)
  );

  return bits;
}

uint32_t write_block(
    data_output& out,
    const uint64_t* RESTRICT decoded,
    uint32_t size,
    uint64_t* RESTRICT encoded) {
  assert(size);
  assert(encoded);
  assert(decoded);

  if (irstd::all_equal(decoded, decoded + size)) {
    out.write_vint(ALL_EQUAL);
    out.write_vlong(*decoded);
    return ALL_EQUAL;
  }

  const auto bits = packed::bits_required_64(
    *std::max_element(decoded, decoded + size)
  );

  std::memset(encoded, 0, sizeof(uint64_t) * size);
  packed::pack(decoded, decoded + size, encoded, bits);

  out.write_vint(bits);
  out.write_bytes(
    reinterpret_cast<const byte_type*>(encoded),
    packed::bytes_required_64(size, bits)
  );

  return bits;
}


NS_END // bitpack
NS_END // encode

// ----------------------------------------------------------------------------
// --SECTION--                                                      I/O streams
// ----------------------------------------------------------------------------

/* bytes_output */

bytes_output::bytes_output(size_t capacity) {
  oversize(buf_, capacity);
}

bytes_output::bytes_output(bytes_output&& other) NOEXCEPT
  : buf_(std::move(other.buf_)) {
  this->data_ = buf_.data();
  this->size_ = other.size();

}

bytes_output& bytes_output::operator=(bytes_output&& other) NOEXCEPT {
  if (this != &other) {
    buf_ = std::move(other.buf_);
    this->data_ = buf_.data();
    this->size_ = other.size();
    other.size_ = 0;
  }

  return *this;
}

/* bytes_ref_input */

bytes_ref_input::bytes_ref_input() : pos_(0) {}

bytes_ref_input::bytes_ref_input(const bytes_ref& ref)
  : data_(ref), pos_(0) {
}

byte_type bytes_ref_input::read_byte() {
  assert(pos_ <= data_.size());
  return this->data_[pos_++];
}

size_t bytes_ref_input::read_bytes( byte_type* b, size_t size) {
  size = std::min(size, data_.size() - pos_);
  std::memcpy( b, data_.c_str() + pos_, sizeof( byte_type ) * size );
  pos_ += size;
  return size;
  //assert( pos_ + size <= data_.size() );
}

void bytes_ref_input::read_bytes(bstring& buf, size_t size) {
  auto used = buf.size();

  buf.resize(used + size);

  #ifdef IRESEARCH_DEBUG
    const auto read = read_bytes(&(buf[0]) + used, size);
    assert(read == size);
  #else
    read_bytes(&(buf[0]) + used, size);
  #endif // IRESEARCH_DEBUG
}

/* bytes_input */

bytes_input::bytes_input() : pos_(0) {}

bytes_input::bytes_input(const bytes_ref& data):
  buf_(data.c_str(), data.size()), pos_(0) {
  this->data_ = buf_.data();
  this->size_ = data.size();
}

bytes_input::bytes_input(bytes_input&& other) NOEXCEPT
  : buf_(std::move(other.buf_)), pos_(other.pos_) {
  this->data_ = buf_.data();
  this->size_ = other.size();
  other.pos_ = 0;
  other.size_ = 0;
}

bytes_input& bytes_input::operator=(const bytes_ref& data) {
  if (this != &data) {
    buf_.assign(data.c_str(), data.size());
    this->data_ = buf_.data();
    this->size_ = data.size();
    pos_ = 0;
  }

  return *this;
}

bytes_input& bytes_input::operator=(bytes_input&& other) NOEXCEPT {
  if (this != &other) {
    buf_ = std::move(other.buf_);
    this->data_ = buf_.data();
    this->size_ = other.size();
    other.pos_ = 0;
    other.size_ = 0;
  }

  return *this;
}

void bytes_input::read_bytes(bstring& buf, size_t size) {
  auto used = buf.size();

  buf.resize(used + size);

  #ifdef IRESEARCH_DEBUG
    const auto read = read_bytes(&(buf[0]) + used, size);
    assert(read == size);
  #else
    read_bytes(&(buf[0]) + used, size);
  #endif // IRESEARCH_DEBUG
}

byte_type bytes_input::read_byte() {
  assert(pos_ <= size());
  return this->data_[pos_++];
}

size_t bytes_input::read_bytes(byte_type* b, size_t size) {
  assert(pos_ + size <= this->size());
  size = std::min(size, this->size() - pos_);
  std::memcpy(b, this->data_ + pos_, sizeof(byte_type) * size);
  pos_ += size;
  return size;

}

void bytes_input::read_from(data_input& in, size_t size) {
  if (!size) {
    /* nothing to read*/
    return;
  }

  oversize(buf_, size);

#ifdef IRESEARCH_DEBUG
  const auto read = in.read_bytes(&(buf_[0]), size);
  assert(read == size);
#else 
  in.read_bytes(&(buf_[0]), size);
#endif // IRESEARCH_DEBUG

  this->data_ = buf_.data();
  this->size_ = size;
  pos_ = 0;
}

NS_END
