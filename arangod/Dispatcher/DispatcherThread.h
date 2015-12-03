////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher thread
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
/// @author Martin Schoenert
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_DISPATCHER_DISPATCHER_THREAD_H
#define ARANGODB_DISPATCHER_DISPATCHER_THREAD_H 1

#include "Basics/Thread.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class DispatcherQueue;
    class Job;
    class Scheduler;

// -----------------------------------------------------------------------------
// --SECTION--                                                  class Dispatcher
// -----------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
/// @brief job dispatcher thread
/////////////////////////////////////////////////////////////////////////////

    class DispatcherThread : public basics::Thread {
      DispatcherThread (DispatcherThread const&) = delete;
      DispatcherThread& operator= (DispatcherThread const&) = delete;

      friend class Dispatcher;
      friend class DispatcherQueue;

// -----------------------------------------------------------------------------
// --SECTION--                                            thread local variables
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief a global, but thread-local place to hold the current dispatcher
/// thread. If we are not in a dispatcher thread this is set to nullptr.
////////////////////////////////////////////////////////////////////////////////

        static thread_local DispatcherThread* currentDispatcherThread;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a dispatcher thread
////////////////////////////////////////////////////////////////////////////////

        explicit DispatcherThread (DispatcherQueue*);

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void run ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates that thread is doing a blocking operation
////////////////////////////////////////////////////////////////////////////////

        void block ();

////////////////////////////////////////////////////////////////////////////////
/// @brief indicates that thread has resumed work
////////////////////////////////////////////////////////////////////////////////

        void unblock ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief do the real work
////////////////////////////////////////////////////////////////////////////////

        void handleJob (Job* job);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the dispatcher
////////////////////////////////////////////////////////////////////////////////

        DispatcherQueue* _queue;
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
