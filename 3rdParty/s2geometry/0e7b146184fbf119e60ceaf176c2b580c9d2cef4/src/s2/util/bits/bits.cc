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

#include "absl/numeric/int128.h"

#include "s2/base/integral_types.h"

using absl::uint128;

int Bits::Count(const void *m, int num_bytes) {
  int nbits = 0;
  const uint8 *s = (const uint8 *)m;
  for (int i = 0; i < num_bytes; i++) nbits += absl::popcount(*s++);
  return nbits;
}

int Bits::Difference(const void *m1, const void *m2, int num_bytes) {
  int nbits = 0;
  const uint8 *s1 = (const uint8 *)m1;
  const uint8 *s2 = (const uint8 *)m2;
  for (int i = 0; i < num_bytes; i++)
    nbits += absl::popcount(static_cast<uint8>((*s1++) ^ (*s2++)));
  return nbits;
}

int Bits::CappedDifference(const void *m1, const void *m2, int num_bytes,
                           int cap) {
  int nbits = 0;
  const uint8 *s1 = (const uint8 *)m1;
  const uint8 *s2 = (const uint8 *)m2;
  for (int i = 0; i < num_bytes && nbits <= cap; i++)
    nbits += absl::popcount(static_cast<uint8>((*s1++) ^ (*s2++)));
  return nbits;
}

int Bits::Log2Ceiling(uint32 n) {
  int floor = (absl::bit_width(n) - 1);
  if ((n & (n - 1)) == 0)  // zero or a power of two
    return floor;
  else
    return floor + 1;
}

int Bits::Log2Ceiling64(uint64 n) {
  int floor = (absl::bit_width(n) - 1);
  if ((n & (n - 1)) == 0)  // zero or a power of two
    return floor;
  else
    return floor + 1;
}

int Bits::Log2Ceiling128(absl::uint128 n) {
  int floor = Log2Floor128(n);
  if ((n & (n - 1)) == 0)  // zero or a power of two
    return floor;
  else
    return floor + 1;
}
