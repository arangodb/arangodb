////////////////////////////////////////////////////////////////////////////////
/// @brief general server with dispatcher
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

#ifndef ARANGODB_GENERAL_SERVER_GENERAL_SERVER_DISPATCHER_H
#define ARANGODB_GENERAL_SERVER_GENERAL_SERVER_DISPATCHER_H 1

#include "Basics/Common.h"

#include "GeneralServer/GeneralServer.h"

#include "Dispatcher/Dispatcher.h"
#include "Dispatcher/Job.h"
#include "GeneralServer/GeneralCommTask.h"
#include "GeneralServer/GeneralAsyncCommTask.h"
#include "HttpServer/AsyncJobManager.h"
#include "Rest/AsyncJobServer.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class AsyncJobManager;
    class Dispatcher;

// -----------------------------------------------------------------------------
// --SECTION--                                     class GeneralServerDispatcher
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief general server with dispatcher
////////////////////////////////////////////////////////////////////////////////

    template<typename S, typename HF, typename CT>
    class GeneralServerDispatcher : virtual public GeneralServer<S, HF, CT>, public AsyncJobServer {
      private:
        GeneralServerDispatcher (GeneralServerDispatcher const&);
        GeneralServerDispatcher& operator= (GeneralServerDispatcher const&);

        typedef typename HF::GeneralHandler GeneralHandler;
        typedef typename GeneralServer<S, HF, CT>::handler_task_job_t handler_task_job_t;
        typedef GeneralServerJob<S, typename HF::GeneralHandler> ServerJob;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new general server
////////////////////////////////////////////////////////////////////////////////

        explicit
        GeneralServerDispatcher (Scheduler* scheduler, double keepAliveTimeout)
          : GeneralServer<S, HF, CT>(scheduler, keepAliveTimeout),
            _dispatcher(0),
            _jobManager(0) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new general server
////////////////////////////////////////////////////////////////////////////////

        GeneralServerDispatcher (Scheduler* scheduler,
                                 Dispatcher* dispatcher,
                                 AsyncJobManager* jobManager,
                                 double keepAliveTimeout)
          : GeneralServer<S, HF, CT>(scheduler, keepAliveTimeout),
            _dispatcher(dispatcher),
            _jobManager(jobManager) {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the dispatcher
////////////////////////////////////////////////////////////////////////////////

        Dispatcher* getDispatcher () const {
          return _dispatcher;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down handlers
////////////////////////////////////////////////////////////////////////////////

        void shutdownHandlers () {
          GENERAL_SERVER_LOCK(&this->_mappingLock);

          size_t size;
          handler_task_job_t const* table = this->_handlers.tableAndSize(size);

          for (size_t i = 0;  i < size;  ++i) {
            if (table[i]._handler != nullptr) {
              ServerJob* job = table[i]._job;

              if (job != nullptr) {
                job->abandon();
                const_cast<handler_task_job_t*>(table)[i]._job = nullptr;
              }
            }
          }

          GENERAL_SERVER_UNLOCK(&this->_mappingLock);

          GeneralServer<S, HF, CT>::shutdownHandlers();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief callback if the handler received a signal
////////////////////////////////////////////////////////////////////////////////

        void handleAsync (Task* task) {
          GENERAL_SERVER_LOCK(&this->_mappingLock);

          handler_task_job_t element = this->_task2handler.removeKey(task);

          if (element._task != task) {
            LOG_WARNING("cannot find a task for the handler, giving up");

            GENERAL_SERVER_UNLOCK(&this->_mappingLock);
            return;
          }

          GeneralHandler* handler = element._handler;

          this->_handlers.removeKey(handler);

          GENERAL_SERVER_UNLOCK(&this->_mappingLock);

          typename HF::GeneralResponse * response = handler->getResponse();

          if (response == nullptr) {
            basics::InternalError err("no response received from handler", __FILE__, __LINE__);

            handler->handleError(err);
            response = handler->getResponse();
          }

          if (response != nullptr) {
            GeneralAsyncCommTask<S, HF, CT>* atask
              = dynamic_cast<GeneralAsyncCommTask<S, HF, CT>*>(task);

            if (atask != nullptr) {
              handler->RequestStatisticsAgent::transfer(atask);

              atask->handleResponse(response);
            }
            else {
              LOG_ERROR("expected a GeneralAsyncCommTask, giving up");
            }
          }
          else {
            LOG_ERROR("cannot get any response");
          }

          delete handler;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                            AsyncJobServer methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void jobDone (Job* ajob) {
          ServerJob* job = dynamic_cast<ServerJob*>(ajob);

          if (job == nullptr) {
            LOG_WARNING("jobDone called, but Job is no ServerJob");
            return;
          }

          // locate the handler
          GeneralHandler* handler = job->getHandler();

          if (job->isDetached()) {
            if (handler != nullptr) {
              _jobManager->finishAsyncJob<S, HF>(job);
              delete handler;
            }

            return;
          }

          GENERAL_SERVER_LOCK(&this->_mappingLock);
          handler_task_job_t const& element = this->_handlers.findKey(handler);

          if (element._handler != handler) {
            LOG_WARNING("jobDone called, but handler is unknown");

            GENERAL_SERVER_UNLOCK(&this->_mappingLock);
            return;
          }

          // remove the job from the mapping
          const_cast<handler_task_job_t&>(element)._job = nullptr;

          // if there is no task, assume the client has died
          if (element._task == nullptr) {
            LOG_DEBUG("jobDone called, but no task is known, assume client has died");
            this->_handlers.removeKey(handler);

            delete handler;

            GENERAL_SERVER_UNLOCK(&this->_mappingLock);
            return;
          }

          // signal the task, to continue its work
          GeneralAsyncCommTask<S, HF, CT>* task = dynamic_cast<GeneralAsyncCommTask<S, HF, CT>*>(element._task);

          GENERAL_SERVER_UNLOCK(&this->_mappingLock);

          if (task == nullptr) {
            LOG_WARNING("task for handler is no GeneralAsyncCommTask, giving up");
            return;
          }

          task->signal();
        }

// -----------------------------------------------------------------------------
// --SECTION--                                             GeneralServer methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void handleConnected (TRI_socket_t s, ConnectionInfo& info) {
          GeneralAsyncCommTask<S, HF, CT>* task = new GeneralAsyncCommTask<S, HF, CT>(dynamic_cast<S*>(this), s, info, this->_keepAliveTimeout);

          GENERAL_SERVER_LOCK(&this->_commTasksLock);
          this->_commTasks.insert(dynamic_cast<GeneralCommTask<S, HF>*>(task));
          GENERAL_SERVER_UNLOCK(&this->_commTasksLock);

          this->_scheduler->registerTask(task);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a job for asynchronous execution (using the dispatcher)
////////////////////////////////////////////////////////////////////////////////

        bool handleRequestAsync (GeneralHandler* handler,
                                 uint64_t* jobId) {
          if (_dispatcher == nullptr) {
            // without a dispatcher, simply give up
            RequestStatisticsAgentSetExecuteError(handler);

            delete handler;

            LOG_WARNING("no dispatcher is known");
            return false;
          }

          // execute the handler using the dispatcher
          Job* ajob = handler->createJob(this, true);

          if (ajob == nullptr) {
            RequestStatisticsAgentSetExecuteError(handler);

            delete handler;

            LOG_WARNING("task is indirect, but handler failed to create a job - this cannot work!");
            return false;
          }

          ServerJob* job = dynamic_cast<ServerJob*>(ajob);
          TRI_ASSERT(job != nullptr);

          if (jobId != 0) {
            try {
              _jobManager->initAsyncJob<S, HF>(job, jobId);
            }
            catch (...) {
              LOG_WARNING("unable to initialize job");
              
              delete handler;
              return false;
            }
          }

          if (_dispatcher->addJob(job) != TRI_ERROR_NO_ERROR) {
            // could not add job to job queue
            delete handler;

            return false;
          }

          // job is in queue now
          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the handler directly, or add it to the dispatcher queue
////////////////////////////////////////////////////////////////////////////////

        bool handleRequest (CT * task, GeneralHandler* handler) {
          this->registerHandler(handler, task);

          // execute handler and (possibly) requeue
          while (true) {

            // directly execute the handler within the scheduler thread
            if (handler->isDirect()) {
              Handler::status_t status = this->handleRequestDirectly(task, handler);

              if (status.status != Handler::HANDLER_REQUEUE) {
                this->shutdownHandlerByTask(task);
                return true;
              }
            }

            // execute the handler using the dispatcher
            else if (_dispatcher != nullptr) {
              GeneralAsyncCommTask<S, HF, CT>* atask = dynamic_cast<GeneralAsyncCommTask<S, HF, CT>*>(task);

              if (atask == nullptr) {
                RequestStatisticsAgentSetExecuteError(handler);

                LOG_WARNING("task is indirect, but not asynchronous - this cannot work!");

                this->shutdownHandlerByTask(task);
                return false;
              }
              else {
                Job* ajob = handler->createJob(this, false);

                if (ajob == nullptr) {
                  RequestStatisticsAgentSetExecuteError(handler);

                  LOG_WARNING("task is indirect, but handler failed to create a job - this cannot work!");

                  this->shutdownHandlerByTask(task);
                  return false;
                }

                ServerJob* job = dynamic_cast<ServerJob*>(ajob);
                TRI_ASSERT(job != nullptr);

                registerJob(handler, job);
                return true;
              }
            }

            // without a dispatcher, simply give up
            else {
              RequestStatisticsAgentSetExecuteError(handler);

              LOG_WARNING("no dispatcher is known");

              this->shutdownHandlerByTask(task);
              return false;
            }
          }

          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down a handler for a task
////////////////////////////////////////////////////////////////////////////////

        void shutdownHandlerByTask (Task* task) {
          GENERAL_SERVER_LOCK(&this->_mappingLock);

          // remove the task from the map
          handler_task_job_t element = this->_task2handler.removeKey(task);

          if (element._task != task) {
            LOG_DEBUG("shutdownHandler called, but no handler is known for task");

            GENERAL_SERVER_UNLOCK(&this->_mappingLock);
            return;
          }

          GeneralHandler* handler = element._handler;

          // check if the handler contains a job or not
          handler_task_job_t const& element2 = this->_handlers.findKey(handler);

          if (element2._handler != handler) {
            LOG_DEBUG("shutdownHandler called, but handler of task is unknown");

            GENERAL_SERVER_UNLOCK(&this->_mappingLock);
            return;
          }

          // if we do not know a job, delete handler
          Job* job = element2._job;

          if (job == nullptr) {
            this->_handlers.removeKey(handler);

            GENERAL_SERVER_UNLOCK(&this->_mappingLock);

            delete handler;
            return;
          }

          // initiate shutdown if a job is known
          else {
            const_cast<handler_task_job_t&>(element2)._task = nullptr;
            job->beginShutdown();

            GENERAL_SERVER_UNLOCK(&this->_mappingLock);
            return;
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a new job
////////////////////////////////////////////////////////////////////////////////

        void registerJob (GeneralHandler* handler, ServerJob* job) {
          GENERAL_SERVER_LOCK(&this->_mappingLock);

          // update the handler information
          handler_task_job_t const& element = this->_handlers.findKey(handler);

          if (element._handler != handler) {
            LOG_DEBUG("registerJob called for an unknown handler");

            GENERAL_SERVER_UNLOCK(&this->_mappingLock);
            return;
          }

          const_cast<handler_task_job_t&>(element)._job = job;

          GENERAL_SERVER_UNLOCK(&this->_mappingLock);

          handler->RequestStatisticsAgent::transfer(job);

          _dispatcher->addJob(job);
        }

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the dispatcher
////////////////////////////////////////////////////////////////////////////////

        Dispatcher* _dispatcher;

////////////////////////////////////////////////////////////////////////////////
/// @brief the job manager
////////////////////////////////////////////////////////////////////////////////

        AsyncJobManager* _jobManager;
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
