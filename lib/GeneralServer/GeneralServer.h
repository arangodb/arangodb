////////////////////////////////////////////////////////////////////////////////
/// @brief general server
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

#ifndef TRIAGENS_GENERAL_SERVER_GENERAL_SERVER_H
#define TRIAGENS_GENERAL_SERVER_GENERAL_SERVER_H 1

#include "Basics/Common.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "Basics/Exceptions.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "GeneralServer/GeneralListenTask.h"
#include "GeneralServer/SpecificCommTask.h"
#include "Logger/Logger.h"
#include "Rest/Handler.h"
#include "Scheduler/ListenTask.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SocketTask.h"

#define TRI_USE_SPIN_LOCK_GENERAL_SERVER 1

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
    class GeneralServer : protected TaskManager {
      private:
        GeneralServer (GeneralServer const&);
        GeneralServer const& operator= (GeneralServer const&);

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
        GeneralServer (Scheduler* scheduler)
          : _scheduler(scheduler),
            _ports(),
            _listenTasks(),
            _commTasks(),
            _handler2task(),
            _task2handler() {
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

        Scheduler* getScheduler () {
          return _scheduler;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief adds port for general connections
////////////////////////////////////////////////////////////////////////////////

        void addPort (int port, bool reuseAddress)  {
          return addPort("", port, reuseAddress);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief adds address and port for general connections
////////////////////////////////////////////////////////////////////////////////

        void addPort (string const& address, int port, bool reuseAddress) {
          port_info_t pi;
          pi._address = address;
          pi._port = port;
          pi._reuseAddress = reuseAddress;

          _ports.push_back(pi);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief starts listining
////////////////////////////////////////////////////////////////////////////////

        void startListening () {
          deque<port_info_t> addresses;
          addresses.insert(addresses.begin(), _ports.begin(), _ports.end());

          while (! addresses.empty()) {
            port_info_t ap = addresses[0];
            addresses.pop_front();

            if (ap._address.empty()) {
              LOGGER_TRACE << "trying to open port " << ap._port << " for requests";
            }
            else {
              LOGGER_TRACE << "trying to open address " << ap._address << " on port " << ap._port << " for requests";
            }

            bool ok = openListenPort(ap);

            if (ok) {
              LOGGER_DEBUG << "opened port " << ap._port << " for " << (ap._address.empty() ? "any" : ap._address);
            }
            else {
              LOGGER_TRACE << "failed to open port " << ap._port << " for " << (ap._address.empty() ? "any" : ap._address);
              addresses.push_back(ap);

              if (_scheduler->isShutdownInProgress()) {
                break;
              }
              else {
                sleep(1);
              }
            }
          }
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
/// @brief handles connection request
////////////////////////////////////////////////////////////////////////////////

        virtual void handleConnected (socket_t socket, ConnectionInfo& info) {
          GeneralCommTask<S, HF>* task = new SpecificCommTask<S, HF, CT>(dynamic_cast<S*>(this), socket, info);

          _scheduler->registerTask(task);

          GENERAL_SERVER_LOCK(&_commTasksLock);
          _commTasks.insert(task);
          GENERAL_SERVER_UNLOCK(&_commTasksLock);
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

        virtual bool handleRequest (CT * task, typename HF::GeneralHandler* handler) {
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
/// @brief port and address
////////////////////////////////////////////////////////////////////////////////

        struct port_info_t {
          string _address;
          int _port;
          bool _reuseAddress;
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

        bool openListenPort (port_info_t ap) {
          struct addrinfo *result, *aip;
          struct addrinfo hints;
          int error;

          memset(&hints, 0, sizeof (struct addrinfo));
          hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
          hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
          hints.ai_socktype = SOCK_STREAM;
          hints.ai_protocol = 0;

          string portString = basics::StringUtils::itoa(ap._port);

          if (ap._address.empty()) {
            error = getaddrinfo(NULL, portString.c_str(), &hints, &result);
          }
          else {
            error = getaddrinfo(ap._address.c_str(), portString.c_str(), &hints, &result);
          }

          if (error != 0) {
            LOGGER_ERROR << "getaddrinfo for host: " << ap._address.c_str() << " => " << gai_strerror(error);
            return false;
          }

          bool gotTask = false;

          // Try all returned addresses
          for (aip = result; aip != NULL; aip = aip->ai_next) {
            ListenTask* task = new GeneralListenTask<S> (dynamic_cast<S*> (this), aip, ap._reuseAddress);

            if (! task->isBound()) {
              deleteTask(task);
            }
            else {
              _scheduler->registerTask(task);
              _listenTasks.push_back(task);
              gotTask = true;
            }

          }

          freeaddrinfo(result);

          return gotTask;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief handle request directly
////////////////////////////////////////////////////////////////////////////////

        Handler::status_e handleRequestDirectly (CT* task, typename HF::GeneralHandler * handler) {
          Handler::status_e status = Handler::HANDLER_FAILED;

          try {
            try {
              status = handler->execute();
            }
            catch (basics::TriagensError const& ex) {
              handler->handleError(ex);
            }
            catch (std::exception const& ex) {
              basics::InternalError err(ex);

              handler->handleError(err);
            }
            catch (...) {
              basics::InternalError err;
              handler->handleError(err);
            }

            if (status == Handler::HANDLER_REQUEUE) {
              return status;
            }

            typename HF::GeneralResponse * response = handler->getResponse();

            if (response == 0) {
              basics::InternalError err("no response received from handler");

              handler->handleError(err);
              response = handler->getResponse();
            }

            if (response != 0) {
              task->handleResponse(response);
            }
            else {
              LOGGER_ERROR << "cannot get any response in " << __FILE__ << "@" << __LINE__;
            }
          }
          catch (basics::TriagensError const& ex) {
            LOGGER_ERROR << "caught exception in " << __FILE__ << "@" << __LINE__ << ": " << DIAGNOSTIC_INFORMATION(ex);
          }
          catch (std::exception const& ex) {
            LOGGER_ERROR << "caught exception in " << __FILE__ << "@" << __LINE__ << ": " << ex.what();
          }
          catch (...) {
            LOGGER_ERROR << "caught exception in " << __FILE__ << "@" << __LINE__;
          }

          return status;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a task
////////////////////////////////////////////////////////////////////////////////

        void registerHandler (typename HF::GeneralHandler* handler, Task* task) {
          GENERAL_SERVER_LOCK(&_mappingLock);

          _task2handler[task] = handler;
          _handler2task[handler] = task;

          GENERAL_SERVER_UNLOCK(&_mappingLock);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs a handler for a task
////////////////////////////////////////////////////////////////////////////////

        virtual void shutdownHandlerByTask (Task* task) {
          GENERAL_SERVER_LOCK(&_mappingLock);

          typename std::map< Task*, typename HF::GeneralHandler* >::iterator i = _task2handler.find(task);

          if (i == _task2handler.end()) {
            LOGGER_DEBUG << "shutdownHandler called, but no handler is known for task";
          }
          else {
            typename HF::GeneralHandler* handler = i->second;

            _task2handler.erase(i);
            _handler2task.erase(handler);

            delete handler;
          }

          GENERAL_SERVER_UNLOCK(&_mappingLock);
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
/// @brief defined ports and addresses
////////////////////////////////////////////////////////////////////////////////

        std::vector< port_info_t > _ports;

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

        std::map< typename HF::GeneralHandler*, Task* > _handler2task;

////////////////////////////////////////////////////////////////////////////////
/// @brief map task to handler
////////////////////////////////////////////////////////////////////////////////

        std::map< Task*, typename HF::GeneralHandler* > _task2handler;
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
