////////////////////////////////////////////////////////////////////////////////
/// @brief general server
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_GENERAL_SERVER_GENERAL_SERVER_H
#define TRIAGENS_GENERAL_SERVER_GENERAL_SERVER_H 1

#include "Basics/Common.h"

#include <sys/types.h>

#ifdef TRI_HAVE_LINUX_SOCKETS
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/file.h>
#endif


#ifdef TRI_HAVE_WINSOCK2_H
#include <winsock2.h>
#include <ws2tcpip.h>
#endif





#include "Basics/AssociativeArray.h"
#include "Basics/Exceptions.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "GeneralServer/EndpointServer.h"
#include "GeneralServer/GeneralListenTask.h"
#include "GeneralServer/GeneralServerJob.h"
#include "GeneralServer/SpecificCommTask.h"
#include "Logger/Logger.h"
#include "Rest/Endpoint.h"
#include "Rest/EndpointList.h"
#include "Rest/Handler.h"
#include "Scheduler/ListenTask.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SocketTask.h"

// #define TRI_USE_SPIN_LOCK_GENERAL_SERVER 1

#ifdef TRI_USE_SPIN_LOCK_GENERAL_SERVER
#define GENERAL_SERVER_INIT TRI_InitSpin
#define GENERAL_SERVER_DESTROY TRI_DestroySpin
#define GENERAL_SERVER_LOCK TRI_LockSpin
#define GENERAL_SERVER_UNLOCK TRI_UnlockSpin
#else
#define GENERAL_SERVER_INIT TRI_InitMutex
#define GENERAL_SERVER_DESTROY TRI_DestroyMutex
#define GENERAL_SERVER_LOCK TRI_LockMutex
#define GENERAL_SERVER_UNLOCK TRI_UnlockMutex
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                               class GeneralServer
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief general server
////////////////////////////////////////////////////////////////////////////////

    template<typename S, typename HF, typename CT>
    class GeneralServer : public EndpointServer, protected TaskManager {
      private:
        GeneralServer (GeneralServer const&);
        GeneralServer const& operator= (GeneralServer const&);

        typedef typename HF::GeneralHandler GeneralHandler;
        typedef GeneralServerJob<S, typename HF::GeneralHandler> ServerJob;

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
        GeneralServer (Scheduler* scheduler, double keepAliveTimeout)
          : EndpointServer(),
            _scheduler(scheduler),
            _listenTasks(),
            _commTasks(),
            _handlers(1024),
            _task2handler(1024),
            _keepAliveTimeout(keepAliveTimeout) {
          GENERAL_SERVER_INIT(&_commTasksLock);
          GENERAL_SERVER_INIT(&_mappingLock);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a general server
////////////////////////////////////////////////////////////////////////////////

        ~GeneralServer () {
          for (typename set<GeneralCommTask<S, HF>*>::iterator i = _commTasks.begin();  i != _commTasks.end();  ++i) {
            _scheduler->destroyTask(*i);
          }

          stopListening();

          GENERAL_SERVER_DESTROY(&_mappingLock);
          GENERAL_SERVER_DESTROY(&_commTasksLock);
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
/// @brief return the scheduler
////////////////////////////////////////////////////////////////////////////////

        Scheduler* getScheduler () const {
          return _scheduler;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief starts listining
////////////////////////////////////////////////////////////////////////////////

        void startListening () {
          EndpointList::ListType endpoints = _endpointList->getEndpoints(this->getProtocol(), this->getEncryption());

          for (EndpointList::ListType::const_iterator i = endpoints.begin(); i != endpoints.end(); ++i) {
            LOGGER_TRACE("trying to bind to endpoint '" << (*i)->getSpecification() << "' for requests");

            bool ok = openEndpoint(*i);

            if (ok) {
              LOGGER_DEBUG("bound to endpoint '" << (*i)->getSpecification() << "'");
            }
            else {
              LOGGER_FATAL_AND_EXIT("failed to bind to endpoint '" << (*i)->getSpecification() << "'");
            }
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down handlers
////////////////////////////////////////////////////////////////////////////////

        virtual void shutdownHandlers () {
          GENERAL_SERVER_LOCK(&_commTasksLock);

          for (typename set<GeneralCommTask<S, HF>*>::iterator i = _commTasks.begin();  i != _commTasks.end();  ++i) {
            (*i)->beginShutdown();
          }

          GENERAL_SERVER_UNLOCK(&_commTasksLock);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief stops listining
////////////////////////////////////////////////////////////////////////////////

        void stopListening () {
          for (vector<ListenTask*>::iterator i = _listenTasks.begin();  i != _listenTasks.end();  ++i) {
            _scheduler->destroyTask(*i);
          }

          _listenTasks.clear();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief removes all listen and comm tasks
////////////////////////////////////////////////////////////////////////////////

        virtual void stop () {
          GENERAL_SERVER_LOCK(&_commTasksLock);

          while (! _commTasks.empty()) {
            GeneralCommTask<S, HF>* task = *_commTasks.begin();
            _commTasks.erase(task);


            GENERAL_SERVER_UNLOCK(&_commTasksLock);
            _scheduler->destroyTask(task);
            GENERAL_SERVER_LOCK(&_commTasksLock);
          }

          GENERAL_SERVER_UNLOCK(&_commTasksLock);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief handles connection request
////////////////////////////////////////////////////////////////////////////////

        virtual void handleConnected (TRI_socket_t s, ConnectionInfo& info) {

          GeneralCommTask<S, HF>* task = new SpecificCommTask<S, HF, CT>(dynamic_cast<S*>(this), s, info, _keepAliveTimeout);

          GENERAL_SERVER_LOCK(&_commTasksLock);
          _commTasks.insert(task);
          GENERAL_SERVER_UNLOCK(&_commTasksLock);

          _scheduler->registerTask(task);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a connection close
////////////////////////////////////////////////////////////////////////////////

        void handleCommunicationClosed (Task* task) {
          shutdownHandlerByTask(task);

          GENERAL_SERVER_LOCK(&_commTasksLock);
          _commTasks.erase(dynamic_cast<GeneralCommTask<S, HF>*>(task));
          GENERAL_SERVER_UNLOCK(&_commTasksLock);

          _scheduler->destroyTask(task);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a connection failure
////////////////////////////////////////////////////////////////////////////////

        void handleCommunicationFailure (Task* task) {
          shutdownHandlerByTask(task);

          GENERAL_SERVER_LOCK(&_commTasksLock);
          _commTasks.erase(dynamic_cast<GeneralCommTask<S, HF>*>(task));
          GENERAL_SERVER_UNLOCK(&_commTasksLock);

          _scheduler->destroyTask(task);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a request
////////////////////////////////////////////////////////////////////////////////

        virtual bool handleRequest (CT * task, GeneralHandler* handler) {
          registerHandler(handler, task);

          // execute handle and requeue
          while (true) {
            Handler::status_e status = handleRequestDirectly(task, handler);

            if (status != Handler::HANDLER_REQUEUE) {
              shutdownHandlerByTask(task);
              return true;
            }
          }

          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   protected types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief Handler, Job, and Task tuple
////////////////////////////////////////////////////////////////////////////////

        struct handler_task_job_t {
          GeneralHandler* _handler;
          Task* _task;
          ServerJob* _job;
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief Handler lookup
////////////////////////////////////////////////////////////////////////////////

        struct handler_task_job_handler_desc {
          public:
            static void clearElement (handler_task_job_t& element) {
              element._handler = 0;
            }

            static bool isEmptyElement (handler_task_job_t const& element) {
              return element._handler == 0;
            }

            static bool isEqualKeyElement (GeneralHandler* handler, handler_task_job_t const& element) {
              return handler == element._handler;
            }

            static bool isEqualElementElement (handler_task_job_t const& left, handler_task_job_t const& right) {
              return left._handler == right._handler;
            }

            static uint32_t hashKey (GeneralHandler* key) {
              return (uint32_t) (uintptr_t) key;
            }

            static uint32_t hashElement (handler_task_job_t const& element) {
              return (uint32_t) (uintptr_t) element._handler;
            }
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief Task lookup
////////////////////////////////////////////////////////////////////////////////

        struct handler_task_job_task_desc {
          public:
            static void clearElement (handler_task_job_t& element) {
              element._task = 0;
            }

            static bool isEmptyElement (handler_task_job_t const& element) {
              return element._task == 0;
            }

            static bool isEqualKeyElement (Task* task, handler_task_job_t const& element) {
              return task == element._task;
            }

            static bool isEqualElementElement (handler_task_job_t const& left, handler_task_job_t const& right) {
              return left._task == right._task;
            }

            static uint32_t hashKey (Task* key) {
              return (uint32_t) (uintptr_t) key;
            }

            static uint32_t hashElement (handler_task_job_t const& element) {
              return (uint32_t) (uintptr_t) element._task;
            }
        };

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
/// @brief opens a listen port
////////////////////////////////////////////////////////////////////////////////

        bool openEndpoint (Endpoint* endpoint) {
          ListenTask* task = new GeneralListenTask<S> (dynamic_cast<S*> (this), endpoint, true);

          // ...................................................................
          // For some reason we have failed in our endeavour to bind to the socket -
          // this effectively terminates the server
          // ...................................................................

          if (! task->isBound()) {
            deleteTask(task);
            return false;
          }
          _scheduler->registerTask(task);
          _listenTasks.push_back(task);

          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief handle request directly
////////////////////////////////////////////////////////////////////////////////

        Handler::status_e handleRequestDirectly (CT* task, GeneralHandler * handler) {
          Handler::status_e status = Handler::HANDLER_FAILED;

          RequestStatisticsAgentSetRequestStart(handler);

          try {
            try {
              status = handler->execute();
            }
            catch (basics::TriagensError const& ex) {
              RequestStatisticsAgentSetExecuteError(handler);

              handler->handleError(ex);
            }
            catch (std::exception const& ex) {
              RequestStatisticsAgentSetExecuteError(handler);

              basics::InternalError err(ex, __FILE__, __LINE__);
              handler->handleError(err);
            }
            catch (...) {
              RequestStatisticsAgentSetExecuteError(handler);

              basics::InternalError err("handleRequestDirectly", __FILE__, __LINE__);
              handler->handleError(err);
            }

            if (status == Handler::HANDLER_REQUEUE) {
              handler->RequestStatisticsAgent::transfer(task);
              return status;
            }

            typename HF::GeneralResponse * response = handler->getResponse();

            if (response == 0) {
              basics::InternalError err("no response received from handler", __FILE__, __LINE__);

              handler->handleError(err);
              response = handler->getResponse();
            }

            RequestStatisticsAgentSetRequestEnd(handler);
            handler->RequestStatisticsAgent::transfer(task);

            if (response != 0) {
              task->handleResponse(response);
            }
            else {
              LOGGER_ERROR("cannot get any response");
            }
          }
          catch (basics::TriagensError const& ex) {
            RequestStatisticsAgentSetExecuteError(handler);

            LOGGER_ERROR("caught exception: " << DIAGNOSTIC_INFORMATION(ex));
          }
          catch (std::exception const& ex) {
            RequestStatisticsAgentSetExecuteError(handler);

            LOGGER_ERROR("caught exception: " << ex.what());
          }
          catch (...) {
            RequestStatisticsAgentSetExecuteError(handler);

            LOGGER_ERROR("caught exception");
          }

          return status;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a task
////////////////////////////////////////////////////////////////////////////////

        void registerHandler (GeneralHandler* handler, Task* task) {
          GENERAL_SERVER_LOCK(&_mappingLock);

          handler_task_job_t element;
          element._handler = handler;
          element._task = task;
          element._job = 0;

          _handlers.addElement(element);
          _task2handler.addElement(element);

          GENERAL_SERVER_UNLOCK(&_mappingLock);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs a handler for a task
////////////////////////////////////////////////////////////////////////////////

        virtual void shutdownHandlerByTask (Task* task) {
          GENERAL_SERVER_LOCK(&_mappingLock);

          // remove the job from the map
          handler_task_job_t element = _task2handler.removeKey(task);

          if (element._task != task) {
            LOGGER_DEBUG("shutdownHandler called, but no handler is known for task");

            GENERAL_SERVER_UNLOCK(&_mappingLock);
            return;
          }

          // remove and delete the handler
          _handlers.removeKey(element._handler);

          GENERAL_SERVER_UNLOCK(&_mappingLock);

          delete element._handler;
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
/// @brief the scheduler
////////////////////////////////////////////////////////////////////////////////

        Scheduler* _scheduler;

////////////////////////////////////////////////////////////////////////////////
/// @brief active listen tasks
////////////////////////////////////////////////////////////////////////////////

        std::vector< ListenTask* > _listenTasks;

////////////////////////////////////////////////////////////////////////////////
/// @brief mutex for comm tasks
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_USE_SPIN_LOCK_GENERAL_SERVER
        TRI_spin_t _commTasksLock;
#else
        TRI_mutex_t _commTasksLock;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief active comm tasks
////////////////////////////////////////////////////////////////////////////////

        std::set< GeneralCommTask<S, HF>* > _commTasks;

////////////////////////////////////////////////////////////////////////////////
/// @brief mutex for mapping structures
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_USE_SPIN_LOCK_GENERAL_SERVER
        TRI_spin_t _mappingLock;
#else
        TRI_mutex_t _mappingLock;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief map handler to task
////////////////////////////////////////////////////////////////////////////////

        basics::AssociativeArray<GeneralHandler*, handler_task_job_t, handler_task_job_handler_desc> _handlers;

////////////////////////////////////////////////////////////////////////////////
/// @brief map task to handler
////////////////////////////////////////////////////////////////////////////////

        basics::AssociativeArray<Task*, handler_task_job_t, handler_task_job_task_desc> _task2handler;

////////////////////////////////////////////////////////////////////////////////
/// @brief keep-alive timeout
////////////////////////////////////////////////////////////////////////////////

        double _keepAliveTimeout;
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
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
