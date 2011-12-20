////////////////////////////////////////////////////////////////////////////////
/// @brief read locker
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
/// @author Frank Celler
/// @author Achim Brandt
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_JUTLAND_BASICS_READ_LOCKER_H
#define TRIAGENS_JUTLAND_BASICS_READ_LOCKER_H 1

#include <BasicsC/Common.h>

#include <Basics/ReadWriteLock.h>

////////////////////////////////////////////////////////////////////////////////
/// @brief construct locker with file and line information
////////////////////////////////////////////////////////////////////////////////

#define READ_LOCKER_VAR_A(a) _read_lock_variable ## a
#define READ_LOCKER_VAR_B(a) READ_LOCKER_VAR_A(a)

#define READ_LOCKER(b) \
  triagens::basics::ReadLocker READ_LOCKER_VAR_B(__LINE__)(&b, __FILE__, __LINE__)

namespace triagens {
  namespace basics {

    ////////////////////////////////////////////////////////////////////////////////
    /// @ingroup Threads
    /// @brief read locker
    ///
    /// A ReadLocker read-locks a read-write lock during its lifetime and unlocks
    /// the lock when it is destroyed.
    ////////////////////////////////////////////////////////////////////////////////

    class ReadLocker {
      private:
        ReadLocker (ReadLocker const&);
        ReadLocker& operator= (ReadLocker const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief aquires a read-lock
        ///
        /// The constructors read-locks the lock, the destructors unlocks the lock.
        ////////////////////////////////////////////////////////////////////////////////

        explicit
        ReadLocker (ReadWriteLock* readWriteLock);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief aquires a read-lock
        ///
        /// The constructors read-locks the lock, the destructors unlocks the lock.
        ////////////////////////////////////////////////////////////////////////////////

        ReadLocker (ReadWriteLock* readWriteLock, char const* file, int line);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief releases the read-lock
        ////////////////////////////////////////////////////////////////////////////////

        ~ReadLocker ();

      private:
        ReadWriteLock* _readWriteLock;
        char const* _file;
        int _line;
    };
  }
}

#endif
