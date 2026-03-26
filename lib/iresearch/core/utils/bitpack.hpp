////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "shared.hpp"
#include "store/data_input.hpp"
#include "store/data_output.hpp"
#include "utils/simd_utils.hpp"

namespace irs {

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

inline constexpr uint32_t ALL_EQUAL = 0U;

// returns true if one can use run length encoding for the specified numberof
// bits
constexpr bool rl(const uint32_t bits) noexcept { return ALL_EQUAL == bits; }

// skip block of the specified size that was previously
// written with the corresponding 'write_block' function
inline void skip_block32(index_input& in, uint32_t size) {
  IRS_ASSERT(size);

  const uint32_t bits = in.read_byte();
  if (ALL_EQUAL == bits) {
    in.read_vint();
  } else {
    in.seek(in.file_pointer() + packed::bytes_required_32(size, bits));
  }
}

// writes block of 'size' 32 bit integers to a stream
//   all values are equal -> RL encoding,
//   otherwise            -> bit packing
// returns number of bits used to encoded the block (0 == RL)
template<typename PackFunc>
IRS_FORCE_INLINE uint32_t write_block32(PackFunc&& pack, data_output& out,
                                        const uint32_t* IRS_RESTRICT decoded,
                                        uint32_t size,
                                        uint32_t* IRS_RESTRICT encoded) {
  IRS_ASSERT(decoded);
  IRS_ASSERT(size);
  if (simd::all_equal<false>(decoded, size)) {
    out.write_byte(ALL_EQUAL);
    out.write_vint(*decoded);
    return ALL_EQUAL;
  }

  // prior AVX2 scalar version works faster for 32-bit values
#ifdef HWY_CAP_GE256
  const uint32_t bits = simd::maxbits<false>(decoded, size);
#else
  const uint32_t bits = packed::maxbits32(decoded, decoded + size);
#endif
  IRS_ASSERT(bits);
  IRS_ASSERT(encoded);

  const size_t buf_size = packed::bytes_required_32(size, bits);
  // TODO(MBkkt) memset looks unnecessary
  std::memset(encoded, 0, buf_size);
  pack(decoded, encoded, bits);

  // TODO(MBkkt) direct write api?
  //  out.get_buffer(buf_size + 1, /*fallback=*/encoded)?
  out.write_byte(static_cast<byte_type>(bits & 0xFF));
  out.write_bytes(reinterpret_cast<byte_type*>(encoded), buf_size);

  return bits;
}

template<uint32_t Size, typename PackFunc>
uint32_t write_block32(PackFunc&& pack, data_output& out,
                       const uint32_t* IRS_RESTRICT decoded,
                       uint32_t* IRS_RESTRICT encoded) {
  return write_block32(std::forward<PackFunc>(pack), out, decoded, Size,
                       encoded);
}

// writes block of 'size' 64 bit integers to a stream
//   all values are equal -> RL encoding,
//   otherwise            -> bit packing
// returns number of bits used to encoded the block (0 == RL)
template<typename PackFunc>
uint32_t write_block64(PackFunc&& pack, data_output& out,
                       const uint64_t* IRS_RESTRICT decoded, uint64_t size,
                       uint64_t* IRS_RESTRICT encoded) {
  IRS_ASSERT(size);
  IRS_ASSERT(encoded);
  IRS_ASSERT(decoded);

  if (simd::all_equal<false>(decoded, size)) {
    out.write_byte(ALL_EQUAL);
    out.write_vlong(*decoded);
    return ALL_EQUAL;
  }

  // scalar version is always faster for 64-bit values
  const uint32_t bits = packed::maxbits64(decoded, decoded + size);

  const size_t buf_size = packed::bytes_required_64(size, bits);
  std::memset(encoded, 0, buf_size);
  pack(decoded, encoded, size, bits);

  out.write_byte(static_cast<byte_type>(bits & 0xFF));
  out.write_bytes(reinterpret_cast<const byte_type*>(encoded), buf_size);

  return bits;
}

template<typename UnpackFunc>
IRS_FORCE_INLINE void read_block_impl32(UnpackFunc&& unpack, data_input& in,
                                        uint32_t* IRS_RESTRICT encoded,
                                        uint32_t size,
                                        uint32_t* IRS_RESTRICT decoded) {
  IRS_ASSERT(size);
  IRS_ASSERT(encoded);
  IRS_ASSERT(decoded);

  const uint32_t bits = in.read_byte();
  if (ALL_EQUAL == bits) {
    std::fill_n(decoded, size, in.read_vint());
  } else {
    const size_t required = packed::bytes_required_32(size, bits);

    const auto* buf = in.read_buffer(required, BufferHint::NORMAL);

    if (buf) {
      unpack(decoded, reinterpret_cast<const uint32_t*>(buf), bits);
      return;
    }

    [[maybe_unused]] const auto read =
      in.read_bytes(reinterpret_cast<byte_type*>(encoded), required);
    IRS_ASSERT(read == required);

    unpack(decoded, encoded, bits);
  }
}

// reads block of 'Size' 32 bit integers from the stream
// that was previously encoded with the corresponding
// 'write_block32' function
template<uint32_t Size, typename UnpackFunc>
void read_block32(UnpackFunc&& unpack, data_input& in,
                  uint32_t* IRS_RESTRICT encoded,
                  uint32_t* IRS_RESTRICT decoded) {
  return read_block_impl32(std::forward<UnpackFunc>(unpack), in, encoded, Size,
                           decoded);
}

}  // namespace bitpack
}  // namespace irs
