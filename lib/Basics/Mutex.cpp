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
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "Mutex.h"

#include "Logger/Logger.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a mutex
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_POSIX_THREADS

Mutex::Mutex() : _mutex() { pthread_mutex_init(&_mutex, nullptr); }

#endif

#ifdef TRI_HAVE_WIN32_THREADS

Mutex::Mutex() : _mutex() { InitializeSRWLock(&_mutex); }

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes the mutex
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_POSIX_THREADS

Mutex::~Mutex() { pthread_mutex_destroy(&_mutex); }

#endif

#ifdef TRI_HAVE_WIN32_THREADS

Mutex::~Mutex() {}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires the lock
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_POSIX_THREADS

void Mutex::lock() {
  int rc = pthread_mutex_lock(&_mutex);

  if (rc != 0) {
    if (rc == EDEADLK) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "mutex deadlock detected";
    }

    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "could not lock the mutex object: " << strerror(rc);
    FATAL_ERROR_ABORT();
  }
}

bool Mutex::tryLock() {
  int rc = pthread_mutex_trylock(&_mutex);
  
  if (rc != 0) {
    if (rc == EBUSY) { // lock is already beeing held
      return false;
    } else if (rc == EDEADLK) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "mutex deadlock detected";
    }
    
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "could not lock the mutex object: " << strerror(rc);
    FATAL_ERROR_ABORT();
  }
  return true;
}

#endif

#ifdef TRI_HAVE_WIN32_THREADS

void Mutex::lock() { AcquireSRWLockExclusive(&_mutex); }

bool Mutex::tryLock() { return TryAcquireSRWLockExclusive(&_mutex) != 0; }

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief releases the lock
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_POSIX_THREADS

void Mutex::unlock() {
  int rc = pthread_mutex_unlock(&_mutex);

  if (rc != 0) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "could not release the mutex: " << strerror(rc);
    FATAL_ERROR_ABORT();
  }
}

#endif

#ifdef TRI_HAVE_WIN32_THREADS

void Mutex::unlock() { ReleaseSRWLockExclusive(&_mutex); }

#endif
