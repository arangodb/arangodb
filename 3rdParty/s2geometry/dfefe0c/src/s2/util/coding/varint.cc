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

#include <string>

#include "s2/base/integral_types.h"

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

const char* Varint::Parse64Fallback(const char* p, uint64* OUTPUT) {
  const unsigned char* ptr = reinterpret_cast<const unsigned char*>(p);
  assert(*ptr >= 128);
#if defined(__x86_64__)
  // This approach saves one redundant operation on the last byte (masking a
  // byte that doesn't need it). This is conditional on x86 because:
  // - PowerPC has specialized bit instructions that make masking and
  //   shifting very efficient
  // - x86 seems to be one of the few architectures that has a single
  //   instruction to add 3 values.
  //
  // e.g.
  // Input: 0xff, 0x40
  // Mask & Or calculates: (0xff & 0x7f) | ((0x40 & 0x7f) << 7) = 0x207f
  // Sub1 & Add calculates: 0xff         + ((0x40    - 1) << 7) = 0x207f
  //
  // The subtract one removes the bit set by the previous byte used to
  // indicate that more bytes are present. It also has the potential to
  // allow instructions like LEA to combine 2 adds into one instruction.
  //
  // E.g. on an x86 architecture, %rcx = %rax + (%rbx - 1) << 7 could be
  // emitted as:
  //   shlq $7, %rbx
  //   leaq -0x80(%rax, %rbx), %rcx
  //
  // Fast path: need to accumulate data in upto three result fragments
  //    res1    bits 0..27
  //    res2    bits 28..55
  //    res3    bits 56..63

  uint64 byte, res1, res2 = 0, res3 = 0;
  byte = *(ptr++); res1 = byte;
  byte = *(ptr++); res1 += (byte - 1) <<  7; if (byte < 128) goto done1;
  byte = *(ptr++); res1 += (byte - 1) << 14; if (byte < 128) goto done1;
  byte = *(ptr++); res1 += (byte - 1) << 21; if (byte < 128) goto done1;

  byte = *(ptr++); res2 = byte;              if (byte < 128) goto done2;
  byte = *(ptr++); res2 += (byte - 1) <<  7; if (byte < 128) goto done2;
  byte = *(ptr++); res2 += (byte - 1) << 14; if (byte < 128) goto done2;
  byte = *(ptr++); res2 += (byte - 1) << 21; if (byte < 128) goto done2;

  byte = *(ptr++); res3 = byte;              if (byte < 128) goto done3;
  byte = *(ptr++); res3 += (byte - 1) <<  7; if (byte < 2) goto done3;

  return nullptr;       // Value is too long to be a varint64

 done1:
  assert(res2 == 0);
  assert(res3 == 0);
  *OUTPUT = res1;
  return reinterpret_cast<const char*>(ptr);

 done2:
  assert(res3 == 0);
  *OUTPUT = res1 + ((res2 - 1) << 28);
  return reinterpret_cast<const char*>(ptr);

 done3:
  *OUTPUT = res1 + ((res2 - 1) << 28) + ((res3 - 1) << 56);
  return reinterpret_cast<const char*>(ptr);
#else
  uint32 byte, res1, res2=0, res3=0;
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
#endif
}

const char* Varint::Parse32BackwardSlow(const char* ptr, const char* base,
                                        uint32* OUTPUT) {
  // Since this method is rarely called, for simplicity, we just skip backward
  // and then parse forward.
  const char* prev = Skip32BackwardSlow(ptr, base);
  if (prev == nullptr)
    return nullptr; // no value before 'ptr'

  Parse32(prev, OUTPUT);
  return prev;
}

const char* Varint::Parse64BackwardSlow(const char* ptr, const char* base,
                                        uint64* OUTPUT) {
  // Since this method is rarely called, for simplicity, we just skip backward
  // and then parse forward.
  const char* prev = Skip64BackwardSlow(ptr, base);
  if (prev == nullptr)
    return nullptr; // no value before 'ptr'

  Parse64(prev, OUTPUT);
  return prev;
}

const char* Varint::Parse64WithLimit(const char* p,
                                     const char* l,
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

void Varint::Append32Slow(string* s, uint32 value) {
  const size_t start = s->size();
  s->resize(start + Varint::Length32(value));
  Varint::Encode32(&((*s)[start]), value);
}

void Varint::Append64Slow(string* s, uint64 value) {
  const size_t start = s->size();
  s->resize(start + Varint::Length64(value));
  Varint::Encode64(&((*s)[start]), value);
}

void Varint::EncodeTwo32Values(string* s, uint32 a, uint32 b) {
  uint64 v = 0;
  int shift = 0;
  while ((a > 0) || (b > 0)) {
    uint8 one_byte = (a & 0xf) | ((b & 0xf) << 4);
    v |= ((static_cast<uint64>(one_byte)) << shift);
    shift += 8;
    a >>= 4;
    b >>= 4;
  }
  Append64(s, v);
}

namespace {

// Skip the least signficant "offset" bits and extract the next "bits" bits.
uint32 GetBits(uint32 byte, uint32 offset, uint32 bits) {
  return (byte >> offset) & ((1u << bits) - 1);
}

template <bool RespectLimit>
const char* DecodeTwo32ValuesInternal(const char* ptr, const char* limit,
                                      uint32* a, uint32* b) {
  const uint8* uptr = reinterpret_cast<const uint8*>(ptr);
  const uint8* ulimit = reinterpret_cast<const uint8*>(limit);

  // Assert that an optimized path is used in DecodeTwo32Values before
  // DecodeTwo32ValuesSlow is invoked. This optimization does not exist for
  // DecodeTwo32ValuesWithLimit (the same way Parse64WithLimit does not have
  // this optimization).
  if (!RespectLimit) {
    assert(*uptr >= 128);
  }

// Extract portions of bits for *a and *b from a byte that is formatted
// as follows (least significant bits first):
//  shift bits for b
//  4 bits for a
//  3-shift bits for b
//  varint termination bit
#define PARSE_BITS_DECODE_TWO(shift, a, a_shift, b, b_shift)     \
  if (RespectLimit && uptr >= ulimit) return nullptr;            \
  byte = *(uptr++);                                              \
  b |= GetBits(byte, 0, shift) << b_shift;                       \
  a |= GetBits(byte, shift, 4) << a_shift;                       \
  b |= GetBits(byte, shift + 4, 3 - shift) << (b_shift + shift); \
  if (byte < 128) goto done;                                     \
  a_shift += 4;                                                  \
  b_shift += 3;

  uint32 res1 = 0, res2 = 0, shift1 = 0, shift2 = 0, byte;

  // First four bytes contain four consecutive bits of res1 each.
  PARSE_BITS_DECODE_TWO(0, res1, shift1, res2, shift2);
  PARSE_BITS_DECODE_TWO(1, res1, shift1, res2, shift2);
  PARSE_BITS_DECODE_TWO(2, res1, shift1, res2, shift2);
  PARSE_BITS_DECODE_TWO(3, res1, shift1, res2, shift2);

  // Next four bytes contain four consecutive bits of res2 each.
  // Note the switched order of variables.
  PARSE_BITS_DECODE_TWO(0, res2, shift2, res1, shift1);
  PARSE_BITS_DECODE_TWO(1, res2, shift2, res1, shift1);
  PARSE_BITS_DECODE_TWO(2, res2, shift2, res1, shift1);
  PARSE_BITS_DECODE_TWO(3, res2, shift2, res1, shift1);

  // Back to the original order.
  PARSE_BITS_DECODE_TWO(0, res1, shift1, res2, shift2);

#undef PARSE_BITS_DECODE_TWO

  byte = *(uptr++);
  // 10th byte has at most one bit set.
  if (byte > 1) {
    return nullptr;  // Value is too long to be two encoded values.
  }
  res2 |= byte << 31;

done:
  *a = res1;
  *b = res2;
  return reinterpret_cast<const char*>(uptr);
}

}  // namespace

const char* Varint::DecodeTwo32ValuesSlow(const char* p, uint32* a, uint32* b) {
  return DecodeTwo32ValuesInternal<false>(p, nullptr, a, b);
}

const char* Varint::DecodeTwo32ValuesWithLimit(const char* ptr,
                                               const char* limit, uint32* a,
                                               uint32* b) {
  if (ptr + kMax64 <= limit) {
    return DecodeTwo32Values(ptr, a, b);
  }
  return DecodeTwo32ValuesInternal<true>(ptr, limit, a, b);
}
