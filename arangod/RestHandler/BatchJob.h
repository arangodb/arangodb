////////////////////////////////////////////////////////////////////////////////
/// @brief batch job
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_REST_HANDLER_BATCH_JOB_H
#define TRIAGENS_REST_HANDLER_BATCH_JOB_H 1

#include "GeneralServer/GeneralServerJob.h"
#include "HttpServer/HttpHandler.h"
#include "Basics/Mutex.h"
#include "RestHandler/BatchSubjob.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    template<typename S> class BatchSubjob;

// -----------------------------------------------------------------------------
// --SECTION--                                                    class BatchJob
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief general server job
////////////////////////////////////////////////////////////////////////////////

    template<typename S>
    class BatchJob : public GeneralServerJob<S, HttpHandler> {
      private:
        BatchJob (BatchJob const&);
        BatchJob& operator= (BatchJob const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief who accomplished the batch job?
////////////////////////////////////////////////////////////////////////////////

        enum Accomplisher {
          NOONE  = 0,
          TASK   = 1,
          DIRECT = 2,
          ASYNC  = 3
        };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new server job
////////////////////////////////////////////////////////////////////////////////

        BatchJob (S* server, HttpHandler* handler)
        : GeneralServerJob<S, HttpHandler>(server, handler),
          _doneAccomplisher(NOONE),
          _handlers(),
          _subjobs(),
          _doneLock(),
          _setupLock(),
          _jobsDone(0),
          _cleanup(false) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a server job
////////////////////////////////////////////////////////////////////////////////

        ~BatchJob () {
          MUTEX_LOCKER(_setupLock);
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       Job methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief directly execute job
////////////////////////////////////////////////////////////////////////////////

        void executeDirectHandler (HttpHandler* handler) {
          Handler::status_e status = Handler::HANDLER_FAILED;

          do {
            try {
              status = handler->execute();
            }
            catch (triagens::basics::TriagensError const& ex) {
              handler->handleError(ex);
            }
            catch (std::exception const& ex) {
              triagens::basics::InternalError err(ex);

              handler->handleError(err);
            }
            catch (...) {
              triagens::basics::InternalError err;
              handler->handleError(err);
            }
          }
          while (status == Handler::HANDLER_REQUEUE);

          if (status == Handler::HANDLER_DONE) {
            this->_handler->addResponse(handler);
          }

          MUTEX_LOCKER(this->_doneLock);
          ++this->_jobsDone;

          if (this->_jobsDone >= _handlers.size()) {
            _doneAccomplisher = DIRECT;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief sub-job is done
////////////////////////////////////////////////////////////////////////////////

        void jobDone (BatchSubjob<S>* subjob) {
          this->_handler->addResponse(subjob->getHandler());
          _doneLock.lock(); 

          ++_jobsDone;

          if (_jobsDone >= _handlers.size()) {
            // all sub-jobs are done
            if (_cleanup) {
              _doneLock.unlock();

              // cleanup might delete ourselves!
              GeneralServerJob<S, HttpHandler>::cleanup();
            }
            else {
              _doneAccomplisher = ASYNC;
              _subjobs.clear();
              _doneLock.unlock();

              cleanup();
            }
          }
          else {
            // still something to do
            _subjobs.erase(subjob);
            _doneLock.unlock();
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        Job::status_e work () {
          LOGGER_TRACE << "beginning job " << static_cast<Job*>(this);

          if (this->_shutdown != 0) {
            return Job::JOB_DONE;
          }

          // we must grab this lock so no one else can kill us while we're iterating 
          // over the sub handlers 
          MUTEX_LOCKER(this->_setupLock);

          // handler::execute() is called to prepare the batch handler
          // if it returns anything else but HANDLER_DONE, this is an
          // indication of an error
          if (this->_handler->execute() != Handler::HANDLER_DONE) {
            // handler failed
            this->_doneAccomplisher = DIRECT;

            return Job::JOB_FAILED;
          }

          // setup did not fail

          bool hasAsync = false;
          _handlers = this->_handler->subhandlers();

          for (vector<HttpHandler*>::const_iterator i = _handlers.begin();  i != _handlers.end();  ++i) {
            HttpHandler* handler = *i;

            if (handler->isDirect()) {
              executeDirectHandler(handler);
            }
            else {
              if (!hasAsync) {
                // we must do this ourselves. it is not safe to have the dispatcherThread
                // call this method because the job might be deleted before that
                this->_handler->setDispatcherThread(0);
                hasAsync = true;
              }
              createSubjob(handler);
            }
          }

          if (!hasAsync) {
            // only jobs executed directly, we're done and let the dispatcher kill us
            return Job::JOB_DONE;
          }

          MUTEX_LOCKER(this->_doneLock);
          if (this->_doneAccomplisher == DIRECT) {
            // all jobs already done. last job was finished by direct execution
            return Job::JOB_DONE;
          }

          // someone else must kill this job
          return Job::JOB_DETACH;
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void cleanup () {
          bool done = false;

          {
            MUTEX_LOCKER(_doneLock);

            if (_doneAccomplisher != NOONE) {
              done = true;
            }
            else {
              _cleanup = true;
            }
          }

          if (done) {
            GeneralServerJob<S, HttpHandler>::cleanup();
          }
        } 

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool beginShutdown () {
          LOGGER_TRACE << "shutdown job " << static_cast<Job*>(this);

          bool cleanup;
          {
            MUTEX_LOCKER(_doneLock);
            this->_shutdown = 1;

            {
              MUTEX_LOCKER(this->_abandonLock);

              for (typename set<BatchSubjob<S>*>::iterator i = _subjobs.begin();  i != _subjobs.end();  ++i) {
                (*i)->abandon();
              }
            }

            _doneAccomplisher = TASK;
            cleanup = _cleanup;
          }

          if (cleanup) {
            GeneralServerJob<S, HttpHandler>::cleanup();
          }

          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief create a subjob
////////////////////////////////////////////////////////////////////////////////

        void createSubjob (HttpHandler* handler) {
          BatchSubjob<S>* job = new BatchSubjob<S>(this, this->_server, handler);
          {
            MUTEX_LOCKER(_doneLock);
            _subjobs.insert(job);
          }
          this->_server->getDispatcher()->addJob(job);
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief who finalised the batch job?
////////////////////////////////////////////////////////////////////////////////

        Accomplisher _doneAccomplisher;

////////////////////////////////////////////////////////////////////////////////
/// @brief list of handlers
////////////////////////////////////////////////////////////////////////////////

        vector<HttpHandler*> _handlers;

////////////////////////////////////////////////////////////////////////////////
/// @brief list of sub-jobs
////////////////////////////////////////////////////////////////////////////////

        set<BatchSubjob<S>*> _subjobs;

////////////////////////////////////////////////////////////////////////////////
/// @brief done lock
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Mutex _doneLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief setup lock
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Mutex _setupLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of completed jobs
////////////////////////////////////////////////////////////////////////////////

        size_t _jobsDone;

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup seen
////////////////////////////////////////////////////////////////////////////////

        bool _cleanup;
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
