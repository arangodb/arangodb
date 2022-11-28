// Copyright 2022 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ABSL_CRC_INTERNAL_NON_TEMPORAL_MEMCPY_H_
#define ABSL_CRC_INTERNAL_NON_TEMPORAL_MEMCPY_H_

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>

#include "absl/base/config.h"
#include "absl/base/optimization.h"

#ifdef __SSE__
// Only include if we're running on a CPU that supports SSE ISA, needed for
// sfence
#include <immintrin.h>  // IWYU pragma: keep
#endif
#ifdef __SSE2__
// Only include if we're running on a CPU that supports SSE2 ISA, needed for
// movdqa, movdqu, movntdq
#include <emmintrin.h>  // IWYU pragma: keep
#endif
#ifdef __aarch64__
// Only include if we're running on a CPU that supports ARM NEON ISA, needed for
// sfence, movdqa, movdqu, movntdq
#include "absl/crc/internal/non_temporal_arm_intrinsics.h"
#endif

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace crc_internal {
// This non-temporal memcpy does regular load and non-temporal store memory
// copy. It is compatible to both 16-byte aligned and unaligned addresses. If
// data at the destination is not immediately accessed, using non-temporal
// memcpy can save 1 DRAM load of the destination cacheline.

constexpr int kCacheLineSize = ABSL_CACHELINE_SIZE;

// If the objects overlap, the behavior is undefined.
// MSVC does not have proper header support for some of these intrinsics,
// so it should go to fallback
inline void *non_temporal_store_memcpy(void *__restrict dst,
                                       const void *__restrict src, size_t len) {
#if (defined(__SSE3__) || defined(__aarch64__)) && !defined(_MSC_VER)
  uint8_t *d = reinterpret_cast<uint8_t *>(dst);
  const uint8_t *s = reinterpret_cast<const uint8_t *>(src);

  // memcpy() the misaligned header. At the end of this if block, <d> is
  // aligned to a 64-byte cacheline boundary or <len> == 0.
  if (reinterpret_cast<uintptr_t>(d) & (kCacheLineSize - 1)) {
    uintptr_t bytes_before_alignment_boundary =
        kCacheLineSize -
        (reinterpret_cast<uintptr_t>(d) & (kCacheLineSize - 1));
    int header_len = (std::min)(bytes_before_alignment_boundary, len);
    assert(bytes_before_alignment_boundary < kCacheLineSize);
    memcpy(d, s, header_len);
    d += header_len;
    s += header_len;
    len -= header_len;
  }

  if (len >= kCacheLineSize) {
    _mm_sfence();
    __m128i *dst_cacheline = reinterpret_cast<__m128i *>(d);
    const __m128i *src_cacheline = reinterpret_cast<const __m128i *>(s);
    constexpr int kOpsPerCacheLine = kCacheLineSize / sizeof(__m128i);
    uint64_t loops = len / kCacheLineSize;

    while (len >= kCacheLineSize) {
      __m128i temp1, temp2, temp3, temp4;
      temp1 = _mm_lddqu_si128(src_cacheline + 0);
      temp2 = _mm_lddqu_si128(src_cacheline + 1);
      temp3 = _mm_lddqu_si128(src_cacheline + 2);
      temp4 = _mm_lddqu_si128(src_cacheline + 3);
      _mm_stream_si128(dst_cacheline + 0, temp1);
      _mm_stream_si128(dst_cacheline + 1, temp2);
      _mm_stream_si128(dst_cacheline + 2, temp3);
      _mm_stream_si128(dst_cacheline + 3, temp4);
      src_cacheline += kOpsPerCacheLine;
      dst_cacheline += kOpsPerCacheLine;
      len -= kCacheLineSize;
    }
    d += loops * kCacheLineSize;
    s += loops * kCacheLineSize;
    _mm_sfence();
  }

  // memcpy the tail.
  if (len) {
    memcpy(d, s, len);
  }
  return dst;
#else
  // Fallback to regular memcpy when SSE2/3 & aarch64 is not available.
  return memcpy(dst, src, len);
#endif  // __SSE3__ || __aarch64__
}

// MSVC does not have proper header support for some of these intrinsics,
// so it should go to fallback
inline void *non_temporal_store_memcpy_avx(void *__restrict dst,
                                           const void *__restrict src,
                                           size_t len) {
#if defined(__AVX__) && !defined(_MSC_VER)
  uint8_t *d = reinterpret_cast<uint8_t *>(dst);
  const uint8_t *s = reinterpret_cast<const uint8_t *>(src);

  // memcpy() the misaligned header. At the end of this if block, <d> is
  // aligned to a 64-byte cacheline boundary or <len> == 0.
  if (reinterpret_cast<uintptr_t>(d) & (kCacheLineSize - 1)) {
    uintptr_t bytes_before_alignment_boundary =
        kCacheLineSize -
        (reinterpret_cast<uintptr_t>(d) & (kCacheLineSize - 1));
    int header_len = (std::min)(bytes_before_alignment_boundary, len);
    assert(bytes_before_alignment_boundary < kCacheLineSize);
    memcpy(d, s, header_len);
    d += header_len;
    s += header_len;
    len -= header_len;
  }

  if (len >= kCacheLineSize) {
    _mm_sfence();
    __m256i *dst_cacheline = reinterpret_cast<__m256i *>(d);
    const __m256i *src_cacheline = reinterpret_cast<const __m256i *>(s);
    constexpr int kOpsPerCacheLine = kCacheLineSize / sizeof(__m256i);
    int loops = len / kCacheLineSize;

    while (len >= kCacheLineSize) {
      __m256i temp1, temp2;
      temp1 = _mm256_lddqu_si256(src_cacheline + 0);
      temp2 = _mm256_lddqu_si256(src_cacheline + 1);
      _mm256_stream_si256(dst_cacheline + 0, temp1);
      _mm256_stream_si256(dst_cacheline + 1, temp2);
      src_cacheline += kOpsPerCacheLine;
      dst_cacheline += kOpsPerCacheLine;
      len -= kCacheLineSize;
    }
    d += loops * kCacheLineSize;
    s += loops * kCacheLineSize;
    _mm_sfence();
  }

  // memcpy the tail.
  if (len) {
    memcpy(d, s, len);
  }
  return dst;
#else
  // Fallback to regular memcpy when AVX is not available.
  return memcpy(dst, src, len);
#endif  // __AVX__
}

}  // namespace crc_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_CRC_INTERNAL_NON_TEMPORAL_MEMCPY_H_
