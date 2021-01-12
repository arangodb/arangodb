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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_LOCKS_H
#define ARANGODB_BASICS_LOCKS_H 1

#include "Basics/operating-system.h"
#include "Basics/system-compiler.h"

#ifdef TRI_HAVE_POSIX_THREADS
#include "Basics/locks-posix.h"
#endif

#ifdef TRI_HAVE_WIN32_THREADS
#include "Basics/locks-win32.h"
#endif

/// @brief initializes a new condition variable
void TRI_InitCondition(TRI_condition_t* cond);

/// @brief destroys a condition variable
void TRI_DestroyCondition(TRI_condition_t* cond);

/// @brief signals a condition variable
///
/// Note that you must hold the lock.
void TRI_SignalCondition(TRI_condition_t* cond);

/// @brief broad casts a condition variable
///
/// Note that you must hold the lock.
void TRI_BroadcastCondition(TRI_condition_t* cond);

/// @brief waits for a signal on a condition variable
///
/// Note that you must hold the lock.
void TRI_WaitCondition(TRI_condition_t* cond);

/// @brief waits for a signal with a timeout in micro-seconds
/// returns true when the condition was signaled, false on timeout
///
/// Note that you must hold the lock.
bool TRI_TimedWaitCondition(TRI_condition_t* cond, uint64_t delay);

/// @brief locks the mutex of a condition variable
void TRI_LockCondition(TRI_condition_t* cond);

/// @brief unlocks the mutex of a condition variable
void TRI_UnlockCondition(TRI_condition_t* cond);

#endif
