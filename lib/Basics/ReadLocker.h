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

#ifndef ARANGODB_BASICS_READ_LOCKER_H
#define ARANGODB_BASICS_READ_LOCKER_H 1

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

#define READ_LOCKER(obj, lock) \
  arangodb::basics::ReadLocker obj(&lock, __FILE__, __LINE__)

#define READ_LOCKER_EVENTUAL(obj, lock, t) \
  arangodb::basics::ReadLocker obj(&lock, t, __FILE__, __LINE__)

#else

#define READ_LOCKER(obj, lock) arangodb::basics::ReadLocker obj(&lock)

#define READ_LOCKER_EVENTUAL(obj, lock, t) \
  arangodb::basics::ReadLocker obj(&lock, t)

#endif

#define TRY_READ_LOCKER(obj, lock) arangodb::basics::TryReadLocker obj(&lock)

namespace arangodb {
namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief read locker
///
/// A ReadLocker read-locks a read-write lock during its lifetime and unlocks
/// the lock when it is destroyed.
////////////////////////////////////////////////////////////////////////////////

class ReadLocker {
  ReadLocker(ReadLocker const&) = delete;
  ReadLocker& operator=(ReadLocker const&) = delete;

 public:
////////////////////////////////////////////////////////////////////////////////
/// @brief aquires a read-lock
///
/// The constructors read-locks the lock, the destructors unlocks the lock.
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_SHOW_LOCK_TIME

  ReadLocker(ReadWriteLock* readWriteLock, char const* file, int line)
      : _readWriteLock(readWriteLock), _file(file), _line(line) {
    double t = TRI_microtime();
    _readWriteLock->readLock();
    _time = TRI_microtime() - t;
  }

#else

  explicit ReadLocker(ReadWriteLock* readWriteLock)
      : _readWriteLock(readWriteLock) {
    _readWriteLock->readLock();
  }

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief aquires a read-lock, with periodic sleeps while not acquired
/// sleep time is specified in nanoseconds
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_SHOW_LOCK_TIME

  ReadLocker(ReadWriteLock* readWriteLock, uint64_t sleepTime, char const* file,
             int line)
      : _readWriteLock(readWriteLock), _file(file), _line(line) {
    double t = TRI_microtime();
    while (!_readWriteLock->tryReadLock()) {
#ifdef _WIN32
      usleep((unsigned long)sleepTime);
#else
      usleep((useconds_t)sleepTime);
#endif
    }
    _time = TRI_microtime() - t;
  }

#else

  ReadLocker(ReadWriteLock* readWriteLock, uint64_t sleepTime)
      : _readWriteLock(readWriteLock) {
    while (!_readWriteLock->tryReadLock()) {
#ifdef _WIN32
      usleep((unsigned long)sleepTime);
#else
      usleep((useconds_t)sleepTime);
#endif
    }
  }

#endif

  //////////////////////////////////////////////////////////////////////////////
  /// @brief releases the read-lock
  //////////////////////////////////////////////////////////////////////////////

  ~ReadLocker() {
    _readWriteLock->unlock();

#ifdef TRI_SHOW_LOCK_TIME
    if (_time > TRI_SHOW_LOCK_THRESHOLD) {
      LOG(WARN) << "ReadLocker " << _file << ":" << _line << " took " << _time << " s";
    }
#endif
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the read-write lock
  //////////////////////////////////////////////////////////////////////////////

  ReadWriteLock* _readWriteLock;

#ifdef TRI_SHOW_LOCK_TIME

  //////////////////////////////////////////////////////////////////////////////
  /// @brief file
  //////////////////////////////////////////////////////////////////////////////

  char const* _file;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief line number
  //////////////////////////////////////////////////////////////////////////////

  int _line;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lock time
  //////////////////////////////////////////////////////////////////////////////

  double _time;

#endif
};

class TryReadLocker {
  TryReadLocker(TryReadLocker const&) = delete;
  TryReadLocker& operator=(TryReadLocker const&) = delete;

 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief tries to aquire a read-lock
  ///
  /// The constructors tries to read-lock the lock, the destructors unlocks the
  /// lock if it was acquired in the constructor
  ////////////////////////////////////////////////////////////////////////////////

  explicit TryReadLocker(ReadWriteLock* readWriteLock)
      : _readWriteLock(readWriteLock), _isLocked(false) {
    _isLocked = _readWriteLock->tryReadLock();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief releases the read-lock
  //////////////////////////////////////////////////////////////////////////////

  ~TryReadLocker() {
    if (_isLocked) {
      _readWriteLock->unlock();
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not we acquired the lock
  //////////////////////////////////////////////////////////////////////////////

  bool isLocked() const { return _isLocked; }
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief unlocks the read-write lock
  //////////////////////////////////////////////////////////////////////////////

  bool unlock() {
    if (_isLocked) {
      _readWriteLock->unlock();
      _isLocked = false;
      return true;
    }
    return false;
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the read-write lock
  //////////////////////////////////////////////////////////////////////////////

  ReadWriteLock* _readWriteLock;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not we acquired the lock
  //////////////////////////////////////////////////////////////////////////////

  bool _isLocked;
};
}
}

#endif
