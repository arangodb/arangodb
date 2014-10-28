////////////////////////////////////////////////////////////////////////////////
/// @brief application server with dispatcher
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_DISPATCHER_APPLICATION_DISPATCHER_H
#define ARANGODB_DISPATCHER_APPLICATION_DISPATCHER_H 1

#include "Basics/Common.h"

#include "ApplicationServer/ApplicationFeature.h"

#include "Scheduler/ApplicationScheduler.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class ApplicationScheduler;
    class Dispatcher;
    class Task;

// -----------------------------------------------------------------------------
// --SECTION--                                       class ApplicationDispatcher
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief application server with dispatcher
////////////////////////////////////////////////////////////////////////////////

    class ApplicationDispatcher : virtual public ApplicationFeature {
      private:
        ApplicationDispatcher (ApplicationDispatcher const&);
        ApplicationDispatcher& operator= (ApplicationDispatcher const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ApplicationDispatcher ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~ApplicationDispatcher ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the scheduler
////////////////////////////////////////////////////////////////////////////////

        void setApplicationScheduler (ApplicationScheduler*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the dispatcher
////////////////////////////////////////////////////////////////////////////////

        Dispatcher* dispatcher () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the dispatcher queue
////////////////////////////////////////////////////////////////////////////////

        void buildStandardQueue (size_t nrThreads,
                                 size_t maxSize);

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void setupOptions (std::map<std::string, basics::ProgramOptionsDescription>&);

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool prepare ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool start ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool open ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void close ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void stop ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the dispatcher
////////////////////////////////////////////////////////////////////////////////

        void buildDispatcher (Scheduler*);

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the dispatcher reporter
////////////////////////////////////////////////////////////////////////////////

        void buildDispatcherReporter ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief application dispatcher
////////////////////////////////////////////////////////////////////////////////

        ApplicationScheduler* _applicationScheduler;

////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher
////////////////////////////////////////////////////////////////////////////////

        Dispatcher* _dispatcher;

////////////////////////////////////////////////////////////////////////////////
/// @brief reporting task
////////////////////////////////////////////////////////////////////////////////

        Task* _dispatcherReporterTask;

////////////////////////////////////////////////////////////////////////////////
/// @brief interval for reports
////////////////////////////////////////////////////////////////////////////////

        double _reportInterval;
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
