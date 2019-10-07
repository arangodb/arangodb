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

#ifndef ARANGODB_BASICS_MUTEX_UNLOCKER_H
#define ARANGODB_BASICS_MUTEX_UNLOCKER_H 1

#include "Basics/Common.h"
#include "Basics/Locking.h"
#include "Basics/debugging.h"

#ifdef ARANGODB_SHOW_LOCK_TIME
#include "Logger/Logger.h"
#endif

#include <thread>

#define MUTEX_UNLOCKER(obj, lock) \
  arangodb::basics::MutexUnlocker<typename std::decay<decltype(lock)>::type> obj(&(lock), __FILE__, __LINE__)

namespace arangodb {
namespace basics {

/// @brief mutex locker
/// A MutexUnlocker unlocks a mutex during its lifetime und locks the mutex
/// when it is destroyed.
template <class LockType>
class MutexUnlocker {
  MutexUnlocker(MutexUnlocker const&) = delete;
  MutexUnlocker& operator=(MutexUnlocker const&) = delete;

 public:
  /// The constructor unlocks the mutex, the destructor locks the mutex.
  MutexUnlocker(LockType* mutex, char const* file, int line)
      : _mutex(mutex), _file(file), _line(line), _isLocked(true) {
    unlock();
  }

  /// @brief releases the read-lock
  ~MutexUnlocker() {
    if (!_isLocked) {
      lock();
    }
  }

  bool isLocked() const { return _isLocked; }

  /// @brief acquire the mutex, blocking
  void lock() {
    TRI_ASSERT(!_isLocked);
    _mutex->lock();
    _isLocked = true;
  }

  /// @brief unlocks the mutex if we own it
  bool unlock() {
    if (_isLocked) {
      _isLocked = false;
      _mutex->unlock();
      return true;
    }
    return false;
  }

 private:
  /// @brief the mutex
  LockType* _mutex;

  /// @brief file
  char const* _file;

  /// @brief line number
  int _line;

  /// @brief whether or not the mutex is locked
  bool _isLocked;
};

}  // namespace basics
}  // namespace arangodb

#endif
