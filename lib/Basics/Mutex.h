////////////////////////////////////////////////////////////////////////////////
/// @brief Mutex
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
/// @author Achim Brandt
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_MUTEX_H
#define ARANGODB_BASICS_MUTEX_H 1

#include "Basics/Common.h"

#include "Basics/locks.h"

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                                       class Mutex
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief mutex
////////////////////////////////////////////////////////////////////////////////

    class Mutex {
        Mutex (Mutex const&);
        Mutex& operator= (Mutex const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a mutex
////////////////////////////////////////////////////////////////////////////////

        Mutex ();

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes the mutex
////////////////////////////////////////////////////////////////////////////////

        ~Mutex ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief acquires the lock
////////////////////////////////////////////////////////////////////////////////

        void lock ();

////////////////////////////////////////////////////////////////////////////////
/// @brief releases the lock
////////////////////////////////////////////////////////////////////////////////

        void unlock ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief mutex variable
////////////////////////////////////////////////////////////////////////////////

        TRI_mutex_t _mutex;
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
