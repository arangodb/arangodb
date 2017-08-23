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

#include "Cache/BucketState.h"
#include "Basics/Common.h"
#include "Basics/cpu-relax.h"

#include <stdint.h>
#include <atomic>

using namespace arangodb::basics;
using namespace arangodb::cache;

BucketState::BucketState() : _state(0) {}

BucketState::BucketState(BucketState const& other)
    : _state(other._state.load()) {}

BucketState& BucketState::operator=(BucketState const& other) {
  if (this != &other) {
    _state = other._state.load();
  }

  return *this;
}

bool BucketState::isLocked() const {
  return ((_state.load() & static_cast<uint32_t>(Flag::locked)) > 0);
}

bool BucketState::lock(int64_t maxTries, BucketState::CallbackType cb) {
  int64_t attempt = 0;
  while (maxTries < 0 || attempt < maxTries) {
    // expect unlocked, but need to preserve migrating status
    uint32_t current = _state.load(std::memory_order_relaxed);
    uint32_t expected = current & (~static_cast<uint32_t>(Flag::locked));
    if (current == expected) {
      uint32_t desired = expected | static_cast<uint32_t>(Flag::locked);
      // try to lock
      bool success = _state.compare_exchange_strong(
          expected, desired,
          std::memory_order_acq_rel, std::memory_order_relaxed);
      if (success) {
        cb();
        return true;
      }
    }
    attempt++;
    cpu_relax();
    // TODO: exponential back-off for failure?
  }

  return false;
}

void BucketState::unlock() {
  TRI_ASSERT(isLocked());
  _state.fetch_and(~static_cast<uint32_t>(Flag::locked), std::memory_order_release);
}

bool BucketState::isSet(BucketState::Flag flag) const {
  TRI_ASSERT(isLocked());
  return ((_state.load() & static_cast<uint32_t>(flag)) > 0);
}

bool BucketState::isSet(BucketState::Flag flag1,
                        BucketState::Flag flag2) const {
  TRI_ASSERT(isLocked());
  return ((_state.load() &
           (static_cast<uint32_t>(flag1) | static_cast<uint32_t>(flag2))) > 0);
}

void BucketState::toggleFlag(BucketState::Flag flag) {
  TRI_ASSERT(isLocked());
  _state ^= static_cast<uint32_t>(flag);
}

void BucketState::clear() {
  TRI_ASSERT(isLocked());
  _state = static_cast<uint32_t>(Flag::locked);
}
