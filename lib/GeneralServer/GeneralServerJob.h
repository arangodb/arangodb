////////////////////////////////////////////////////////////////////////////////
/// @brief general server job
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
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_GENERAL_SERVER_GENERAL_SERVER_JOB_H
#define ARANGODB_GENERAL_SERVER_GENERAL_SERVER_JOB_H 1

#include "Basics/Common.h"

#include "Dispatcher/Job.h"

#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/logging.h"
#include "Rest/Handler.h"
#include "Scheduler/AsyncTask.h"

// -----------------------------------------------------------------------------
// --SECTION--                                            class GeneralServerJob
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief general server job
////////////////////////////////////////////////////////////////////////////////

    template<typename S, typename H>
    class GeneralServerJob : public Job {
      private:
        GeneralServerJob (GeneralServerJob const&);
        GeneralServerJob& operator= (GeneralServerJob const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new server job
////////////////////////////////////////////////////////////////////////////////

        GeneralServerJob (S* server,
                          H* handler,
                          bool isDetached)
          : Job("HttpServerJob"),
            _server(server),
            _handler(handler),
            _shutdown(0),
            _abandon(false),
            _isDetached(isDetached) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a server job
////////////////////////////////////////////////////////////////////////////////

        ~GeneralServerJob () {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief abandon job
////////////////////////////////////////////////////////////////////////////////

        void abandon () {
          MUTEX_LOCKER(_abandonLock);
          _abandon = true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the underlying handler
////////////////////////////////////////////////////////////////////////////////

        H* getHandler () const {
          return _handler;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the job is detached
////////////////////////////////////////////////////////////////////////////////

        bool isDetached () const {
          return _isDetached;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                       Job methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        JobType type () {
          return _handler->type();
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        std::string const& queue () {
          return _handler->queue();
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void setDispatcherThread (DispatcherThread* thread) {
          _handler->setDispatcherThread(thread);
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        status_t work () {
          LOG_TRACE("beginning job %p", (void*) this);

          this->RequestStatisticsAgent::transfer(_handler);

          if (_shutdown != 0) {
            return status_t(Job::JOB_DONE);
          }

          RequestStatisticsAgentSetRequestStart(_handler);
          _handler->prepareExecute();
          Handler::status_t status;
          try {
            status = _handler->execute();
          }
          catch (...) {
            _handler->finalizeExecute();
            throw;
          }
          _handler->finalizeExecute();
          RequestStatisticsAgentSetRequestEnd(_handler);

          LOG_TRACE("finished job %p with status %d", (void*) this, (int) status.status);

          return status.jobStatus();
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool cancel (bool running) {
          return _handler->cancel(running);
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void cleanup () {
          bool abandon;

          {
            MUTEX_LOCKER(_abandonLock);
            abandon = _abandon;
          }

          if (! abandon && _server != 0) {
            _server->jobDone(this);
          }

          delete this;
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool beginShutdown () {
          LOG_TRACE("shutdown job %p", (void*) this);

          _shutdown = 1;
          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void handleError (basics::TriagensError const& ex) {
          _handler->handleError(ex);
        }

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief general server
////////////////////////////////////////////////////////////////////////////////

        S* _server;

////////////////////////////////////////////////////////////////////////////////
/// @brief handler
////////////////////////////////////////////////////////////////////////////////

        H* _handler;

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown in progress
////////////////////////////////////////////////////////////////////////////////

        volatile sig_atomic_t _shutdown;

////////////////////////////////////////////////////////////////////////////////
/// @brief server is dead lock
////////////////////////////////////////////////////////////////////////////////

        basics::Mutex _abandonLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief server is dead
////////////////////////////////////////////////////////////////////////////////

        bool _abandon;

////////////////////////////////////////////////////////////////////////////////
/// @brief job is detached (executed without a comm-task)
////////////////////////////////////////////////////////////////////////////////

        bool _isDetached;
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
