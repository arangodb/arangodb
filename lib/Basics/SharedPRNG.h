////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGO_SHARED_PRNG_H
#define ARANGO_SHARED_PRNG_H 1

#include <memory>

#include "Basics/Thread.h"
#include "Basics/fasthash.h"
#include "Basics/xoroshiro128plus.h"

namespace arangodb {
namespace basics {

struct PaddedPRNG {
  PaddedPRNG();
  void seed(uint64_t seed1, uint64_t seed2);
  inline uint64_t next() { return _prng.next(); }

 private:
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif
  // this padding is intentionally here

  uint8_t frontPadding[64];
  xoroshiro128plus _prng;
  uint8_t backpadding[64 - sizeof(xoroshiro128plus)];

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
};

struct SharedPRNG {
  static inline uint64_t rand() { return _global->next(); }
  SharedPRNG();

 private:
  // never want two live threads to hash to the same stripe
  static constexpr uint64_t _stripes = (1 << 16);
  static std::unique_ptr<SharedPRNG> _global;

  PaddedPRNG _prng[_stripes];
  uint64_t _mask;

  inline uint64_t id() {
    return fasthash64_uint64(Thread::currentThreadNumber(), 0xdeadbeefdeadbeefULL);
  }

  inline uint64_t next() { return _prng[id() & _mask].next(); }
};

}  // namespace basics
}  // namespace arangodb

#endif
