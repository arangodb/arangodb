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
#include "Basics/SharedCounter.h"
#include "Basics/cpu-relax.h"

#include <stdint.h>
#include <atomic>
#include <chrono>
#include <thread>

using namespace arangodb::basics;
using namespace arangodb::cache;
using namespace std;

State::~State() {}

State::State() : _state(0), _lock() {}

State::State(State const& other)
    : _state(other._state.load()), _lock(other._lock) {}

State& State::operator=(State const& other) {
  if (this != &other) {
    _state = other._state.load();
    _lock = other._lock;
  }

  return *this;
}

bool State::isLocked() const { return _lock.isLocked(); }

bool State::isWriteLocked() const {
  return _lock.isWriteLocked();
}

bool State::readLock(int64_t maxTries) {
  uint64_t uTries = (maxTries < 0) ? UINT64_MAX : static_cast<uint64_t>(maxTries);
  return _lock.readLock(uTries);
}

bool State::writeLock(int64_t maxTries) {
  uint64_t uTries = (maxTries < 0) ? UINT64_MAX : static_cast<uint64_t>(maxTries);
  return _lock.writeLock(uTries);
}

void State::readUnlock() {
  _lock.readUnlock();
}

void State::writeUnlock() {
  _lock.writeUnlock();
}

bool State::isSet(State::Flag flag) const {
  return ((_state.load() & static_cast<uint32_t>(flag)) > 0);
}

bool State::isSet(State::Flag flag1, State::Flag flag2) const {
  return ((_state.load() &
           (static_cast<uint32_t>(flag1) | static_cast<uint32_t>(flag2))) > 0);
}

void State::toggleFlag(State::Flag flag) {
  _state ^= static_cast<uint32_t>(flag);
}

void State::clear() {
  _state = 0;
}
