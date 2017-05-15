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

#include <stdint.h>
#include <atomic>

using namespace arangodb::cache;

State::State() : _state(0) {}

State::State(State const& other) : _state(other._state.load()) {}

State& State::operator=(State const& other) {
  if (this != &other) {
    _state = other._state.load();
  }

  return *this;
}

bool State::isLocked() const {
  return ((_state.load() & static_cast<uint32_t>(Flag::locked)) > 0);
}

bool State::lock(int64_t maxTries, State::CallbackType cb) {
  int64_t attempt = 0;
  while (maxTries < 0 || attempt < maxTries) {
    // expect unlocked, but need to preserve migrating status
    uint32_t expected = _state.load() & (~static_cast<uint32_t>(Flag::locked));
    bool success = _state.compare_exchange_strong(
        expected,
        (expected | static_cast<uint32_t>(Flag::locked)));  // try to lock
    if (success) {
      cb();
      return true;
    }
    attempt++;
    // TODO: exponential back-off for failure?
  }

  return false;
}

void State::unlock() {
  TRI_ASSERT(isLocked());
  _state &= ~static_cast<uint32_t>(Flag::locked);
}

bool State::isSet(State::Flag flag) const {
  TRI_ASSERT(isLocked());
  return ((_state.load() & static_cast<uint32_t>(flag)) > 0);
}

bool State::isSet(State::Flag flag1, State::Flag flag2) const {
  TRI_ASSERT(isLocked());
  return ((_state.load() & (static_cast<uint32_t>(flag1) | static_cast<uint32_t>(flag2))) > 0);
}

void State::toggleFlag(State::Flag flag) {
  TRI_ASSERT(isLocked());
  _state ^= static_cast<uint32_t>(flag);
}

void State::clear() {
  TRI_ASSERT(isLocked());
  _state = static_cast<uint32_t>(Flag::locked);
}
