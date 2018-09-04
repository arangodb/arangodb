////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
/// Copyright 2015-present Facebook, Inc.
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
/// @author Keith Adams <kma@fb.com>
/// @author Jordan DeLong <delong.j@fb.com>
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_SPIN_LOCK_H 1
#define ARANGO_SPIN_LOCK_H 1

#include <atomic>
#include "cpu-relax.h"

namespace arangodb {
namespace basics {

/// A really, *really* small spinlock for fine-grained locking of lots
/// of teeny-tiny data.
///
/// Zero initializing these is guaranteed to be as good as calling
/// init(), since the free state is guaranteed to be all-bits zero.
///
/// This class should be kept a POD, so we can used it in other packed
/// structs (gcc does not allow __attribute__((__packed__)) on structs that
/// contain non-POD data).  This means avoid adding a constructor, or
/// making some members private, etc.
class MicroSpinLock {
  enum { FREE = 0, LOCKED = 1 };
  // lock_ can't be std::atomic<> to preserve POD-ness.
  uint8_t lock_;
  
  // Initialize this MSL.  It is unnecessary to call this if you
  // zero-initialize the MicroSpinLock.
  void init() {
    payload()->store(FREE);
  }
  
  bool try_lock() {
    return cas(FREE, LOCKED);
  }
  
  void lock() {
    while (!try_lock()) {
      do {
        cpu_relax();
      } while (payload()->load(std::memory_order_relaxed) == LOCKED);
    }
    assert(payload()->load() == LOCKED);
  }
  
  void unlock() {
    assert(payload()->load() == LOCKED);
    payload()->store(FREE, std::memory_order_release);
  }
  
private:
  std::atomic<uint8_t>* payload() {
    return reinterpret_cast<std::atomic<uint8_t>*>(&this->lock_);
  }
  
  bool cas(uint8_t compare, uint8_t newVal) {
    return std::atomic_compare_exchange_strong_explicit(payload(), &compare, newVal,
                                                        std::memory_order_acquire,
                                                        std::memory_order_relaxed);
  }
};
static_assert(std::is_pod<MicroSpinLock>::value, "MicroSpinLock must be kept a POD type.");
    
}}

#endif // ARANGO_SPIN_LOCK_H
