////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher thread
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
/// @author Martin Schoenert
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_FYN_DISPATCHER_DISPATCHER_THREAD_H
#define TRIAGENS_FYN_DISPATCHER_DISPATCHER_THREAD_H 1

#include <Basics/Common.h>

#include <Basics/Thread.h>

#include "Dispatcher/Job.h"

namespace triagens {
  namespace rest {
    class DispatcherQueue;

    /////////////////////////////////////////////////////////////////////////////
    /// @ingroup Dispatcher
    /// @brief job dispatcher thread
    /////////////////////////////////////////////////////////////////////////////

    class DispatcherThread : public basics::Thread {
      friend class DispatcherImpl;
      friend class DispatcherQueue;

      DispatcherThread (DispatcherThread const&);
      DispatcherThread& operator= (DispatcherThread const&);

      public:

        /////////////////////////////////////////////////////////////////////////
        /// @brief constructs a dispatcher thread
        /////////////////////////////////////////////////////////////////////////

        explicit
        DispatcherThread (DispatcherQueue*);

      protected:

        /////////////////////////////////////////////////////////////////////////
        /// @brief report status
        /////////////////////////////////////////////////////////////////////////

        virtual void reportStatus ();

        /////////////////////////////////////////////////////////////////////////
        /// @brief called after job finished
        /////////////////////////////////////////////////////////////////////////

        virtual void tick (bool idle);

      protected:

        /////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        /////////////////////////////////////////////////////////////////////////

        void run ();

      private:

        /////////////////////////////////////////////////////////////////////////
        /// @brief the dispatcher (shared variables)
        /////////////////////////////////////////////////////////////////////////

        DispatcherQueue * queue;

        /////////////////////////////////////////////////////////////////////////
        /// @brief current job type
        /////////////////////////////////////////////////////////////////////////

        Job::JobType jobType;
    };
  }
}

#endif
