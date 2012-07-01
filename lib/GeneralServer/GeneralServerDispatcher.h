////////////////////////////////////////////////////////////////////////////////
/// @brief general server with dispatcher
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
/// @author Achim Brandt
/// @author Copyright 2009-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_GENERAL_SERVER_GENERAL_SERVER_DISPATCHER_H
#define TRIAGENS_GENERAL_SERVER_GENERAL_SERVER_DISPATCHER_H 1

#include "GeneralServer/GeneralServer.h"

#include "Dispatcher/Dispatcher.h"
#include "Dispatcher/Job.h"
#include "GeneralServer/GeneralCommTask.h"
#include "GeneralServer/GeneralAsyncCommTask.h"
#include "Rest/AsyncJobServer.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class Dispatcher;

// -----------------------------------------------------------------------------
// --SECTION--                                     class GeneralServerDispatcher
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief general server with dispatcher
////////////////////////////////////////////////////////////////////////////////

    template<typename S, typename HF, typename CT>
    class GeneralServerDispatcher : public GeneralServer<S, HF, CT>, public AsyncJobServer {
      private:
        GeneralServerDispatcher (GeneralServerDispatcher const&);
        GeneralServerDispatcher& operator= (GeneralServerDispatcher const&);

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
/// @brief constructs a new general server
////////////////////////////////////////////////////////////////////////////////

        explicit
        GeneralServerDispatcher (Scheduler* scheduler)
          : GeneralServer<S, HF, CT>(scheduler),
            _dispatcher(0),
            _handler2job(),
            _job2handler() {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new general server
////////////////////////////////////////////////////////////////////////////////

        GeneralServerDispatcher (Scheduler* scheduler, Dispatcher* dispatcher)
          : GeneralServer<S, HF, CT>(scheduler), _dispatcher(dispatcher) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down handlers
////////////////////////////////////////////////////////////////////////////////

        void shutdownHandlers () {
          GeneralServer<S, HF, CT>::shutdownHandlers();          

          GENERAL_SERVER_LOCK(&this->_mappingLock);

          for (typename std::map< Job*, typename HF::GeneralHandler* >::iterator i = _job2handler.begin();
               i != _job2handler.end();
               ++i) {
            Job* job = i->first;
            typename HF::GeneralHandler* handler = i->second;

            handler->abandonJob(_dispatcher, job);
          }

          GENERAL_SERVER_UNLOCK(&this->_mappingLock);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief callback if the handler received a signal
////////////////////////////////////////////////////////////////////////////////

        void handleAsync (Task* task) {
          typename HF::GeneralHandler* handler = 0;

          GENERAL_SERVER_LOCK(&this->_mappingLock);
          
          typename std::map< Task*, typename HF::GeneralHandler* >::iterator i = this->_task2handler.find(task);

          if (i == this->_task2handler.end()) {
            LOGGER_WARNING << "cannot find a task for the handler, giving up";

            GENERAL_SERVER_UNLOCK(&this->_mappingLock);
            return;
          }

          handler = i->second;

          this->_task2handler.erase(i);
          this->_handler2task.erase(handler);

          GENERAL_SERVER_UNLOCK(&this->_mappingLock);

          typename HF::GeneralResponse * response = handler->getResponse();
          
          if (response == 0) {
            basics::InternalError err("no response received from handler");

            handler->handleError(err);
            response = handler->getResponse();
          }

          if (response != 0) {
            GeneralAsyncCommTask<S, HF, CT>* atask
              = dynamic_cast<GeneralAsyncCommTask<S, HF, CT>*>(task);

            if (atask != 0) {
              atask->handleResponse(response);
            }
            else {
              LOGGER_ERROR << "expected a GeneralAsyncCommTask, giving up";
            }
          }
          else {
            LOGGER_ERROR << "cannot get any response";
          }

          delete handler;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                            AsyncJobServer methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void jobDone (Job* job) {
          GeneralAsyncCommTask<S, HF, CT>* task = 0;

          GENERAL_SERVER_LOCK(&this->_mappingLock);

          // extract the handler
          typename std::map< Job*, typename HF::GeneralHandler* >::iterator i
            = _job2handler.find(job);

          if (i == _job2handler.end()) {
            LOGGER_WARNING << "jobDone called, but no handler is known";

            GENERAL_SERVER_UNLOCK(&this->_mappingLock);
            return;
          }

          typename HF::GeneralHandler* handler = i->second;

          // clear job map
          _job2handler.erase(i);
          _handler2job.erase(handler);

          // look up the task
          typename std::map< typename HF::GeneralHandler*, Task* >::iterator j
            = this->_handler2task.find(handler);

          if (j == this->_handler2task.end()) {
            LOGGER_DEBUG << "jobDone called, but no task is known, assume client has died";

            delete handler;

            GENERAL_SERVER_UNLOCK(&this->_mappingLock);
            return;
          }

          task = dynamic_cast<GeneralAsyncCommTask<S, HF, CT>*>(j->second);

          GENERAL_SERVER_UNLOCK(&this->_mappingLock);

          if (task == 0) {
            LOGGER_WARNING << "task for handler is not an async task, giving up";
            return;
          }

          task->signal();
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             GeneralServer methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void handleConnected (socket_t socket, ConnectionInfo& info) {
          GeneralAsyncCommTask<S, HF, CT>* task = new GeneralAsyncCommTask<S, HF, CT>(dynamic_cast<S*>(this), socket, info);

          this->_scheduler->registerTask(task);
          this->_commTasks.insert(task);
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool handleRequest (CT * task, typename HF::GeneralHandler* handler) {
          registerHandler(handler, task);

          // execute handler and (possibly) requeue
          while (true) {

            // directly execute the handler within the scheduler thread
            if (handler->isDirect()) {
              Handler::status_e status = this->handleRequestDirectly(task, handler);

              if (status != Handler::HANDLER_REQUEUE) {
                this->shutdownHandlerByTask(task);
                return true;
              }
            }

            // execute the handler using the dispatcher
            else if (_dispatcher != 0) {
              GeneralAsyncCommTask<S, HF, CT>* atask = dynamic_cast<GeneralAsyncCommTask<S, HF, CT>*>(task);

              if (atask == 0) {
                LOGGER_WARNING << "task is indirect, but not asynchronous - this cannot work!";

                this->shutdownHandlerByTask(task);
                return false;
              }
              else {
                Job* job = handler->createJob(_dispatcher, this);

                if (job == 0) {
                  LOGGER_WARNING << "task is indirect, but handler failed to create a job - this cannot work!";

                  this->shutdownHandlerByTask(task);
                  return false;
                }

                registerJob(handler, job);
                return true;
              }
            }

            // without a dispatcher, simply give up
            else {
              LOGGER_WARNING << "no dispatcher is known";

              this->shutdownHandlerByTask(task);
              return false;
            }
          }

          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs a handler for a task
////////////////////////////////////////////////////////////////////////////////

        void shutdownHandlerByTask (Task* task) { 
          GENERAL_SERVER_LOCK(&this->_mappingLock);

          typename std::map< Task*, typename HF::GeneralHandler* >::iterator i
            = this->_task2handler.find(task);

          if (i == this->_task2handler.end()) {
            LOGGER_DEBUG << "shutdownHandler called, but no handler is known for task";
          }
          else {
            typename HF::GeneralHandler* handler = i->second;

            this->_task2handler.erase(i);
            this->_handler2task.erase(handler);

            typename std::map< typename HF::GeneralHandler*, Job* >::iterator j = _handler2job.find(handler);

            if (j == _handler2job.end()) {
              delete handler;
            }
            else {
              j->second->beginShutdown();
            }
          }

          GENERAL_SERVER_UNLOCK(&this->_mappingLock);
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
/// @brief registers a new job
////////////////////////////////////////////////////////////////////////////////

        void registerJob (typename HF::GeneralHandler* handler, Job* job) {
          GENERAL_SERVER_LOCK(&this->_mappingLock);

          _job2handler[job] = handler;
          _handler2job[handler] = job;

          GENERAL_SERVER_UNLOCK(&this->_mappingLock);

          _dispatcher->addJob(job);
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
/// @brief the dispatcher
////////////////////////////////////////////////////////////////////////////////

        Dispatcher* _dispatcher;

////////////////////////////////////////////////////////////////////////////////
/// @brief map handler to job
////////////////////////////////////////////////////////////////////////////////

        std::map< typename HF::GeneralHandler*, Job* > _handler2job;

////////////////////////////////////////////////////////////////////////////////
/// @brief map task to handler
////////////////////////////////////////////////////////////////////////////////

        std::map< Job*, typename HF::GeneralHandler* > _job2handler;
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
