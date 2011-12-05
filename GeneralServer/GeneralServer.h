////////////////////////////////////////////////////////////////////////////////
/// @brief general server
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_FYN_GENERAL_SERVER_GENERAL_SERVER_H
#define TRIAGENS_FYN_GENERAL_SERVER_GENERAL_SERVER_H 1

#include <Basics/Common.h>

#include <Basics/Exceptions.h>
#include <Basics/Logger.h>
#include <Basics/ReadLocker.h>
#include <Basics/ReadWriteLock.h>
#include <Basics/WriteLocker.h>
#include <Rest/Handler.h>
#include <Rest/ListenTask.h>
#include <Rest/Scheduler.h>
#include <Rest/SocketTask.h>

#include "GeneralServer/GeneralListenTask.h"
#include "GeneralServer/SpecificCommTask.h"

namespace triagens {
  namespace rest {
    class Scheduler;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief general server
    ////////////////////////////////////////////////////////////////////////////////

    template<typename S, typename HF, typename CT>
    class GeneralServer : protected TaskManager {
      GeneralServer (GeneralServer const&);
      GeneralServer const& operator= (GeneralServer const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new general server
        ////////////////////////////////////////////////////////////////////////////////

        explicit
        GeneralServer (Scheduler* scheduler)
          : _scheduler(scheduler), _handlerFactory(0), _maintenance(false), _maintenanceLock() {
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief destructs a general server
        ////////////////////////////////////////////////////////////////////////////////

        ~GeneralServer () {
          if (_handlerFactory != 0) {
            delete _handlerFactory;
          }
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief sets a handler factory
        ///
        /// Note that the general server claims the ownership of the factory.
        ////////////////////////////////////////////////////////////////////////////////

        void setHandlerFactory (HF* factory) {
          if (_handlerFactory != 0) {
            delete _handlerFactory;
          }

          _handlerFactory = factory;
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief activates the maintenance mode
        ////////////////////////////////////////////////////////////////////////////////

        void activateMaintenance () {
          WRITE_LOCKER(_maintenanceLock);

          _maintenance = 1;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief deactivates the maintenance mode
        ////////////////////////////////////////////////////////////////////////////////

        void deactivateMaintenance () {
          WRITE_LOCKER(_maintenanceLock);

          _maintenance = 0;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief creates a new request
        ////////////////////////////////////////////////////////////////////////////////

        typename HF::GeneralRequest * createRequest (char const* ptr, size_t length) {
          READ_LOCKER(_maintenanceLock);

          if (_maintenance) {
            return ((S*) this)->createMaintenanceRequest(ptr, length);
          }

          return _handlerFactory == 0 ? 0 : _handlerFactory->createRequest(ptr, length);
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief creates a new request
        ////////////////////////////////////////////////////////////////////////////////

        typename HF::GeneralHandler * createHandler (typename HF::GeneralRequest* request) {
          READ_LOCKER(_maintenanceLock);

          if (_maintenance) {
            return ((S*) this)->createMaintenanceHandler();
          }

          return _handlerFactory == 0 ? 0 : _handlerFactory->createHandler(request);
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief destroys a finished handler
        ////////////////////////////////////////////////////////////////////////////////

        void destroyHandler (typename HF::GeneralHandler* handler) {
          if (_handlerFactory == 0) {
            delete handler;
          }
          else {
            _handlerFactory->destroyHandler(handler);
          }
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns header and body size restrictions
        ////////////////////////////////////////////////////////////////////////////////

        pair<size_t, size_t> sizeRestrictions () {
          static size_t m = (size_t) -1;

          if (_handlerFactory == 0) {
            return make_pair(m, m);
          }
          else {
            return _handlerFactory->sizeRestrictions();
          }
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief starts listening for general connections on given port
        ///
        /// The token is passed to the listen task which sends it back when accepting
        /// a connection.
        ////////////////////////////////////////////////////////////////////////////////

        bool addPort (int port, bool reuseAddress)  {
          return addPort("", port, reuseAddress);
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief starts listening for general connections on given port and address
        ///
        /// The token is passed to the listen task which sends it back when accepting
        /// a connection.
        ////////////////////////////////////////////////////////////////////////////////

        bool addPort (string const& address, int port, bool reuseAddress) {
          ListenTask* task = new GeneralListenTask<S>(dynamic_cast<S*>(this), address, port, reuseAddress);

          if (! task->isBound()) {
            deleteTask(task);
            return false;
          }

          _scheduler->registerTask(task);

          return true;
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief handles connection request
        ////////////////////////////////////////////////////////////////////////////////

        virtual void handleConnected (socket_t socket, ConnectionInfo& info) {
          SocketTask* task = new SpecificCommTask<S, HF, CT>(dynamic_cast<S*>(this), socket, info);

          _scheduler->registerTask(task);
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief handles a request
        ////////////////////////////////////////////////////////////////////////////////

        virtual bool handleRequest (CT * task, typename HF::GeneralHandler * handler) {

          // execute handle and requeue
          bool done = false;

          while (! done) {
            Handler::status_e status = handleRequestDirectly(task, handler);

            if (status != Handler::HANDLER_REQUEUE) {
              done = true;
              destroyHandler(handler);
            }
            else {
              continue;
            }
          }

          return true;
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief handles a connection close
        ////////////////////////////////////////////////////////////////////////////////

        void handleCommunicationClosed (Task* task)  {
          _scheduler->destroyTask(task);
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief handles a connection failure
        ////////////////////////////////////////////////////////////////////////////////

        void handleCommunicationFailure (Task* task)  {
          _scheduler->destroyTask(task);
        }

      protected:

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

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief the scheduler
        ////////////////////////////////////////////////////////////////////////////////

        Scheduler* _scheduler;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief the handler factory
        ////////////////////////////////////////////////////////////////////////////////

        HF* _handlerFactory;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief maintenance mode
        ////////////////////////////////////////////////////////////////////////////////

        bool _maintenance;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief maintenance mode lock
        ////////////////////////////////////////////////////////////////////////////////

        basics::ReadWriteLock _maintenanceLock;
    };
  }
}

#endif
