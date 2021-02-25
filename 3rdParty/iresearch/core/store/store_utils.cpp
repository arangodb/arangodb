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
#include "store_utils.hpp"

#include "utils/crc.hpp"
#include "utils/std.hpp"
#include "utils/string_utils.hpp"
#include "utils/memory.hpp"

namespace {

using namespace irs;

constexpr size_t BLOCK_SIZE = 128;

FORCE_INLINE void pack_block(
    const uint32_t* RESTRICT decoded,
    uint32_t* RESTRICT encoded,
    const uint32_t bits) noexcept {
  assert(encoded);
  assert(decoded);

  packed::pack_block(decoded, encoded, bits);
  packed::pack_block(decoded + packed::BLOCK_SIZE_32, encoded + bits, bits);
  packed::pack_block(decoded + 2*packed::BLOCK_SIZE_32, encoded + 2*bits, bits);
  packed::pack_block(decoded + 3*packed::BLOCK_SIZE_32, encoded + 3*bits, bits);
}

FORCE_INLINE void unpack_block(
    uint32_t* RESTRICT decoded,
    const uint32_t* RESTRICT encoded,
    const uint32_t bits) noexcept {
  assert(encoded);
  assert(decoded);

  packed::unpack_block(encoded, decoded, bits);
  packed::unpack_block(encoded + bits, decoded + packed::BLOCK_SIZE_32, bits);
  packed::unpack_block(encoded + 2*bits, decoded + 2*packed::BLOCK_SIZE_32, bits);
  packed::unpack_block(encoded + 3*bits, decoded + 3*packed::BLOCK_SIZE_32, bits);
}

FORCE_INLINE void pack_block(
    const uint64_t* RESTRICT decoded,
    uint64_t* RESTRICT encoded,
    const uint32_t bits) noexcept {
  assert(encoded);
  assert(decoded);

  packed::pack_block(decoded, encoded, bits);
  packed::pack_block(decoded + packed::BLOCK_SIZE_64, encoded + bits, bits);
}

FORCE_INLINE void unpack_block(
    uint64_t* RESTRICT decoded,
    const uint64_t* RESTRICT encoded,
    const uint64_t bits) noexcept {
  assert(encoded);
  assert(decoded);

  packed::unpack_block(encoded, decoded, bits);
  packed::unpack_block(encoded + bits, decoded + packed::BLOCK_SIZE_64, bits);
}

}

namespace iresearch {

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

  // positive float (ensure read order)
  const auto part = uint32_t(uint16_t(in.read_short())) << 8;

  return numeric_utils::i32tof(
    (b << 24) | part | uint32_t(in.read_byte())
  );
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
    return double_t((b & 0x7F) - 1);
  }

  // positive double (ensure read order)
  const auto part1 = uint64_t(uint32_t(in.read_int())) << 24;
  const auto part2 = uint64_t(uint16_t(in.read_short())) << 8;

  return numeric_utils::i64tod(
    (b << 56) | part1 | part2 | uint64_t(in.read_byte())
  );
}

// ----------------------------------------------------------------------------
// --SECTION--                                              bit packing helpers
// ----------------------------------------------------------------------------

namespace encode {
namespace bitpack {

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
    const size_t required = packed::bytes_required_32(size, bits);

    const auto* buf = in.read_buffer(required, BufferHint::NORMAL);

    if (buf) {
      packed::unpack(
        decoded, decoded + size,
        reinterpret_cast<const uint32_t*>(buf), bits);

      return;
    }

#ifdef IRESEARCH_DEBUG
    const auto read = in.read_bytes(
      reinterpret_cast<byte_type*>(encoded),
      required);
    assert(read == required);
    UNUSED(read);
#else
    in.read_bytes(
      reinterpret_cast<byte_type*>(encoded),
      required);
#endif // IRESEARCH_DEBUG

    packed::unpack(decoded, decoded + size, encoded, bits);
  }
}

void read_block(
    data_input& in,
    uint32_t* RESTRICT encoded,
    uint32_t* RESTRICT decoded) {
  assert(encoded);
  assert(decoded);
  constexpr size_t BLOCK_SIZE = packed::BLOCK_SIZE_32*4;

  const uint32_t bits = in.read_vint();
  if (ALL_EQUAL == bits) {
    std::fill(decoded, decoded + BLOCK_SIZE, in.read_vint());
  } else {
    const size_t required = packed::bytes_required_32(BLOCK_SIZE, bits);

    const auto* buf = in.read_buffer(required, BufferHint::NORMAL);

    if (buf) {
      ::unpack_block(decoded, reinterpret_cast<const uint32_t*>(buf), bits);

      return;
    }

#ifdef IRESEARCH_DEBUG
    const auto read = in.read_bytes(
      reinterpret_cast<byte_type*>(encoded),
      required);
    assert(read == required);
    UNUSED(read);
#else
    in.read_bytes(
      reinterpret_cast<byte_type*>(encoded),
      required);
#endif // IRESEARCH_DEBUG

    ::unpack_block(decoded, encoded, bits);
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
    const auto required = packed::bytes_required_64(size, bits);

    const auto* buf = in.read_buffer(required, BufferHint::NORMAL);

    if (buf) {
      packed::unpack(decoded, decoded + size,
                     reinterpret_cast<const uint64_t*>(buf), bits);
      return;
    }

#ifdef IRESEARCH_DEBUG
    const auto read = in.read_bytes(
      reinterpret_cast<byte_type*>(encoded),
      required);
    assert(read == required);
    UNUSED(read);
#else
    in.read_bytes(
      reinterpret_cast<byte_type*>(encoded),
      required);
#endif // IRESEARCH_DEBUG

    packed::unpack(decoded, decoded + size, encoded, bits);
  }
}

void read_block(
    data_input& in,
    uint64_t* RESTRICT encoded,
    uint64_t* RESTRICT decoded) {
  assert(encoded);
  assert(decoded);
  constexpr size_t BLOCK_SIZE = packed::BLOCK_SIZE_64*2;

  const uint32_t bits = in.read_vint();
  if (ALL_EQUAL == bits) {
    std::fill(decoded, decoded + BLOCK_SIZE, in.read_vlong());
  } else {
    const auto required = packed::bytes_required_64(BLOCK_SIZE, bits);

    const auto* buf = in.read_buffer(required, BufferHint::NORMAL);

    if (buf) {
      ::unpack_block(decoded, reinterpret_cast<const uint64_t*>(buf), bits);

      return;
    }

#ifdef IRESEARCH_DEBUG
    const auto read = in.read_bytes(
      reinterpret_cast<byte_type*>(encoded),
      required);
    assert(read == required);
    UNUSED(read);
#else
    in.read_bytes(
      reinterpret_cast<byte_type*>(encoded),
      required);
#endif // IRESEARCH_DEBUG

    unpack_block(decoded, encoded, bits);
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
    decoded, decoded + size
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
    const uint32_t* RESTRICT decoded,
    uint32_t* RESTRICT encoded) {
  assert(encoded);
  assert(decoded);
  if (irstd::all_equal(decoded, decoded + BLOCK_SIZE)) {
    out.write_vint(ALL_EQUAL);
    out.write_vint(*decoded);
    return ALL_EQUAL;
  }

  const auto bits = packed::bits_required_32(decoded, decoded + BLOCK_SIZE);

  std::memset(encoded, 0, sizeof(uint32_t) * BLOCK_SIZE);
  ::pack_block(decoded, encoded, bits);

  out.write_vint(bits);
  out.write_bytes(
    reinterpret_cast<const byte_type*>(encoded),
    packed::bytes_required_32(BLOCK_SIZE, bits));

  return bits;
}

uint32_t write_block(
    data_output& out,
    const uint64_t* RESTRICT decoded,
    uint64_t size,
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
    decoded, decoded + size);

  std::memset(encoded, 0, sizeof(uint64_t) * size);
  packed::pack(decoded, decoded + size, encoded, bits);

  out.write_vint(bits);
  out.write_bytes(
    reinterpret_cast<const byte_type*>(encoded),
    packed::bytes_required_64(size, bits));

  return bits;
}

uint32_t write_block(
    data_output& out,
    const uint64_t* RESTRICT decoded,
    uint64_t* RESTRICT encoded) {
  assert(encoded);
  assert(decoded);
  constexpr size_t BLOCK_SIZE = packed::BLOCK_SIZE_64*2;

  if (irstd::all_equal(decoded, decoded + BLOCK_SIZE)) {
    out.write_vint(ALL_EQUAL);
    out.write_vlong(*decoded);
    return ALL_EQUAL;
  }

  const auto bits = packed::bits_required_64(decoded, decoded + BLOCK_SIZE);

  std::memset(encoded, 0, sizeof(uint64_t) * BLOCK_SIZE);
  ::pack_block(decoded, encoded, bits);

  out.write_vint(bits);
  out.write_bytes(
    reinterpret_cast<const byte_type*>(encoded),
    packed::bytes_required_64(BLOCK_SIZE, bits));

  return bits;
}

} // bitpack
} // encode

// ----------------------------------------------------------------------------
// --SECTION--                                   bytes_ref_input implementation
// ----------------------------------------------------------------------------

bytes_ref_input::bytes_ref_input(const bytes_ref& ref)
  : data_(ref), pos_(data_.begin()) {
}

size_t bytes_ref_input::read_bytes(byte_type* b, size_t size) {
  size = std::min(size, size_t(std::distance(pos_, data_.end())));
  std::memcpy(b, pos_, sizeof(byte_type) * size);
  pos_ += size;
  return size;
}

// append to buf
void bytes_ref_input::read_bytes(bstring& buf, size_t size) {
  auto used = buf.size();

  buf.resize(used + size);

  #ifdef IRESEARCH_DEBUG
    const auto read = read_bytes(&(buf[0]) + used, size);
    assert(read == size);
    UNUSED(read);
  #else
    read_bytes(&(buf[0]) + used, size);
  #endif // IRESEARCH_DEBUG
}

int64_t bytes_ref_input::checksum(size_t offset) const {
  crc32c crc;

  crc.process_block(pos_, std::min(pos_ + offset, data_.end()));

  return crc.checksum();
}

}
