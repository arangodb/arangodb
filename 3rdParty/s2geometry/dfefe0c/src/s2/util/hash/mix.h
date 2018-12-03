// Copyright 2013 Google Inc. All Rights Reserved.
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

// Author: jyrki@google.com (Jyrki Alakuijala)
//
// This file contains routines for mixing hashes.

#ifndef S2_UTIL_HASH_MIX_H_
#define S2_UTIL_HASH_MIX_H_

#include <cstddef>
#include <limits>

// Fast mixing of hash values -- not strong enough for fingerprinting.
// May change from time to time.
//
// Values given are expected to be hashes from good hash functions.
// What constitutes a good hash may depend on your application. As a rule of
// thumb, if std::hash<int> is strong enough for your hashing need if
// your data were just ints, it will most likely be the correct choice
// for a mixed hash of data members. HashMix does one round of multiply and
// rotate mixing, so you get some additional collision avoidance guarantees
// compared to just using std::hash<int> directly.
//
// Possible use:
//
// struct Xyzzy {
//   int x;
//   int y;
//   string id;
// };
//
// #ifndef SWIG
// template<> struct XyzzyHash<Xyzzy> {
//   size_t operator()(const Xyzzy& c) const {
//     HashMix mix(hash<int>()(c.x));
//     mix.Mix(hash<int>()(c.y));
//     mix.Mix(GoodFastHash<string>()(c.id));
//     return mix.get();
//   }
// }
// #endif
//
// HashMix is a lower level interface than std::hash<std::tuple<>>.
// Use std::hash<std::tuple<>> instead of HashMix where appropriate.

class HashMix {
 public:
  HashMix() : hash_(1) {}
  explicit HashMix(size_t val) : hash_(val + 83) {}
  void Mix(size_t val) {
    static const size_t kMul = static_cast<size_t>(0xdc3eb94af8ab4c93ULL);
    // Multiplicative hashing will mix bits better in the msb end ...
    hash_ *= kMul;
    // ... and rotating will move the better mixed msb-bits to lsb-bits.
    hash_ = ((hash_ << 19) |
             (hash_ >> (std::numeric_limits<size_t>::digits - 19))) + val;
  }
  size_t get() const { return hash_; }
 private:
  size_t hash_;
};

#endif  // S2_UTIL_HASH_MIX_H_
