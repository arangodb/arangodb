////////////////////////////////////////////////////////////////////////////////
/// @brief Read Locker
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

#include "ReadLocker.h"

using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires a read-lock
///
/// The constructors read-lock the lock, the destructors unlock the lock.
////////////////////////////////////////////////////////////////////////////////

ReadLocker::ReadLocker (ReadWriteLock* readWriteLock)
  : _readWriteLock(readWriteLock), _file(0), _line(0) {
  _readWriteLock->readLock();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires a read-lock
///
/// The constructors read-lock the lock, the destructors unlock the lock.
////////////////////////////////////////////////////////////////////////////////

ReadLocker::ReadLocker (ReadWriteLock* readWriteLock, char const* file, int line)
  : _readWriteLock(readWriteLock), _file(file), _line(line) {
  _readWriteLock->readLock();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases the read-lock
////////////////////////////////////////////////////////////////////////////////

ReadLocker::~ReadLocker () {
  _readWriteLock->unlock();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
