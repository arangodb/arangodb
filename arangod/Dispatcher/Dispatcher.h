////////////////////////////////////////////////////////////////////////////////
/// @brief interface of a job dispatcher
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

#ifndef ARANGODB_DISPATCHER_DISPATCHER_H
#define ARANGODB_DISPATCHER_DISPATCHER_H 1

#include "Basics/Common.h"

#include "Basics/Mutex.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class DispatcherQueue;
    class DispatcherThread;
    class Job;
    class Scheduler;

// -----------------------------------------------------------------------------
// --SECTION--                                                  class Dispatcher
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief interface of a job dispatcher
////////////////////////////////////////////////////////////////////////////////

    class Dispatcher {
      Dispatcher (Dispatcher const&) = delete;
      Dispatcher& operator= (Dispatcher const&) = delete;

// -----------------------------------------------------------------------------
// --SECTION--                                           static public variables
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief standard queue
////////////////////////////////////////////////////////////////////////////////

        static const size_t STANDARD_QUEUE = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief aql queue
////////////////////////////////////////////////////////////////////////////////

        static const size_t AQL_QUEUE = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of queues
////////////////////////////////////////////////////////////////////////////////

        static const size_t SYSTEM_QUEUE_SIZE = 2;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief queue thread creator
////////////////////////////////////////////////////////////////////////////////

        typedef DispatcherThread* (*newDispatcherThread_fptr)(DispatcherQueue*);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        explicit Dispatcher (Scheduler*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        virtual ~Dispatcher ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new queue
////////////////////////////////////////////////////////////////////////////////

        void addStandardQueue (size_t nrThreads, size_t maxSize);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new AQL queue
////////////////////////////////////////////////////////////////////////////////

        void addAQLQueue (size_t nrThreads, size_t maxSize);

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a new named queue
////////////////////////////////////////////////////////////////////////////////

	int addExtraQueue (size_t identifier,
			   size_t nrThreads,
			   size_t maxSize);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new job
///
/// The method is called from the scheduler to add a new job request.  It
/// returns immediately (i.e. without waiting for the job to finish).  When the
/// job is finished the scheduler will be awoken and the scheduler will write
/// the response over the network to the caller.
////////////////////////////////////////////////////////////////////////////////

        int addJob (Job*);

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to cancel a job
////////////////////////////////////////////////////////////////////////////////

        bool cancelJob (uint64_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief begins shutdown process
////////////////////////////////////////////////////////////////////////////////

        void beginShutdown ();

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs the queue
////////////////////////////////////////////////////////////////////////////////

        void shutdown ();

////////////////////////////////////////////////////////////////////////////////
/// @brief reports status of dispatcher queues
////////////////////////////////////////////////////////////////////////////////

        void reportStatus ();

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the process affinity
////////////////////////////////////////////////////////////////////////////////

        void setProcessorAffinity (size_t id, const std::vector<size_t>& cores);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief scheduler
////////////////////////////////////////////////////////////////////////////////

        Scheduler* _scheduler;

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown indicator
////////////////////////////////////////////////////////////////////////////////

        std::atomic<bool> _stopping;

////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher queues
////////////////////////////////////////////////////////////////////////////////

	std::vector<DispatcherQueue*> _queues;
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
