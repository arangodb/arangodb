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

#ifndef IRESEARCH_CRC_H
#define IRESEARCH_CRC_H

#include <shared.hpp>

#ifdef IRESEARCH_SSE4_2

#include <nmmintrin.h>

namespace iresearch {

class crc32c {
 public:
 explicit crc32c(uint32_t seed = 0) noexcept
   : value_(seed) {
 }

 FORCE_INLINE void process_bytes(const void* buffer, size_t size) noexcept {
   const auto* begin = reinterpret_cast<const uint8_t*>(buffer);
   process_block(begin, begin + size);
 }

 FORCE_INLINE void process_block(const void* buffer_begin, const void* buffer_end) noexcept {
   const auto* begin = process_block_32(buffer_begin, buffer_end);
   const auto* end = reinterpret_cast<const uint8_t*>(buffer_end);

   for (;begin != end; ++begin) {
     value_ = _mm_crc32_u8(value_, *begin);
   }
 }

 FORCE_INLINE uint32_t checksum() const noexcept {
   return value_;
 }

 private:
  FORCE_INLINE const uint8_t* process_block_32(const void* buffer_begin, const void* buffer_end) noexcept {
    const size_t BLOCK_SIZE = 8*sizeof(uint32_t);

    const auto k = std::distance(
      reinterpret_cast<const uint8_t*>(buffer_begin),
      reinterpret_cast<const uint8_t*>(buffer_end)
    ) / BLOCK_SIZE;

    const auto* begin = reinterpret_cast<const uint32_t*>(buffer_begin);
    const auto* end = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(buffer_begin) + k*BLOCK_SIZE);

    for (; begin != end; ++begin) {
      value_ = _mm_crc32_u32(value_, *begin);
    }

    return reinterpret_cast<const uint8_t*>(begin);
  }

  uint32_t value_;
}; // crc32c

}

#else

#if defined(_MSC_VER)
  #pragma warning(disable : 4244)
  #pragma warning(disable : 4245)
#elif defined (__GNUC__)
  // NOOP
#endif

#include <boost/crc.hpp>

#if defined(_MSC_VER)
  #pragma warning(default: 4244)
  #pragma warning(default: 4245)
#elif defined (__GNUC__)
  // NOOP
#endif

namespace iresearch {

typedef boost::crc_optimal<32, 0x1EDC6F41, 0, 0, true, true> crc32c;

}

#endif // IRESEARCH_SSE
#endif // IRESEARCH_CRC_H
