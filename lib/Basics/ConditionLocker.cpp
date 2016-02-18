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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "ConditionLocker.h"
#include "Basics/ConditionVariable.h"

#ifdef TRI_SHOW_LOCK_TIME
#include "Basics/Logger.h"
#endif

using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief locks the condition variable
///
/// The constructors locks the condition variable, the destructors unlocks
/// the condition variable
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_SHOW_LOCK_TIME

ConditionLocker::ConditionLocker(ConditionVariable* conditionVariable,
                                 char const* file, int line)
    : _conditionVariable(conditionVariable),
      _file(file),
      _line(line),
      _time(0.0) {
  double t = TRI_microtime();
  _conditionVariable->lock();
  _time = TRI_microtime() - t;
}

#else

ConditionLocker::ConditionLocker(ConditionVariable* conditionVariable)
    : _conditionVariable(conditionVariable) {
  _conditionVariable->lock();
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks the condition variable
////////////////////////////////////////////////////////////////////////////////

ConditionLocker::~ConditionLocker() {
  _conditionVariable->unlock();

#ifdef TRI_SHOW_LOCK_TIME
  if (_time > TRI_SHOW_LOCK_THRESHOLD) {
    LOG(WARN) << "ConditionLocker " << _file << ":" << _line << " took " << _time << " s";
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for an event to occur
////////////////////////////////////////////////////////////////////////////////

void ConditionLocker::wait() { _conditionVariable->wait(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for an event to occur, with a timeout in microseconds
////////////////////////////////////////////////////////////////////////////////

bool ConditionLocker::wait(uint64_t delay) {
  return _conditionVariable->wait(delay);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief broadcasts an event
////////////////////////////////////////////////////////////////////////////////

void ConditionLocker::broadcast() { _conditionVariable->broadcast(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief signals an event
////////////////////////////////////////////////////////////////////////////////

void ConditionLocker::signal() { _conditionVariable->signal(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks the variable (handle with care, no exception allowed)
////////////////////////////////////////////////////////////////////////////////

void ConditionLocker::unlock() { _conditionVariable->unlock(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief relock the variable after unlock
////////////////////////////////////////////////////////////////////////////////

void ConditionLocker::lock() { _conditionVariable->lock(); }
