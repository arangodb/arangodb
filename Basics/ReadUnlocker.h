////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocker
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

#ifndef TRIAGENS_JUTLAND_BASICS_READ_UNLOCKER_H
#define TRIAGENS_JUTLAND_BASICS_READ_UNLOCKER_H 1

#include <Basics/Common.h>

#include <Basics/ReadWriteLock.h>

////////////////////////////////////////////////////////////////////////////////
/// @brief construct unlocker with file and line information
////////////////////////////////////////////////////////////////////////////////

#define READ_UNLOCKER_VAR_A(a) _read_unlock_variable ## a
#define READ_UNLOCKER_VAR_B(a) READ_UNLOCKER_VAR_A(a)

#define READ_UNLOCKER(b) \
  triagens::basics::ReadUnlocker READ_UNLOCKER_VAR_B(__LINE__)(&b, __FILE__, __LINE__)

namespace triagens {
  namespace basics {

    ////////////////////////////////////////////////////////////////////////////////
    /// @ingroup Threads
    /// @brief read unlocker
    ///
    /// A ReadUnlocker unlocks a read-write lock during its lifetime and reaquires
    /// the read-lock again when it is destroyed.
    ////////////////////////////////////////////////////////////////////////////////

    class ReadUnlocker {
      private:
        ReadUnlocker (ReadUnlocker const&);
        ReadUnlocker& operator= (ReadUnlocker const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief unlocks the lock
        ///
        /// The constructors unlocks the lock, the destructors aquires a read-lock.
        ///
        /// @param
        ///    readWriteLock [in]       read-write lock
        ////////////////////////////////////////////////////////////////////////////////

        explicit
        ReadUnlocker (ReadWriteLock* readWriteLock);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief unlocks the lock
        ///
        /// The constructors unlocks the lock, the destructors aquires a read-lock.
        ///
        /// @param
        ///    readWriteLock [in]       read-write lock
        ///    file [in]                file
        ///    line [in]                line number
        ////////////////////////////////////////////////////////////////////////////////

        ReadUnlocker (ReadWriteLock* readWriteLock, char const* file, int line);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief aquires the read-lock
        ////////////////////////////////////////////////////////////////////////////////

        ~ReadUnlocker ();

      private:
        ReadWriteLock* _readWriteLock;
        char const* _file;
        int _line;
    };
  }
}

#endif
