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

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
// initialize _holder to "maximum" thread id. this will work if the type of
// _holder is numeric, but will not work if its type is more complex.
Mutex::Mutex()
  : _holder((std::numeric_limits<decltype(_holder)>::max)()) {}
#else
Mutex::Mutex() {}
#endif


Mutex::~Mutex() {}

void Mutex::lock() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // we must not hold the lock ourselves here
  TRI_ASSERT(_holder != Thread::currentThreadId());
#endif

  _mutex.lock();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _holder = Thread::currentThreadId();
#endif
}

bool Mutex::tryLock() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // we must not hold the lock ourselves here
  TRI_ASSERT(_holder != Thread::currentThreadId());
#endif

  if (!_mutex.try_lock()) {
    return false;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _holder = Thread::currentThreadId();
#endif

  return true;
}

void Mutex::unlock() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(_holder == Thread::currentThreadId());
  _holder = 0;
#endif

  _mutex.unlock();
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void Mutex::assertLockedByCurrentThread() {
  TRI_ASSERT(_holder == Thread::currentThreadId());
}

void Mutex::assertNotLockedByCurrentThread() {
  TRI_ASSERT(_holder != Thread::currentThreadId());
}
#endif

