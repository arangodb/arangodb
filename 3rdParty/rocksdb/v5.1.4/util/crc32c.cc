//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// A portable implementation of crc32c, optimized to handle
// four bytes at a time.

#include "util/crc32c.h"

#include <cassert>
#include <stdint.h>
#include "util/coding.h"

namespace rocksdb {
namespace crc32c {

extern Function Choose_Extend_SSE42();

// Used to fetch a naturally-aligned 32-bit word in little endian byte-order
static inline uint32_t LE_LOAD32(const uint8_t *p) {
  return DecodeFixed32(reinterpret_cast<const char*>(p));
}

static inline void Slow_CRC32(uint64_t* l, uint8_t const **p) {
  uint32_t c = static_cast<uint32_t>(*l ^ LE_LOAD32(*p));
  *p += 4;
  *l = table3_[c & 0xff] ^
  table2_[(c >> 8) & 0xff] ^
  table1_[(c >> 16) & 0xff] ^
  table0_[c >> 24];
  // DO it twice.
  c = static_cast<uint32_t>(*l ^ LE_LOAD32(*p));
  *p += 4;
  *l = table3_[c & 0xff] ^
  table2_[(c >> 8) & 0xff] ^
  table1_[(c >> 16) & 0xff] ^
  table0_[c >> 24];
}

// Detect if SS42 or not.
static bool isSSE42() {
#if defined(__GNUC__) && defined(__x86_64__) && !defined(IOS_CROSS_COMPILE)
  uint32_t c_;
  uint32_t d_;
  __asm__("cpuid" : "=c"(c_), "=d"(d_) : "a"(1) : "ebx");
  return c_ & (1U << 20);  // copied from CpuId.h in Folly.
#else
  return false;
#endif
}

static Function Choose_Extend_C() {
  return ExtendImpl<Slow_CRC32>;
}

static inline Function Choose_Extend() {
  Function f = nullptr;

  if (isSSE42()) {
    f = Choose_Extend_SSE42();
  } 
  if (f == nullptr) {
    f = Choose_Extend_C();
  }
  assert(f != nullptr);
  return f;
}

bool IsFastCrc32Supported() {
  return isSSE42();
}

Function ChosenExtend = Choose_Extend();

uint32_t Extend(uint32_t crc, const char* buf, size_t size) {
  return ChosenExtend(crc, buf, size);
}

}  // namespace crc32c
}  // namespace rocksdb
