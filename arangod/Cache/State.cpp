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

#include "Cache/State.h"
#include "Basics/Common.h"
#include "Cache/Common.h"

#include <stdint.h>
#include <atomic>
#include <chrono>
#include <thread>

using namespace arangodb::cache;
using namespace std;

State::~State() {}

State::State() : _state(0), _lock(0) {}

State::State(State const& other)
    : _state(other._state.load()), _lock(other._lock.load()) {}

State& State::operator=(State const& other) {
  if (this != &other) {
    _state = other._state.load();
    _lock = other._lock.load();
  }

  return *this;
}

bool State::isLocked() const { return (_lock.load() > 0); }

bool State::isWriteLocked() const {
  return (_lock.load() == static_cast<uint32_t>(LockMask::write));
}

bool State::readLock(int64_t maxTries) {
  int64_t attempt = 0;
  while (maxTries < 0 || attempt < maxTries) {
    // expect not write-locked, not read-saturated
    uint32_t current = _lock.load(memory_order_relaxed);
    uint32_t expected =
        current & (~static_cast<uint32_t>(LockMask::write));
    if (current == expected) {
      uint32_t desired = expected + 1;
      // assume we never get read counter overflow

      bool success = _lock.compare_exchange_strong(expected, desired,
          memory_order_acquire, memory_order_relaxed);
      if (success) {
        return true;
      }
    }
    attempt++;
    cpu_relax();
    // TODO: exponential back-off for failure?
  }

  return false;
}

bool State::writeLock(int64_t maxTries) {
  int64_t attempt = 0;
  while (maxTries < 0 || attempt < maxTries) {
    // first try to set the write flag
    uint32_t current = _lock.load(memory_order_relaxed);
    uint32_t expected =
        current & (~static_cast<uint32_t>(LockMask::write));
    if (current == expected) {
      uint32_t desired = expected | static_cast<uint32_t>(LockMask::write);
      bool success = _lock.compare_exchange_strong(expected, desired,
          memory_order_acquire, memory_order_relaxed);
      if (success) {
        // now wait for the readers to finish
        while ((maxTries < 0 || attempt < maxTries) &&
               (_lock.load(memory_order_acquire) & static_cast<uint32_t>(LockMask::read)) > 0) {
          attempt++;
          cpu_relax();
          continue;
        }
        if ((_lock.load(memory_order_acquire) & static_cast<uint32_t>(LockMask::read)) > 0) {
          // too many attempts, unset write flag and bail out
          _lock &= ~static_cast<uint32_t>(LockMask::write);
          return false;
        }
        return true;  // locked!
      }
    }
    attempt++;
    cpu_relax();
  }

  return false;
}

void State::unlock() {
  TRI_ASSERT(isLocked());
  while (true) {
    uint32_t expected = _lock.load(memory_order_relaxed);
    uint32_t desired = (expected == static_cast<uint32_t>(LockMask::write))
                           ? 0
                           : (expected - 1);
    bool success = _lock.compare_exchange_strong(expected, desired,
        memory_order_release, memory_order_relaxed);
    if (success) {
      break;
    }
  }
}

bool State::isSet(State::Flag flag) const {
  TRI_ASSERT(isLocked());
  return ((_state.load() & static_cast<uint32_t>(flag)) > 0);
}

bool State::isSet(State::Flag flag1, State::Flag flag2) const {
  TRI_ASSERT(isLocked());
  return ((_state.load() &
           (static_cast<uint32_t>(flag1) | static_cast<uint32_t>(flag2))) > 0);
}

void State::toggleFlag(State::Flag flag) {
  TRI_ASSERT(isWriteLocked());
  _state ^= static_cast<uint32_t>(flag);
}

void State::clear() {
  TRI_ASSERT(isWriteLocked());
  _state = 0;
}
