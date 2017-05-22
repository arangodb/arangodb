//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/crc32c.h"

#include <cassert>
#include <stdint.h>
#ifdef __SSE4_2__
#include <nmmintrin.h>
#endif
#include "util/coding.h"

namespace rocksdb {
namespace crc32c {

#ifdef __SSE4_2__

// Used to fetch a naturally-aligned 32-bit word in little endian byte-order
static inline uint32_t LE_LOAD32(const uint8_t *p) {
  return DecodeFixed32(reinterpret_cast<const char*>(p));
}

#ifdef __LP64__
static inline uint64_t LE_LOAD64(const uint8_t *p) {
  return DecodeFixed64(reinterpret_cast<const char*>(p));
}
#endif

static inline void Fast_CRC32(uint64_t* l, uint8_t const **p) {
#ifdef __LP64__
  *l = _mm_crc32_u64(*l, LE_LOAD64(*p));
  *p += 8;
#else
  *l = _mm_crc32_u32(static_cast<unsigned int>(*l), LE_LOAD32(*p));
  *p += 4;
  *l = _mm_crc32_u32(static_cast<unsigned int>(*l), LE_LOAD32(*p));
  *p += 4;
#endif
}

Function Choose_Extend_SSE42() {
  return ExtendImpl<Fast_CRC32>;
}

#else

Function Choose_Extend_SSE42() {
  return nullptr;
}

#endif

}  // namespace crc32c
}  // namespace rocksdb
