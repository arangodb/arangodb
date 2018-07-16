// Copyright 2002 Google Inc. All Rights Reserved.
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

//
// Derived from code by Moses Charikar

#include "s2/util/bits/bits.h"

#include <cassert>
#include "s2/third_party/absl/numeric/int128.h"

using absl::uint128;

// this array gives the number of bits for any number from 0 to 255
// (We could make these ints.  The tradeoff is size (eg does it overwhelm
// the cache?) vs efficiency in referencing sub-word-sized array elements)
const char Bits::num_bits[] = {
  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8 };

int Bits::Count(const void *m, int num_bytes) {
  int nbits = 0;
  const uint8 *s = (const uint8 *) m;
  for (int i = 0; i < num_bytes; i++)
    nbits += num_bits[*s++];
  return nbits;
}

int Bits::Difference(const void *m1, const void *m2, int num_bytes) {
  int nbits = 0;
  const uint8 *s1 = (const uint8 *) m1;
  const uint8 *s2 = (const uint8 *) m2;
  for (int i = 0; i < num_bytes; i++)
    nbits += num_bits[(*s1++) ^ (*s2++)];
  return nbits;
}

int Bits::CappedDifference(const void *m1, const void *m2,
                           int num_bytes, int cap) {
  int nbits = 0;
  const uint8 *s1 = (const uint8 *) m1;
  const uint8 *s2 = (const uint8 *) m2;
  for (int i = 0; i < num_bytes && nbits <= cap; i++)
    nbits += num_bits[(*s1++) ^ (*s2++)];
  return nbits;
}

int Bits::Log2Floor_Portable(uint32 n) {
  if (n == 0)
    return -1;
  int log = 0;
  uint32 value = n;
  for (int i = 4; i >= 0; --i) {
    int shift = (1 << i);
    uint32 x = value >> shift;
    if (x != 0) {
      value = x;
      log += shift;
    }
  }
  assert(value == 1);
  return log;
}

int Bits::Log2Ceiling(uint32 n) {
  int floor = Log2Floor(n);
  if ((n & (n - 1)) == 0)              // zero or a power of two
    return floor;
  else
    return floor + 1;
}

int Bits::Log2Ceiling64(uint64 n) {
  int floor = Log2Floor64(n);
  if ((n & (n - 1)) == 0)              // zero or a power of two
    return floor;
  else
    return floor + 1;
}

int Bits::Log2Ceiling128(absl::uint128 n) {
  int floor = Log2Floor128(n);
  if ((n & (n - 1)) == 0)              // zero or a power of two
    return floor;
  else
    return floor + 1;
}

int Bits::FindLSBSetNonZero_Portable(uint32 n) {
  int rc = 31;
  for (int i = 4, shift = 1 << 4; i >= 0; --i) {
    const uint32 x = n << shift;
    if (x != 0) {
      n = x;
      rc -= shift;
    }
    shift >>= 1;
  }
  return rc;
}

int Bits::CountLeadingZeros32_Portable(uint32 n) {
  int bits = 1;
  if (n == 0)
    return 32;
  if ((n >> 16) == 0) {
    bits += 16;
    n <<= 16;
  }
  if ((n >> 24) == 0) {
    bits += 8;
    n <<= 8;
  }
  if ((n >> 28) == 0) {
    bits += 4;
    n <<= 4;
  }
  if ((n >> 30) == 0) {
    bits += 2;
    n <<= 2;
  }
  return bits - (n >> 31);
}

int Bits::CountLeadingZeros64_Portable(uint64 n) {
  return ((n >> 32)
           ? Bits::CountLeadingZeros32_Portable(n >> 32)
           : 32 +  Bits::CountLeadingZeros32_Portable(n));
}
