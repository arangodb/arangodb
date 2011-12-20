////////////////////////////////////////////////////////////////////////////////
/// @brief Mutex Locker
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2008-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_JUTLAND_BASICS_MUTEX_LOCKER_H
#define TRIAGENS_JUTLAND_BASICS_MUTEX_LOCKER_H 1

#include <BasicsC/Common.h>

#include <Basics/Mutex.h>

////////////////////////////////////////////////////////////////////////////////
/// @brief construct locker with file and line information
////////////////////////////////////////////////////////////////////////////////

#define MUTEX_LOCKER_VAR_A(a) _mutex_lock_variable_ ## a
#define MUTEX_LOCKER_VAR_B(a) MUTEX_LOCKER_VAR_A(a)

#define MUTEX_LOCKER(b) \
  triagens::basics::MutexLocker MUTEX_LOCKER_VAR_B(__LINE__)(&b, __FILE__, __LINE__)

namespace triagens {
  namespace basics {

    ////////////////////////////////////////////////////////////////////////////////
    /// @ingroup Threads
    /// @brief mutex locker
    ///
    /// A MutexLocker locks a mutex during its lifetime und unlocks the mutex
    /// when it is destroyed.
    ////////////////////////////////////////////////////////////////////////////////

    class MutexLocker {
      private:
        MutexLocker (MutexLocker const&);
        MutexLocker& operator= (MutexLocker const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief aquires a lock
        ///
        /// The constructors aquires a lock, the destructors releases the lock.
        ////////////////////////////////////////////////////////////////////////////////

        explicit
        MutexLocker (Mutex* mutex);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief aquires a lock
        ///
        /// The constructors aquires a lock, the destructors releases the lock.
        ////////////////////////////////////////////////////////////////////////////////

        MutexLocker (Mutex* mutex, char const* file, int line);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief releases the lock
        ////////////////////////////////////////////////////////////////////////////////

        ~MutexLocker ();

      private:
        Mutex* _mutex;
        char const* _file;
        int _line;
    };
  }
}

#endif
