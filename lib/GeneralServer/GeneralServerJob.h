////////////////////////////////////////////////////////////////////////////////
/// @brief general server job
///
/// @file
///
/// DISCLAIMER
///
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_GENERAL_SERVER_GENERAL_SERVER_JOB_H
#define TRIAGENS_GENERAL_SERVER_GENERAL_SERVER_JOB_H 1

#include "Dispatcher/Job.h"

#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "BasicsC/logging.h"
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
            _id(0),
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
/// @brief assign an id to the job. note: the id might be 0
////////////////////////////////////////////////////////////////////////////////

        void assignId (uint64_t id) {
          _id = id;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief assign an id to the job
////////////////////////////////////////////////////////////////////////////////

        uint64_t id () const {
          return _id;
        }

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

        string const& queue () {
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

        status_e work () {
          LOG_TRACE("beginning job %p", (void*) this);

          this->RequestStatisticsAgent::transfer(_handler);

          if (_shutdown != 0) {
            return Job::JOB_DONE;
          }

          RequestStatisticsAgentSetRequestStart(_handler);
          Handler::status_e status = _handler->execute();
          RequestStatisticsAgentSetRequestEnd(_handler);

          LOG_TRACE("finished job %p with status %d", (void*) this, (int) status);

          switch (status) {
            case Handler::HANDLER_DONE:    return Job::JOB_DONE;
            case Handler::HANDLER_REQUEUE: return Job::JOB_REQUEUE;
            case Handler::HANDLER_FAILED:  return Job::JOB_FAILED;
          }

          return Job::JOB_FAILED;
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
/// @brief job id (only used for detached jobs)
////////////////////////////////////////////////////////////////////////////////

        uint64_t _id;

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
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
