////////////////////////////////////////////////////////////////////////////////
/// @brief mutexes, locks and condition variables for MacOS
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "locks.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                              SPIN
// -----------------------------------------------------------------------------

#ifdef TRI_HAVE_MACOS_SPIN

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a new spin-lock
////////////////////////////////////////////////////////////////////////////////

#ifndef TRI_FAKE_SPIN_LOCKS
void TRI_InitSpin (TRI_spin_t* spinLock) {
  *spinLock = 0;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a spin-lock
////////////////////////////////////////////////////////////////////////////////

#ifndef TRI_FAKE_SPIN_LOCKS
void TRI_DestroySpin (TRI_spin_t* spinLock) {
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief locks spin-lock
////////////////////////////////////////////////////////////////////////////////

#ifndef TRI_FAKE_SPIN_LOCKS
void TRI_LockSpin (TRI_spin_t* spinLock) {
  OSSpinLockLock(spinLock);
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks spin-lock
////////////////////////////////////////////////////////////////////////////////

#ifndef TRI_FAKE_SPIN_LOCKS
void TRI_UnlockSpin (TRI_spin_t* spinLock) {
  OSSpinLockUnlock(spinLock);
}
#endif

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
