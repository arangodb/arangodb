////////////////////////////////////////////////////////////////////////////////
/// @brief Mutex
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

#ifndef TRIAGENS_BASICS_MUTEX_POSIX_H
#define TRIAGENS_BASICS_MUTEX_POSIX_H 1

#include <Basics/Common.h>

namespace triagens {
  namespace basics {

    ////////////////////////////////////////////////////////////////////////////////
    /// @ingroup Threads
    /// @brief mutex
    ///
    /// Mutual exclusion (often abbreviated to mutex) algorithms are used in
    /// concurrent programming to avoid the simultaneous use of a common resource,
    /// such as a global variable, by pieces of computer code called critical
    /// sections. A critical section is a piece of code in which a process or thread
    /// accesses a common resource. The critical section by itself is not a
    /// mechanism or algorithm for mutual exclusion. A program, process, or thread
    /// can have the critical section in it without any mechanism or algorithm which
    /// implements mutual exclusion. For details see www.wikipedia.org.
    ////////////////////////////////////////////////////////////////////////////////

    class Mutex {
      private:
        Mutex (Mutex const&);
        Mutex& operator= (Mutex const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a mutex
        ////////////////////////////////////////////////////////////////////////////////

        Mutex ();

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief deletes the mutex
        ////////////////////////////////////////////////////////////////////////////////

        ~Mutex ();

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief acquires the lock
        ////////////////////////////////////////////////////////////////////////////////

        bool lock () WARN_UNUSED;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief releases the lock
        ////////////////////////////////////////////////////////////////////////////////

        bool unlock () WARN_UNUSED;

      private:
        pthread_mutex_t _mutex;
    };
  }
}

#endif
