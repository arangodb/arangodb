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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_WRITE_LOCKER_H
#define ARANGODB_BASICS_WRITE_LOCKER_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"

#ifdef TRI_SHOW_LOCK_TIME
#include "Logger/Logger.h"
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief construct locker with file and line information
///
/// Ones needs to use macros twice to get a unique variable based on the line
/// number.
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_SHOW_LOCK_TIME

#define WRITE_LOCKER(obj, lock) \
  arangodb::basics::WriteLocker obj(&lock, __FILE__, __LINE__)

#define WRITE_LOCKER_EVENTUAL(obj, lock, t) \
  arangodb::basics::WriteLocker obj(&lock, t, __FILE__, __LINE__)

#else

#define WRITE_LOCKER(obj, lock) arangodb::basics::WriteLocker obj(&lock)

#define WRITE_LOCKER_EVENTUAL(obj, lock, t) \
  arangodb::basics::WriteLocker obj(&lock, t)

#endif

#define TRY_WRITE_LOCKER(obj, lock) arangodb::basics::TryWriteLocker obj(&lock)

#define CONDITIONAL_WRITE_LOCKER(obj, lock, condition) arangodb::basics::ConditionalWriteLocker obj(&lock, (condition))

namespace arangodb {
namespace basics {

/// @brief write locker
/// A WriteLocker write-locks a read-write lock during its lifetime and unlocks
/// the lock when it is destroyed.
class WriteLocker {
  WriteLocker(WriteLocker const&) = delete;
  WriteLocker& operator=(WriteLocker const&) = delete;

 public:
#ifdef TRI_SHOW_LOCK_TIME

  /// @brief aquires a write-lock
  /// The constructors acquire a write lock, the destructor unlocks the lock.
  WriteLocker(ReadWriteLock* readWriteLock, char const* file, int line)
      : _readWriteLock(readWriteLock), _file(file), _line(line), _isLocked(false) {
    double t = TRI_microtime();
    lock();
    _time = TRI_microtime() - t;
  }
  
  /// @brief aquires a write-lock, with periodic sleeps while not acquired
  /// sleep time is specified in nanoseconds
  WriteLocker(ReadWriteLock* readWriteLock, uint64_t sleepTime,
              char const* file, int line)
      : _readWriteLock(readWriteLock), _file(file), _line(line), _isLocked(false) {
    double t = TRI_microtime();
    lockEventual(sleepTime);
    _time = TRI_microtime() - t;
  }

#else

  /// @brief aquires a write-lock
  /// The constructors acquire a write lock, the destructor unlocks the lock.
  explicit WriteLocker(ReadWriteLock* readWriteLock)
      : _readWriteLock(readWriteLock), _isLocked(false) {
    lock();
  }

  /// @brief aquires a write-lock, with periodic sleeps while not acquired
  /// sleep time is specified in nanoseconds
  WriteLocker(ReadWriteLock* readWriteLock, uint64_t sleepTime)
      : _readWriteLock(readWriteLock), _isLocked(false) {
    lockEventual(sleepTime);
    _isLocked = true;
  }

#endif

  /// @brief releases the write-lock
  ~WriteLocker() {
    if (_isLocked) {
      _readWriteLock->unlock();
    }

#ifdef TRI_SHOW_LOCK_TIME
    if (_time > TRI_SHOW_LOCK_THRESHOLD) {
      LOG(WARN) << "WriteLocker " << _file << ":" << _line << " took " << _time << " s";
    }
#endif
  }
  
  /// @brief whether or not we acquired the lock
  bool isLocked() const { return _isLocked; }

  /// @brief eventually acquire the write lock
  void lockEventual(uint64_t sleepTime) {
    while (!_readWriteLock->tryWriteLock()) {
#ifdef _WIN32
      usleep((unsigned long)sleepTime);
#else
      usleep((useconds_t)sleepTime);
#endif
    }
    _isLocked = true;
  }

  /// @brief acquire the write lock, blocking
  void lock() {
    _readWriteLock->writeLock(); 
    _isLocked = true;
  }

  /// @brief unlocks the lock if we own it
  bool unlock() {
    if (_isLocked) {
      _readWriteLock->unlock();
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
  ReadWriteLock* _readWriteLock;

#ifdef TRI_SHOW_LOCK_TIME

  /// @brief file
  char const* _file;

  /// @brief line number
  int _line;

  /// @brief lock time
  double _time;

#endif

  /// @brief whether or not the lock was acquired
  bool _isLocked;
};

class TryWriteLocker {
  TryWriteLocker(TryWriteLocker const&) = delete;
  TryWriteLocker& operator=(TryWriteLocker const&) = delete;

 public:
  /// @brief tries to acquire a write-lock
  /// The constructor tries to aquire a write lock, the destructors unlocks the
  /// lock if we acquired it in the constructor
  explicit TryWriteLocker(ReadWriteLock* readWriteLock)
      : _readWriteLock(readWriteLock), _isLocked(false) {
    _isLocked = _readWriteLock->tryWriteLock();
  }

  /// @brief releases the write-lock
  ~TryWriteLocker() {
    if (_isLocked) {
      _readWriteLock->unlock();
    }
  }

  /// @brief whether or not we acquired the lock
  bool isLocked() const { return _isLocked; }
  
  /// @brief eventually acquire the write lock
  void lockEventual(uint64_t sleepTime) {
    while (!_readWriteLock->tryWriteLock()) {
#ifdef _WIN32
      usleep((unsigned long)sleepTime);
#else
      usleep((useconds_t)sleepTime);
#endif
    }
    _isLocked = true;
  }

  /// @brief acquire the write lock, blocking
  void lock() { 
    _readWriteLock->writeLock(); 
    _isLocked = true;
  }
  
  /// @brief unlocks the read-write lock
  bool unlock() {
    if (_isLocked) {
      _readWriteLock->unlock();
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
  ReadWriteLock* _readWriteLock;

  /// @brief whether or not we acquired the lock
  bool _isLocked;
};

class ConditionalWriteLocker {
  ConditionalWriteLocker(ConditionalWriteLocker const&) = delete;
  ConditionalWriteLocker& operator=(ConditionalWriteLocker const&) = delete;

 public:
  /// @brief acquire a write-lock
  /// The constructor tries to write-lock the lock, the destructor unlocks the
  /// lock if it was acquired in the constructor
  ConditionalWriteLocker(ReadWriteLock* readWriteLock, bool condition)
      : _readWriteLock(readWriteLock), _isLocked(false) {
    if (condition) {
      _readWriteLock->writeLock();
      _isLocked = true;
    }
  }

  /// @brief releases the write-lock
  ~ConditionalWriteLocker() {
    if (_isLocked) {
      _readWriteLock->unlock();
    }
  }

  /// @brief whether or not we acquired the lock
  bool isLocked() const { return _isLocked; }
  
  /// @brief eventually acquire the write lock
  void lockEventual(uint64_t sleepTime) {
    while (!_readWriteLock->tryWriteLock()) {
#ifdef _WIN32
      usleep((unsigned long)sleepTime);
#else
      usleep((useconds_t)sleepTime);
#endif
    }
    _isLocked = true;
  }

  /// @brief acquire the write lock, blocking
  void lock() { 
    _readWriteLock->writeLock(); 
    _isLocked = true; 
  }
  
  /// @brief unlocks the read-write lock
  bool unlock() {
    if (_isLocked) {
      _readWriteLock->unlock();
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

  static constexpr bool DoLock() { return true; }
  static constexpr bool DoNotLock() { return false; }

 private:
  /// @brief the read-write lock
  ReadWriteLock* _readWriteLock;

  /// @brief whether or not we acquired the lock
  bool _isLocked;
};

}
}

#endif
