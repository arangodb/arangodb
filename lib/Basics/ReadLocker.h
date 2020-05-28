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
/// @author Frank Celler
/// @author Achim Brandt
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_READ_LOCKER_H
#define ARANGODB_BASICS_READ_LOCKER_H 1

#include "Basics/Common.h"
#include "Basics/Locking.h"
#include "Basics/debugging.h"

#ifdef ARANGODB_SHOW_LOCK_TIME
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"
#endif

#include <thread>

/// @brief construct locker with file and line information
#define READ_LOCKER(obj, lock)                                                 \
  arangodb::basics::ReadLocker<typename std::decay<decltype(lock)>::type> obj( \
      &lock, arangodb::basics::LockerType::BLOCKING, true, __FILE__, __LINE__)

#define READ_LOCKER_EVENTUAL(obj, lock)                                        \
  arangodb::basics::ReadLocker<typename std::decay<decltype(lock)>::type> obj( \
      &lock, arangodb::basics::LockerType::EVENTUAL, true, __FILE__, __LINE__)

#define TRY_READ_LOCKER(obj, lock)                                             \
  arangodb::basics::ReadLocker<typename std::decay<decltype(lock)>::type> obj( \
      &lock, arangodb::basics::LockerType::TRY, true, __FILE__, __LINE__)

#define CONDITIONAL_READ_LOCKER(obj, lock, condition)                          \
  arangodb::basics::ReadLocker<typename std::decay<decltype(lock)>::type> obj( \
      &lock, arangodb::basics::LockerType::BLOCKING, (condition), __FILE__, __LINE__)

namespace arangodb {
namespace basics {

/// @brief read locker
/// A ReadLocker read-locks a read-write lock during its lifetime and unlocks
/// the lock when it is destroyed.
template <class LockType>
class ReadLocker {
  ReadLocker(ReadLocker const&) = delete;
  ReadLocker& operator=(ReadLocker const&) = delete;

 public:
  /// @brief acquires a read-lock
  /// The constructor acquires a read lock, the destructor unlocks the lock.
  ReadLocker(LockType* readWriteLock, LockerType type, bool condition,
             char const* file, int line)
      : _readWriteLock(readWriteLock),
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
  ~ReadLocker() {
    if (_isLocked) {
      _readWriteLock->unlockRead();
    }

#ifdef ARANGODB_SHOW_LOCK_TIME
    if (_time > TRI_SHOW_LOCK_THRESHOLD) {
      LOG_TOPIC("8e47e", INFO, arangodb::Logger::PERFORMANCE)
          << "ReadLocker for lock [" << _readWriteLock << "] " << _file << ":"
          << _line << " took " << _time << " s";
    }
#endif
  }

  /// @brief whether or not we acquired the lock
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
    if (_readWriteLock->tryLockRead()) {
      _isLocked = true;
    }
    return _isLocked;
  }

  /// @brief acquire the read lock, blocking
  void lock() {
    TRI_ASSERT(!_isLocked);
    _readWriteLock->lockRead();
    _isLocked = true;
  }

  /// @brief unlocks the lock if we own it
  bool unlock() {
    if (_isLocked) {
      _readWriteLock->unlockRead();
      _isLocked = false;
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
  /// @brief the read-write lock
  LockType* _readWriteLock;

  /// @brief file
  char const* _file;

  /// @brief line number
  int _line;

  /// @brief whether or not we acquired the lock
  bool _isLocked;

#ifdef ARANGODB_SHOW_LOCK_TIME
  /// @brief lock time
  double _time;
#endif
};

}  // namespace basics
}  // namespace arangodb

#endif
