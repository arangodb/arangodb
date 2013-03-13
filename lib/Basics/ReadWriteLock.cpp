////////////////////////////////////////////////////////////////////////////////
/// @brief Read-Write Lock
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Frank Celler
/// @author Achim Brandt
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ReadWriteLock.h"

using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a read-write lock
////////////////////////////////////////////////////////////////////////////////

ReadWriteLock::ReadWriteLock ()
  : _rwlock(),
    _writeLocked(false) {
  TRI_InitReadWriteLock(&_rwlock);

#ifdef TRI_READ_WRITE_LOCK_COUNTER
  TRI_InitMutex(&_mutex);

  _readLockedCounter = 0;
  _writeLockedCounter = 0;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes read-write lock
////////////////////////////////////////////////////////////////////////////////

ReadWriteLock::~ReadWriteLock () {
  TRI_DestroyReadWriteLock(&_rwlock);

#ifdef TRI_READ_WRITE_LOCK_COUNTER
  TRI_DestroyMutex(&_mutex);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief check for read locked
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_READ_WRITE_LOCK_COUNTER

bool ReadWriteLock::isReadLocked () const {
  return _readLockedCounter > 0;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief locks for reading
////////////////////////////////////////////////////////////////////////////////

void ReadWriteLock::readLock () {
  TRI_ReadLockReadWriteLock(&_rwlock);

#ifdef TRI_READ_WRITE_LOCK_COUNTER

  TRI_LockMutex(&_mutex);
  _readLockedCounter++;
  TRI_UnlockMutex(&_mutex);

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check for write locked
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_READ_WRITE_LOCK_COUNTER

bool ReadWriteLock::isWriteLocked () const {
  return _writeLockedCounter > 0;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief locks for writing
////////////////////////////////////////////////////////////////////////////////

void ReadWriteLock::writeLock () {
  TRI_WriteLockReadWriteLock(&_rwlock);

  _writeLocked = true;

#ifdef TRI_READ_WRITE_LOCK_COUNTER

  TRI_LockMutex(&_mutex);
  _writeLockedCounter++;
  TRI_UnlockMutex(&_mutex);

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases the read-lock or write-lock
////////////////////////////////////////////////////////////////////////////////

void ReadWriteLock::unlock () {
#ifdef TRI_READ_WRITE_LOCK_COUNTER

  TRI_LockMutex(&_mutex);

  if (_writeLocked) {
    _writeLockedCounter--;
  }
  else {
    _readLockedCounter--;
  }

  TRI_UnlockMutex(&_mutex);

#endif

  if (_writeLocked) {
    _writeLocked = false;
    TRI_WriteUnlockReadWriteLock(&_rwlock);
  }
  else {
    TRI_ReadUnlockReadWriteLock(&_rwlock);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
