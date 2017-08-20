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

#ifndef ARANGO_SHARED_PRNG_H
#define ARANGO_SHARED_PRNG_H 1

#include "Basics/Common.h"
#include "Basics/Thread.h"
#include "Basics/cpu-relax.h"
#include "Basics/splitmix64.h"
#include "Basics/xoroshiro128plus.h"

#include <atomic>

namespace arangodb {
namespace basics {

template <uint64_t stripes = 64>
struct SharedPRNG {
  typedef std::function<uint64_t()> IdFunc;
  static uint64_t DefaultIdFunc() { return Thread::currentThreadNumber(); }

  SharedPRNG() : SharedPRNG(DefaultIdFunc) {}

  SharedPRNG(IdFunc f) : _id(f) {
    for (_mask = 1; _mask <= stripes; _mask <<= 1) {
    }
    TRI_ASSERT(_mask > stripes);
    _mask >>= 1;
    TRI_ASSERT(_mask <= stripes);
    _mask -= 1;

    splitmix64 seeder(0xdeadbeefdeadbeefULL);
    for (size_t i = 0; i < stripes; i++) {
      uint64_t seed1 = seeder.next();
      uint64_t seed2 = seeder.next();
      _prng[i].seed(seed1, seed2);
    }
  }

  uint64_t next() { return _prng[_id() & _mask].next(); }

 private:
  struct SingleSharedPRNG {
    SingleSharedPRNG() : _lock(false) {}

    void seed(uint64_t seed1, uint64_t seed2) {
      _lock = false;
      _prng.seed(seed1, seed2);
    }

    uint64_t next() {
      while (!lock()) {
        cpu_relax();
      }
      uint64_t result = _prng.next();
      _lock.store(false, std::memory_order_release);
      return result;
    }

   private:
    bool lock() {
      bool expected = false;
      return _lock.compare_exchange_weak(
          expected, true, std::memory_order_acquire, std::memory_order_release);
    }

   private:
    uint8_t _frontPadding[64];
    xoroshiro128plus _prng;
    std::atomic<bool> _lock;
    uint8_t _backpadding[64 - sizeof(xoroshiro128plus)];
  };

  SingleSharedPRNG _prng[stripes];
  IdFunc _id;
  uint64_t _mask;
};

}  // namespace basics
}  // namespace arangodb

#endif
