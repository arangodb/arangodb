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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////


#include "shared.hpp"

#ifdef IRESEARCH_SSE2

#include "store_utils_optimized.hpp"

#include "store_utils.hpp"
#include "utils/std.hpp"

extern "C" {
#include <simdcomp/include/simdcomputil.h>

void simdpackwithoutmask(const uint32_t*  in, __m128i*  out, const uint32_t bit);
void simdunpack(const __m128i*  in, uint32_t*  out, const uint32_t bit);
}

#ifdef IRESEARCH_SSE4_2
#include <smmintrin.h> // for _mm_testc_si128
#endif

NS_LOCAL

bool all_equal(
    const uint32_t* RESTRICT begin,
    const uint32_t* RESTRICT end
) NOEXCEPT {
#ifdef IRESEARCH_SSE4_2
  assert(0 == (std::distance(begin, end) % SIMDBlockSize));

  if (begin == end) {
    return true;
  }

  const __m128i* mmbegin = reinterpret_cast<const __m128i*>(begin);
  const __m128i* mmend = reinterpret_cast<const __m128i*>(end);

  const __m128i value = _mm_set1_epi32(*begin);

  while (mmbegin != mmend) {
    const __m128i neq = _mm_xor_si128(value, _mm_loadu_si128(mmbegin++));

    if (!_mm_test_all_zeros(neq,neq)) {
      return false;
    }
  }

  return true;
#else
  return irs::irstd::all_equal(begin, end);
#endif
}

void fill(
    uint32_t* RESTRICT begin,
    uint32_t* RESTRICT end,
    const uint32_t value
) NOEXCEPT {
  assert(0 == (std::distance(begin, end) % SIMDBlockSize));

  auto* mmbegin = reinterpret_cast<__m128i*>(begin);
  auto* mmend = reinterpret_cast<__m128i*>(end);

  const auto mmvalue = _mm_set1_epi32(value);

  for (; mmbegin != mmend; ++mmbegin) {
    _mm_storeu_si128(mmbegin, mmvalue);
  }
}

NS_END

NS_ROOT
NS_BEGIN(encode)
NS_BEGIN(bitpack)

void read_block_optimized(
    data_input& in,
    uint32_t size,
    uint32_t* RESTRICT encoded,
    uint32_t* RESTRICT decoded) {
  assert(size && 0 == size % SIMDBlockSize);
  assert(encoded);
  assert(decoded);

  const uint32_t bits = in.read_vint();
  if (ALL_EQUAL == bits) {
    fill(decoded, decoded + size, in.read_vint());
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

    const auto* decoded_end = decoded + size;
    const size_t step = 4*bits;

    while (decoded != decoded_end) {
      ::simdunpack(reinterpret_cast<const __m128i*>(encoded), decoded, bits);
      decoded += SIMDBlockSize;
      encoded += step;
    }
  }
}

uint32_t write_block_optimized(
    data_output& out,
    const uint32_t* RESTRICT decoded,
    uint32_t size,
    uint32_t* RESTRICT encoded) {
  assert(size && 0 == size % SIMDBlockSize);
  assert(encoded);
  assert(decoded);

  if (all_equal(decoded, decoded + size)) {
    out.write_vint(ALL_EQUAL);
    out.write_vint(*decoded);
    return ALL_EQUAL;
  }

  const auto bits = ::maxbits_length(decoded, size);

  std::memset(encoded, 0, sizeof(uint32_t) * size);

  const auto* decoded_end = decoded + size;
  const size_t step = 4*bits;

  for (auto* begin = encoded; decoded != decoded_end;) {
    ::simdpackwithoutmask(decoded, reinterpret_cast<__m128i*>(begin), bits);
    decoded += SIMDBlockSize;
    begin += step;
  }

  out.write_vint(bits);
  out.write_bytes(
    reinterpret_cast<const byte_type*>(encoded),
    size/SIMDBlockSize*4*step
  );

  return bits;
}

NS_END // encode
NS_END // bitpack
NS_END // ROOT

#endif // IRESEARCH_SSE2

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------