////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_XOROSHIRO128PLUS_H
#define ARANGO_XOROSHIRO128PLUS_H 1

#include <cstdint>
#include <cstdlib>

namespace arangodb {
namespace basics {

struct xoroshiro128plus {
  ////////////////////////////////////////////////////////////////////////////////
  /// The code in this struct has been lightly adapted from its original source:
  /// http://xoroshiro.di.unimi.it/xoroshiro128plus.c
  /// The original authorship/license information appears below
  ////////////////////////////////////////////////////////////////////////////////

  /*  Written in 2016 by David Blackman and Sebastiano Vigna (vigna@acm.org)

  To the extent possible under law, the author has dedicated all copyright
  and related and neighboring rights to this software to the public domain
  worldwide. This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/>. */

  /* This is the successor to xorshift128+. It is the fastest full-period
     generator passing BigCrush without systematic failures, but due to the
     relatively short period it is acceptable only for applications with a
     mild amount of parallelism; otherwise, use a xorshift1024* generator.

     Beside passing BigCrush, this generator passes the PractRand test suite
     up to (and included) 16TB, with the exception of binary rank tests,
     which fail due to the lowest bit being an LFSR; all other bits pass all
     tests. We suggest to use a sign test to extract a random Boolean value.

     Note that the generator uses a simulated rotate operation, which most C
     compilers will turn into a single instruction. In Java, you can use
     Long.rotateLeft(). In languages that do not make low-level rotation
     instructions accessible xorshift128+ could be faster.

     The state must be seeded so that it is not everywhere zero. If you have
     a 64-bit seed, we suggest to seed a splitmix64 generator and use its
     output to fill s. */

  xoroshiro128plus() : _s{0, 0} {}

  void seed(uint64_t seed1, uint64_t seed2) {
    _s[0] = seed1;
    _s[1] = seed2;
  }

  static inline uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
  }

  uint64_t next(void) {
    const uint64_t s0 = _s[0];
    uint64_t s1 = _s[1];
    const uint64_t result = s0 + s1;

    s1 ^= s0;
    _s[0] = rotl(s0, 55) ^ s1 ^ (s1 << 14);  // a, b
    _s[1] = rotl(s1, 36);                    // c

    return result;
  }

  /* This is the jump function for the generator. It is equivalent
     to 2^64 calls to next(); it can be used to generate 2^64
     non-overlapping subsequences for parallel computations. */
  void jump(void) {
    static const uint64_t JUMP[] = {0xbeac0467eba5facb, 0xd86b048b86aa9922};

    uint64_t s0 = 0;
    uint64_t s1 = 0;
    for (size_t i = 0; i < sizeof JUMP / sizeof *JUMP; i++)
      for (size_t b = 0; b < 64; b++) {
        if (JUMP[i] & UINT64_C(1) << b) {
          s0 ^= _s[0];
          s1 ^= _s[1];
        }
        next();
      }

    _s[0] = s0;
    _s[1] = s1;
  }

 private:
  uint64_t _s[2];
};

}  // namespace basics
}  // namespace arangodb

#endif
