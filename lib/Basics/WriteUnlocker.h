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

#ifndef ARANGODB_BASICS_WRITE_UNLOCKER_H
#define ARANGODB_BASICS_WRITE_UNLOCKER_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief construct unlocker with file and line information
///
/// Ones needs to use macros twice to get a unique variable based on the line
/// number.
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_SHOW_LOCK_TIME

#define WRITE_UNLOCKER(obj, lock) \
  arangodb::basics::WriteUnlocker obj(&lock, __FILE__, __LINE__)

#else

#define WRITE_UNLOCKER(obj, lock) arangodb::basics::WriteUnlocker obj(&lock)

#endif

namespace arangodb {
namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocker
///
/// A WriteUnlocker unlocks a read-write lock during its lifetime and reaquires
/// the write lock when it is destroyed.
////////////////////////////////////////////////////////////////////////////////

class WriteUnlocker {
  WriteUnlocker(WriteUnlocker const&) = delete;
  WriteUnlocker& operator=(WriteUnlocker const&) = delete;

 public:
//////////////////////////////////////////////////////////////////////////////
/// @brief unlocks the lock
///
/// The constructor unlocks the lock, the destructors aquires a write-lock.
//////////////////////////////////////////////////////////////////////////////

#ifdef TRI_SHOW_LOCK_TIME

  //////////////////////////////////////////////////////////////////////////////
  /// @brief unlocks the lock
  ///
  /// The constructor unlocks the lock, the destructors aquires a write-lock.
  //////////////////////////////////////////////////////////////////////////////

  WriteUnlocker(ReadWriteLocker* readWriteLock, char const* file, int line)
      : _readWriteLock(readWriteLock), _file(file), _line(line) {
    _readWriteLock->unlock();
  }

#else

  explicit WriteUnlocker(ReadWriteLock* readWriteLock)
      : _readWriteLock(readWriteLock) {
      _readWriteLock->unlock();
    }

#endif

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief aquires the write-lock
  ////////////////////////////////////////////////////////////////////////////////

  ~WriteUnlocker() { _readWriteLock->writeLock(); }

 private:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the read-write lock
  ////////////////////////////////////////////////////////////////////////////////

  basics::ReadWriteLock* _readWriteLock;

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
}
}

#endif
