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

#pragma once

#include "store/store_utils.hpp"
#include "utils/bitpack.hpp"

namespace irs::encode::avg {

// Encodes block denoted by [begin;end) using average encoding algorithm
// Returns block std::pair{ base, average }
inline std::tuple<uint64_t, uint64_t, bool> encode(uint64_t* begin,
                                                   uint64_t* end) noexcept {
  IRS_ASSERT(std::distance(begin, end) > 0 && std::is_sorted(begin, end));
  --end;

  const uint64_t base = *begin;
  const std::ptrdiff_t distance = std::distance(begin, end);

  const uint64_t avg =
    std::lround(static_cast<double>(*end - base) /
                (distance > 0 ? static_cast<double>(distance) : 1.0));

  uint64_t value = 0;
  *begin++ = 0;  // zig_zag_encode64(*begin - base - avg*0) == 0
  for (uint64_t avg_base = base; begin <= end; ++begin) {
    *begin = zig_zag_encode64(*begin - (avg_base += avg));
    value |= *begin;
  }

  return std::make_tuple(base, avg, !value);
}

// Encodes block denoted by [begin;end) using average encoding algorithm
// Returns block std::pair{ base, average }
inline std::pair<uint32_t, uint32_t> encode(uint32_t* begin,
                                            uint32_t* end) noexcept {
  IRS_ASSERT(std::distance(begin, end) > 0 && std::is_sorted(begin, end));
  --end;

  const uint32_t base = *begin;
  const std::ptrdiff_t distance =
    std::distance(begin, end);  // prevent division by 0

  const auto avg = static_cast<uint32_t>(std::lround(
    (*end - base) / (distance > 0 ? static_cast<double>(distance) : 1.0)));

  *begin++ = 0;  // zig_zag_encode32(*begin - base - avg*0) == 0
  for (uint32_t avg_base = base; begin <= end; ++begin) {
    *begin = zig_zag_encode32(*begin - (avg_base += avg));
  }

  return std::make_pair(base, avg);
}

// Visit average compressed block denoted by [begin;end) with the
// specified 'visitor'
template<typename Visitor>
inline void visit(uint64_t base, const uint64_t avg, uint64_t* begin,
                  uint64_t* end, Visitor visitor) {
  for (; begin != end; ++begin, base += avg) {
    visitor(base + zig_zag_decode64(*begin));
  }
}

// Visit average compressed block denoted by [begin;end) with the
// specified 'visitor'
template<typename Visitor>
inline void visit(uint32_t base, const uint32_t avg, uint32_t* begin,
                  uint32_t* end, Visitor visitor) {
  for (; begin != end; ++begin, base += avg) {
    visitor(base + zig_zag_decode32(*begin));
  }
}

// Visit average compressed, bit packed block denoted
// by [begin;begin+size) with the specified 'visitor'
template<typename Visitor>
inline void visit_packed(uint64_t base, const uint64_t avg, uint64_t* begin,
                         size_t size, const uint32_t bits, Visitor visitor) {
  for (size_t i = 0; i < size; ++i, base += avg) {
    visitor(base + zig_zag_decode64(packed::at(begin, i, bits)));
  }
}

// Visit average compressed, bit packed block denoted
// by [begin;begin+size) with the specified 'visitor'
template<typename Visitor>
inline void visit_packed(uint32_t base, const uint32_t avg, uint32_t* begin,
                         size_t size, const uint32_t bits, Visitor visitor) {
  for (size_t i = 0; i < size; ++i, base += avg) {
    visitor(base + zig_zag_decode32(packed::at(begin, i, bits)));
  }
}

// Decodes average compressed block denoted by [begin;end)
inline void decode(const uint64_t base, const uint64_t avg, uint64_t* begin,
                   uint64_t* end) {
  visit(base, avg, begin, end,
        [begin](uint64_t decoded) mutable { *begin++ = decoded; });
}

// Decodes average compressed block denoted by [begin;end)
inline void decode(const uint32_t base, const uint32_t avg, uint32_t* begin,
                   uint32_t* end) {
  visit(base, avg, begin, end,
        [begin](uint32_t decoded) mutable { *begin++ = decoded; });
}

template<typename PackFunc>
inline uint32_t write_block(
  PackFunc&& pack, data_output& out, const uint64_t base, const uint64_t avg,
  const uint64_t* IRS_RESTRICT decoded,
  const uint64_t size,  // same type as 'read_block'/'write_block'
  uint64_t* IRS_RESTRICT encoded) {
  out.write_vlong(base);
  out.write_vlong(avg);
  return bitpack::write_block64(std::forward<PackFunc>(pack), out, decoded,
                                size, encoded);
}

template<typename PackFunc>
inline uint32_t write_block(
  PackFunc&& pack, data_output& out, const uint32_t base, const uint32_t avg,
  const uint32_t* IRS_RESTRICT decoded,
  const uint32_t size,  // same type as 'read_block'/'write_block'
  uint32_t* IRS_RESTRICT encoded) {
  out.write_vint(base);
  out.write_vint(avg);
  return bitpack::write_block32(std::forward<PackFunc>(pack), out, decoded,
                                size, encoded);
}

// Skips average encoded 64-bit block
inline void skip_block32(index_input& in, uint32_t size) {
  in.read_vint();  // skip base
  in.read_vint();  // skip avg
  bitpack::skip_block32(in, size);
}

template<typename Visitor>
inline void visit_block_rl64(data_input& in, uint64_t base, const uint64_t avg,
                             size_t size, Visitor visitor) {
  base += in.read_vlong();
  for (; size; --size, base += avg) {
    visitor(base);
  }
}

template<typename Visitor>
inline void visit_block_rl32(data_input& in, uint32_t base, const uint32_t avg,
                             size_t size, Visitor visitor) {
  base += in.read_vint();
  for (; size; --size, base += avg) {
    visitor(base);
  }
}

inline bool check_block_rl64(data_input& in, uint64_t expected_avg) {
  in.read_vlong();  // skip base
  const uint64_t avg = in.read_vlong();
  const uint32_t bits = in.read_vint();
  const uint64_t value = in.read_vlong();

  return expected_avg == avg && bitpack::ALL_EQUAL == bits &&
         0 == value;  // delta
}

inline bool check_block_rl32(data_input& in, uint32_t expected_avg) {
  in.read_vint();  // skip base
  const uint32_t avg = in.read_vint();
  const uint32_t bits = in.read_vint();
  const uint32_t value = in.read_vint();

  return expected_avg == avg && bitpack::ALL_EQUAL == bits &&
         0 == value;  // delta
}

inline bool read_block_rl64(data_input& in, uint64_t& base, uint64_t& avg) {
  base = in.read_vlong();
  avg = in.read_vlong();
  const uint32_t bits = in.read_vint();
  const uint64_t value = in.read_vlong();

  return bitpack::ALL_EQUAL == bits && 0 == value;  // delta
}

inline bool read_block_rl32(data_input& in, uint32_t& base, uint32_t& avg) {
  base = in.read_vint();
  avg = in.read_vint();
  const uint32_t bits = in.read_vint();
  const uint32_t value = in.read_vint();

  return bitpack::ALL_EQUAL == bits && 0 == value;  // delta
}

template<typename Visitor>
inline void visit_block_packed_tail(data_input& in, size_t size,
                                    uint64_t* packed, Visitor visitor) {
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
    sizeof(uint64_t) * packed::blocks_required_64(block_size, bits));

  visit_packed(base, avg, packed, size, bits, visitor);
}

template<typename Visitor>
inline void visit_block_packed_tail(data_input& in, uint32_t size,
                                    uint32_t* packed, Visitor visitor) {
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
    sizeof(uint32_t) * packed::blocks_required_32(block_size, bits));

  visit_packed(base, avg, packed, size, bits, visitor);
}

template<typename Visitor>
inline void visit_block_packed(data_input& in, size_t size, uint64_t* packed,
                               Visitor visitor) {
  const uint64_t base = in.read_vlong();
  const uint64_t avg = in.read_vlong();
  const uint32_t bits = in.read_vint();

  if (bitpack::ALL_EQUAL == bits) {
    visit_block_rl64(in, base, avg, size, visitor);
    return;
  }

  in.read_bytes(reinterpret_cast<byte_type*>(packed),
                sizeof(uint64_t) * packed::blocks_required_64(size, bits));

  visit_packed(base, avg, packed, size, bits, visitor);
}

template<typename Visitor>
inline void visit_block_packed(data_input& in, uint32_t size, uint32_t* packed,
                               Visitor visitor) {
  const uint32_t base = in.read_vint();
  const uint32_t avg = in.read_vint();
  const uint32_t bits = in.read_vint();

  if (bitpack::ALL_EQUAL == bits) {
    visit_block_rl32(in, base, avg, size, visitor);
    return;
  }

  in.read_bytes(reinterpret_cast<byte_type*>(packed),
                sizeof(uint32_t) * packed::blocks_required_32(size, bits));

  visit_packed(base, avg, packed, size, bits, visitor);
}

}  // namespace irs::encode::avg
