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

#ifndef ARANGODB_BASICS_MUTEX_H
#define ARANGODB_BASICS_MUTEX_H 1

#include "Basics/Common.h"

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#define ARANGO_ENABLE_DEADLOCK_DETECTION
#if defined(__SANITIZE_THREAD__)
// Avoid fals positives with ThreadSanitizer
#undef ARANGO_ENABLE_DEADLOCK_DETECTION
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#undef ARANGO_ENABLE_DEADLOCK_DETECTION
#endif
#endif
#endif

#ifdef ARANGO_ENABLE_DEADLOCK_DETECTION
#include "Basics/Thread.h"
#endif

namespace arangodb {

class Mutex {
 private:
  Mutex(Mutex const&) = delete;
  Mutex& operator=(Mutex const&) = delete;

 public:
  Mutex();
  ~Mutex();

 public:
  void lock();
  bool tryLock();
  void unlock();

  // assert that the mutex is locked by the current thread. will do
  // nothing in non-maintainer mode and will do nothing for non-posix locks
#ifdef ARANGO_ENABLE_DEADLOCK_DETECTION
  void assertLockedByCurrentThread();
  void assertNotLockedByCurrentThread();
#else
  inline void assertLockedByCurrentThread() {}
  inline void assertNotLockedByCurrentThread() {}
#endif

 private:
#ifdef TRI_HAVE_POSIX_THREADS
  pthread_mutex_t _mutex;
  pthread_mutexattr_t _attributes;
#endif

#ifdef TRI_HAVE_WIN32_THREADS
  // as of VS2013, exclusive SRWLocks tend to be faster than native mutexes
  SRWLOCK _mutex;
#endif

#ifdef ARANGO_ENABLE_DEADLOCK_DETECTION
  TRI_tid_t _holder;
#endif
};
}  // namespace arangodb

#endif
