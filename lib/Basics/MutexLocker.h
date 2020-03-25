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

#ifndef ARANGODB_BASICS_MUTEX_LOCKER_H
#define ARANGODB_BASICS_MUTEX_LOCKER_H 1

#include "Basics/Common.h"
#include "Basics/Locking.h"
#include "Basics/debugging.h"

#ifdef ARANGODB_SHOW_LOCK_TIME
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"
#endif

#include <thread>

#define MUTEX_LOCKER(obj, lock)                                                 \
  arangodb::basics::MutexLocker<typename std::decay<decltype(lock)>::type> obj( \
      &(lock), arangodb::basics::LockerType::BLOCKING, true, __FILE__, __LINE__)

#define MUTEX_LOCKER_EVENTUAL(obj, lock, t)                                     \
  arangodb::basics::MutexLocker<typename std::decay<decltype(lock)>::type> obj( \
      &(lock), arangodb::basics::LockerType::EVENTUAL, true, __FILE__, __LINE__)

#define TRY_MUTEX_LOCKER(obj, lock)                                             \
  arangodb::basics::MutexLocker<typename std::decay<decltype(lock)>::type> obj( \
      &(lock), arangodb::basics::LockerType::TRY, true, __FILE__, __LINE__)

#define CONDITIONAL_MUTEX_LOCKER(obj, lock, condition)                          \
  arangodb::basics::MutexLocker<typename std::decay<decltype(lock)>::type> obj( \
      &(lock), arangodb::basics::LockerType::BLOCKING, (condition), __FILE__, __LINE__)

namespace arangodb {
namespace basics {

/// @brief mutex locker
/// A MutexLocker locks a mutex during its lifetime und unlocks the mutex
/// when it is destroyed.
template <class LockType>
class MutexLocker {
  MutexLocker(MutexLocker const&) = delete;
  MutexLocker& operator=(MutexLocker const&) = delete;

 public:
  /// @brief acquires a mutex
  /// The constructor acquires the mutex, the destructor unlocks the mutex.
  MutexLocker(LockType* mutex, LockerType type, bool condition, char const* file, int line)
      : _mutex(mutex),
        _file(file),
        _line(line),
#ifdef ARANGODB_SHOW_LOCK_TIME
        _isLocked(false),
        _time(0.0) {
#else
        _isLocked(false) {
#endif

#ifdef ARANGODB_SHOW_LOCK_TIME
    // fetch current time
    double t = TRI_microtime();
#endif

    if (condition) {
      if (type == LockerType::BLOCKING) {
        lock();
        TRI_ASSERT(_isLocked);
      } else if (type == LockerType::EVENTUAL) {
        lockEventual();
        TRI_ASSERT(_isLocked);
      } else if (type == LockerType::TRY) {
        _isLocked = tryLock();
      }
    }

#ifdef ARANGODB_SHOW_LOCK_TIME
    // add elapsed time to time tracker
    _time = TRI_microtime() - t;
#endif
  }

  /// @brief releases the read-lock
  ~MutexLocker() {
    if (_isLocked) {
      _mutex->unlock();
    }

#ifdef ARANGODB_SHOW_LOCK_TIME
    if (_time > TRI_SHOW_LOCK_THRESHOLD) {
      LOG_TOPIC("bb435", INFO, arangodb::Logger::PERFORMANCE)
          << "MutexLocker for lock [" << _mutex << "]" << _file << ":" << _line
          << " took " << _time << " s";
    }
#endif
  }

  bool isLocked() const { return _isLocked; }

  /// @brief eventually acquire the read lock
  void lockEventual() {
    while (!tryLock()) {
      std::this_thread::yield();
    }
    TRI_ASSERT(_isLocked);
  }

  bool tryLock() {
    TRI_ASSERT(!_isLocked);
    if (_mutex->tryLock()) {
      _isLocked = true;
    }
    return _isLocked;
  }

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

  /// @brief steals the lock, but does not unlock it
  bool steal() {
    if (_isLocked) {
      _isLocked = false;
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

#ifdef ARANGODB_SHOW_LOCK_TIME
  /// @brief lock time
  double _time;
#endif
};

}  // namespace basics
}  // namespace arangodb

#endif
