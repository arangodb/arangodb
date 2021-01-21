////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGO_SPLITMIX64_H
#define ARANGO_SPLITMIX64_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace basics {

struct splitmix64 {
  ////////////////////////////////////////////////////////////////////////////////
  /// The code in this struct has been lightly adapted from its original source:
  /// http://xoroshiro.di.unimi.it/splitmix64.c
  /// The original authorship/license information appears below
  ////////////////////////////////////////////////////////////////////////////////

  /*  Written in 2015 by Sebastiano Vigna (vigna@acm.org)

  To the extent possible under law, the author has dedicated all copyright
  and related and neighboring rights to this software to the public domain
  worldwide. This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/>. */

  /* This is a fixed-increment version of Java 8's SplittableRandom generator
     See http://dx.doi.org/10.1145/2714064.2660195 and
     http://docs.oracle.com/javase/8/docs/api/java/util/SplittableRandom.html

     It is a very fast generator passing BigCrush, and it can be useful if
     for some reason you absolutely want 64 bits of state; otherwise, we
     rather suggest to use a xoroshiro128+ (for moderately parallel
     computations) or xorshift1024* (for massively parallel computations)
     generator. */

  explicit splitmix64(uint64_t seed) : _x(seed) {}

  uint64_t next() {
    uint64_t z = (_x += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
  }

 private:
  uint64_t _x;
};

}  // namespace basics
}  // namespace arangodb

#endif
