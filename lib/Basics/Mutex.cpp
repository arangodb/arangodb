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

#include <limits>

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION                                              TRI_HAVE_POSIX_THREADS
// -----------------------------------------------------------------------------

#if defined(TRI_HAVE_POSIX_THREADS)

#ifdef ARANGO_ENABLE_DEADLOCK_DETECTION
// initialize _holder to "maximum" thread id. this will work if the type of
// _holder is numeric, but will not work if its type is more complex.
Mutex::Mutex()
    : _mutex(), _holder((std::numeric_limits<decltype(_holder)>::max)()) {
#else
Mutex::Mutex() : _mutex() {
#endif
  pthread_mutexattr_init(&_attributes);

#ifdef __linux__
#ifdef ARANGO_ENABLE_DEADLOCK_DETECTION
  // use an error checking mutex if available (only for LinuxThread) and only
  // in maintainer mode
  pthread_mutexattr_settype(&_attributes, PTHREAD_MUTEX_ERRORCHECK_NP);
#endif
#endif
  
  pthread_mutex_init(&_mutex, &_attributes);
}

Mutex::~Mutex() {
  pthread_mutex_destroy(&_mutex);
  pthread_mutexattr_destroy(&_attributes);
}

void Mutex::lock() {
#ifdef ARANGO_ENABLE_DEADLOCK_DETECTION
  // we must not hold the lock ourselves here
  TRI_ASSERT(_holder != Thread::currentThreadId());
#endif

  int rc = pthread_mutex_lock(&_mutex);

  if (rc != 0) {
    if (rc == EDEADLK) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "mutex deadlock detected";
    }

    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "could not lock the mutex object: " << strerror(rc);
    FATAL_ERROR_ABORT();
  }

#ifdef ARANGO_ENABLE_DEADLOCK_DETECTION
  _holder = Thread::currentThreadId();
#endif
}

bool Mutex::tryLock() {
#ifdef ARANGO_ENABLE_DEADLOCK_DETECTION
  // we must not hold the lock ourselves here
  TRI_ASSERT(_holder != Thread::currentThreadId());
#endif

  int rc = pthread_mutex_trylock(&_mutex);

  if (rc != 0) {
    if (rc == EBUSY) {  // lock is already being held
      return false;
    } else if (rc == EDEADLK) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "mutex deadlock detected";
    }

    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "could not lock the mutex object: " << strerror(rc);
    FATAL_ERROR_ABORT();
  }

#ifdef ARANGO_ENABLE_DEADLOCK_DETECTION
  _holder = Thread::currentThreadId();
#endif

  return true;
}

void Mutex::unlock() {
#ifdef ARANGO_ENABLE_DEADLOCK_DETECTION
  TRI_ASSERT(_holder == Thread::currentThreadId());
  _holder = 0;
#endif
  int rc = pthread_mutex_unlock(&_mutex);

  if (rc != 0) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "could not release the mutex: " << strerror(rc);
    FATAL_ERROR_ABORT();
  }
}

#ifdef ARANGO_ENABLE_DEADLOCK_DETECTION
void Mutex::assertLockedByCurrentThread() {
  TRI_ASSERT(_holder == Thread::currentThreadId());
}

void Mutex::assertNotLockedByCurrentThread() {
  TRI_ASSERT(_holder != Thread::currentThreadId());
}
#endif

// -----------------------------------------------------------------------------
// --SECTION                                              TRI_HAVE_WIN32_THREADS
// -----------------------------------------------------------------------------

#elif defined(TRI_HAVE_WIN32_THREADS)

Mutex::Mutex() : _mutex() { InitializeSRWLock(&_mutex); }
Mutex::~Mutex() {}

void Mutex::lock() { AcquireSRWLockExclusive(&_mutex); }

bool Mutex::tryLock() { return TryAcquireSRWLockExclusive(&_mutex) != 0; }

void Mutex::unlock() { ReleaseSRWLockExclusive(&_mutex); }

#ifdef ARANGO_ENABLE_DEADLOCK_DETECTION
void Mutex::assertLockedByCurrentThread() {}
void Mutex::assertNotLockedByCurrentThread() {}
#endif

#endif
