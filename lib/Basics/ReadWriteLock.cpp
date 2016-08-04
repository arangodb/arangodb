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

#include "ReadWriteLock.h"

using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a read-write lock
////////////////////////////////////////////////////////////////////////////////

ReadWriteLock::ReadWriteLock() : _rwlock(), _writeLocked(false) {
  TRI_InitReadWriteLock(&_rwlock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes read-write lock
////////////////////////////////////////////////////////////////////////////////

ReadWriteLock::~ReadWriteLock() { TRI_DestroyReadWriteLock(&_rwlock); }

////////////////////////////////////////////////////////////////////////////////
/// @brief locks for reading
////////////////////////////////////////////////////////////////////////////////

void ReadWriteLock::readLock() { TRI_ReadLockReadWriteLock(&_rwlock); }

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to lock for reading
////////////////////////////////////////////////////////////////////////////////

bool ReadWriteLock::tryReadLock() {
  return TRI_TryReadLockReadWriteLock(&_rwlock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to lock for reading, sleeping if the lock cannot be
/// acquired instantly, sleepTime is in microseconds
////////////////////////////////////////////////////////////////////////////////

bool ReadWriteLock::tryReadLock(uint64_t sleepTime) {
  while (true) {
    if (tryReadLock()) {
      return true;
    }

#ifdef _WIN32
    usleep((unsigned long)sleepTime);
#else
    usleep((useconds_t)sleepTime);
#endif
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks for writing
////////////////////////////////////////////////////////////////////////////////

void ReadWriteLock::writeLock() {
  TRI_WriteLockReadWriteLock(&_rwlock);

  _writeLocked = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to lock for writing
////////////////////////////////////////////////////////////////////////////////

bool ReadWriteLock::tryWriteLock() {
  if (!TRI_TryWriteLockReadWriteLock(&_rwlock)) {
    return false;
  }

  _writeLocked = true;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to lock for writing, sleeping if the lock cannot be
/// acquired instantly, sleepTime is in microseconds
////////////////////////////////////////////////////////////////////////////////

bool ReadWriteLock::tryWriteLock(uint64_t sleepTime) {
  while (true) {
    if (tryWriteLock()) {
      return true;
    }

#ifdef _WIN32
    usleep((unsigned long)sleepTime);
#else
    usleep((useconds_t)sleepTime);
#endif
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases the read-lock or write-lock
////////////////////////////////////////////////////////////////////////////////

void ReadWriteLock::unlock() {
  if (_writeLocked) {
    _writeLocked = false;
    unlockWrite();
  } else {
    unlockRead();
  }
}

void ReadWriteLock::unlockRead() {
  TRI_ReadUnlockReadWriteLock(&_rwlock);
}

void ReadWriteLock::unlockWrite() {
  TRI_WriteUnlockReadWriteLock(&_rwlock);
}
