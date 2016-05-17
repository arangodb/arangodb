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

namespace arangodb {
namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief write locker
///
/// A WriteLocker write-locks a read-write lock during its lifetime and unlocks
/// the lock when it is destroyed.
////////////////////////////////////////////////////////////////////////////////

class WriteLocker {
  WriteLocker(WriteLocker const&) = delete;
  WriteLocker& operator=(WriteLocker const&) = delete;

 public:
////////////////////////////////////////////////////////////////////////////////
/// @brief aquires a write-lock
///
/// The constructors aquires a write lock, the destructors unlocks the lock.
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_SHOW_LOCK_TIME

  WriteLocker(ReadWriteLock* readWriteLock, char const* file, int line)
      : _readWriteLock(readWriteLock), _file(file), _line(line) {
    double t = TRI_microtime();
    _readWriteLock->writeLock();
    _time = TRI_microtime() - t;
  }

#else

  explicit WriteLocker(ReadWriteLock* readWriteLock)
      : _readWriteLock(readWriteLock) {
    _readWriteLock->writeLock();
  }

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief aquires a write-lock, with periodic sleeps while not acquired
/// sleep time is specified in nanoseconds
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_SHOW_LOCK_TIME

  WriteLocker(ReadWriteLock* readWriteLock, uint64_t sleepTime,
              char const* file, int line)
      : _readWriteLock(readWriteLock), _file(file), _line(line) {
    double t = TRI_microtime();
    while (!_readWriteLock->tryWriteLock()) {
#ifdef _WIN32
      usleep((unsigned long)sleepTime);
#else
      usleep((useconds_t)sleepTime);
#endif
    }
    _time = TRI_microtime() - t;
  }

#else

  WriteLocker(ReadWriteLock* readWriteLock, uint64_t sleepTime)
      : _readWriteLock(readWriteLock) {
    while (!_readWriteLock->tryWriteLock()) {
#ifdef _WIN32
      usleep((unsigned long)sleepTime);
#else
      usleep((useconds_t)sleepTime);
#endif
    }
  }

#endif

  //////////////////////////////////////////////////////////////////////////////
  /// @brief releases the write-lock
  //////////////////////////////////////////////////////////////////////////////

  ~WriteLocker() {
    _readWriteLock->unlock();

#ifdef TRI_SHOW_LOCK_TIME
    if (_time > TRI_SHOW_LOCK_THRESHOLD) {
      LOG(WARN) << "WriteLocker " << _file << ":" << _line << " took " << _time << " s";
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

class TryWriteLocker {
  TryWriteLocker(TryWriteLocker const&) = delete;
  TryWriteLocker& operator=(TryWriteLocker const&) = delete;

 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief tries to aquire a write-lock
  ///
  /// The constructor tries to aquire a write lock, the destructors unlocks the
  /// lock if we acquired it in the constructor
  ////////////////////////////////////////////////////////////////////////////////

  explicit TryWriteLocker(ReadWriteLock* readWriteLock)
      : _readWriteLock(readWriteLock) {
    _isLocked = _readWriteLock->tryWriteLock();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief releases the write-lock
  //////////////////////////////////////////////////////////////////////////////

  ~TryWriteLocker() {
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
