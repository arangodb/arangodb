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

#ifndef ARANGODB_BASICS_READ_UNLOCKER_H
#define ARANGODB_BASICS_READ_UNLOCKER_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief construct unlocker with file and line information
///
/// Ones needs to use macros twice to get a unique variable based on the line
/// number.
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_SHOW_LOCK_TIME

#define READ_UNLOCKER(obj, lock) \
  arangodb::basics::ReadUnlocker obj(&lock, __FILE__, __LINE__)

#else

#define READ_UNLOCKER(obj, lock) arangodb::basics::ReadUnlocker obj(&lock)

#endif

namespace arangodb {
namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocker
///
/// A ReadUnlocker unlocks a read-write lock during its lifetime and reacquires
/// the read-lock again when it is destroyed.
////////////////////////////////////////////////////////////////////////////////

class ReadUnlocker {
  ReadUnlocker(ReadUnlocker const&) = delete;
  ReadUnlocker& operator=(ReadUnlocker const&) = delete;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief unlocks the lock
  ///
  /// The constructor unlocks the lock, the destructors acquires a read-lock.
  //////////////////////////////////////////////////////////////////////////////

#ifdef TRI_SHOW_LOCK_TIME

  ReadUnlocker(ReadWriteLock* readWriteLock, char const* file, int line)
      : _readWriteLock(readWriteLock), _file(file), _line(line) {
    _readWriteLock->unlockRead();
  }

#else

  explicit ReadUnlocker(ReadWriteLock* readWriteLock)
      : _readWriteLock(readWriteLock) {
    _readWriteLock->unlockRead();
  }

#endif

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief unlocks the lock
  ///
  /// The constructor unlocks the lock, the destructors acquires a
  /// read-lock.
  ////////////////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief acquires the read-lock
  ////////////////////////////////////////////////////////////////////////////////

  ~ReadUnlocker() { _readWriteLock->readLock(); }

 private:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the read-write lock
  ////////////////////////////////////////////////////////////////////////////////

  ReadWriteLock* _readWriteLock;

#ifdef TRI_SHOW_LOCK_TIME

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief file
  ////////////////////////////////////////////////////////////////////////////////

  char const* _file;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief line number
  ////////////////////////////////////////////////////////////////////////////////

  int _line;

#endif
};
}  // namespace basics
}  // namespace arangodb

#endif
