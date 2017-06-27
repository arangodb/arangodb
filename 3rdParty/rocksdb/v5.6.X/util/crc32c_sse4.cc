//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//  This source code is also licensed under the GPLv2 license found in the
//  COPYING file in the root directory of this source tree.
//
// A portable implementation of crc32c, optimized to handle
// four bytes at a time.

#include "util/crc32c.h"
#include <cstdint>

#ifdef HAVE_SSE42
#include <nmmintrin.h>
#endif

namespace rocksdb {
namespace crc32c {

#if defined(HAVE_SSE42) && (defined(__LP64__) || defined(_WIN64))
static inline uint64_t LE_LOAD64(const uint8_t *p) {
  return DecodeFixed64(reinterpret_cast<const char*>(p));
}
#endif

bool IsFastCrc32Supported(){
#if defined(HAVE_SSE42) || defined(_WIN64)
  static bool sse = isSSE42();
  return sse;
#else
  return false;
#endif
}


void Fast_CRC32(uint64_t* l, uint8_t const **p) {
#if defined(HAVE_SSE42) && (defined(__LP64__) || defined(_WIN64))
  *l = _mm_crc32_u64(*l, LE_LOAD64(*p));
  *p += 8;
#elif defined(HAVE_SSE42)
  *l = _mm_crc32_u32(static_cast<unsigned int>(*l), LE_LOAD32(*p));
  *p += 4;
  *l = _mm_crc32_u32(static_cast<unsigned int>(*l), LE_LOAD32(*p));
  *p += 4;
#endif
}

}  // namespace crc32c
}  // namespace rocksdb
