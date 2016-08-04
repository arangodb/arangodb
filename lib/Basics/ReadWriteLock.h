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

#ifndef ARANGODB_BASICS_READ_WRITE_LOCK_H
#define ARANGODB_BASICS_READ_WRITE_LOCK_H 1

#include "Basics/Common.h"
#include "Basics/locks.h"

namespace arangodb {
namespace basics {

/// @brief read-write lock
class ReadWriteLock {
  ReadWriteLock(ReadWriteLock const&) = delete;
  ReadWriteLock& operator=(ReadWriteLock const&) = delete;

 public:
  /// @brief constructs a read-write lock
  ReadWriteLock();

  /// @brief deletes read-write lock
  ~ReadWriteLock();

 public:
  /// @brief locks for reading
  void readLock();

  /// @brief tries to lock for reading
  bool tryReadLock();

  /// @brief tries to lock for reading, sleeping if the lock cannot be
  /// acquired instantly, sleepTime is in microseconds
  bool tryReadLock(uint64_t sleepTime);

  /// @brief locks for writing
  void writeLock();

  /// @brief tries to lock for writing
  bool tryWriteLock();

  /// @brief tries to lock for writing, sleeping if the lock cannot be
  /// acquired instantly, sleepTime is in microseconds
  bool tryWriteLock(uint64_t sleepTime);

  /// @brief releases the read-lock or write-lock
  void unlock();

  /// @brief releases the read-lock
  void unlockRead();
  
  /// @brief releases the write-lock
  void unlockWrite();

 private:
  /// @brief read-write lock variable
  TRI_read_write_lock_t _rwlock;

  /// @brief write lock marker
  bool _writeLocked;
};
}
}

#endif
