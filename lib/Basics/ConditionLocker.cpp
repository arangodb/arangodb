////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "ConditionLocker.h"

#include "Basics/ConditionVariable.h"
#include "Basics/debugging.h"

using namespace arangodb::basics;

ConditionLocker::ConditionLocker(ConditionVariable* conditionVariable) noexcept
    : _conditionVariable(conditionVariable), _isLocked(true) {
  _conditionVariable->lock();
}

/// @brief unlocks the condition variable
ConditionLocker::~ConditionLocker() {
  if (_isLocked) {
    unlock();
  }
}

/// @brief waits for an event to occur
void ConditionLocker::wait() { _conditionVariable->wait(); }

/// @brief waits for an event to occur, with a timeout in microseconds
/// returns true when the condition was signaled, false on timeout
bool ConditionLocker::wait(uint64_t delay) {
  return _conditionVariable->wait(delay);
}

/// @brief waits for an event to occur, with a timeout
/// returns true when the condition was signaled, false on timeout
bool ConditionLocker::wait(std::chrono::microseconds timeout) {
  return _conditionVariable->wait(timeout.count());
}

/// @brief broadcasts an event
void ConditionLocker::broadcast() noexcept { _conditionVariable->broadcast(); }

/// @brief signals an event
void ConditionLocker::signal() noexcept { _conditionVariable->signal(); }

/// @brief unlocks the variable (handle with care, no exception allowed)
void ConditionLocker::unlock() noexcept {
  if (_isLocked) {
    _conditionVariable->unlock();
    _isLocked = false;
  }
}

/// @brief relock the variable after unlock
void ConditionLocker::lock() noexcept {
  TRI_ASSERT(!_isLocked);
  _conditionVariable->lock();
  _isLocked = true;
}
