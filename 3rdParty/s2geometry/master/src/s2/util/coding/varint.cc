// Copyright 2001 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//


#include "s2/util/coding/varint.h"

#include <cstdint>
#include <string>

#include "s2/base/integral_types.h"
#include "s2/base/logging.h"

#ifndef _MSC_VER
const int Varint::kMax32;
const int Varint::kMax64;
const int Varint::kSlopBytes;
#endif

char* Varint::Encode32(char* sptr, uint32 v) {
  return Encode32Inline(sptr, v);
}

char* Varint::Encode64(char* sptr, uint64 v) {
  if (v < (1u << 28)) {
    return Varint::Encode32(sptr, v);
  } else {
    // Operate on characters as unsigneds
    unsigned char* ptr = reinterpret_cast<unsigned char*>(sptr);
    // Rather than computing four subresults and or'ing each with 0x80,
    // we can do two ors now.  (Doing one now wouldn't work.)
    const uint32 x32 = v | (1 << 7) | (1 << 21);
    const uint32 y32 = v | (1 << 14) | (1 << 28);
    *(ptr++) = x32;
    *(ptr++) = y32 >> 7;
    *(ptr++) = x32 >> 14;
    *(ptr++) = y32 >> 21;
    if (v < (1ull << 35)) {
      *(ptr++) = v >> 28;
      return reinterpret_cast<char*>(ptr);
    } else {
      *(ptr++) = (v >> 28) | (1 << 7);
      return Varint::Encode32(reinterpret_cast<char*>(ptr), v >> 35);
    }
  }
}

const char* Varint::Parse32Fallback(const char* ptr, uint32* OUTPUT) {
  return Parse32FallbackInline(ptr, OUTPUT);
}

#if defined(__x86_64__)

std::pair<const char*, uint64> Varint::Parse64FallbackPair(const char* p,
                                                             int64 res1) {
  // The algorithm relies on sign extension to set all high bits when the varint
  // continues. This way it can use "and" to aggregate in to the result.
  auto ptr = reinterpret_cast<const int8*>(p);
  // However this requires the low bits after shifting to be 1's as well. On
  // x86_64 a shld from a single register filled with enough 1's in the high
  // bits can accomplish all this in one instruction. It so happens that res1
  // has 57 high bits of ones, which is enough for the largest shift done.
  S2_DCHECK_EQ(res1 >> 7, -1);
  uint64 ones = res1;  // save the useful high bit 1's in res1
  uint64 byte;
  int64 res2, res3;
#define SHLD(n) byte = ((byte << (n * 7)) | (ones >> (64 - (n * 7))))
  int sign_bit;
  // Micro benchmarks show a substantial improvement to capture the sign
  // of the result in the case of just assigning the result of the shift
  // (ie first 2 steps).
#ifdef __GCC_ASM_FLAG_OUTPUTS__
#define SHLD_SIGN(n)                  \
  asm("shldq %3, %2, %1"              \
      : "=@ccs"(sign_bit), "+r"(byte) \
      : "r"(ones), "i"(n * 7))
#else
#define SHLD_SIGN(n)                         \
  do {                                       \
    SHLD(n);                                 \
    sign_bit = static_cast<int64>(byte) < 0; \
  } while (0)
#endif
  byte = ptr[1];
  SHLD_SIGN(1);
  res2 = byte;
  if (!sign_bit) goto done2;
  byte = ptr[2];
  SHLD_SIGN(2);
  res3 = byte;
  if (!sign_bit) goto done3;
  byte = ptr[3];
  SHLD(3);
  res1 &= byte;
  if (res1 >= 0) goto done4;
  byte = ptr[4];
  SHLD(4);
  res2 &= byte;
  if (res2 >= 0) goto done5;
  byte = ptr[5];
  SHLD(5);
  res3 &= byte;
  if (res3 >= 0) goto done6;
  byte = ptr[6];
  SHLD(6);
  res1 &= byte;
  if (res1 >= 0) goto done7;
  byte = ptr[7];
  SHLD(7);
  res2 &= byte;
  if (res2 >= 0) goto done8;
  byte = ptr[8];
  SHLD(8);
  res3 &= byte;
  if (res3 >= 0) goto done9;
  byte = ptr[9];
  // Last byte only contains 0 or 1 for valid 64bit varints. If it's 0 it's
  // a denormalized varint that shouldn't happen. The continuation bit of byte
  // 9 has already the right value hence just expect byte to be 1.
  if (ABSL_PREDICT_TRUE(byte == 1)) goto done10;
  if (byte == 0) {
    res3 ^= static_cast<uint64>(1) << 63;
    goto done10;
  }

  return {nullptr, 0};  // Value is too long to be a varint64

#define DONE(n) done##n : return {p + n, res1 & res2 & res3};
done2:
  return {p + 2, res1 & res2};
  DONE(3)
  DONE(4)
  DONE(5)
  DONE(6)
  DONE(7)
  DONE(8)
  DONE(9)
  DONE(10)
#undef DONE
}

#endif  // defined(__x86_64__)

const char* Varint::Parse64Fallback(const char* p, uint64* OUTPUT) {
  const unsigned char* ptr = reinterpret_cast<const unsigned char*>(p);
  assert(*ptr >= 128);
  // Fast path: need to accumulate data in upto three result fragments
  //    res1    bits 0..27
  //    res2    bits 28..55
  //    res3    bits 56..63
  uint32 byte, res1, res2 = 0, res3 = 0;
  byte = *(ptr++); res1 = byte & 127;
  byte = *(ptr++); res1 |= (byte & 127) <<  7; if (byte < 128) goto done1;
  byte = *(ptr++); res1 |= (byte & 127) << 14; if (byte < 128) goto done1;
  byte = *(ptr++); res1 |= (byte & 127) << 21; if (byte < 128) goto done1;

  byte = *(ptr++); res2 = byte & 127;          if (byte < 128) goto done2;
  byte = *(ptr++); res2 |= (byte & 127) <<  7; if (byte < 128) goto done2;
  byte = *(ptr++); res2 |= (byte & 127) << 14; if (byte < 128) goto done2;
  byte = *(ptr++); res2 |= (byte & 127) << 21; if (byte < 128) goto done2;

  byte = *(ptr++); res3 = byte & 127;          if (byte < 128) goto done3;
  byte = *(ptr++); res3 |= (byte & 127) <<  7; if (byte < 2) goto done3;

  return nullptr;       // Value is too long to be a varint64

 done1:
  assert(res2 == 0);
  assert(res3 == 0);
  *OUTPUT = res1;
  return reinterpret_cast<const char*>(ptr);

 done2:
  assert(res3 == 0);
  *OUTPUT = res1 | (uint64(res2) << 28);
  return reinterpret_cast<const char*>(ptr);

 done3:
   *OUTPUT = res1 | (uint64(res2) << 28) | (uint64(res3) << 56);
   return reinterpret_cast<const char*>(ptr);
 }

 const char* Varint::Parse32BackwardSlow(const char* ptr, const char* base,
                                         uint32* OUTPUT) {
   // Since this method is rarely called, for simplicity, we just skip backward
   // and then parse forward.
   const char* prev = Skip32BackwardSlow(ptr, base);
   if (prev == nullptr) return nullptr;  // no value before 'ptr'

   Parse32(prev, OUTPUT);
   return prev;
 }

 const char* Varint::Parse64BackwardSlow(const char* ptr, const char* base,
                                         uint64* OUTPUT) {
   // Since this method is rarely called, for simplicity, we just skip backward
   // and then parse forward.
   const char* prev = Skip64BackwardSlow(ptr, base);
   if (prev == nullptr) return nullptr;  // no value before 'ptr'

   Parse64(prev, OUTPUT);
   return prev;
 }

 const char* Varint::Parse64WithLimit(const char* p, const char* l,
                                      uint64* OUTPUT) {
   if (p + kMax64 <= l) {
     return Parse64(p, OUTPUT);
   } else {
     // See detailed comment in Varint::Parse64Fallback about this general
     // approach.
     const unsigned char* ptr = reinterpret_cast<const unsigned char*>(p);
     const unsigned char* limit = reinterpret_cast<const unsigned char*>(l);
     uint64 b, result;
#if defined(__x86_64__)
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result = b;              if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result += (b - 1) <<  7; if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result += (b - 1) << 14; if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result += (b - 1) << 21; if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result += (b - 1) << 28; if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result += (b - 1) << 35; if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result += (b - 1) << 42; if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result += (b - 1) << 49; if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result += (b - 1) << 56; if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result += (b - 1) << 63; if (b < 2) goto done;
    return nullptr;       // Value is too long to be a varint64
#else
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result = b & 127;          if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result |= (b & 127) <<  7; if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result |= (b & 127) << 14; if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result |= (b & 127) << 21; if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result |= (b & 127) << 28; if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result |= (b & 127) << 35; if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result |= (b & 127) << 42; if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result |= (b & 127) << 49; if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result |= (b & 127) << 56; if (b < 128) goto done;
    if (ptr >= limit) return nullptr;
    b = *(ptr++); result |= (b & 127) << 63; if (b < 2) goto done;
    return nullptr;       // Value is too long to be a varint64
#endif
   done:
    *OUTPUT = result;
    return reinterpret_cast<const char*>(ptr);
  }
 }

const char* Varint::Skip32BackwardSlow(const char* p, const char* b) {
  const unsigned char* ptr = reinterpret_cast<const unsigned char*>(p);
  const unsigned char* base = reinterpret_cast<const unsigned char*>(b);
  assert(ptr >= base);

  // If the initial pointer is at the base or if the previous byte is not
  // the last byte of a varint, we return nullptr since there is nothing to
  // skip.
  if (ptr == base) return nullptr;
  if (*(--ptr) > 127) return nullptr;

  for (int i = 0; i < 5; i++) {
    if (ptr == base)    return reinterpret_cast<const char*>(ptr);
    if (*(--ptr) < 128) return reinterpret_cast<const char*>(ptr + 1);
  }

  return nullptr; // value is too long to be a varint32
}

const char* Varint::Skip64BackwardSlow(const char* p, const char* b) {
  const unsigned char* ptr = reinterpret_cast<const unsigned char*>(p);
  const unsigned char* base = reinterpret_cast<const unsigned char*>(b);
  assert(ptr >= base);

  // If the initial pointer is at the base or if the previous byte is not
  // the last byte of a varint, we return nullptr since there is nothing to
  // skip.
  if (ptr == base) return nullptr;
  if (*(--ptr) > 127) return nullptr;

  for (int i = 0; i < 10; i++) {
    if (ptr == base)    return reinterpret_cast<const char*>(ptr);
    if (*(--ptr) < 128) return reinterpret_cast<const char*>(ptr + 1);
  }

  return nullptr; // value is too long to be a varint64
}

void Varint::Append32Slow(std::string* s, uint32 value) {
  const size_t start = s->size();
  s->resize(
                                             start + Varint::Length32(value));
  Varint::Encode32(&((*s)[start]), value);
}

void Varint::Append64Slow(std::string* s, uint64 value) {
  const size_t start = s->size();
  s->resize(
                                             start + Varint::Length64(value));
  Varint::Encode64(&((*s)[start]), value);
}
