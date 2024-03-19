////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "Basics/Locking.h"
#include "Basics/debugging.h"

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
      &lock, arangodb::basics::LockerType::BLOCKING, (condition), __FILE__,    \
      __LINE__)

namespace arangodb::basics {

/// @brief read locker
/// A ReadLocker read-locks a read-write lock during its lifetime and unlocks
/// the lock when it is destroyed.
template<class LockType>
class ReadLocker {
  ReadLocker(ReadLocker const&) = delete;
  ReadLocker& operator=(ReadLocker const&) = delete;
  ReadLocker& operator=(ReadLocker&& other) = delete;

 public:
  /// @brief acquires a read-lock
  /// The constructor acquires a read lock, the destructor unlocks the lock.
  ReadLocker(LockType* readWriteLock, LockerType type, bool condition,
             char const* file, int line) noexcept
      : _readWriteLock(readWriteLock),
        _file(file),
        _line(line),
        _isLocked(false) {
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
  }

  ReadLocker(ReadLocker&& other) noexcept
      : _readWriteLock(other._readWriteLock),
        _file(other._file),
        _line(other._line),
        _isLocked(other._isLocked) {
    // make only ourselves responsible for unlocking
    other.steal();
  }

  /// @brief releases the read-lock
  ~ReadLocker() {
    if (_isLocked) {
      _readWriteLock->unlockRead();
    }
  }

  /// @brief whether or not we acquired the lock
  bool isLocked() const noexcept { return _isLocked; }

  /// @brief eventually acquire the read lock
  void lockEventual() noexcept {
    while (!tryLock()) {
      std::this_thread::yield();
    }
    TRI_ASSERT(_isLocked);
  }

  bool tryLock() noexcept {
    TRI_ASSERT(!_isLocked);
    if (_readWriteLock->tryLockRead()) {
      _isLocked = true;
    }
    return _isLocked;
  }

  /// @brief acquire the read lock, blocking
  void lock() noexcept {
    TRI_ASSERT(!_isLocked);
    _readWriteLock->lockRead();
    _isLocked = true;
  }

  /// @brief unlocks the lock if we own it
  bool unlock() noexcept {
    if (_isLocked) {
      _readWriteLock->unlockRead();
      _isLocked = false;
      return true;
    }
    return false;
  }

  /// @brief steals the lock, but does not unlock it
  bool steal() noexcept {
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
};

}  // namespace arangodb::basics
