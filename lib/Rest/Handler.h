////////////////////////////////////////////////////////////////////////////////
/// @brief abstract class for handlers
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

#ifndef ARANGODB_REST_HANDLER_H
#define ARANGODB_REST_HANDLER_H 1

#include "Basics/Common.h"

#include "Statistics/StatisticsAgent.h"

#include "Basics/Exceptions.h"
#include "Dispatcher/Job.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class AsyncJobServer;
    class Dispatcher;

// -----------------------------------------------------------------------------
// --SECTION--                                                     class Handler
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract class for handlers
////////////////////////////////////////////////////////////////////////////////

    class Handler : public RequestStatisticsAgent {
      private:
        Handler (Handler const&);
        Handler& operator= (Handler const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief status of execution
////////////////////////////////////////////////////////////////////////////////

        enum status_e {
          HANDLER_DONE,
          HANDLER_REQUEUE,
          HANDLER_FAILED
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief result of execution
////////////////////////////////////////////////////////////////////////////////

        class status_t {
          public:
            status_t ()
              : status(HANDLER_FAILED) {
            }

            explicit
            status_t (status_e status)
              : status(status) {
            }

            Job::status_t jobStatus () {
              switch (status) {
                case Handler::HANDLER_DONE:
                  return Job::status_t(Job::JOB_DONE);

                case Handler::HANDLER_REQUEUE: {
                  Job::status_t result(Job::JOB_REQUEUE);
                  result.sleep = sleep;
                  return result;
                }

                case Handler::HANDLER_FAILED:
                default:
                  return Job::status_t(Job::JOB_FAILED);
              }
            }

            status_e status;
            double sleep;
        };

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        Handler ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a handler
////////////////////////////////////////////////////////////////////////////////

        virtual ~Handler ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the job type
////////////////////////////////////////////////////////////////////////////////

        virtual Job::JobType type ();

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if a handler is executed directly
////////////////////////////////////////////////////////////////////////////////

        virtual bool isDirect () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the queue name
////////////////////////////////////////////////////////////////////////////////

        virtual std::string const& queue () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the thread which currently dealing with the job
////////////////////////////////////////////////////////////////////////////////

        virtual void setDispatcherThread (DispatcherThread*);

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares execution of a handler, has to be called before execute
////////////////////////////////////////////////////////////////////////////////

        virtual void prepareExecute ();

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a handler
////////////////////////////////////////////////////////////////////////////////

        virtual status_t execute () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief finalizes execution of a handler, has to be called after execute
////////////////////////////////////////////////////////////////////////////////

        virtual void finalizeExecute ();

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to cancel an execution
////////////////////////////////////////////////////////////////////////////////

        virtual bool cancel (bool running);

////////////////////////////////////////////////////////////////////////////////
/// @brief handles error
////////////////////////////////////////////////////////////////////////////////

        virtual void handleError (basics::TriagensError const&) = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a job
////////////////////////////////////////////////////////////////////////////////

        virtual Job* createJob (AsyncJobServer*, bool) = 0;

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief current dispatcher thread
////////////////////////////////////////////////////////////////////////////////

        DispatcherThread* _dispatcherThread;
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
