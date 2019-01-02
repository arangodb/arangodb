////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "locks.h"

/// @brief initializes a new condition variable
void TRI_InitCondition(TRI_condition_t* cond) {
  InitializeCriticalSection(&cond->_lockWaiters);
  InitializeConditionVariable(&cond->_conditionVariable);
}

/// @brief destroys a condition variable
void TRI_DestroyCondition(TRI_condition_t* cond) {
  DeleteCriticalSection(&cond->_lockWaiters);
}

/// @brief signals a condition variable
///
/// Note that you must hold the lock.
void TRI_SignalCondition(TRI_condition_t* cond) {
  WakeConditionVariable(&cond->_conditionVariable);
}

/// @brief broad casts a condition variable
///
/// Note that you must hold the lock.
void TRI_BroadcastCondition(TRI_condition_t* cond) {
  WakeAllConditionVariable(&cond->_conditionVariable);
}

/// @brief waits for a signal on a condition variable
///
/// Note that you must hold the lock.
void TRI_WaitCondition(TRI_condition_t* cond) {
  SleepConditionVariableCS(&cond->_conditionVariable, &cond->_lockWaiters, INFINITE);
}

/// @brief waits for a signal with a timeout in micro-seconds
///
/// Note that you must hold the lock.
bool TRI_TimedWaitCondition(TRI_condition_t* cond, uint64_t delay) {
  // ...........................................................................
  // The POSIX threads function pthread_cond_timedwait accepts microseconds
  // while the Windows function accepts milliseconds
  // ...........................................................................
  delay = delay / 1000;

  if (SleepConditionVariableCS(&cond->_conditionVariable, &cond->_lockWaiters,
                               (DWORD)delay) != 0) {
    return true;
  }

  DWORD res = GetLastError();

  if (res == ERROR_TIMEOUT) {
    return false;
  }

  return false;
}

/// @brief locks the mutex of a condition variable
void TRI_LockCondition(TRI_condition_t* cond) {
  EnterCriticalSection(&cond->_lockWaiters);
}

/// @brief unlocks the mutex of a condition variable
void TRI_UnlockCondition(TRI_condition_t* cond) {
  LeaveCriticalSection(&cond->_lockWaiters);
}
