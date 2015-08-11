////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base class for jobs
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_DISPATCHER_JOB_H
#define ARANGODB_DISPATCHER_JOB_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Statistics/StatisticsAgent.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class DispatcherQueue;
    class DispatcherThread;

// -----------------------------------------------------------------------------
// --SECTION--                                                         class Job
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base class for jobs
////////////////////////////////////////////////////////////////////////////////

    class Job : public RequestStatisticsAgent {
      private:
        Job (Job const&);
        Job& operator= (Job const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief status of execution
////////////////////////////////////////////////////////////////////////////////

        enum status_e {
          JOB_DONE,
          JOB_REQUEUE,
          JOB_FAILED
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief result of execution
////////////////////////////////////////////////////////////////////////////////

        class status_t {
          public:
            status_t ()
              : status(JOB_FAILED) {
            }

            explicit
            status_t (status_e status)
              : status(status) {
            }

            status_e status;
            double sleep;
        };

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a job
////////////////////////////////////////////////////////////////////////////////

        explicit
        Job (std::string const& name);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        virtual ~Job ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the name
/// Note: currently unused
////////////////////////////////////////////////////////////////////////////////

        virtual const std::string& getName () const {
          return _name;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the jobs id.
///
/// Note: the id might be 0
////////////////////////////////////////////////////////////////////////////////

        void setId (uint64_t id) {
          _id = id;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the job id
////////////////////////////////////////////////////////////////////////////////

        uint64_t id () const {
          return _id;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the queue position
////////////////////////////////////////////////////////////////////////////////

        void setQueuePosition (size_t id) {
          _queuePosition = id;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the queue position
////////////////////////////////////////////////////////////////////////////////

        size_t queuePosition () const {
          return _queuePosition;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                            virtual public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the queue name to use
////////////////////////////////////////////////////////////////////////////////

        virtual size_t queue () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the thread which currently dealing with the job
////////////////////////////////////////////////////////////////////////////////

        virtual void setDispatcherThread (DispatcherThread*);

////////////////////////////////////////////////////////////////////////////////
/// @brief starts working
////////////////////////////////////////////////////////////////////////////////

        virtual status_t work () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to cancel execution
////////////////////////////////////////////////////////////////////////////////

        virtual bool cancel () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief cleans up after work and delete
////////////////////////////////////////////////////////////////////////////////

        virtual void cleanup (DispatcherQueue*) = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief handle error and delete
////////////////////////////////////////////////////////////////////////////////

        virtual void handleError (basics::Exception const&) = 0;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the job
////////////////////////////////////////////////////////////////////////////////

        std::string const _name;

////////////////////////////////////////////////////////////////////////////////
/// @brief job id (only used for detached jobs)
////////////////////////////////////////////////////////////////////////////////

        uint64_t _id;

////////////////////////////////////////////////////////////////////////////////
/// @brief queue position
////////////////////////////////////////////////////////////////////////////////

        size_t _queuePosition;
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|/// @startDocuBlock\\|// --SECTION--\\|/// @\\}"
// End:
