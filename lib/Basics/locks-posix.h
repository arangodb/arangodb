////////////////////////////////////////////////////////////////////////////////
/// @brief mutexes, locks and condition variables in posix
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

#ifndef ARANGODB_BASICS_C_LOCKS__POSIX_H
#define ARANGODB_BASICS_C_LOCKS__POSIX_H 1

#include "Basics/Common.h"

#ifdef TRI_HAVE_POSIX_THREADS

#include <pthread.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief mutex type
////////////////////////////////////////////////////////////////////////////////

#define TRI_mutex_t pthread_mutex_t

////////////////////////////////////////////////////////////////////////////////
/// @brief spin-lock type
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_FAKE_SPIN_LOCKS

#define TRI_spin_t pthread_mutex_t

#else

#ifdef TRI_HAVE_POSIX_SPIN
#define TRI_spin_t pthread_spinlock_t
#endif

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief read-write-lock type
////////////////////////////////////////////////////////////////////////////////

#define TRI_read_write_lock_t pthread_rwlock_t

////////////////////////////////////////////////////////////////////////////////
/// @brief condition variable
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_condition_s {
  pthread_cond_t _cond;
  pthread_mutex_t* _mutex;
}
TRI_condition_t;

#endif

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
