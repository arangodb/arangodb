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
// --SECTION--                                   bytes_ref_input implementation
// ----------------------------------------------------------------------------

bytes_ref_input::bytes_ref_input(const bytes_ref& ref)
  : data_(ref), pos_(data_.begin()) {
}

size_t bytes_ref_input::read_bytes(byte_type* b, size_t size) noexcept {
  size = std::min(size, size_t(std::distance(pos_, data_.end())));
  std::memcpy(b, pos_, sizeof(byte_type) * size);
  pos_ += size;
  return size;
}

size_t bytes_ref_input::read_bytes(size_t offset, byte_type* b, size_t size) noexcept {
  if (offset < data_.size()) {
    size = std::min(size, size_t(data_.size() - offset));
    std::memcpy(b, data_.begin() + offset, sizeof(byte_type) * size);
    pos_ = data_.begin() + offset + size;
    return size;
  }

  pos_ = data_.end();
  return 0;
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
