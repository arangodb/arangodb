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
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "ConditionVariable.h"

using namespace arangodb::basics;

/// @brief constructs a condition variable
ConditionVariable::ConditionVariable() : _condition() {
  TRI_InitCondition(&_condition);
}

/// @brief deletes the condition variable
ConditionVariable::~ConditionVariable() { TRI_DestroyCondition(&_condition); }

/// @brief locks the condition variable
void ConditionVariable::lock() { TRI_LockCondition(&_condition); }

/// @brief releases the lock on the condition variable
void ConditionVariable::unlock() { TRI_UnlockCondition(&_condition); }

/// @brief waits for an event
void ConditionVariable::wait() { TRI_WaitCondition(&_condition); }

/// @brief waits for an event with timeout in micro seconds
/// returns true when the condition was signaled, false on timeout
bool ConditionVariable::wait(uint64_t delay) {
  return TRI_TimedWaitCondition(&_condition, delay);
}
bool ConditionVariable::wait(std::chrono::microseconds delay_us) {
  return TRI_TimedWaitCondition(&_condition, delay_us.count());
}

/// @brief signals all waiting threads
void ConditionVariable::broadcast() { TRI_BroadcastCondition(&_condition); }

/// @brief signals a waiting thread
void ConditionVariable::signal() { TRI_SignalCondition(&_condition); }
