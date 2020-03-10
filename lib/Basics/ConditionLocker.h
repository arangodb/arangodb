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

#ifndef ARANGODB_BASICS_CONDITION_LOCKER_H
#define ARANGODB_BASICS_CONDITION_LOCKER_H 1

#include "Basics/Common.h"
#include "Basics/Locking.h"

#include <chrono>

/// @brief construct locker with file and line information
#ifdef ARANGODB_SHOW_LOCK_TIME

#define CONDITION_LOCKER(a, b) \
  arangodb::basics::ConditionLocker a(&(b), __FILE__, __LINE__)

#else

#define CONDITION_LOCKER(a, b) \
  ::arangodb::basics::ConditionLocker a(&(b))

#endif

namespace arangodb {
namespace basics {
class ConditionVariable;

/// @brief condition locker
///
/// A ConditionLocker locks a condition when constructed and releases the lock
/// when destroyed. It is possible the wait for an event in which case the lock
/// is released or to broadcast an event.
class ConditionLocker {
  ConditionLocker(ConditionLocker const&);
  ConditionLocker& operator=(ConditionLocker const&);

 public:
/// @brief locks the condition variable
///
/// The constructor locks the condition variable, the destructor unlocks
/// the condition variable
#ifdef ARANGODB_SHOW_LOCK_TIME

  ConditionLocker(ConditionVariable* conditionVariable, char const* file,
                  int line, bool showLockTime = true);

#else

  explicit ConditionLocker(ConditionVariable* conditionVariable);

#endif

  /// @brief unlocks the condition variable
  ~ConditionLocker();

 public:
  /// @brief whether or not the condition is locked
  bool isLocked() const { return _isLocked; }

  /// @brief waits for an event to occur
  void wait();

  /// @brief waits for an event to occur, using a timeout in micro seconds
  /// returns true when the condition was signaled, false on timeout
  bool wait(uint64_t);

  /// @brief waits for an event to occur, using a timeout
  /// returns true when the condition was signaled, false on timeout
  bool wait(std::chrono::microseconds);

  /// @brief broadcasts an event
  void broadcast();

  /// @brief signals an event
  void signal();

  /// @brief unlocks the variable (handle with care, no exception allowed)
  void unlock();

  /// @brief relock the variable after unlock
  void lock();

 private:
  /// @brief the condition
  ConditionVariable* _conditionVariable;

  /// @brief lock state
  bool _isLocked;

#ifdef ARANGODB_SHOW_LOCK_TIME

  /// @brief file
  char const* _file;

  /// @brief line number
  int _line;

  bool _showLockTime;

  /// @brief lock time
  double _time;

#endif
};
}  // namespace basics
}  // namespace arangodb

#endif
